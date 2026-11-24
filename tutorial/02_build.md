# 2. Build the viewer

## 2.1 With the script (recommended)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\windows\build_eyenavgs.ps1
```

The script:

1. finds `nvcc` and its version;
2. finds a Visual Studio instance that has **both** the CUDA MSBuild integration and a CUDA-compatible toolset (v143 / v142);
3. configures CMake into `build\`;
4. builds **Release**;
5. installs into `install\`.

**First run:**

- The configure step downloads the prebuilt dependencies (about 300 MB, into `extlibs\`): OpenCV, Boost, GLEW, GLFW, the OpenXR loader, ...
- It also clones small libraries and the CUDA rasterizer from GitLab/GitHub.
- The first full build takes about 10 minutes.

**Options:**

| Option | Effect |
|---|---|
| `-Config Release` (default) / `RelWithDebInfo` / `Debug` | Build configuration. Debug is too slow for VR. |
| `-BuildDir <folder>` | Build folder, relative to the repository root (default `build`). |
| `-Clean` | Delete the build folder first. Needed if you switch Visual Studio instance or generator. |
| `-CudaVersion 12.6` | Force a CUDA version when several are installed. |
| `-VsInstallPath <path>` | Force a Visual Studio instance. |

**Result:** `install\bin\SIBR_gaussianViewer_app.exe`, with its DLLs, shaders and resources. `RelWithDebInfo` builds `..._rwdi.exe` and `Debug` builds `..._d.exe`.

## 2.2 Manually (equivalent commands)

```powershell
# VS 2022 Build Tools; for VS 2022 Community use "C:/Program Files/Microsoft Visual Studio/2022/Community"
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -T "v143,cuda=12.6" `
      "-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools" `
      -DBUILD_IBR_GAUSSIANVIEWER=ON
cmake --build build --config Release --target INSTALL --parallel
```

You can also open `build\sibr_projects.sln` in Visual Studio 2022, then build `ALL_BUILD` and then `INSTALL` in *Release*.

## 2.3 Good to know

- The option names `BUILD_SIBR_OPENXR` and `BUILD_gaussian_viewer` given by the upstream README (commit `87d02bf`) do not exist:
  - the OpenXR module is always built;
  - the viewer option is `BUILD_IBR_GAUSSIANVIEWER` (ON by default).
- Harmless configure messages:
  - `Untested version of cmake`;
  - `Selecting Windows SDK version 10.0.26100.0 to target Windows 10.0.15063.0`;
  - `Policy CMP0144/CMP0167 is not set`.
- If you **add** `.cpp` files under `src/core/openxr`, re-run the configure step: sources are collected with `file(GLOB ...)`.
- Build errors you may know from older checkouts are fixed here:
  - `Compatibility with CMake < 3.5 has been removed` (xatlas, CMake 4);
  - `C7555` (designated initializers);
  - `C2065 'initial_sacle'`.

See [step 9](09_code_changes.md).

---
[← 1. Requirements](01_requirements.md) · [Tutorial index](README.md) · [3. VR and eye-tracking setup →](03_vr_eye_tracking_setup.md)
