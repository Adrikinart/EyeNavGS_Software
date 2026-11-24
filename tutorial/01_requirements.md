# 1. Requirements and environment check

## 1.1 Hardware

- Windows 10/11 **64-bit** PC with an **NVIDIA GPU**. The 3DGS renderer is CUDA-based; an RTX 3080 Ti was tested. A VR-ready GPU is needed to keep a high frame rate.
- **HTC VIVE Pro Eye**: headset, link box, SteamVR base stations, controllers.

## 1.2 Tested configuration (the "specs")

| Item | Version |
|---|---|
| Windows 11 Pro | 10.0.26200 |
| GPU / driver | NVIDIA RTX 3080 Ti / 610.88 |
| Visual Studio | **VS 2022 Build Tools**, MSVC 14.44 (toolset **v143**) |
| CUDA | **12.6** (12.6.20) |
| CMake | 4.2.0 (≥ 3.22 required) |
| Git | any recent version |
| SteamVR | Steam build 25998080 |
| VIVE Console for SteamVR | Steam build 16936426 (Dec. 2025) |
| SR_Runtime (SRanipal) | **1.3.6.12**: the one shipped *inside* VIVE Console |
| VIVE OpenXR facial tracking layer | 0.1.8.13 (bundles the SRanipal client 1.3.6.5) |
| Tobii VRU02 runtime / eye chip firmware | 1.16.36 / 2.41.0 |
| Python (optional, analysis tools) | 3.8+ (Anaconda 3 tested) |

Other versions may work. The constraints that **do** matter:

- **CUDA 12.x needs an MSVC toolset < 19.50.**
  - That means v143 (VS 2022) or v142 (VS 2019).
  - The VS 2026 default toolset v145 is rejected.
  - Current VS 2022 releases (MSVC 14.40+) also need **CUDA ≥ 12.4**.
- **The SRanipal runtime that runs must be 1.3.6.x**, matching VIVE Console's eye-tracking layer ([step 3](03_vr_eye_tracking_setup.md#33-the-sranipal-version-trap)).

## 1.3 Software to install (in this order)

1. **Visual Studio 2022** with the C++ toolset **v143**. Either install works:
   - [Build Tools for Visual Studio 2022](https://visualstudio.microsoft.com/downloads/#build-tools-for-visual-studio-2022) (no IDE, enough);
   - Visual Studio 2022 Community.

   Select the workload **"Desktop development with C++"**, including a **Windows 10/11 SDK**.

   > **Already have Visual Studio 2026?** Install VS 2022 Build Tools next to it, or add the "MSVC v143" component to VS 2026. The build script picks a compatible instance automatically.
2. **CUDA Toolkit 12.x** (12.4 or newer; 12.6 tested), installed **after** Visual Studio.
   - The CUDA installer adds its MSBuild integration (`CUDA 12.6.props`) only to the Visual Studio instances present at that moment.
   - If you installed Visual Studio later, re-run the CUDA installer: *Custom > Visual Studio Integration*.
3. **CMake ≥ 3.22** ([cmake.org](https://cmake.org/download/); 4.x works) and **Git** ([git-scm.com](https://git-scm.com/)), both on the `PATH`.
4. **Steam**, **SteamVR**, and **VIVE Console for SteamVR** (free, from Steam). Details in [step 3](03_vr_eye_tracking_setup.md).
5. *(Optional)* **Python 3.8+** for the analysis tools. `check_trace.py` only uses the standard library. Python is not needed to build.

   > On a fresh Windows, `python` can be the *Microsoft Store stub*, which only opens the Store. Install a real Python (python.org, Anaconda, ...).

## 1.4 Get the repository

```powershell
git clone <URL of this repository>  EyeNavGS_Software
cd EyeNavGS_Software
```

Keep the path short, without spaces or accents, e.g. `D:\EyeNavGS_Software`.

## 1.5 Check your PC

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1
```

The script is **read-only**. It prints `OK` / `WARN` / `FAIL` lines with a hint for each problem, covering:

- the build toolchain: OS, NVIDIA GPU, CMake, Git, CUDA, Visual Studio instances with CUDA integration and toolset, Python, viewer build;
- the VR stack: SteamVR, OpenXR runtime, VIVE Console, eye-tracking layer, SR_Runtime versions, SRanipal user settings, headset on USB, Tobii runtime, base stations.

Use `-BuildOnly` or `-VROnly` to check one part. The exit code is the number of `FAIL` lines.

Example on the tested PC, before the one-time SteamVR setup of step 3:

```
== Build toolchain (tutorial/02_build.md) ==
[ OK ] CMake 4.2.0 (C:\Program Files (x86)\CMake\bin\cmake.exe)
[ OK ] CUDA 12.6 (C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.6\bin\nvcc.exe)
[ OK ] Visual Studio Build Tools 2022 17.14.37906.3: CUDA 12.6 integration=True, toolset v143=True
[INFO] Visual Studio Community 2026 18.10.12224.181: CUDA 12.6 integration=False, toolset v143=False
...
== VR / eye-tracking stack (tutorial/03_vr_eye_tracking_setup.md) ==
[ OK ] Active OpenXR runtime: SteamVR
[WARN] VIVE OpenXR Facial tracking layer: registered but DISABLED in SteamVR (registry value 1)
[ OK ] SR_Runtime (VIVE Console): 1.3.6.12
[WARN] Old standalone SRanipal runtime 1.3.2.0 installed (C:\Program Files\VIVE\SRanipal\sr_runtime.exe)
```

---
[← 0. Overview](00_overview.md) · [Tutorial index](README.md) · [2. Build →](02_build.md)
