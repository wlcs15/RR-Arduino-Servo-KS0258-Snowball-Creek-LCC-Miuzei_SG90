# On-target Unity via simavr (ATmega2560 @ 16 MHz). Never flashes hardware.
# Linux CI: scripts/run_on_target_tests.py builds sketches/RrServoUnity and
# runs simavr. On Windows simavr is not supported: SKIP and exit 0
# (same spirit as check_tools.ps1). Does not upload or flash.
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\run_on_target_tests.ps1
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root
$Skip = "SKIP on-target-via-simavr is not supported on Windows (Linux CI only; does not flash hardware)."
$py = $null
foreach ($name in @("python", "python3", "py")) {
    if (Get-Command $name -ErrorAction SilentlyContinue) {
        $py = $name
        break
    }
}
if (-not $py) {
    Write-Host $Skip
    exit 0
}
if ($py -eq "py") {
    & $py -3 -u (Join-Path $Root "scripts\run_on_target_tests.py") @args
} else {
    & $py -u (Join-Path $Root "scripts\run_on_target_tests.py") @args
}
exit $LASTEXITCODE
