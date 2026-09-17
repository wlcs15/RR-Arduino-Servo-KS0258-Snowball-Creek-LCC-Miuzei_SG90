# Host clang-tidy fail gate (same as scripts/run_clang_tidy.py).
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_clang_tidy.ps1
# Does not scan third_party/ or Mega/ESP32 .ino.
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$py = $null
foreach ($name in @("python", "python3", "py")) {
    if (Get-Command $name -ErrorAction SilentlyContinue) {
        $py = $name
        break
    }
}
if (-not $py) { throw "python not found (python -u scripts\run_clang_tidy.py)" }
if ($py -eq "py") {
    & $py -3 -u (Join-Path $Root "scripts\run_clang_tidy.py") @args
} else {
    & $py -u (Join-Path $Root "scripts\run_clang_tidy.py") @args
}
exit $LASTEXITCODE
