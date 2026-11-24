# SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
# SPDX-License-Identifier: Apache-2.0
#
# This file is part of the EyeNavGS project. See LICENSE_EYENAVGS.md and NOTICE for details.
<#
.SYNOPSIS
  Check that this PC matches the requirements of the EyeNavGS + HTC VIVE Pro Eye tutorial.

.DESCRIPTION
  Read-only: nothing is installed, started or modified. Prints OK / WARN / FAIL lines with a hint for each
  problem, for the build toolchain and for the VR / eye-tracking stack. See tutorial/01_requirements.md.
  Exit code = number of FAIL lines.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\windows\check_environment.ps1 -BuildOnly
#>
param(
    [switch]$BuildOnly,  # only check the build toolchain
    [switch]$VROnly      # only check the VR / eye-tracking stack
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$script:Failures = 0
function Fail([string]$msg, [string]$hint = "") {
    Write-Bad $msg
    if ($hint) { Write-Host "       -> $hint" -ForegroundColor DarkYellow }
    $script:Failures++
}
function Hint([string]$msg, [string]$hint) {
    Write-Warn $msg
    if ($hint) { Write-Host "       -> $hint" -ForegroundColor DarkYellow }
}

Write-Host "EyeNavGS environment check - repository: $($script:EyeNavGSRepo)`n"

# ======================================================================================================
if (-not $VROnly) {
    Write-Host "== Build toolchain (tutorial/02_build.md) ==" -ForegroundColor Cyan

    $os = Get-CimInstance Win32_OperatingSystem
    if ($os.OSArchitecture -match "64") { Write-Ok "$($os.Caption) $($os.Version) ($($os.OSArchitecture))" }
    else { Fail "$($os.Caption) is not 64-bit" "A 64-bit Windows 10/11 is required" }

    $gpus = @(Get-CimInstance Win32_VideoController | Where-Object { $_.Name -match "NVIDIA" })
    if ($gpus) { foreach ($g in $gpus) { Write-Ok "GPU: $($g.Name) (driver $($g.DriverVersion))" } }
    else { Fail "No NVIDIA GPU found" "The 3DGS renderer needs CUDA, i.e. an NVIDIA GPU" }

    $cmake = Get-CMakeVersion
    if (-not $cmake) { Fail "CMake not found in PATH" "Install CMake >= $($script:MinCMakeVersion) (https://cmake.org/download) and add it to PATH" }
    elseif ($cmake.Version -lt $script:MinCMakeVersion) { Fail "CMake $($cmake.Version) too old" "CMake >= $($script:MinCMakeVersion) required (4.x works)" }
    else { Write-Ok "CMake $($cmake.Version) ($($cmake.Path))" }

    $git = Get-Command git -ErrorAction SilentlyContinue
    if ($git) { Write-Ok "Git ($($git.Source))" } else { Fail "Git not found in PATH" "Install Git for Windows (https://git-scm.com)" }

    $cuda = Get-CudaInfo
    if (-not $cuda) { Fail "CUDA toolkit (nvcc) not found" "Install CUDA 12.x (12.6 tested) AFTER Visual Studio" }
    else {
        $cudaVersion = [version]$cuda.Version
        if ($cudaVersion.Major -ne 12) { Hint "CUDA $($cuda.Version) (12.x was tested)" "Other versions may need other Visual Studio toolsets" }
        elseif ($cudaVersion -lt [version]"12.4") { Hint "CUDA $($cuda.Version)" "MSVC 14.40+ (current VS 2022) requires CUDA >= 12.4" }
        else { Write-Ok "CUDA $($cuda.Version) ($($cuda.Nvcc))" }

        $vs = @(Get-VisualStudioCandidates $cuda.Version)
        if (-not $vs) { Fail "No Visual Studio with C++ tools found" "Install 'Visual Studio 2022 Build Tools', workload 'Desktop development with C++'" }
        $usable = $null
        foreach ($c in $vs) {
            $ok = $c.HasCudaIntegration -and $c.HasToolset -and ($c.Major -lt 18 -or ($cmake -and $cmake.Version -ge [version]"4.2"))
            $line = "{0} {1}: CUDA {2} integration={3}, toolset {4}={5}" -f $c.Name, $c.Version, $cuda.Version, $c.HasCudaIntegration, $c.Toolset, $c.HasToolset
            if ($ok) { Write-Ok $line; $usable = $c } else { Write-Info $line }
        }
        if ($vs -and -not $usable) {
            Fail "No Visual Studio instance can build with CUDA $($cuda.Version)" ("Install VS 2022 Build Tools (toolset v143), then re-run the CUDA $($cuda.Version) installer " +
                 "(Custom > Visual Studio Integration). VS 2026 works only with the extra 'MSVC v143' component AND the CUDA integration " +
                 "added to it, and CMake >= 4.2: its default MSVC 19.50 is rejected by CUDA 12.x")
        }
    }

    $python = Find-Python
    if ($python) { Write-Ok "Python: $python (only needed for the analysis tools)" }
    else { Hint "No usable Python found (the 'python' Microsoft Store stub does not count)" "Optional: install Python 3.8+ to run utils\CheckTrace\check_trace.py" }

    $exe = Get-ViewerExe "Release"
    if (Test-Path $exe) { Write-Ok "Viewer built: $exe" }
    else { Hint "Viewer not built yet ($exe)" "Run scripts\windows\build_eyenavgs.ps1" }
    Write-Host ""
}

# ======================================================================================================
if (-not $BuildOnly) {
    Write-Host "== VR / eye-tracking stack (tutorial/03_vr_eye_tracking_setup.md) ==" -ForegroundColor Cyan

    $steamvr = Find-SteamApp "250820" "SteamVR"
    if ($steamvr) { Write-Ok "SteamVR installed ($($steamvr.Folder), build $($steamvr.BuildId))" }
    else { Fail "SteamVR not found" "Install Steam, then SteamVR" }

    $active = Get-ActiveOpenXRRuntime
    if ($active -match "steamxr") { Write-Ok "Active OpenXR runtime: SteamVR" }
    elseif ($active) { Fail "Active OpenXR runtime is not SteamVR: $active" "SteamVR > Settings > OpenXR: set SteamVR as the OpenXR runtime" }
    else { Fail "No active OpenXR runtime registered" "SteamVR > Settings > OpenXR: set SteamVR as the OpenXR runtime" }

    $vive = Find-SteamApp "1635730" "VIVEDriver"
    $layer = Get-ViveEyeLayerInfo
    if ($layer.Root) {
        $build = ""
        if ($vive -and $vive.BuildId) { $build = " (Steam build $($vive.BuildId))" }
        Write-Ok "VIVE Console for SteamVR: $($layer.Root)$build"
    } else { Fail "VIVE Console for SteamVR not found" "Install 'VIVE Console for SteamVR' from Steam (it provides the eye-tracking OpenXR layer and SR_Runtime)" }

    if ($layer.Exists) {
        if ($layer.Enabled) { Write-Ok "VIVE OpenXR Facial tracking layer: enabled in SteamVR" }
        elseif ($layer.Registered) { Hint "VIVE OpenXR Facial tracking layer: registered but DISABLED in SteamVR (registry value $($layer.RegistryValue))" "SteamVR > Settings > OpenXR > Manage OpenXR API layers > 'VIVE OpenXR Facial tracking' = Yes. (record_vive_pro_eye.ps1 also enables it per session.)" }
        else { Hint "VIVE OpenXR Facial tracking layer: present but not registered" "record_vive_pro_eye.ps1 enables it per session; reinstalling VIVE Console registers it" }
    } elseif ($layer.Root) { Fail "VIVE OpenXR Facial tracking layer missing ($($layer.Json))" "Update/reinstall VIVE Console for SteamVR" }

    $sr = Get-SRRuntimeInfo
    if ($sr.ViveRuntime) {
        if ($sr.ViveVersion -ge $script:MinSRRuntimeVersion) { Write-Ok "SR_Runtime (VIVE Console): $($sr.ViveVersion)" }
        else { Fail "SR_Runtime (VIVE Console) is $($sr.ViveVersion)" "Update VIVE Console ($($script:MinSRRuntimeVersion)+ needed by its eye-tracking layer)" }
    } elseif ($layer.Root) { Fail "SR_Runtime not found in VIVE Console (App\SRanipal\sr_runtime.exe)" "Update/reinstall VIVE Console for SteamVR" }
    if ($sr.StandaloneRuntime) {
        if ($sr.StandaloneVersion -lt $script:MinSRRuntimeVersion) {
            Hint "Old standalone SRanipal runtime $($sr.StandaloneVersion) installed ($($sr.StandaloneRuntime))" "It makes the VIVE layer kill the viewer if it is the one running: always use record_vive_pro_eye.ps1 (it starts the VIVE Console one), see tutorial/03 section 'SRanipal version trap'"
        } else { Write-Ok "Standalone SRanipal runtime $($sr.StandaloneVersion)" }
    }
    if ($sr.RunningVersion) {
        if ($sr.RunningVersion -ge $script:MinSRRuntimeVersion) { Write-Ok "SR_Runtime running: $($sr.RunningVersion)" }
        else { Hint "SR_Runtime running with version $($sr.RunningVersion)" "Exit it (tray icon) - record_vive_pro_eye.ps1 starts the right one" }
    } else { Write-Info "SR_Runtime not running (record_vive_pro_eye.ps1 starts it)" }

    $cfg = Join-Path $env:USERPROFILE "AppData\LocalLow\HTC Corporation\SR_Config\SRanipalConfig.ini"
    if (Test-Path $cfg) {
        $text = Get-Content $cfg -Raw
        if ($text -match "EnableEyeTracking=1") { Write-Ok "SRanipal: eye tracking enabled" } else { Hint "SRanipal: eye tracking not enabled" "SR_Runtime tray icon > settings: enable eye tracking" }
        if ($text -match "AcceptEULA=1") { Write-Ok "SRanipal: user agreement accepted" } else { Hint "SRanipal: user agreement not accepted yet" "Accept it when SR_Runtime shows it (headset on)" }
        $m = [regex]::Match($text, "LastCalibrationDate=(\d{8})")
        if ($m.Success -and $m.Groups[1].Value -ne "19700101") { Write-Info "Last eye calibration on this PC: $($m.Groups[1].Value) (calibrate each participant anyway)" }
    } else { Write-Info "No SRanipal user configuration yet (created at the first SR_Runtime start)" }

    $hmd = @(Get-PnpDevice -PresentOnly -ErrorAction SilentlyContinue | Where-Object { $_.InstanceId -match "VID_0BB4&PID_0309" })
    if ($hmd) { Write-Ok "VIVE Pro headset connected (USB VID_0BB4 PID_0309)" }
    else { Hint "VIVE Pro headset not detected on USB" "Check the link box: power, USB 3 and DisplayPort" }

    $tobii = Get-ItemProperty HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*, HKLM:\Software\WOW6432Node\Microsoft\Windows\CurrentVersion\Uninstall\* -ErrorAction SilentlyContinue |
             Where-Object { $_.DisplayName -match "Tobii" } | Select-Object -First 1
    if ($tobii) { Write-Ok "Eye tracker runtime: $($tobii.DisplayName) $($tobii.DisplayVersion)" }
    else { Hint "No Tobii eye tracker runtime found" "It is installed with SRanipal / VIVE Console for the VIVE Pro Eye" }

    # SteamVR logs live in the main Steam folder (first entry of the library list), whatever the library of SteamVR
    $vrlog = Join-Path (@(Get-SteamLibraries)[0]) "logs\vrserver.txt"
    if ($vrlog -and (Test-Path $vrlog)) {
        $last = Get-Content $vrlog -Tail 3000 | Select-String -Pattern "No base stations seen|BOOTSTRAPPED base|CALIBRATED base" | Select-Object -Last 1
        if ($last -and $last.Line -match "No base stations seen") { Hint "SteamVR's last tracking message: 'No base stations seen'" "Power on the base stations and check line of sight" }
        elseif ($last) { Write-Ok "SteamVR saw base stations in its last session" }
    }
    Write-Host ""
}

if ($script:Failures -eq 0) { Write-Host "No blocking problem found." -ForegroundColor Green }
else { Write-Host "$($script:Failures) blocking problem(s) found (see FAIL lines)." -ForegroundColor Red }
exit $script:Failures
