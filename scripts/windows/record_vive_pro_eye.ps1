# SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
# SPDX-License-Identifier: Apache-2.0
#
# This file is part of the EyeNavGS project. See LICENSE_EYENAVGS.md and NOTICE for details.
<#
.SYNOPSIS
  Launch an EyeNavGS headset recording session with an HTC VIVE Pro Eye (SteamVR + VIVE Console + SRanipal).

.DESCRIPTION
  Pre-flight checks and environment preparation, then launch of SIBR_gaussianViewer_app in headset mode:
   1. the viewer executable and the 3DGS scene folder exist;
   2. the active OpenXR runtime is SteamVR;
   3. the "VIVE OpenXR Facial tracking" API layer (XR_APILAYER_VIVE_facial_tracking, which provides
      XR_HTC_eye_tracker on top of SRanipal) is enabled - if it is disabled in SteamVR, it is enabled for
      this launch only, through the OpenXR loader environment variables (no admin rights needed);
   4. the SRanipal runtime (SR_Runtime) matching the VIVE layer (1.3.6.x, shipped with VIVE Console) is
      running. An older SR_Runtime (e.g. the standalone 1.3.2 from 2021) makes the VIVE layer abort the app.
  After the session, every new trace is checked with utils\CheckTrace\check_trace.py (if Python is found).
  See tutorial/05_recording.md.

.EXAMPLE
  .\scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\truck -Participant P01
.EXAMPLE
  # Fixed 120 s recording starting automatically, with the 'truck' initialization of the EyeNavGS paper
  .\scripts\windows\record_vive_pro_eye.ps1 -Scene D:\scenes\truck -Participant P01 -Duration 120 `
      -InitialPosition "-2 1.8 -4" -InitialQuaternion "-0.0872 0 0 0.9962" -InitialScale 0.8
#>
param(
    [Parameter(Mandatory = $true)][string]$Scene,  # 3DGS model folder (contains cfg_args and point_cloud\iteration_*)
    [string]$OutDir = "",                          # default: <repo>\recordings\<scene>\<participant or 'session'>_<date>
    [string]$Participant = "",
    [double]$Duration = 0,                         # > 0: start recording automatically and stop after N seconds
    [ValidateRange(1, 8)][int]$Downscale = 1,      # per-eye resolution divider: higher = faster = more rows/s
    [string]$InitialPosition = "",                 # "X Y Z"
    [string]$InitialQuaternion = "",               # "QX QY QZ QW"
    [string]$InitialScale = "",                    # "S"
    [string]$Iteration = "",                       # 3DGS iteration to load (default: largest)
    [ValidateSet("auto", "htc", "ext", "fb", "none")][string]$EyeTracking = "auto",
    [string]$Exe = "",                             # default: <repo>\install\bin\SIBR_gaussianViewer_app.exe
    [switch]$SkipSRanipal                          # do not check/start SR_Runtime
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$repo = $script:EyeNavGSRepo
function Fail($msg) { Write-Bad $msg; exit 1 }

# --- 1. executable and scene ------------------------------------------------------------------------
if (-not $Exe) { $Exe = Get-ViewerExe "Release" }
if (-not (Test-Path $Exe)) { Fail "Viewer not found: $Exe (build it first: scripts\windows\build_eyenavgs.ps1)" }
$Exe = (Resolve-Path $Exe).Path
Write-Ok "Viewer: $Exe"

if (-not (Test-Path $Scene)) { Fail "Scene folder not found: $Scene" }
$Scene = (Resolve-Path $Scene).Path.TrimEnd("\")
foreach ($required in @("cfg_args", "cameras.json", "input.ply")) {
    # cameras.json and input.ply are needed when the viewer falls back to the model folder (the usual case)
    if (-not (Test-Path (Join-Path $Scene $required))) { Fail "No '$required' in $Scene (is it a complete 3D Gaussian Splatting model folder? see tutorial/04_scenes.md)" }
}
$plys = @(Get-ChildItem (Join-Path $Scene "point_cloud") -Directory -Filter "iteration_*" -ErrorAction SilentlyContinue |
          Where-Object { Test-Path (Join-Path $_.FullName "point_cloud.ply") })
if (-not $plys) { Fail "No point_cloud\iteration_*\point_cloud.ply in $Scene" }
Write-Ok "Scene: $Scene ($(($plys | ForEach-Object Name) -join ', '))"
$sceneName = Split-Path $Scene -Leaf

# --- 2. OpenXR runtime -------------------------------------------------------------------------------
$active = Get-ActiveOpenXRRuntime
if ($active -match "steamxr") { Write-Ok "Active OpenXR runtime: SteamVR ($active)" }
else { Write-Warn "Active OpenXR runtime is '$active', not SteamVR. In SteamVR: Settings > OpenXR > 'Set SteamVR as OpenXR runtime'." }

# --- 3. VIVE eye tracking API layer --------------------------------------------------------------------
$env:EYENAVGS_EYE_TRACKING = $EyeTracking
if ($EyeTracking -in @("auto", "htc", "ext")) {
    $layer = Get-ViveEyeLayerInfo
    if (-not $layer.Exists) {
        Write-Warn "VIVE OpenXR facial tracking layer not found: install 'VIVE Console for SteamVR' from Steam (no eye tracking without it)."
    } elseif ($layer.Enabled) {
        Write-Ok "VIVE OpenXR facial tracking layer enabled in SteamVR ($($layer.Json))"
    } else {
        $env:XR_API_LAYER_PATH = $layer.Dir
        $env:XR_ENABLE_API_LAYERS = $script:ViveEyeLayerName
        Write-Ok "VIVE OpenXR facial tracking layer disabled in SteamVR -> enabled for this session only (XR_ENABLE_API_LAYERS)"
    }
    Remove-Item Env:\DISABLE_XR_APILAYER_VIVE_FACIAL_TRACKING_1 -ErrorAction SilentlyContinue
}

# --- 4. SRanipal runtime (SR_Runtime) -------------------------------------------------------------------
if (-not $SkipSRanipal -and $EyeTracking -in @("auto", "htc", "ext")) {
    $sr = Get-SRRuntimeInfo
    if (-not $sr.ViveRuntime) { Fail "SR_Runtime of VIVE Console not found (VIVEDriver\App\SRanipal\sr_runtime.exe): install/update 'VIVE Console for SteamVR'" }
    if ($sr.ViveVersion -lt $script:MinSRRuntimeVersion) { Write-Warn "VIVE Console SR_Runtime is $($sr.ViveVersion) ($($script:MinSRRuntimeVersion)+ expected): update VIVE Console" }

    if ($sr.RunningVersion -and $sr.RunningVersion -ge $script:MinSRRuntimeVersion) {
        Write-Ok "SR_Runtime $($sr.RunningVersion) is running"
    } else {
        if ($sr.RunningVersion) {
            Write-Warn "An old SR_Runtime ($($sr.RunningVersion)) is running: it is incompatible with the VIVE OpenXR layer (the viewer would be killed). Stopping it."
            try {
                Get-Process sr_runtime -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction Stop
            } catch {
                Fail ("Could not stop the running SR_Runtime ($($_.Exception.Message)). It may run with administrator rights: " +
                      "exit it from its tray icon (right-click the robot icon > Exit) or run this script from an administrator PowerShell, then re-run.")
            }
            Start-Sleep -Seconds 2
        }
        Write-Host "Starting SR_Runtime $($sr.ViveVersion) ($($sr.ViveRuntime)) - accept the UAC prompt if Windows shows one ..."
        if (Start-ViveSRRuntime $sr.ViveRuntime 60) { Write-Ok "SR_Runtime is ready (robot icon in the Windows notification area)" }
        else { Write-Warn "SR_Runtime did not report 'ready' within 60 s: eye tracking may not work" }
    }
}

# --- Output directory -------------------------------------------------------------------------------------
if (-not $OutDir) {
    $tag = "session"
    if ($Participant) { $tag = $Participant }
    $OutDir = Join-Path $repo ("recordings\{0}\{1}_{2}" -f $sceneName, $tag, (Get-Date -Format "yyyyMMdd_HHmmss"))
}
New-Item -ItemType Directory -Force $OutDir | Out-Null
$OutDir = (Resolve-Path $OutDir).Path
Write-Ok "Traces will be written to: $OutDir"

# --- Launch -----------------------------------------------------------------------------------------------
$viewerArgs = @("-m", $Scene, "--rendering-mode", "2", "--trace-dir", $OutDir, "--downscale", "$Downscale")
if ($Iteration) { $viewerArgs += @("--iteration", $Iteration) }
if ($InitialPosition) { $viewerArgs += @("--initial-position") + @($InitialPosition -split "[\s,]+" | Where-Object { $_ }) }
if ($InitialQuaternion) { $viewerArgs += @("--initial-quaternion") + @($InitialQuaternion -split "[\s,]+" | Where-Object { $_ }) }
if ($InitialScale) { $viewerArgs += @("--initial-scale", $InitialScale) }
if ($Duration -gt 0) { $viewerArgs += @("--record-trace", "--record-duration", ([string]$Duration)) }

$before = @(Get-ChildItem $OutDir -Filter *.csv -ErrorAction SilentlyContinue | ForEach-Object FullName)
$logFile = Join-Path $OutDir "viewer_console.log"
@(
    "scene=$Scene", "participant=$Participant", "date=$(Get-Date -Format o)",
    "viewer=$Exe", "args=$($viewerArgs -join ' ')", "EYENAVGS_EYE_TRACKING=$env:EYENAVGS_EYE_TRACKING",
    "XR_ENABLE_API_LAYERS=$env:XR_ENABLE_API_LAYERS"
) | Set-Content -Encoding utf8 (Join-Path $OutDir "session_info.txt")

Write-Host ""
Write-Host "Put the headset on. In the desktop window, 'OpenXR' panel:" -ForegroundColor Cyan
Write-Host "  - check 'Eye tracking: HTC_eye_tracker' and the gaze status line (green = valid gaze)" -ForegroundColor Cyan
if ($Duration -gt 0) {
    Write-Host "  - recording starts automatically when the app is focused, and stops after $Duration s" -ForegroundColor Cyan
} else {
    Write-Host "  - click 'Start Trace Recording', then 'End Trace Recording' (or close the window)" -ForegroundColor Cyan
}
Write-Host "  - close the viewer window when you are done"
Write-Host "> $Exe $($viewerArgs -join ' ')`n"

Push-Location (Split-Path $Exe)
$previousPreference = $ErrorActionPreference
# Windows PowerShell wraps the stderr lines of native programs into error records: do not stop on them,
# and convert them back to plain text for the console and the log file.
$ErrorActionPreference = "Continue"
# UTF-8 log written line by line (Tee-Object of Windows PowerShell 5.1 writes UTF-16)
$writer = New-Object System.IO.StreamWriter($logFile, $false, (New-Object System.Text.UTF8Encoding($false)))
$writer.AutoFlush = $true
try {
    & $Exe @viewerArgs 2>&1 | ForEach-Object { $line = "$_"; $writer.WriteLine($line); $line }
} finally {
    $writer.Close()
    $ErrorActionPreference = $previousPreference
    Pop-Location
}

# --- Check the new traces ----------------------------------------------------------------------------------
$new = @(Get-ChildItem $OutDir -Filter *.csv -ErrorAction SilentlyContinue | Where-Object { $before -notcontains $_.FullName })
if (-not $new) { Write-Warn "No trace was recorded in $OutDir"; exit 0 }
$python = Find-Python
foreach ($csv in $new) {
    Write-Host "`n=== $($csv.FullName)" -ForegroundColor Cyan
    if ($python) { & $python (Join-Path $repo "utils\CheckTrace\check_trace.py") $csv.FullName }
    else { Write-Host "(Python not found: run utils\CheckTrace\check_trace.py manually)" }
}
