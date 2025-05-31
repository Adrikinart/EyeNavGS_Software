# 👁️NavGS: A 6-DoF Navigation Dataset and Record-n-Replay Software for Real-World 3DGS Scenes in VR

-------------------------------------------------------------------------------------------------------

## Overview

**Eye Gaze Overlay Tool**    from `👁️NavGS(EyeNavGS)`

This utility folder provides two Python scripts that **overlay eye-gaze positions** onto **per-eye VR recordings** and then **merge** the left/right overlay outputs into one video. Given a **trajectory CSV file** containing head and gaze quaternions (with FOV parameters) and two input MP4 files (left-eye and right-eye views), the `add_gaze.py` script computes each frame’s gaze position in screen space and draws the gaze circle on both left- and right-eye frames. The `merge_eye_vid.py` script then stacks those two overlay videos side-by-side into a single MP4 for viewing.

Use This tool to visualize participant attention during VR replay sessions. The final output helps illustrate where users fixated throughout immersive navigation tasks.

---

## 1. Prerequisites

1. **Python 3.7+**

2. **Install required Python packages** (run from your virtual environment or system):
   
   ```bash
   pip install numpy pandas opencv-python scipy tqdm
   ```

3. **FFmpeg**
   
   - Install FFmpeg and ensure `ffmpeg` is in your system PATH.
   
   - Verify by running:
     
     ```bash
     ffmpeg -version
     ```

---

## 2. Folder Structure

```
utils/AddEyeGazeTracking/
├── add_gaze.py
├── merge_eye_vid.py
└── README.md
```

- **add_gaze.py**: Overlays gaze markers onto left/right eye MP4s using a matching CSV trace.

- **merge_eye_vid.py**: Combines the two overlay MP4s side-by-side into one stereoscopic video.

---

## 3. How to Use

### Step 1: Prepare Inputs

1. **Record per-eye VR videos** using the SIBR/EyeNavGS viewer. You should have:
   
   - Raw left-eye video: `left_eye_raw.mp4`
   
   - Raw right-eye video: `right_eye_raw.mp4`

2. **Obtain the matching eye-gaze CSV** (one row per frame per eye) with columns including `ViewIndex`, `FOV1-4`, head position/quaternion, gaze quaternion, and gaze position.

### Step 2: Run `add_gaze.py`

1. **Open a terminal in `utils/AddEyeGazeTracking/`**

2. **Execute the script**:
   
   ```bash
   python add_gaze.py \
    --csv /path/to/eye_trace.csv \
    --left /path/to/left_eye_raw.mp4 \
    --right /path/to/right_eye_raw.mp4 \
    --output /path/to/output_prefix
   ```
- Replace paths as needed. `output_prefix` will be used to name the overlay files:
  
  - `<output_prefix>_left_eye_overlay.mp4`
  
  - `<output_prefix>_right_eye_overlay.mp4`
  
  - `<output_prefix>_merged.mp4` (created automatically)

### Step 3: (Optional) Run `merge_eye_vid.py` Manually

If you only need to merge two existing overlay videos:

```bash
python merge_eye_vid.py \
  /path/to/left_eye_overlay.mp4 \
  /path/to/right_eye_overlay.mp4 \
  /path/to/output_combined.mp4
```

- **Arguments**:
  
  1. Path to left-eye overlay MP4
  
  2. Path to right-eye overlay MP4
  
  3. Path for the combined stereoscopic MP4

---

## 4. Configuration Tips

- **CSV Format**: Ensure your CSV has one row per eye frame, with `ViewIndex` = 0 (left) or 1 (right). If column names differ, update the parsing logic at the top of `add_gaze.py` before running.

- **Resolution Settings**: The default internal resolution is set for 2160×2224 px per eye. If your replay videos use a different resolution, edit the `screen_width` and `screen_height` values in `add_gaze.py`.

- **Overlay Style**: To adjust marker appearance (size, color, transparency), modify the arguments in the `overlay_circle_with_alpha()` calls inside `add_gaze.py`.

- **Output Paths**: Always use absolute or correctly resolved relative paths to avoid file-not-found errors.

---

## 5. Example Workflow

1. **Navigate** to the utility folder:
   
   ```bash
   cd utils/AddEyeGazeTracking/
   ```

2. **Run overlay generation**:
   
   ```bash
   python add_gaze.py \
    --csv ../../data/scene1_trace.csv \
    --left ../../data/scene1_left.mp4 \
    --right ../../data/scene1_right.mp4 \
    --output ../../results/scene1_gaze
   ```

3. **Verify outputs**:
   
   - `../../results/scene1_gaze_left_eye_overlay.mp4`
   
   - `../../results/scene1_gaze_right_eye_overlay.mp4`
   
   - `../../results/scene1_gaze_merged.mp4`

---

## 6. Troubleshooting

- **`ffmpeg` not found**: Install FFmpeg and add to PATH.

- **CSV rows mismatch**: Ensure the number of rows for each eye in CSV ≥ video frames.

- **Video won’t play**: Confirm FFmpeg run completed successfully; check for `_raw.mp4` if final merge failed. Or try VLC player.

- **Incorrect gaze position**: Verify FOV values and quaternion formats match those recorded by the viewer.
