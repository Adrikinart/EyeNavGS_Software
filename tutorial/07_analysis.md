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

Exit codes:

| Code | Meaning |
|---|---|
| `0` | The trace contains valid eye-tracking data. |
| `1` | **No valid gaze** in the file. |
| `2` | Format error: unreadable file, missing columns, bad command line. |

It also accepts the old 24-column EyeNavGS files.

Full output for the sample trace (headset held, not worn: low valid-gaze rate and frame rate, see [`examples/`](../examples/README.md)):

```
File           : examples\sample_trace_vive_pro_eye.csv
Columns        : 34 -> ViewIndex,FOV1,FOV2,FOV3,FOV4,...,PupilDiameter_mm,PupilPosX,PupilPosY,EyeOpenness,timestep
Rows           : 222 (left: 111, right: 111, strictly alternating L/R: True)
Duration       : 14.96 s -> 7.4 frames/s (one frame = left+right rows)
WARNING        : low frame rate. One row pair is written per rendered frame, so this is also the trace sampling rate. Use a lighter scene or --downscale 2 (or the 'Down scale factor' slider), and keep the headset worn (SteamVR throttles a headset in standby).
Start / end    : 2026-11-23T18:01:51.757+01:00  ->  2026-11-23T18:02:06.715+01:00
|head quat|    : median 1.00 | p5 1.00 | p95 1.00 | min 1.00 | max 1.00
|gaze quat|    : median 1.00 | p5 1.00 | p95 1.00 | min 1.00 | max 1.00
Valid gaze     : 34/222 rows (15.3 %) [GazeValid column] | source(s): HTC_eye_tracker
Gaze vs head   : angle (deg) median 13.28 | p5 5.08 | p95 32.74 | min 4.27 | max 36.25
Gaze origin    : distance to eye position (cm) median 4.21 | p5 3.93 | p95 4.41 | min 3.93 | max 18.36
PupilDiameter_mm: 37/222 rows (16.7 %) mm median 4.33 | p5 3.77 | p95 4.74 | min 3.74 | max 5.10
EyeOpenness    : 40/222 rows (18.0 %) [0-1] median 1.00 | p5 0.50 | p95 1.00 | min 0.04 | max 1.00

RESULT: OK, the trace contains eye-tracking data.
```

(The `Columns` line is shortened here.)

With a calibrated participant wearing the headset, most rows should be valid; short runs of `GazeValid=0` are blinks.

## 7.2 Load a trace in Python

This example needs `numpy`, `pandas` and `scipy` (`pip install numpy pandas scipy`), unlike `check_trace.py`.

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

`scipy`'s `Rotation.from_quat` uses the **x, y, z, w** order (scalar last), like the CSV.

## 7.3 Replay a session

The replay modes read the first 12 columns by position, so they accept the new files. They use the recorded poses as they are, which already include the scene initialization, so no `--initial-*` option is needed:

```powershell
cd install\bin
# in the headset (also writes one video per eye, <scene>_left.mp4 / <scene>_right.mp4, in the working directory):
.\SIBR_gaussianViewer_app.exe -m D:\scenes\truck -in D:\recordings\P01\truck_trace_0.csv --rendering-mode 3
# on the desktop (side by side); replace 2016 2240 by the "Headset resolution (per eye)" shown in the OpenXR panel:
.\SIBR_gaussianViewer_app.exe -m D:\scenes\truck -in D:\recordings\P01\truck_trace_0.csv --rendering-mode 4 --rendering-size 2016 2240
cd ..\..   # back to the repository root
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
