# 7. Check and analyze traces

## 7.1 Quick check: `check_trace.py`

```powershell
python utils\CheckTrace\check_trace.py <trace.csv>          # --plot: gaze eccentricity over time (needs matplotlib)
python utils\CheckTrace\check_trace.py examples\sample_trace_vive_pro_eye.csv
```

It only uses the Python standard library. It reports:

- column count and strict L/R alternation;
- duration and frame rate (warning below 45 frames/s);
- quaternion norms;
- **% of rows with valid gaze**;
- gaze eccentricity (angle between head forward and gaze, typically below 30°);
- gaze-origin offset;
- pupil diameter and eye openness statistics.

Exit code `1` means **no valid gaze** in the file; `2` means a format error. It also accepts the old 24-column EyeNavGS files.

Output for the sample trace (headset held, not worn: low valid-gaze rate and frame rate, see [`examples/`](../examples/README.md)):

```
Rows           : 222 (left: 111, right: 111, strictly alternating L/R: True)
Duration       : 14.96 s -> 7.4 frames/s (one frame = left+right rows)
Valid gaze     : 34/222 rows (15.3 %) [GazeValid column] | source(s): HTC_eye_tracker
Gaze vs head   : angle (deg) median 13.28 | p5 5.08 | p95 32.74 | min 4.27 | max 36.25
Gaze origin    : distance to eye position (cm) median 4.21 | p5 3.93 | p95 4.41 | min 3.93 | max 18.36
PupilDiameter_mm: 37/222 rows (16.7 %) mm median 4.33 | p5 3.77 | p95 4.74 | min 3.74 | max 5.10
EyeOpenness    : 40/222 rows (18.0 %) [0-1] median 1.00 | p5 0.50 | p95 1.00 | min 0.04 | max 1.00
RESULT: OK, the trace contains eye-tracking data.
```

With a calibrated participant wearing the headset, most rows should be valid; short runs of `GazeValid=0` are blinks.

## 7.2 Load a trace in Python

```python
import numpy as np, pandas as pd
from scipy.spatial.transform import Rotation as R

df = pd.read_csv("truck_trace_0.csv")
left = df[(df.ViewIndex == 0) & (df.GazeValid == 1)]
q_head = left[["QuaternionX", "QuaternionY", "QuaternionZ", "QuaternionW"]].to_numpy()   # xyzw
q_gaze = left[["GazeQX", "GazeQY", "GazeQZ", "GazeQW"]].to_numpy()                       # xyzw, world frame
gaze_dir = R.from_quat(q_gaze).apply([0, 0, -1])                                         # OpenXR forward = -Z
head_dir = R.from_quat(q_head).apply([0, 0, -1])
ecc_deg = np.degrees(np.arccos(np.clip((gaze_dir * head_dir).sum(1), -1, 1)))
```

`scipy` and `pandas` also use the **x, y, z, w** order, like the CSV.

## 7.3 Replay a session

The replay modes read the first 12 columns by position, so they accept the new files:

```powershell
cd install\bin
# in the headset (also writes one video per eye), same initialization as the recording:
.\SIBR_gaussianViewer_app.exe -m D:\scenes\truck -in <trace.csv> --rendering-mode 3 --initial-position -2 1.8 -4 --initial-quaternion -0.0872 0 0 0.9962 --initial-scale 0.8
# on the desktop (side by side), <w> <h> = "Headset resolution (per eye)" shown in the OpenXR panel:
.\SIBR_gaussianViewer_app.exe -m D:\scenes\truck -in <trace.csv> --rendering-mode 4 --rendering-size <w> <h>
```

## 7.4 EyeNavGS utilities (`utils/`)

These are described in the [original README](../docs/ORIGINAL_README.md#eyenavgs-utility-tools):

| Tool | Notes on the new traces |
|---|---|
| `AddEyeGazeTracking/` (gaze overlay on replay videos) | Uses `ViewIndex`, the FOVs and gaze (columns 1–19). |
| `WorldCoordConvert/` | Expects the Rutgers dataset files (column `Timestamp`, file names `user<ID>_<scene>.csv`). |
| `JsonCsvTraceConvert/` (`CsvToJson`) | Accepts only 12-column rows: keep the first 12 columns first. |

Scripts written for the NTHU dataset, which select rows with `timestep` and `ViewIndex`, work directly: `timestep` is column 34.

---
[← 6. Trace format](06_trace_format.md) · [Tutorial index](README.md) · [8. Troubleshooting →](08_troubleshooting.md)
