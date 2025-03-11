# Eye Gaze Overlay Tool

This repository provides a Python script that **overlays eye gaze positions** (from a trajectory CSV file) onto **two input videos** (left-eye video and right-eye video). The script computes the eye gaze position per frame based on **camera quaternions**, **eye gaze quaternions**, and **FOV** parameters, then draws a **semi-transparent red circle** on the corresponding frame of each video.

---

## 1. Installation Requirements

- **Python 3.7+** (tested on Python 3.8+)
- **pip** or **conda** for package management

### Required Python Packages

- **numpy**
- **pandas**
- **opencv-python** (or `opencv-contrib-python` if you need extra OpenCV modules)
- **scipy** (for `scipy.spatial.transform.Rotation`)
- **tqdm** (for progress bars)
- **argparse** (usually included in standard Python library)

You can install these dependencies with:
```bash
pip install numpy pandas opencv-python scipy tqdm
```

## 2. How to Run

To use this script for overlaying eye gaze on two input videos (left-eye and right-eye), follow the steps below:

## Command Line Usage

```bash
python add_gaze.py \
    path/to/input.csv \
    path/to/left_eye_video.mp4 \
    path/to/right_eye_video.mp4 \
    --output output \
    --alpha 0.2
```
## Arguments

| **Argument**       | **Description**                                                                                                                                                                                          | **Default** |
|--------------------|----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|------------|
| `input_csv`        | Path to the **CSV file** containing eye gaze data. Must include columns for `ViewIndex`, `FOV1-FOV4`, camera quaternions (`QuaternionX`, `QuaternionY`, `QuaternionZ`, `QuaternionW`), and gaze quaternions (`GazeQX`, `GazeQY`, `GazeQZ`, `GazeQW`). | *(required)* |
| `left_video`       | Path to the **left-eye** input video. The script will overlay the gaze data corresponding to `ViewIndex = 0` on this video.                                                                              | *(required)* |
| `right_video`      | Path to the **right-eye** input video. The script will overlay the gaze data corresponding to `ViewIndex = 1` on this video.                                                                             | *(required)* |
| `--output, -o`     | Output file prefix for the resulting videos. Two files will be created: `<prefix>_left_eye_overlay.mp4` and `<prefix>_right_eye_overlay.mp4`.                                                             | `eye_gaze`  |
| `--alpha`          | The **transparency** of the red overlay circle. A value of `0.2` means 20% opaque and 80% transparent. Valid range is `0.0` (fully transparent) to `1.0` (fully opaque).                                  | `0.2`       |
| `--width, -W`      | (Optional) Screen width in pixels. Only used if you modify the script to create blank backgrounds or for other custom usage.                                                                              | `1920`      |
| `--height, -H`     | (Optional) Screen height in pixels. Same usage context as `--width`.                                                                                                                                    | `1080`      |
| `--fps`            | (Optional) Frames per second for the output videos if needed. If you are overlaying onto existing videos, the script will typically use the input video's frame rate.                                                                          | `30`        |
