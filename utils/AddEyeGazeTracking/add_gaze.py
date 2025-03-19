import numpy as np
import pandas as pd
import cv2
from scipy.spatial.transform import Rotation as R
import argparse
from tqdm import tqdm
import os

def parse_csv(file_path):
    """
    Parse the CSV file containing eye gaze data.
    This function attempts to read the CSV directly. If it fails,
    it tries to read the file line by line and parse it manually.
    """
    try:
        df = pd.read_csv(file_path)
    except:
        try:
            data = []
            columns = [
                "ViewIndex", "FOV1", "FOV2", "FOV3", "FOV4",
                "PositionX", "PositionY", "PositionZ",
                "QuaternionX", "QuaternionY", "QuaternionZ", "QuaternionW",
                "GazeQX", "GazeQY", "GazeQZ", "GazeQW",
                "GazePosX", "GazePosY", "GazePosZ"
            ]
            
            with open(file_path, 'r') as f:
                lines = f.readlines()
                
                # If the first line is not purely numeric, treat it as a header and skip it
                first_line = lines[0].strip().split()
                if not all(item.replace('-', '').replace('.', '').isdigit() for item in first_line):
                    lines = lines[1:]  # Skip header line
                
                for line in lines:
                    values = line.strip().split()
                    if len(values) == len(columns):
                        data.append(values)
            
            df = pd.DataFrame(data, columns=columns)
            # Convert all columns to float except "ViewIndex"
            for col in df.columns:
                if col != "ViewIndex":
                    df[col] = df[col].astype(float)
            df["ViewIndex"] = df["ViewIndex"].astype(int)
        except Exception as e:
            print(f"Error parsing CSV file: {e}")
            raise
    
    return df

def project_gaze_to_screen_openxr(row, screen_width=2160, screen_height=2224):
    """
    Project 3D gaze to 2D screen coordinates using OpenXR FOV values,
    by computing the relative rotation between the camera quaternion and the gaze quaternion.
    Applies a correction of 4.19° leftward for left eye and 4.19° rightward for right eye.
    """
    # 1. Extract camera and gaze quaternions
    cam_quat = [row.QuaternionX, row.QuaternionY, row.QuaternionZ, row.QuaternionW]
    gaze_quat = [row.GazeQX, row.GazeQY, row.GazeQZ, row.GazeQW]
    
    # 2. Compute relative rotation: Q_rel = Q_cam^-1 * Q_gaze
    R_cam = R.from_quat(cam_quat)
    R_cam_inv = R_cam.inv()
    R_gaze = R.from_quat(gaze_quat)
    R_rel = R_cam_inv * R_gaze
    
    # 3. Rotate the default direction (-Z) by Q_rel to obtain gaze_dir
    gaze_dir = R_rel.apply([0, 0, -1])  # (x, y, z)
    
    # 4. Apply the 14+1.55°(deviation of FOV and eye gaze) correction based on ViewIndex
    correction_angle = np.radians(15.55)  # Convert 15.5° to radians
    
    # Create rotation matrix for correction around Y-axis
    if row.ViewIndex == 0:  # Left eye - rotate leftward (negative angle around Y)
        correction_rot = R.from_euler('y', correction_angle)
    else:  # Right eye - rotate rightward (positive angle around Y)
        correction_rot = R.from_euler('y', -correction_angle)
    
    # Apply correction to gaze direction
    gaze_dir = correction_rot.apply(gaze_dir)
    
    # 5. Depending on ViewIndex, select FOV angles (angleLeft, angleRight, angleDown, angleUp)
    if row.ViewIndex == 0:  # Left eye
        angle_left = row.FOV1
        angle_right = row.FOV2
        angle_down = row.FOV3
        angle_up = row.FOV4
    else:  # Right eye
        angle_left = row.FOV1
        angle_right = row.FOV2
        angle_down = row.FOV3
        angle_up = row.FOV4
    
    # 6. Perform asymmetric frustum projection
    if gaze_dir[2] != 0:
        h_ratio = gaze_dir[0] / -gaze_dir[2]
        v_ratio = gaze_dir[1] / -gaze_dir[2]
        # Map h_ratio to [tan(angle_left), tan(angle_right)] → [0,1]
        x_normalized = (h_ratio - np.tan(angle_left)) / (np.tan(angle_right) - np.tan(angle_left))
        # Map v_ratio to [tan(angle_down), tan(angle_up)] → [0,1], then flip Y
        y_normalized = 1.0 - (v_ratio - np.tan(angle_down)) / (np.tan(angle_up) - np.tan(angle_down))
        screen_x = x_normalized * screen_width
        screen_y = y_normalized * screen_height
        # Clamp to screen boundaries
        screen_x = max(0, min(screen_x, screen_width))
        screen_y = max(0, min(screen_y, screen_height))
        return [screen_x, screen_y]
    else:
        # If gaze_dir[2] == 0, default to the center of the screen
        return [screen_width/2, screen_height/2]

def overlay_circle_with_alpha(frame, center_x, center_y, radius=20, alpha=0.2):
    """
    Overlay a semi-transparent red circle (alpha blending) on the given frame.
    The blending formula is: result = overlay * alpha + frame * (1 - alpha).
    """
    overlay = frame.copy()
    cv2.circle(overlay, (center_x, center_y), radius, (0, 0, 255), -1)  # Draw a solid red circle on the overlay
    blended = cv2.addWeighted(overlay, alpha, frame, 1 - alpha, 0)
    return blended

def create_eye_videos_with_overlay(df, 
                                   left_video_path, 
                                   right_video_path, 
                                   output_prefix, 
                                   alpha=0.2):
    """
    Given two input videos (left eye and right eye), this function reads frames from each,
    computes the gaze projection for each frame, and overlays a semi-transparent red circle
    on the original frames. The modified frames are then written to new video files.
    """
    # Separate data for left and right eyes
    left_eye_data = df[df["ViewIndex"] == 0].reset_index(drop=True)
    right_eye_data = df[df["ViewIndex"] == 1].reset_index(drop=True)
    
    # Open the left-eye video
    left_cap = cv2.VideoCapture(left_video_path)
    left_fps = left_cap.get(cv2.CAP_PROP_FPS)
    left_width = int(left_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    left_height = int(left_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    left_frames = int(left_cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    # Open the right-eye video
    right_cap = cv2.VideoCapture(right_video_path)
    right_fps = right_cap.get(cv2.CAP_PROP_FPS)
    right_width = int(right_cap.get(cv2.CAP_PROP_FRAME_WIDTH))
    right_height = int(right_cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
    right_frames = int(right_cap.get(cv2.CAP_PROP_FRAME_COUNT))
    
    # Create output video files
    fourcc = cv2.VideoWriter_fourcc(*'mp4v')
    left_eye_output = f"{output_prefix}_left_eye_overlay.mp4"
    right_eye_output = f"{output_prefix}_right_eye_overlay.mp4"
    
    left_out = cv2.VideoWriter(
        left_eye_output, 
        fourcc, 
        left_fps if left_fps > 0 else 30,
        (left_width, left_height)
    )
    right_out = cv2.VideoWriter(
        right_eye_output, 
        fourcc, 
        right_fps if right_fps > 0 else 30, 
        (right_width, right_height)
    )
    
    # Process the left-eye video
    print("Processing left eye video with overlay...")
    min_left_frames = min(left_frames, len(left_eye_data))
    for i in tqdm(range(min_left_frames)):
        ret, frame = left_cap.read()
        if not ret:
            break
        
        row = left_eye_data.iloc[i]
        # Compute the gaze projection
        screen_pos = project_gaze_to_screen_openxr(row, left_width, left_height)
        x, y = int(screen_pos[0]), int(screen_pos[1])
        
        # Overlay the semi-transparent circle on the original frame
        blended_frame = overlay_circle_with_alpha(frame, x, y, radius=500, alpha=alpha)
        
        # Optionally add text
        cv2.putText(
            blended_frame, 
            f"Left Eye - Frame: {i}", 
            (50, 50), 
            cv2.FONT_HERSHEY_SIMPLEX, 
            1, 
            (0, 0, 0), 
            2
        )
        
        left_out.write(blended_frame)
    
    # Process the right-eye video
    print("Processing right eye video with overlay...")
    min_right_frames = min(right_frames, len(right_eye_data))
    for i in tqdm(range(min_right_frames)):
        ret, frame = right_cap.read()
        if not ret:
            break
        
        row = right_eye_data.iloc[i]
        # Compute the gaze projection
        screen_pos = project_gaze_to_screen_openxr(row, right_width, right_height)
        x, y = int(screen_pos[0]), int(screen_pos[1])
        
        # Overlay the semi-transparent circle
        blended_frame = overlay_circle_with_alpha(frame, x, y, radius=500, alpha=alpha)
        
        cv2.putText(
            blended_frame, 
            f"Right Eye - Frame: {i}", 
            (50, 50), 
            cv2.FONT_HERSHEY_SIMPLEX, 
            1, 
            (0, 0, 0), 
            2
        )
        
        right_out.write(blended_frame)
    
    # Release resources
    left_cap.release()
    right_cap.release()
    left_out.release()
    right_out.release()
    
    print(f"Left eye video saved to: {left_eye_output}")
    print(f"Right eye video saved to: {right_eye_output}")

def main():
    parser = argparse.ArgumentParser(description='Overlay gaze on existing left/right eye videos.')
    parser.add_argument('input_csv', help='Path to the input CSV file with eye gaze data.')
    parser.add_argument('left_video', help='Path to the input left-eye video.')
    parser.add_argument('right_video', help='Path to the input right-eye video.')
    parser.add_argument('--output', '-o', default='eye_gaze', help='Output video file prefix.')
    parser.add_argument('--alpha', type=float, default=0.2, help='Transparency of the red circle overlay (0.0 - 1.0).')
    args = parser.parse_args()
    
    # Read the CSV file
    print(f"Parsing CSV file: {args.input_csv}")
    df = parse_csv(args.input_csv)
    
    # Generate videos with gaze overlay
    create_eye_videos_with_overlay(
        df,
        args.left_video,
        args.right_video,
        args.output,
        alpha=args.alpha
    )

if __name__ == "__main__":
    main()
