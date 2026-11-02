# Runs Kick against the virtual control surface on Windows.
#
#   emu\run.ps1                        Kick plus KICK Surface windows, until you close one
#   emu\run.ps1 -Script file           drive the Surface from a script, exit with its result
#   emu\run.ps1 -Root DIR              emulated device root (default %USERPROFILE%\KickEmu)
#   emu\run.ps1 -Port N                TCP port the Surface listens on (default 50910)
#
# Windows has no pty, so the Surface always serves the serial line over TCP and Kick connects
# with KICK_SERIAL_PORT=tcp://127.0.0.1:<port>.
# Environment: KICK_EMU_ROOT, KICK_EMU_CONTROL_PORT (50901), KICK_EMU_EVENT_PORT (50902),
# KICK_EMU_FRAME_PORT (50903).
param(
    [string]$Script = '',
    [string]$Root = '',
    [int]$Port = 50910
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$out = if ($env:EMU_BUILD_DIR) { $env:EMU_BUILD_DIR } else { 'build' }

if (-not $Root) { $Root = if ($env:KICK_EMU_ROOT) { $env:KICK_EMU_ROOT } else { Join-Path $env:USERPROFILE 'KickEmu' } }
$env:KICK_EMU_ROOT = $Root
New-Item -ItemType Directory -Force $Root | Out-Null

$kick = Get-ChildItem "$repo\$out\emu" -Recurse -Filter Kick.exe -ErrorAction SilentlyContinue | Select-Object -First 1
$surface = Get-ChildItem "$repo\$out\surface" -Recurse -Filter 'KICK Surface.exe' -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $kick -or -not $surface) { throw 'not built yet: run emu\build.ps1' }

# Demo samples need Python 3 (numpy-free). Skip with a note when it is missing.
if (-not (Test-Path "$Root\data\sounds\Emu Kit")) {
    $py = Get-Command python -ErrorAction SilentlyContinue
    if ($py -and $py.Source -notlike '*WindowsApps*') { & $py.Source "$repo\emu\tools\make_samples.py" $Root }
    else { Write-Warning 'Python 3 not found: skipping demo sample generation (winget install Python.Python.3.12).' }
}

Remove-Item "$Root\surface.port" -ErrorAction SilentlyContinue
$surfaceArgs = @('--root', "`"$Root`"", '--tcp', $Port)
if ($Script) { $surfaceArgs += @('--script', "`"$((Resolve-Path $Script).Path)`"") }
$surfaceProc = Start-Process -FilePath $surface.FullName -ArgumentList $surfaceArgs -PassThru -RedirectStandardOutput "$Root\surface.log" -RedirectStandardError "$Root\surface.err.log"

# Kick sends its first message as soon as it connects, so the listener has to exist by then.
for ($i = 0; $i -lt 100 -and -not (Test-Path "$Root\surface.port"); $i++) {
    if ($surfaceProc.HasExited) { Get-Content "$Root\surface.log", "$Root\surface.err.log"; exit 1 }
    Start-Sleep -Milliseconds 100
}
$env:KICK_SERIAL_PORT = (Get-Content "$Root\surface.port" -Raw).Trim()

# Kick resolves .\assets, .\demos and .\templates against its working directory.
$kickProc = Start-Process -FilePath $kick.FullName -WorkingDirectory "$repo\$out\emu" -PassThru -RedirectStandardOutput "$Root\kick.log" -RedirectStandardError "$Root\kick.err.log"
Write-Host "Kick pid $($kickProc.Id), logs in $Root"

try {
    if ($Script) {
        # A script ends the run: its exit status is the test result.
        $surfaceProc.WaitForExit()
        exit $surfaceProc.ExitCode
    }
    Wait-Process -Id $kickProc.Id
}
finally {
    foreach ($p in @($kickProc, $surfaceProc)) { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force } }
}
