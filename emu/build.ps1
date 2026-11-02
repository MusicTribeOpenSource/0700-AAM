# Builds the Kick app (KICK_EMU=ON) and the KICK Surface on Windows, into <repo>\build.
# Usage: emu\build.ps1 [-Debug] [-Jobs N]
# Needs Visual Studio 2022 with the C++ workload (the ARM64 tools on Windows on ARM). CMake and
# Ninja come from Visual Studio's own copies, so nothing else has to be installed.
param(
    [switch]$Debug,
    [int]$Jobs = [Environment]::ProcessorCount
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$buildType = if ($Debug) { 'Debug' } else { 'Release' }
$out = if ($env:EMU_BUILD_DIR) { $env:EMU_BUILD_DIR } else { 'build' }

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Visual Studio 2022 with the C++ tools was not found.' }

# Import the compiler environment for the host architecture (cl.exe, link.exe, SDK paths).
$arch = if ($env:PROCESSOR_ARCHITECTURE -eq 'ARM64') { 'arm64' } else { 'x64' }
$vcvars = Join-Path $vsPath 'VC\Auxiliary\Build\vcvarsall.bat'
cmd /c "`"$vcvars`" $arch >nul && set" | ForEach-Object {
    if ($_ -match '^([^=]+)=(.*)$') { Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2] }
}
$cmakeBin = Join-Path $vsPath 'Common7\IDE\CommonExtensions\Microsoft\CMake'
$env:PATH = "$cmakeBin\CMake\bin;$cmakeBin\Ninja;$env:PATH"

function Invoke-Step([string]$what, [scriptblock]$cmd) {
    # Windows PowerShell 5 turns any native stderr line into a terminating error under 'Stop'.
    $ErrorActionPreference = 'Continue'
    & $cmd
    $ErrorActionPreference = 'Stop'
    if ($LASTEXITCODE -ne 0) { throw "$what failed with exit code $LASTEXITCODE" }
}

# The JUCE multitouch patch must keep LF endings (see .gitattributes). Clone with core.autocrlf=false.
Write-Host "== Kick (KICK_EMU=ON, $buildType, $arch)"
New-Item -ItemType Directory -Force "$repo\$out" | Out-Null
Invoke-Step 'Kick configure' { cmake -S "$repo\Kick" -B "$repo\$out\emu" -G Ninja -DKICK_EMU=ON "-DCMAKE_BUILD_TYPE=$buildType" 2>&1 | Tee-Object "$repo\$out\emu-configure.log" | ForEach-Object { "$_" } }
Invoke-Step 'Kick build' { cmake --build "$repo\$out\emu" -j $Jobs }

Write-Host '== KICK Surface'
Invoke-Step 'Surface configure' { cmake -S "$repo\emu\surface" -B "$repo\$out\surface" -G Ninja "-DCMAKE_BUILD_TYPE=$buildType" 2>&1 | Tee-Object "$repo\$out\surface-configure.log" | ForEach-Object { "$_" } }
Invoke-Step 'Surface build' { cmake --build "$repo\$out\surface" -j $Jobs }

Write-Host "built: Kick and KICK Surface under $repo\$out"
