# Host CMake build (same as scripts/build_host.sh).
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\build_host.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\build_host.py") @args
exit $LASTEXITCODE
