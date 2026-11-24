# 9. What was changed in the code

Compared with the upstream EyeNavGS branch `sibr_gaussian_VR_viewer` (commit `87d02bf`). This fork also merges commit `8d8811d`, "clean up OpenXR build and initial scale handling": `sibr_openxr` in C++20, and an `initial_scale` member.

## Build

| File | Change |
|---|---|
| `CMakeLists.txt` | Sets `CMAKE_POLICY_VERSION_MINIMUM=3.5` with CMake ≥ 4.0, for old third-party libraries (xatlas, glm). |
| `src/core/openxr/OpenXRHMD.cpp` | No more C++20 designated initializers, so the code also builds in C++14 (`sibr_openxr` itself is compiled as C++20, from `8d8811d`). |
| `src/core/openxr/OpenXRRdrMode.{hpp,cpp}` | The undeclared `initial_sacle` became the `initial_scale` member (`8d8811d`). |
| `src/core/openxr/CMakeLists.txt` | `/EHs` instead of `/EHsc`, so C++ exceptions thrown by OpenXR layers through the C API can be caught; C++20 (`8d8811d`). |

## Eye tracking (`src/core/openxr/`)

| File | Change |
|---|---|
| `OpenXRHMD.{hpp,cpp}` | Eye-tracking backend selection (FB / HTC / EXT / none, `EYENAVGS_EYE_TRACKING`); `XR_HTC_eye_tracker` support (per-eye gaze, pupil, openness, with range checks); unified `GazeSample` API and `getEyeTrackingStatus()`. Gaze is only queried when the app is focused and the head has been tracked for 1 s. The HTC tracker is created lazily: the VIVE layer `abort()`s the process if SRanipal cannot start, and that cannot be caught. |
| `OpenXRHMD.{hpp,cpp}` (FB path) | FB eye tracker destroyed before its session; FB gaze query time set (it was 0); FB gaze no longer re-scaled when the query fails. |
| `OpenXRHMD.{hpp,cpp}` (other) | Missing `break`s in `pollEvents()`; null check of the input during shutdown. |
| `OpenXRHTCEyeTracker.h` (new) | Declarations of the vendor extension `XR_HTC_eye_tracker` (not in `openxr.h`), from HTC's VIVE-OpenXR sources, cross-checked with the layer DLL. |
| `OpenXRInput.{hpp,cpp}`, `OpenXRInputType.h`, `interactionprofiles/EyeGazeInteractionProfile.{hpp,cpp}` (new) | `XR_EXT_eye_gaze_interaction` (gaze action, bindings, located in the play space); exceptions from layers caught; logs of gaze state changes; action spaces destroyed. |
| `OpenXRHelper.hpp` | Opt-in OpenXR call tracing (`EYENAVGS_GAZE_DEBUG=1`). |

## Recording

| File | Change |
|---|---|
| `src/core/openxr/OpenXRRdrMode.{hpp,cpp}` | Per-row gaze validity instead of a session-wide "disabled after the first invalid sample" latch. Full-precision rows (the sticky `std::fixed` truncated every value to 3 decimals). One header for both record buttons (one of them wrote 19 column names over 24-column rows). A new file for each session (it used to append to the previous session's file). Flush every frame and on close. 10 new columns ([step 6](06_trace_format.md)). DST-correct `iso_time`. `znear`/`zfar` fixed. `--trace-dir`, `--record-trace`, `--record-duration`, `--downscale`. Eye-tracking and REC status in the GUI. MPEG-4 fallback for videos. Terminate handler that prints uncaught exceptions. |
| `src/projects/gaussianviewer/renderer/Config.hpp`, `apps/gaussianViewer/main.cpp` | The new command-line options. |

## Tutorial tooling (new)

| Path | Purpose |
|---|---|
| `scripts/windows/common.ps1` | Shared helpers: Visual Studio/CUDA detection, Steam libraries, VIVE Console, OpenXR registry (64-bit view), SR_Runtime. |
| `scripts/windows/check_environment.ps1` | Read-only check of the PC against the requirements. |
| `scripts/windows/build_eyenavgs.ps1` | Configure + build + install. |
| `scripts/windows/record_vive_pro_eye.ps1` | Pre-flight checks, SR_Runtime 1.3.6.x, VIVE layer, launch, post-session trace check. |
| `utils/CheckTrace/check_trace.py` | Trace validator (standard library only). |
| `examples/sample_trace_vive_pro_eye.csv` | Real sample trace. |
| `.github/workflows/tutorial-checks.yml` | CI: PowerShell syntax (Windows PowerShell 5.1) + checker on the sample. |

---
[← 8. Troubleshooting](08_troubleshooting.md) · [Tutorial index](README.md)
