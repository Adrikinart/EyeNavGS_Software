# 5. Record a session

## 5.1 Checklist before each session

- [ ] Base stations on, headset tracked (solid icon in SteamVR).
- [ ] SteamVR is the active OpenXR runtime (`powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1 -VROnly`).
- [ ] Participant calibrated ([step 3.5](03_vr_eye_tracking_setup.md#35-eye-calibration-each-participant-each-session)).
- [ ] Scene folder ready, with its `eyenavgs_spawn.json` or known initialization values ([step 4](04_scenes.md#scene-initialization-tilt-scale-start-position)).
- [ ] Participant in the middle of the play area, facing its forward direction (the direction chosen in Room Setup). With a spawn file, they then face the content.

## 5.2 Launch with the script

```powershell
# manual start/stop with the buttons of the desktop window
powershell -ExecutionPolicy Bypass -File scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\truck -Participant P01

# fixed-length recording: starts automatically as soon as the app is focused in the headset, stops after 120 s
powershell -ExecutionPolicy Bypass -File scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\truck -Participant P01 `
    -Duration 120 -InitialPosition "-2 1.8 -4" -InitialQuaternion "-0.0872 0 0 0.9962" -InitialScale 0.8
```

Before launching the viewer, the script:

1. checks the viewer build and the scene folder, and reads the scene's `eyenavgs_spawn.json` if there is one and no `-Initial*` value is given;
2. checks that SteamVR is the OpenXR runtime;
3. enables the VIVE eye-tracking layer for this session if it is off in SteamVR;
4. makes sure **SR_Runtime 1.3.6.x** is the SRanipal runtime that runs ([why](03_vr_eye_tracking_setup.md#33-the-sranipal-version-trap)).

| Parameter | Meaning |
|---|---|
| `-Scene` | 3DGS model folder (mandatory). |
| `-Participant` | Tag used in the output folder name. |
| `-OutDir` | Output folder (default `<repo>\recordings\<scene>\<participant>_<yyyyMMdd_HHmmss>\`, or `session_<...>` without `-Participant`). |
| `-Duration` | > 0: automatic recording of N seconds. |
| `-Downscale` | 1–8, divides the per-eye rendering resolution (higher = faster, [§5.5](#55-frame-rate--sampling-rate)). |
| `-InitialPosition`, `-InitialQuaternion`, `-InitialScale` | Scene initialization ([step 4](04_scenes.md#scene-initialization-tilt-scale-start-position)). Takes precedence over a spawn file. |
| `-SpawnFile <json>` | Spawn file to use (default: `<scene>\eyenavgs_spawn.json` if it exists). |
| `-NoSpawn` | Ignore the spawn file: raw scene orientation and scale. |
| `-Iteration` | 3DGS iteration to load (default: the largest). |
| `-EyeTracking` | `auto` (default), `htc`, `ext`, `fb`, `none`. |
| `-SkipSRanipal` | Do not check or start SR_Runtime. |
| `-Exe <path>` | Viewer to run (default `install\bin\SIBR_gaussianViewer_app.exe`). Needed for a `RelWithDebInfo` / `Debug` build (`..._rwdi.exe` / `..._d.exe`). |

The output folder receives:

- the trace(s) `<scene>_trace_<n>.csv`;
- `viewer_console.log`, the full viewer output;
- `session_info.txt`, with the arguments and environment;
- `eyenavgs_spawn.json`, a copy of the spawn file used, if any. `check_trace.py` reads its scale to report distances in cm.

When you close the viewer, every new trace is checked with `check_trace.py` ([step 7](07_analysis.md)), provided a real Python is found. Otherwise the script says so and you can run it by hand.

## 5.3 In the desktop window and in the headset

The desktop window mirrors both eyes. Its **"OpenXR"** panel shows:

- **Eye tracking: HTC_eye_tracker** and a status line:
  - green **`gaze valid`**: everything works;
  - orange: the problem, for example:
    - `headset NOT tracked`;
    - `waiting for the application to be focused` (close the SteamVR dashboard);
    - `headset tracked: starting eye tracking...` (the first second);
    - `no valid gaze (headset worn? eyes visible? calibrated?)`.
- **Start Trace Recording** / **End Trace Recording**:
  - each recording goes to a **new** `<scene>_trace_<n>.csv` (never overwritten, never appended);
  - rows are flushed every frame, so a crash loses at most one frame;
  - closing the window also closes the file properly.
- While recording, a red **REC** line shows the file, the row count and the live % of valid gaze.
- **Recording and Saving**: same as Start, plus one video per eye (`<scene>_left.mp4`, `<scene>_right.mp4`) and a CSV named `<scene>_output_<n>.csv`.
  - This is heavy: it reads the textures back every frame.
  - The video names have **no index**: the next "Recording and Saving" or headset replay in the same folder overwrites them.
- **Down scale factor** slider, the frame rate, the IPD, *Free world standing / Seated*, *Save VR configuration* (`vr.json` in the scene folder).

In the headset, with the VIVE wands:

- **press** the left trackpad up/down to raise or lower the viewpoint;
- **press** the right trackpad to move forward/back or to strafe.

Physical walking in the play area works as usual. The controller-driven offset is included in the recorded positions.

## 5.4 Launch manually (without the script)

```powershell
# 1. SR_Runtime 1.3.6.x running (step 3.3); VIVE layer enabled in SteamVR (step 3.4), or:
$env:XR_API_LAYER_PATH    = "${env:ProgramFiles(x86)}\Steam\steamapps\common\VIVEDriver\App\ViveVRRuntime\ViveVR_openxr\ViveOpenXRFacialTracking"
$env:XR_ENABLE_API_LAYERS = "XR_APILAYER_VIVE_facial_tracking"
# 2. viewer
cd install\bin
.\SIBR_gaussianViewer_app.exe -m D:\scenes\truck --rendering-mode 2 --trace-dir D:\recordings\P01
cd ..\..   # back to the repository root
```

Viewer options for headset mode (`--rendering-mode 2`):

| Option | Meaning |
|---|---|
| `-m <folder>` | 3DGS model folder. |
| `--iteration <n>` | 3DGS iteration to load. |
| `--initial-position X Y Z`, `--initial-quaternion QX QY QZ QW`, `--initial-scale S` | Scene initialization. |
| `--trace-dir <dir>` | Where CSVs and videos are written (created if needed). Default: the working directory. |
| `--record-trace` | Start recording automatically once the app is focused in the headset. |
| `--record-duration <s>` | With `--record-trace`: stop after N seconds. |
| `--downscale <n>` | Per-eye resolution divider, 1–8. |

Diagnostic environment variables. Set them in PowerShell before launching the viewer; they apply to that PowerShell window:

| PowerShell | Effect |
|---|---|
| `$env:EYENAVGS_EYE_TRACKING = "htc"` (or `fb`, `ext`, `none`) | Force the eye-tracking backend. The launcher sets it from `-EyeTracking` instead. |
| `$env:XR_LOADER_DEBUG = "info"` | The OpenXR loader prints which API layers it loads. |
| `$env:EYENAVGS_GAZE_DEBUG = "1"` | Trace every OpenXR call of the frame loop (very verbose; shows where a runtime or layer crashes). |

Remove one with `Remove-Item Env:\EYENAVGS_GAZE_DEBUG`.

## 5.5 Frame rate = sampling rate

**One row is written per eye and per rendered frame.** The trace sampling rate is the rendering frame rate, not the eye tracker's 120 Hz.

- Check the *Framerate* line of the OpenXR panel. `check_trace.py` warns below 45 frames/s.
- Use `-Downscale 2` (or the slider) for big scenes, or a lighter iteration (`-Iteration 7000`).
- Keep the headset **worn**. When nobody wears it, SteamVR throttles the app to a few frames/s and gaze is invalid.

---
[← 4. Scenes](04_scenes.md) · [Tutorial index](README.md) · [6. Trace format →](06_trace_format.md)
