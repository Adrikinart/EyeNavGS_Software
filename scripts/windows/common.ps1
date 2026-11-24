# SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
# SPDX-License-Identifier: Apache-2.0
#
# This file is part of the EyeNavGS project. See LICENSE_EYENAVGS.md and NOTICE for details.
#
# Shared helpers for the EyeNavGS Windows scripts (Windows PowerShell 5.1 compatible).
# Usage: . (Join-Path $PSScriptRoot "common.ps1")

$script:EyeNavGSRepo = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path

# Versions validated by the tutorial (see tutorial/01_requirements.md)
$script:MinCMakeVersion = [version]"3.22"
$script:MinSRRuntimeVersion = [version]"1.3.6.0"
$script:ViveEyeLayerName = "XR_APILAYER_VIVE_facial_tracking"

function Write-Ok([string]$msg) { Write-Host "[ OK ] $msg" -ForegroundColor Green }
function Write-Warn([string]$msg) { Write-Host "[WARN] $msg" -ForegroundColor Yellow }
function Write-Bad([string]$msg) { Write-Host "[FAIL] $msg" -ForegroundColor Red }
function Write-Info([string]$msg) { Write-Host "[INFO] $msg" -ForegroundColor Gray }

function Open-HKLM64 {
    # Always read the 64-bit view of HKLM (what a 64-bit OpenXR application sees), even from 32-bit PowerShell
    return [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::LocalMachine, [Microsoft.Win32.RegistryView]::Registry64)
}

function Get-CMakeVersion {
    $cmd = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmd) { return $null }
    $first = (& $cmd.Source --version | Select-Object -First 1)
    $m = [regex]::Match($first, "(\d+\.\d+(\.\d+)?)")
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{ Version = [version]$m.Groups[1].Value; Path = $cmd.Source }
}

function Get-CudaInfo([string]$ForcedVersion = "") {
    $nvccPath = $null
    $cmd = Get-Command nvcc -ErrorAction SilentlyContinue
    if ($cmd) { $nvccPath = $cmd.Source }
    elseif ($env:CUDA_PATH -and (Test-Path (Join-Path $env:CUDA_PATH "bin\nvcc.exe"))) { $nvccPath = Join-Path $env:CUDA_PATH "bin\nvcc.exe" }
    if ($ForcedVersion) { return [pscustomobject]@{ Version = $ForcedVersion; Nvcc = $nvccPath } }
    if (-not $nvccPath) { return $null }
    $m = [regex]::Match((& $nvccPath --version | Out-String), "release (\d+\.\d+)")
    if (-not $m.Success) { return $null }
    return [pscustomobject]@{ Version = $m.Groups[1].Value; Nvcc = $nvccPath }
}

function Get-VisualStudioCandidates([string]$CudaVersion) {
    # Visual Studio instances, with whether they can build EyeNavGS with this CUDA version:
    # they need the CUDA MSBuild integration ("CUDA <ver>.props") and an MSVC toolset accepted by CUDA 12.x
    # (v142 / v143; the VS 2026 default toolset v145 = MSVC 19.50 is rejected by CUDA 12.x).
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return @() }
    $json = & $vswhere -all -prerelease -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -format json | Out-String
    # Windows PowerShell 5.1 returns a JSON array as ONE object: enumerate it through the pipeline
    $instances = @($json | ConvertFrom-Json | ForEach-Object { $_ })
    $result = @()
    foreach ($inst in $instances) {
        if (-not $inst -or -not $inst.installationPath) { continue }
        $major = ([version]$inst.installationVersion).Major
        $gen = $null
        switch ($major) {
            16 { $gen = "Visual Studio 16 2019"; $vcDir = "v160"; $toolset = "v142" }
            17 { $gen = "Visual Studio 17 2022"; $vcDir = "v170"; $toolset = "v143" }
            18 { $gen = "Visual Studio 18 2026"; $vcDir = "v180"; $toolset = "v143" }
        }
        if (-not $gen) { continue }
        $props = Join-Path $inst.installationPath "MSBuild\Microsoft\VC\$vcDir\BuildCustomizations\CUDA $CudaVersion.props"
        $toolsetDir = Join-Path $inst.installationPath "MSBuild\Microsoft\VC\$vcDir\Platforms\x64\PlatformToolsets\$toolset"
        $result += [pscustomobject]@{
            Name = $inst.displayName; Path = $inst.installationPath; Version = [version]$inst.installationVersion; Major = $major
            Generator = $gen; Toolset = $toolset
            HasCudaIntegration = (Test-Path $props); HasToolset = (Test-Path $toolsetDir)
        }
    }
    return @($result | Sort-Object Version)
}

function Get-SteamLibraries {
    $libs = @()
    $steamPath = $null
    foreach ($key in @("HKCU:\Software\Valve\Steam", "HKLM:\SOFTWARE\WOW6432Node\Valve\Steam", "HKLM:\SOFTWARE\Valve\Steam")) {
        $p = Get-ItemProperty $key -ErrorAction SilentlyContinue
        if ($p -and $p.SteamPath) { $steamPath = $p.SteamPath; break }
        if ($p -and $p.InstallPath) { $steamPath = $p.InstallPath; break }
    }
    if (-not $steamPath) { $steamPath = Join-Path ${env:ProgramFiles(x86)} "Steam" }
    $steamPath = $steamPath -replace "/", "\"
    $libs += $steamPath
    $vdf = Join-Path $steamPath "steamapps\libraryfolders.vdf"
    if (Test-Path $vdf) {
        foreach ($m in [regex]::Matches((Get-Content $vdf -Raw), '"path"\s+"([^"]+)"')) {
            $libs += ($m.Groups[1].Value -replace "\\\\", "\")
        }
    }
    return @($libs | Select-Object -Unique)
}

function Find-SteamApp([string]$AppId, [string]$FolderName) {
    foreach ($lib in Get-SteamLibraries) {
        $manifest = Join-Path $lib "steamapps\appmanifest_$AppId.acf"
        $folder = Join-Path $lib "steamapps\common\$FolderName"
        if ((Test-Path $manifest) -or (Test-Path $folder)) {
            $build = ""
            if (Test-Path $manifest) {
                $m = [regex]::Match((Get-Content $manifest -Raw), '"buildid"\s+"(\d+)"')
                if ($m.Success) { $build = $m.Groups[1].Value }
            }
            return [pscustomobject]@{ Folder = $folder; BuildId = $build; Library = $lib }
        }
    }
    return $null
}

function Get-ImplicitLayers {
    # Implicit OpenXR API layers registered for 64-bit applications: @{ Json = path; Value = DWORD } (0 = enabled)
    $result = @()
    $key = (Open-HKLM64).OpenSubKey("SOFTWARE\Khronos\OpenXR\1\ApiLayers\Implicit")
    if ($key) {
        foreach ($name in $key.GetValueNames()) { $result += [pscustomobject]@{ Json = $name; Value = $key.GetValue($name) } }
        $key.Close()
    }
    return $result
}

function Get-ActiveOpenXRRuntime {
    $key = (Open-HKLM64).OpenSubKey("SOFTWARE\Khronos\OpenXR\1")
    if (-not $key) { return $null }
    $value = $key.GetValue("ActiveRuntime")
    $key.Close()
    return $value
}

function Find-ViveDriverRoot {
    # 1. from the registered VIVE OpenXR layers (exact location, whatever the Steam library)
    foreach ($layer in Get-ImplicitLayers) {
        $i = $layer.Json.IndexOf("\VIVEDriver\", [System.StringComparison]::OrdinalIgnoreCase)
        if ($i -gt 0) {
            $root = $layer.Json.Substring(0, $i + "\VIVEDriver".Length)
            if (Test-Path $root) { return $root }
        }
    }
    # 2. Steam libraries (VIVE Console for SteamVR = Steam app 1635730, folder "VIVEDriver")
    $app = Find-SteamApp "1635730" "VIVEDriver"
    if ($app -and (Test-Path $app.Folder)) { return $app.Folder }
    return $null
}

function Get-ViveEyeLayerInfo {
    $root = Find-ViveDriverRoot
    $info = [pscustomobject]@{ Root = $root; Dir = $null; Json = $null; Exists = $false; Registered = $false; Enabled = $false; RegistryValue = $null }
    if (-not $root) { return $info }
    $info.Dir = Join-Path $root "App\ViveVRRuntime\ViveVR_openxr\ViveOpenXRFacialTracking"
    $info.Json = Join-Path $info.Dir "ViveOpenXRFacialTracking.json"
    $info.Exists = Test-Path $info.Json
    foreach ($layer in Get-ImplicitLayers) {
        if ($layer.Json -ieq $info.Json) {
            $info.Registered = $true
            $info.RegistryValue = $layer.Value
            $info.Enabled = ($layer.Value -eq 0)
        }
    }
    return $info
}

function Get-SRRuntimeLogPaths {
    $base = Join-Path $env:USERPROFILE "AppData\LocalLow\HTC Corporation\SR_Logs"
    return [pscustomobject]@{ New = (Join-Path $base "SRAnipal_Logs\SR_Runtime.log"); Old = (Join-Path $base "SR_Runtime.log") }
}

function Get-RunningSRRuntimeVersion {
    # The executable path of sr_runtime.exe cannot always be read: use the "Start SR_Runtime x.y.z.w" log lines
    # written after the process started. Returns $null if not running, [version]"0.0" if running but unknown.
    $proc = Get-CimInstance Win32_Process -Filter "Name='sr_runtime.exe'" -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $proc) { return $null }
    $started = $proc.CreationDate
    $logs = Get-SRRuntimeLogPaths
    $best = $null
    foreach ($log in @($logs.New, $logs.Old)) {
        if (-not (Test-Path $log)) { continue }
        foreach ($line in (Select-String -Path $log -Pattern "\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\] \[LOG\] \[Runtime\] Start SR_Runtime ([\d\.]+)")) {
            $t = [datetime]::ParseExact($line.Matches[0].Groups[1].Value, "yyyy-MM-dd HH:mm:ss", [System.Globalization.CultureInfo]::InvariantCulture)
            if ($t -ge $started.AddSeconds(-5) -and (-not $best -or $t -gt $best.Time)) {
                $best = [pscustomobject]@{ Time = $t; Version = [version]$line.Matches[0].Groups[2].Value }
            }
        }
    }
    if ($best) { return $best.Version }
    return [version]"0.0"
}

function Get-SRRuntimeInfo {
    $root = Find-ViveDriverRoot
    $vive = $null
    if ($root) { $vive = Join-Path $root "App\SRanipal\sr_runtime.exe" }
    $standalone = Join-Path $env:ProgramFiles "VIVE\SRanipal\sr_runtime.exe"
    $info = [pscustomobject]@{
        ViveRuntime = $null; ViveVersion = $null; StandaloneRuntime = $null; StandaloneVersion = $null; RunningVersion = $null
    }
    if ($vive -and (Test-Path $vive)) { $info.ViveRuntime = $vive; $info.ViveVersion = [version](Get-Item $vive).VersionInfo.FileVersion }
    if (Test-Path $standalone) { $info.StandaloneRuntime = $standalone; $info.StandaloneVersion = [version](Get-Item $standalone).VersionInfo.FileVersion }
    $info.RunningVersion = Get-RunningSRRuntimeVersion
    return $info
}

function Start-ViveSRRuntime([string]$RuntimePath, [int]$TimeoutSeconds = 60) {
    # Start the SR_Runtime shipped with VIVE Console and wait until its log says it is ready
    $logs = Get-SRRuntimeLogPaths
    $since = Get-Date
    Start-Process -FilePath $RuntimePath -WorkingDirectory (Split-Path $RuntimePath)
    for ($i = 0; $i -lt $TimeoutSeconds; $i++) {
        Start-Sleep -Seconds 1
        if (-not (Test-Path $logs.New)) { continue }
        foreach ($line in (Select-String -Path $logs.New -Pattern "\[(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\] \[LOG\] \[Runtime\] SR_Runtime is ready")) {
            $t = [datetime]::ParseExact($line.Matches[0].Groups[1].Value, "yyyy-MM-dd HH:mm:ss", [System.Globalization.CultureInfo]::InvariantCulture)
            if ($t -ge $since.AddSeconds(-2)) { return $true }
        }
    }
    return $false
}

function Find-Python {
    foreach ($candidate in @("python", "python3")) {
        $cmd = Get-Command $candidate -ErrorAction SilentlyContinue
        if ($cmd -and $cmd.Source -notmatch "WindowsApps") { return $cmd.Source }
    }
    $py = Get-Command py -ErrorAction SilentlyContinue
    if ($py) {
        $exe = (& $py.Source -3 -c "import sys; print(sys.executable)" 2>$null | Select-Object -First 1)
        if ($exe -and (Test-Path $exe)) { return $exe }
    }
    foreach ($path in @("$env:ProgramData\Anaconda3\python.exe", "$env:USERPROFILE\anaconda3\python.exe",
                        "$env:ProgramData\miniconda3\python.exe", "$env:USERPROFILE\miniconda3\python.exe")) {
        if (Test-Path $path) { return $path }
    }
    return $null
}

function Get-ViewerExe([string]$Config = "Release") {
    $suffix = ""
    if ($Config -eq "Debug") { $suffix = "_d" } elseif ($Config -eq "RelWithDebInfo") { $suffix = "_rwdi" }
    return Join-Path $script:EyeNavGSRepo "install\bin\SIBR_gaussianViewer_app$suffix.exe"
}
