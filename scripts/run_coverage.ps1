# Host llvm-cov (same as scripts/run_coverage.sh). Requires Clang.
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_coverage.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = if (Get-Command python -ErrorAction SilentlyContinue) { "python" } else { "python3" }
& $py -u (Join-Path $Root "scripts\run_coverage.py") @args
exit $LASTEXITCODE
