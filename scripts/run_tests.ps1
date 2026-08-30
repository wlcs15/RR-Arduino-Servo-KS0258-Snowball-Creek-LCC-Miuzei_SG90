# Host Unity tests (same as scripts/run_tests.sh). No Wi-Fi password.
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_tests.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\git_version.py") --selftest
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& $py -u (Join-Path $Root "scripts\build_host.py")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$elf = Join-Path $Root "build\host\rr_servo_tests.exe"
if (-not (Test-Path $elf)) { $elf = Join-Path $Root "build\host\rr_servo_tests" }
if (-not (Test-Path $elf)) {
    Write-Error "rr_servo_tests not built in build/host"
    exit 1
}
& $elf
exit $LASTEXITCODE
