# SPDX-FileCopyrightText: 2025 Systems and Multimedia Lab @ Rutgers University
# SPDX-License-Identifier: Apache-2.0
#
# This file is part of the EyeNavGS project. See LICENSE_EYENAVGS.md and NOTICE for details.
<#
.SYNOPSIS
  Configure, build and install EyeNavGS (SIBR gaussianViewer + OpenXR) on Windows.

.DESCRIPTION
  * Detects the CUDA toolkit (nvcc) and a Visual Studio instance that
      - has the CUDA MSBuild integration ("CUDA <ver>.props"), and
      - has an MSVC toolset accepted by that CUDA version (CUDA 12.x rejects MSVC >= 19.50, i.e. the
        VS 2026 default toolset v145: v142 (VS2019) or v143 (VS2022) is used).
  * Configures CMake (CMake 4.x is fine: the repo sets CMAKE_POLICY_VERSION_MINIMUM for old third-party libs),
    builds the requested configuration and installs into <repo>\install.
  See tutorial/02_build.md.

.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\windows\build_eyenavgs.ps1
.EXAMPLE
  powershell -ExecutionPolicy Bypass -File scripts\windows\build_eyenavgs.ps1 -Config RelWithDebInfo -Clean
#>
param(
    [ValidateSet("Release", "RelWithDebInfo", "Debug")]
    [string]$Config = "Release",
    [string]$BuildDir = "build",
    [string]$CudaVersion = "",       # e.g. "12.6"; default: version of the nvcc found in PATH / CUDA_PATH
    [string]$VsInstallPath = "",     # force a Visual Studio instance (installation path)
    [switch]$Clean                   # delete the build directory first
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "common.ps1")
$repo = $script:EyeNavGSRepo
Write-Host "EyeNavGS repository: $repo"

function Fail($msg) { Write-Bad $msg; exit 1 }

# --- CMake / git -----------------------------------------------------------------------------------
$cmake = Get-CMakeVersion
if (-not $cmake) { Fail "cmake not found in PATH (install CMake >= $($script:MinCMakeVersion) from https://cmake.org/download)" }
if ($cmake.Version -lt $script:MinCMakeVersion) { Fail "CMake $($cmake.Version) is too old (>= $($script:MinCMakeVersion) required)" }
Write-Ok "CMake $($cmake.Version): $($cmake.Path)"
if (-not (Get-Command git -ErrorAction SilentlyContinue)) { Fail "git not found in PATH (needed to fetch dependencies)" }

# --- CUDA ------------------------------------------------------------------------------------------
$cuda = Get-CudaInfo $CudaVersion
if (-not $cuda) { Fail "nvcc not found: install the CUDA toolkit (12.x, 12.6 tested) or pass -CudaVersion" }
$CudaVersion = $cuda.Version
Write-Ok "CUDA $CudaVersion"

# --- Visual Studio instance with CUDA integration and a compatible toolset --------------------------
$candidates = Get-VisualStudioCandidates $CudaVersion
if ($VsInstallPath) { $candidates = @($candidates | Where-Object { $_.Path -eq $VsInstallPath }) }
if (-not $candidates) { Fail "No Visual Studio with C++ tools found: install 'Visual Studio 2022 Build Tools' (workload 'Desktop development with C++')" }
$chosen = $null
foreach ($c in $candidates) {
    Write-Host ("  {0} ({1}): CUDA {2} integration={3}, toolset {4}={5}" -f $c.Name, $c.Path, $CudaVersion, $c.HasCudaIntegration, $c.Toolset, $c.HasToolset)
    # the "Visual Studio 18 2026" generator needs CMake >= 4.2
    if ($c.HasCudaIntegration -and $c.HasToolset -and ($c.Major -lt 18 -or $cmake.Version -ge [version]"4.2")) { $chosen = $c }
}
if (-not $chosen) {
    Fail ("No Visual Studio instance has both the CUDA $CudaVersion MSBuild integration and a CUDA-compatible MSVC toolset.`n" +
          "       Install 'Visual Studio 2022 Build Tools' (workload 'Desktop development with C++'), THEN (re)install CUDA $CudaVersion,`n" +
          "       whose installer adds the integration to the Visual Studio instances present at that time.")
}
Write-Ok "Using $($chosen.Name): generator '$($chosen.Generator)', toolset $($chosen.Toolset), CUDA $CudaVersion"

# --- Configure / build / install -------------------------------------------------------------------
$buildPath = Join-Path $repo $BuildDir
if ($Clean -and (Test-Path $buildPath)) {
    Write-Host "Removing $buildPath"
    Remove-Item -Recurse -Force $buildPath
}
$cache = Join-Path $buildPath "CMakeCache.txt"
function Get-CacheValue($name) {
    # the type after ':' depends on how the entry was set (INTERNAL, UNINITIALIZED, PATH, ...)
    $m = Select-String -Path $cache -Pattern ("^" + [regex]::Escape($name) + ":[A-Z]+=(.*)$") | Select-Object -First 1
    if ($m) { return $m.Matches[0].Groups[1].Value } else { return "" }
}
if (Test-Path $cache) {
    $cachedGen = Get-CacheValue "CMAKE_GENERATOR"
    $cachedInst = Get-CacheValue "CMAKE_GENERATOR_INSTANCE"
    $sameInst = (-not $cachedInst) -or (($cachedInst -replace "\\", "/").TrimEnd("/") -eq ($chosen.Path -replace "\\", "/").TrimEnd("/"))
    if (($cachedGen -and $cachedGen -ne $chosen.Generator) -or -not $sameInst) {
        Fail "The existing '$BuildDir' was configured with '$cachedGen' ($cachedInst). Re-run with -Clean."
    }
}

$configureArgs = @(
    "-S", $repo, "-B", $buildPath,
    "-G", $chosen.Generator, "-A", "x64",
    "-T", "$($chosen.Toolset),cuda=$CudaVersion",
    "-DCMAKE_GENERATOR_INSTANCE=$($chosen.Path)",
    "-DBUILD_IBR_GAUSSIANVIEWER=ON"
)
Write-Host "`n> cmake $($configureArgs -join ' ')" -ForegroundColor Cyan
& cmake @configureArgs
if ($LASTEXITCODE -ne 0) { Fail "CMake configure failed (see above)" }

Write-Host "`n> cmake --build $buildPath --config $Config --target INSTALL --parallel" -ForegroundColor Cyan
& cmake --build $buildPath --config $Config --target INSTALL --parallel -- /nologo /v:minimal
if ($LASTEXITCODE -ne 0) { Fail "Build failed (see above)" }

$exe = Get-ViewerExe $Config
if (Test-Path $exe) {
    Write-Ok "Build OK: $exe"
} else {
    Write-Warn "Build finished, but $exe was not found: check install\bin"
}
