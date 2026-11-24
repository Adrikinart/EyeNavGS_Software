# 3. Set up SteamVR, VIVE Console and SRanipal (once)

## 3.1 SteamVR, base stations, room setup

1. Install **Steam** and **SteamVR**. Connect the VIVE Pro Eye link box: power, USB 3, DisplayPort.
2. Power on the **base stations**. In the SteamVR window, the headset icon must be **solid** (tracked), not blinking.
   - If SteamVR's log (`<Steam>\logs\vrserver.txt`) says `No base stations seen`, nothing is tracked and no recording is meaningful.
3. Run **Room Setup** once (Room-scale or Standing only). The viewer uses the OpenXR STAGE space, so the floor and play area come from Room Setup.
4. Make SteamVR the **active OpenXR runtime**: in SteamVR, *Settings > OpenXR* (or *Settings > Developer*) > *Current OpenXR runtime* = SteamVR, or click "Set SteamVR as OpenXR runtime".
   - This matters if Meta Quest Link / Meta Horizon Link is also installed: it can take over the OpenXR runtime.

## 3.2 VIVE Console for SteamVR

Install **VIVE Console for SteamVR** from Steam (free). It provides:

- the VIVE OpenXR API layers (`<Steam library>\steamapps\common\VIVEDriver\App\ViveVRRuntime\ViveVR_openxr\`), including the eye-tracking layer **"VIVE OpenXR Facial tracking"** (`XR_APILAYER_VIVE_facial_tracking`);
- **SR_Runtime 1.3.6.12** (`<Steam library>\steamapps\common\VIVEDriver\App\SRanipal\sr_runtime.exe`).

## 3.3 The SRanipal version trap

If you once installed the standalone **SRanipal runtime** from HTC's developer site, you may also have an **old** SR_Runtime in `C:\Program Files\VIVE\SRanipal\`. `check_environment.ps1` reports it. You can also check by hand:

```powershell
(Get-Item "C:\Program Files\VIVE\SRanipal\sr_runtime.exe").VersionInfo.FileVersion        # e.g. 1.3.2.0 (2021)
(Get-Item "${env:ProgramFiles(x86)}\Steam\steamapps\common\VIVEDriver\App\SRanipal\sr_runtime.exe").VersionInfo.FileVersion   # 1.3.6.12
```

The VIVE eye-tracking layer embeds a **1.3.6.5** SRanipal client. Against a **1.3.2** runtime:

- the runtime logs a `Call_ViveSR_API` error in `%USERPROFILE%\AppData\LocalLow\HTC Corporation\SR_Logs\ViveSR_Runtime.log` (the line literally reads `rror! Call_ViveSR_API`);
- the layer **aborts the whole viewer** (silent exit; Windows event `0xc0000409` in `ucrtbase.dll`).

When no runtime is running, the client may also auto-start the **old** one.

**Make sure the SR_Runtime 1.3.6.x from VIVE Console is the one running before you start the viewer:**

- **Automatic:** `record_vive_pro_eye.ps1` does it every time ([step 5](05_recording.md)). It detects the running runtime version from its log, stops an old one, starts VIVE Console's runtime, and waits until it reports *ready*.
  - `sr_runtime.exe` asks for the highest available privileges, so Windows may show a **UAC prompt** when it starts: accept it.
  - If stopping an old runtime fails with *Access is denied*, exit it from its tray icon (right-click the robot > Exit), or run the script from an administrator PowerShell, then re-run.
- **Manual:** right-click the SRanipal "robot" tray icon > Exit. Then start `...\steamapps\common\VIVEDriver\App\SRanipal\sr_runtime.exe` and wait until `SR_Logs\SRAnipal_Logs\SR_Runtime.log` contains `SR_Runtime is ready` (about 5 s).
- *(Optional cleanup, not tested here):* uninstall the old standalone package (*Windows Settings > Apps > "VIVE_SRanipalInstaller"*). Do it only if no other software of yours depends on it.

The first time SR_Runtime runs with the headset on, accept the SRanipal **user agreement (EULA)**, and keep *"Use eye tracking"* enabled.

## 3.4 Enable the VIVE OpenXR eye-tracking layer

In the SteamVR desktop window: *Menu > Settings > OpenXR > "Manage OpenXR API layers"*. Set **"VIVE OpenXR Facial tracking"** to **Yes** ("Oui" in French). The two other VIVE layers (hand tracking, SRWorks) are not needed.

- The recording script does **not** require this step. If the layer is off, it enables it **for the viewer only**, through the OpenXR loader variables (no admin rights):

  ```powershell
  $env:XR_API_LAYER_PATH    = "<Steam library>\steamapps\common\VIVEDriver\App\ViveVRRuntime\ViveVR_openxr\ViveOpenXRFacialTracking"
  $env:XR_ENABLE_API_LAYERS = "XR_APILAYER_VIVE_facial_tracking"
  ```

- How the OpenXR loader decides:
  - registry key `HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit`, **64-bit view**;
  - value name = path of `ViveOpenXRFacialTracking.json`;
  - value **0** means enabled, any other value means disabled.

  Check it with:

  ```powershell
  reg query "HKLM\SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit" /reg:64
  ```

- Do not set the environment variable `DISABLE_XR_APILAYER_VIVE_FACIAL_TRACKING_1`.

## 3.5 Eye calibration (each participant, each session)

1. Start SR_Runtime 1.3.6.x ([§3.3](#33-the-sranipal-version-trap)) and SteamVR, and turn on a controller.
2. The participant puts the headset on and adjusts the strap and the IPD knob.
3. Press the controller's **System** button to open the SteamVR dashboard, then click **"VIVE Pro Eye"**.
4. Check that *"Use eye tracking"* is enabled, then click **Calibrate**. Follow the IPD adjustment and the 5 dots.
5. Close the dashboard.

Each user must recalibrate. Glasses can lower tracking quality: prefer contact lenses or no correction when possible.

---
[← 2. Build](02_build.md) · [Tutorial index](README.md) · [4. Scenes →](04_scenes.md)
