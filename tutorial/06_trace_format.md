# 6. Trace format reference

File name: `<scene>_trace_<n>.csv`, or `<scene>_output_<n>.csv` for "Recording and Saving".

- `<n>` starts at 0 and increases with each recording of the same viewer session. The counter is shared by `_trace_` and `_output_` files.
- An existing file is never reused: indices already taken in the output folder are skipped.

The file is comma-separated, with one header line. Each rendered frame gives **two rows, left eye (0) then right eye (1)**, strictly alternating from the first row. A sample file is in [`examples/`](../examples/).

## 6.1 Columns (34)

Columns 1–24 keep the **original EyeNavGS format** (names, order, meaning), so the dataset tools, the replay modes and existing scripts keep working. Columns 25–34 are new.

| # | Column | Meaning |
|---|---|---|
| 1 | `ViewIndex` | 0 = left eye, 1 = right eye. |
| 2–5 | `FOV1..FOV4` | OpenXR `angleLeft, angleRight, angleDown, angleUp` (radians; FOV1 and FOV3 are negative). *The upstream README (commit `87d02bf`) said left/right/top/bottom.* |
| 6–8 | `PositionX/Y/Z` | Eye (view) position, see [§6.2](#62-coordinate-frames). |
| 9–12 | `QuaternionX/Y/Z/W` | Eye orientation, **x,y,z,w** (scalar last). |
| 13–16 | `GazeQX/Y/Z/W` | Gaze orientation in the **same frame** as the eye pose (not head-relative), xyzw. Gaze direction = R(GazeQ)·(0,0,−1). `0,0,0,1` when invalid. |
| 17–19 | `GazePosX/Y/Z` | Gaze origin in the same frame, `0,0,0` when invalid. With the VIVE layer it is about 4 cm from the render eye position. |
| 20 | `timestamp` | µs from a **monotonic** clock (QueryPerformanceCounter, origin = boot). **Not Unix time**, despite the name (kept for compatibility). |
| 21 | `realtime` | Local wall-clock time `YYYY-MM-DD hh:mm:ss.mmm`. |
| 22 | `elapsed_ms` | ms since the start of the recording (3 decimals). |
| 23 | `iso_time` | ISO 8601 local time with UTC offset (daylight saving time included). |
| 24 | `epoch_ms` | `timestamp` / 1000. |
| 25 | `GazeValid` | 1 if the eye tracker delivered a valid gaze for this eye and frame. Blinks, eyes not found, ... give 0. **Filter on it.** |
| 26 | `GazeTracked` | 1 if the gaze is an actual measurement. Same as `GazeValid` for HTC/FB; for EXT it is the `ORIENTATION_TRACKED` bit. |
| 27 | `GazeSource` | `HTC_eye_tracker`, `EXT_eye_gaze_interaction`, `FB_eye_tracking_social` or `none`. |
| 28 | `xr_display_time_ns` | OpenXR predicted display time of the frame (ns). On SteamVR/Windows it uses the same time base as `timestamp` (ns vs µs). It is usually a few tens of ms ahead of `timestamp`, but it can lag behind (up to seconds) while SteamVR throttles the app, e.g. when the headset is not worn. |
| 29 | `gaze_sample_time_ns` | Eye-tracker sample time as reported by the runtime (0 if unknown). **Not reliable with the VIVE layer**: sometimes a copy of the requested time, sometimes stale. |
| 30 | `PupilDiameter_mm` | Pupil diameter in mm (HTC only; `nan` if invalid or out of range). |
| 31–32 | `PupilPosX/Y` | Normalized pupil position in the eye-camera sensor area, [0,1], +X right, +Y up (HTC only; `nan` otherwise). |
| 33 | `EyeOpenness` | 0 = closed, 1 = open (HTC only; `nan` if invalid. The layer sometimes returns garbage, which is filtered out). |
| 34 | `timestep` | Integer ms since the start of the recording, = round(`elapsed_ms`), as in the NTHU part of the EyeNavGS dataset. Scripts that select rows by `timestep` work unchanged. |

Precision:

- pose, FOV and gaze columns: 7 significant digits (the upstream version truncated them to 3 decimals);
- `elapsed_ms` and `epoch_ms`: 3 decimals;
- pupil and openness columns: 5 significant digits.

Example row (left eye, valid gaze):

```
0,-0.9363585,0.8817525,-0.9604573,0.9605297,-0.6765468,1.151603,0.8874995,-0.4022361,-0.529048,-0.3459201,0.6623093,
-0.5225479,-0.5241519,-0.3863053,0.5504333,-0.6898365,1.193966,0.8931141,354856496587,2026-11-23 17:56:34.372,13510.856,
2026-11-23T17:56:34.372+01:00,354856496.587,1,1,HTC_eye_tracker,354856420941800,354845669305610,4.6097,0.55873,0.40821,nan,13511
```

(one CSV line, wrapped here)

## 6.2 Coordinate frames

- **Frame:** the OpenXR **STAGE** space defined by SteamVR Room Setup (+x right, +y up, −z forward; meters).
  - It is re-based by `--initial-position` / `--initial-quaternion`, used as the pose of the reference space.
  - Positions are then scaled by `--initial-scale` around the initial position.
  - The controller-driven offset ("Movement") is added to both the eye positions and the gaze origin.
  - No axis flip is applied in the CSV.
- **Units:** positions are in metres only without initialization (`--initial-scale 1`). Otherwise they are in scene units: with a spawn file, `units_per_metre` of `eyenavgs_spawn.json` (6.3 for *figurines*). To get back to STAGE metres with `t`, `R` the initial position and rotation and `s` the scale:
  - `p_stage = R·(t + (p − t)/s) + t`;
  - `R_stage = R·R_csv`.
  - Distances divided by `s` are metres.
- **To go to the 3DGS scene frame** (+y down, +z forward), do what the renderer does with *Y-Invert scene* checked (the default): with `F = diag(1, −1, −1)`, use `p_3dgs = F·p` and `R_3dgs = F·R`, for eye and gaze poses alike.
- To reproduce a live session or compare it with the dataset, use the **same** `--initial-*` values as during the recording. The replay modes do not need them: the recorded poses already include the initialization.

## 6.3 Timing

- One row pair per **rendered frame**: the sampling rate is the rendering frame rate (see [step 5.5](05_recording.md#55-frame-rate--sampling-rate)).
- Gaze is located at the frame's predicted display time (`xr_display_time_ns`), like the head pose.
- For wall-clock alignment with other devices, use `realtime` / `iso_time`, which have ms resolution.
- For intervals, use `elapsed_ms` or `timestamp` (monotonic).

---
[← 5. Recording](05_recording.md) · [Tutorial index](README.md) · [7. Analysis →](07_analysis.md)
