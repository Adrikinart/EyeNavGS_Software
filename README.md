# 👁️NavGS with an HTC VIVE Pro Eye: record 6-DoF navigation and eye-tracking traces in 3D Gaussian Splatting scenes

This repository is a **tutorial-ready fork** of the [EyeNavGS software](https://github.com/symmru/EyeNavGS_Software). EyeNavGS is a SIBR-based VR viewer for 3D Gaussian Splatting (3DGS) scenes with record-and-replay of user navigation. The fork adds:

- **HTC VIVE Pro Eye eye tracking** (OpenXR `XR_HTC_eye_tracker` through SteamVR + VIVE Console). It records per-eye gaze, pupil diameter and eye openness. The original code only supported the Meta Quest Pro (`XR_FB_eye_tracking_social`).
- **A build that works with current toolchains**: VS 2022 Build Tools (or VS 2026 with the v143 toolset and the CUDA integration), CUDA 12.x, CMake 4.x.
- **Fixed recording bugs**: gaze lost after the first blink, values truncated to 3 decimals, wrong headers, sessions appended to old files, ...
- **Natural spawn with real-world scale** for any 3DGS scene: level floor, 1:1 scale, start next to the content ([`utils/SceneSpawn`](utils/SceneSpawn/README.md)). A ready-made spawn for the example scene *figurines* is included.
- **Scripts** that check a PC, build, and run a recording session.
- **A step-by-step tutorial** from a blank Windows PC to validated CSV traces.

## Quick start

From a PowerShell window (all commands after the `cd` are run from the repository root):

```powershell
git clone https://github.com/Adrikinart/EyeNavGS_Software.git EyeNavGS_Software
cd EyeNavGS_Software

# 1. What is missing on this PC? (read-only)
powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1

# 2. Build (Release, about 10 min the first time; downloads ~300 MB of dependencies)
powershell -ExecutionPolicy Bypass -File scripts\windows\build_eyenavgs.ps1

# 3. Record (headset on, participant calibrated); the trace is checked automatically at the end
powershell -ExecutionPolicy Bypass -File scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\truck -Participant P01

# 4. Check any trace (needs a real Python 3.8+; "py -3" also works)
python utils\CheckTrace\check_trace.py examples\sample_trace_vive_pro_eye.csv
```

Step 3 assumes the one-time VR setup is done (SteamVR, VIVE Console, base stations, eye calibration: [tutorial step 3](tutorial/03_vr_eye_tracking_setup.md)) and a 3DGS scene is available ([step 4](tutorial/04_scenes.md)).

## Tutorial

| Step | |
|---|---|
| 0 | [Overview: how it works, and what used to go wrong](tutorial/00_overview.md) |
| 1 | [Requirements and environment check](tutorial/01_requirements.md) |
| 2 | [Build the viewer](tutorial/02_build.md) |
| 3 | [Set up SteamVR, VIVE Console and SRanipal (once)](tutorial/03_vr_eye_tracking_setup.md) |
| 4 | [Get a 3DGS scene](tutorial/04_scenes.md) |
| 5 | [Record a session](tutorial/05_recording.md) |
| 6 | [Trace format reference (34 columns)](tutorial/06_trace_format.md) |
| 7 | [Check and analyze traces](tutorial/07_analysis.md) |
| 8 | [Troubleshooting](tutorial/08_troubleshooting.md) |
| 9 | [What was changed in the code](tutorial/09_code_changes.md) |

## Tested specs

| | |
|---|---|
| OS / GPU | Windows 11 Pro 64-bit, NVIDIA RTX 3080 Ti (driver 610.88) |
| Build | VS 2022 Build Tools (MSVC 14.44, toolset v143), CUDA 12.6, CMake 4.2, Git |
| VR | HTC VIVE Pro Eye, SteamVR (OpenXR runtime), **VIVE Console for SteamVR** (eye-tracking OpenXR layer), **SR_Runtime 1.3.6.12** |
| Analysis | Python 3.8+ (standard library only for `check_trace.py`; `numpy` for `compute_spawn.py`) |

Two constraints matter more than exact versions:

- **CUDA 12.x needs an MSVC toolset < 19.50** (v143 / v142).
- **The SRanipal runtime that runs must be 1.3.6.x.** An old SRanipal 1.3.2 makes HTC's OpenXR layer kill the viewer. The recording script handles it; see [step 3](tutorial/03_vr_eye_tracking_setup.md#33-the-sranipal-version-trap).

## What a trace looks like

Each rendered frame gives two rows (left eye, then right eye):

- head/eye pose and FOV;
- gaze orientation and origin in the same world frame;
- timestamps;
- `GazeValid` and `GazeSource`;
- pupil diameter and eye openness;
- `timestep`.

The first 24 columns are the original EyeNavGS format, so the dataset tools keep working. See the [format reference](tutorial/06_trace_format.md) and the real [sample trace](examples/).

```
ViewIndex,FOV1,...,GazeQX,GazeQY,GazeQZ,GazeQW,GazePosX,GazePosY,GazePosZ,timestamp,realtime,elapsed_ms,iso_time,epoch_ms,
GazeValid,GazeTracked,GazeSource,xr_display_time_ns,gaze_sample_time_ns,PupilDiameter_mm,PupilPosX,PupilPosY,EyeOpenness,timestep
```

## Repository layout

```
scripts/windows/        check_environment.ps1, build_eyenavgs.ps1, record_vive_pro_eye.ps1, common.ps1
tutorial/               the step-by-step tutorial (start at tutorial/README.md)
utils/CheckTrace/       check_trace.py: trace validator
utils/SceneSpawn/       compute_spawn.py: natural start position, level floor and real-world scale of a scene
utils/...               original EyeNavGS tools (gaze overlay, CSV<->JSON, world coordinates, scene selector)
examples/               real sample trace (VIVE Pro Eye), spawn file of the figurines scene
src/core/openxr/        OpenXR HMD / input / eye tracking / recording (most changes are here)
src/projects/gaussianviewer/  the 3DGS viewer application
docs/ORIGINAL_README.md the original EyeNavGS README, lightly updated for this fork (desktop mode, replay, Meta Quest, utilities, SIBR)
```

Generated folders, not versioned: `build/`, `install/`, `extlibs/`, `recordings/`.

## Credits and citation

This work builds on:

- **EyeNavGS**: Zihao Ding, Cheng-Tse Lee, Mufeng Zhu, Tao Guan, Yuan-Chun Sun, Cheng-Hsin Hsu, Yao Liu (Rutgers University, National Tsing Hua University). [Webpage](https://symmru.github.io/EyeNavGS/), [paper](https://doi.org/10.48550/arXiv.2506.02380), [original repository](https://github.com/symmru/EyeNavGS_Software).
- **SIBR** (INRIA GRAPHDECO) and its OpenXR rendering mode (Orange).
- **3D Gaussian Splatting** (Kerbl et al., INRIA).

If you use this software or the EyeNavGS dataset, please cite:

```bibtex
@article{ding2025eyenavgs,
  title={EyeNavGS: A 6-DoF Navigation Dataset and Record-n-Replay Software for Real-World 3DGS Scenes in VR},
  author={Ding, Zihao and Lee, Cheng-Tse and Zhu, Mufeng and Guan, Tao and Sun, Yuan-Chun and Hsu, Cheng-Hsin and Liu, Yao},
  journal={arXiv preprint arXiv:2506.02380},
  year={2025}
}

@misc{sibr2020,
  author = {Bonopera, Sebastien and Esnault, Jerome and Prakash, Siddhant and Rodriguez, Simon and Thonat, Theo and Benadel, Mehdi and Chaurasia, Gaurav and Philip, Julien and Drettakis, George},
  title  = {sibr: A System for Image Based Rendering},
  year   = {2020},
  url    = {https://gitlab.inria.fr/sibr/sibr_core}
}
```

## License

This repository is a **modified fork** of SIBR / EyeNavGS. It contains:

- **original SIBR code**, under its own terms (non-commercial research and evaluation use, see [`LICENSE.md`](LICENSE.md));
- **EyeNavGS components and the additions of this fork**, under the **Apache License 2.0** (see [`LICENSE_EYENAVGS.md`](LICENSE_EYENAVGS.md) and [`NOTICE`](NOTICE)).

Files carry SPDX headers where applicable. HTC, VIVE, SRanipal, SteamVR, Meta and Quest are trademarks of their respective owners. This project is not affiliated with them.
