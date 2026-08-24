# List whether required build tools are on this Windows machine.
# Does not install anything. Does not handle a Wi-Fi password.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check_tools.ps1

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root

$script:MissingReq = 0
$script:MissingOpt = 0

function Write-Ok([string]$Name, [string]$Detail) {
    Write-Host ("  OK       {0,-18} {1}" -f $Name, $Detail)
}
function Write-Fail([string]$Name, [string]$Detail) {
    Write-Host ("  MISSING  {0,-18} {1}" -f $Name, $Detail)
    $script:MissingReq = 1
}
function Write-Warn([string]$Name, [string]$Detail) {
    Write-Host ("  WARN     {0,-18} {1}" -f $Name, $Detail)
    $script:MissingOpt = 1
}

function Find-Cmd([string[]]$Names, [string[]]$ExtraDirs) {
    foreach ($n in $Names) {
        $cmd = Get-Command $n -ErrorAction SilentlyContinue
        if ($cmd) { return $cmd.Source }
        foreach ($dir in $ExtraDirs) {
            if (-not $dir) { continue }
            foreach ($cand in @((Join-Path $dir $n), (Join-Path $dir ($n + ".exe")))) {
                if (Test-Path -LiteralPath $cand) { return $cand }
            }
        }
    }
    return $null
}

$Pf = ${env:ProgramFiles}
if (-not $Pf) { $Pf = "C:\Program Files" }
$Pf86 = ${env:ProgramFiles(x86)}
if (-not $Pf86) { $Pf86 = "C:\Program Files (x86)" }
$HomeDir = $env:USERPROFILE
$LlvmBin = Join-Path $Pf "LLVM\bin"
$CmakeBin = Join-Path $Pf "CMake\bin"
$ArduinoIdeCli = Join-Path $Pf "Arduino IDE\resources\app\lib\backend\resources\arduino-cli.exe"
$ExInstallerCli = Join-Path $HomeDir "ex-installer\arduino-cli\arduino-cli.exe"

function Find-Nmake {
    $nmake = Find-Cmd @("nmake") @()
    if ($nmake) { return $nmake }
    $vswhere = Find-Cmd @("vswhere") @(
        (Join-Path $Pf86 "Microsoft Visual Studio\Installer")
    )
    if (-not $vswhere) { return $null }
    $out = & $vswhere -latest -products * -find "**\nmake.exe" 2>$null
    if (-not $out) { return $null }
    $lines = @($out | ForEach-Object { $_.Trim() } | Where-Object { $_ })
    $pref = $lines | Where-Object { $_ -match "Hostx64" -and $_ -match "x64" }
    if ($pref) { return $pref[0] }
    if ($lines.Count -gt 0) { return $lines[0] }
    return $null
}

Write-Host "Required tools check (Windows)  repo: $Root"
Write-Host ""
Write-Host "=== Host Unity + DEBUG CLI (required) ==="

$git = Find-Cmd @("git") @()
if ($git) {
    Write-Ok "git" ((& $git --version 2>$null | Select-Object -First 1).ToString())
} else {
    Write-Fail "git" "Git for Windows (submodule update --init --recursive)"
}

$py = Find-Cmd @("python", "python3") @()
if ($py) {
    $ver = & $py --version 2>&1 | Select-Object -First 1
    Write-Ok "python" "$ver ($py)"
} else {
    Write-Fail "python" "Python 3 on PATH (python.org or scoop)"
}

$cmake = Find-Cmd @("cmake") @($CmakeBin)
if ($cmake) {
    Write-Ok "cmake" ((& $cmake --version 2>$null | Select-Object -First 1).ToString())
} else {
    Write-Fail "cmake" "CMake 3.10+; add C:\Program Files\CMake\bin to PATH"
}

$clang = Find-Cmd @("clang") @($LlvmBin)
$clangxx = Find-Cmd @("clang++") @($LlvmBin)
if ($clang -and $clangxx) {
    Write-Ok "clang" ((& $clang --version 2>$null | Select-Object -First 1).ToString())
    Write-Ok "clang++" ((& $clangxx --version 2>$null | Select-Object -First 1).ToString())
} else {
    Write-Fail "clang" "LLVM Clang C11/C++11 (C:\Program Files\LLVM\bin). Not MinGW, not cl.exe."
}

$ninja = Find-Cmd @("ninja") @()
$nmake = Find-Nmake
if ($ninja) {
    Write-Ok "ninja" (& $ninja --version 2>$null)
} elseif ($nmake) {
    Write-Ok "nmake" $nmake
} else {
    Write-Fail "generator" "Ninja, or nmake from Visual Studio Build Tools. Not MinGW."
}

$unity = Join-Path $Root "third_party\Unity\src\unity.c"
if (Test-Path -LiteralPath $unity) {
    Write-Ok "Unity" "third_party\Unity\src\unity.c"
} else {
    Write-Fail "Unity" "git submodule update --init --recursive"
}

Write-Host ""
Write-Host "=== Firmware (required for Mega / ESP32 sketches) ==="

$cli = Find-Cmd @("arduino-cli") @(
    (Split-Path -Parent $ExInstallerCli),
    (Split-Path -Parent $ArduinoIdeCli)
)
if (-not $cli) {
    if (Test-Path -LiteralPath $ExInstallerCli) { $cli = $ExInstallerCli }
    elseif (Test-Path -LiteralPath $ArduinoIdeCli) { $cli = $ArduinoIdeCli }
}

if ($cli) {
    $cliVer = & $cli version 2>$null | Select-Object -First 1
    Write-Ok "arduino-cli" "$cliVer"
    $cores = & $cli core list 2>$null | Out-String
    if ($cores -match "arduino:avr") {
        Write-Ok "arduino:avr" "Mega 2560 core installed"
    } else {
        Write-Fail "arduino:avr" "arduino-cli core install arduino:avr"
    }
    if ($cores -match "esp32:esp32") {
        Write-Ok "esp32:esp32" "D1 R32 core installed"
    } else {
        Write-Fail "esp32:esp32" "arduino-cli core install esp32:esp32"
    }
    $libs = & $cli lib list 2>$null | Out-String
    foreach ($name in @("LibLCC", "ACAN2517", "ACAN2515", "M95_EEPROM", "OpenMRNLite", "ESP32Servo")) {
        if ($libs -match [regex]::Escape($name)) {
            Write-Ok "lib $name" "Arduino Library Manager"
        } else {
            Write-Fail "lib $name" "arduino-cli lib install $name"
        }
    }
} else {
    Write-Fail "arduino-cli" "PATH, %USERPROFILE%\ex-installer\arduino-cli\, or Arduino IDE"
    Write-Fail "arduino:avr" "needed once arduino-cli is installed"
    Write-Fail "esp32:esp32" "needed once arduino-cli is installed"
}

Write-Host ""
Write-Host "=== Optional quality / coverage / wrap ==="

$llvmCov = Find-Cmd @("llvm-cov") @($LlvmBin)
$llvmProf = Find-Cmd @("llvm-profdata") @($LlvmBin)
if ($llvmCov -and $llvmProf) {
    Write-Ok "llvm-cov" ((& $llvmCov --version 2>$null | Select-Object -First 1).ToString())
    Write-Ok "llvm-profdata" "host coverage (python scripts/run_coverage.py)"
} else {
    Write-Warn "llvm-cov" "LLVM bin on PATH  (python scripts/run_coverage.py)"
}

if ($py) {
    & $py -c "import lizard" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "lizard" "python module (scripts/run_lizard.py)"
    } else {
        Write-Warn "lizard" "pip install lizard  (or pipx install lizard)"
    }
    & $py -c "import cryptography" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "cryptography" "Python AES-GCM for Wi-Fi wrap"
    } else {
        Write-Warn "cryptography" "pip install cryptography  (host wrap only)"
    }
    & $py -c "import serial" 2>$null
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "pyserial" "scripts/collect_hw_ids.py --port"
    } else {
        Write-Warn "pyserial" "pip install pyserial  (ID harvest only)"
    }
}

$cppcheck = Find-Cmd @("cppcheck") @()
if ($cppcheck) {
    Write-Ok "cppcheck" ((& $cppcheck --version 2>$null | Select-Object -First 1).ToString())
} else {
    Write-Warn "cppcheck" "optional; scripts/run_cppcheck.sh is the Linux driver"
}

$tidy = Find-Cmd @("clang-tidy") @($LlvmBin)
if ($tidy) {
    Write-Ok "clang-tidy" ((& $tidy --version 2>$null | Select-Object -First 1).ToString())
} else {
    Write-Warn "clang-tidy" "LLVM clang-tidy; scripts/run_clang_tidy.sh"
}

Write-Host "  SKIP     oclint             Linux only (not Windows 10/11)"

Write-Host ""
if ($script:MissingReq -ne 0) {
    Write-Host "Required tools are missing. See docs\REQUIRED_TOOLS.txt"
    exit 1
}
if ($script:MissingOpt -ne 0) {
    Write-Host "Host/firmware tools OK. Optional items listed as WARN above."
    Write-Host "Details: docs\REQUIRED_TOOLS.txt"
    exit 0
}
Write-Host "All required and optional tools found."
Write-Host "Details: docs\REQUIRED_TOOLS.txt"
exit 0
