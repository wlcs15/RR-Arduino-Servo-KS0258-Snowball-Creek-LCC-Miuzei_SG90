# Host tests for the arm-arduino and uno-r3 tip. No Wi-Fi password.
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_tests.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$py = if (Get-Command python3 -ErrorAction SilentlyContinue) { "python3" } else { "python" }
& $py -u (Join-Path $Root "scripts\run_tests.py")
exit $LASTEXITCODE
