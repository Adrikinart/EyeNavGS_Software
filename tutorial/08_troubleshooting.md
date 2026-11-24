# 8. Troubleshooting

Start with `powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1`: it detects most of the problems below.

## Build

| Symptom | Cause | Fix |
|---|---|---|
| `Compatibility with CMake < 3.5 has been removed` (xatlas) | CMake 4.x with old third-party libs | Fixed in the root `CMakeLists.txt`. With an old checkout, add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`. |
| `C2065 'initial_sacle'` / `C7555` (designated initializers) | Upstream bugs | Fixed in this repository. |
| nvcc: `unsupported Microsoft Visual Studio version` | MSVC ≥ 19.50 (VS 2026 toolset v145) with CUDA 12.x | Use toolset v143 (VS 2022 / Build Tools 2022). `build_eyenavgs.ps1` picks it. |
| `No CUDA toolset found` | CUDA installed before Visual Studio | Re-run the CUDA installer > Visual Studio Integration. |
| `The existing 'build' was configured with ...` | `build\` was configured with another Visual Studio | `build_eyenavgs.ps1 -Clean` |
| `python` opens the Microsoft Store | Store stub on the PATH | Install Python (python.org / Anaconda). It is not needed for the build. |
| `running scripts is disabled on this system` | PowerShell execution policy | `powershell -ExecutionPolicy Bypass -File <script>` |

## Running / recording

| Symptom | Cause | Fix |
|---|---|---|
| `Cannot determine type of dataset at /f:/...` | `cfg_args` points to the training PC | Harmless. |
| Viewer exits silently a few seconds after the headset shows the scene; event log `0xc0000409 ucrtbase.dll` | VIVE layer (SRanipal client 1.3.6.5) talking to an **old SR_Runtime 1.3.2**: `SR_Logs\ViveSR_Runtime.log` shows `Error! Call_ViveSR_API` | Run VIVE Console's SR_Runtime 1.3.6.12 ([step 3.3](03_vr_eye_tracking_setup.md#33-the-sranipal-version-trap)); `record_vive_pro_eye.ps1` does it. |
| Console: `XR_HTC_eye_tracker available: no` | VIVE layer disabled, or VIVE Console missing | [Step 3.4](03_vr_eye_tracking_setup.md#34-enable-the-vive-openxr-eye-tracking-layer). Check with `XR_LOADER_DEBUG=info` (`... succeeded loading layer XR_APILAYER_VIVE_facial_tracking`). |
| Status `headset NOT tracked` / SteamVR log `No base stations seen` | Base stations off or occluded | Power them on, check the line of sight, redo Room Setup. |
| Status `waiting for the application to be focused` | SteamVR dashboard or calibration overlay open | Close the dashboard. |
| `GazeValid` mostly 0 | Headset not worn or loose, not calibrated, eyes not visible (glasses, lashes, make-up) | Recalibrate; adjust strap and IPD. Short 0-runs are blinks and are normal. |
| Very few rows per second | Heavy scene / high resolution, or headset not worn (throttled) | `-Downscale 2`, `-Iteration 7000`; wear the headset. |
| Trace written in `install\bin` | No `--trace-dir` | Use the launcher, or `--trace-dir`. |
| `Could not open H264 video writers` | No H264 encoder | The viewer falls back to MPEG-4 (`mp4v`). |
| The OpenXR runtime is Meta/Oculus | Meta Horizon Link set as OpenXR runtime | SteamVR > Settings > OpenXR > set SteamVR as the runtime. |

## Useful logs

| What | Where |
|---|---|
| Viewer (launcher sessions) | `<output folder>\viewer_console.log` |
| SteamVR | `<Steam>\logs\vrserver.txt`, `<Steam>\logs\xrclient_SIBR_gaussianViewer_app.txt` |
| VIVE eye-tracking layer | `C:\ProgramData\HTC\ViveSoftware\ViveVRRuntime\logs\htc_openxr_facial_tracking_<date>.txt` (UTF-16, written every 5 s) |
| SRanipal 1.3.6.x | `%USERPROFILE%\AppData\LocalLow\HTC Corporation\SR_Logs\SRAnipal_Logs\` |
| SRanipal 1.3.2 | `%USERPROFILE%\AppData\LocalLow\HTC Corporation\SR_Logs\` |
| Crashes | Windows Event Viewer > Windows Logs > Application ("Application Error") |

For a crash inside a runtime or layer, run the viewer with `EYENAVGS_GAZE_DEBUG=1`. The last `[xr-trace] ... ->` line shows the OpenXR call that did not return.

---
[← 7. Analysis](07_analysis.md) · [Tutorial index](README.md) · [9. Code changes →](09_code_changes.md)
