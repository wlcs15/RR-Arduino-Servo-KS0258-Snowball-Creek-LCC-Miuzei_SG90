# List whether required build tools are on this Windows machine.
# Does not install anything. Does not handle a Wi-Fi password.
#
#   powershell -NoProfile -ExecutionPolicy Bypass -File scripts\check_tools.ps1
#
# Writes local\check_tools-YYYYMMDD-HHMMSS-<host>.log (and check_tools-last.log)
# so a PC without the Grok CLI can still share the result.

$ErrorActionPreference = "Continue"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
Set-Location $Root

if (-not $env:CHECK_TOOLS_INNER) {
    $localDir = Join-Path $Root "local"
    New-Item -ItemType Directory -Force -Path $localDir | Out-Null
    $ts = Get-Date -Format "yyyyMMdd-HHmmss"
    $hn = (($env:COMPUTERNAME, "windows") -ne $null)[0]
    $hn = ($hn -replace "[^A-Za-z0-9._-]", "_")
    $log = Join-Path $localDir "check_tools-$ts-$hn.log"
    $last = Join-Path $localDir "check_tools-last.log"
    $header = @(
        "=== check_tools log (share this file with Grok; Grok CLI not required) ==="
        "file: $log"
        "time: $(Get-Date -Format o)"
        "host: $env:COMPUTERNAME"
        "os: $($PSVersionTable.OS)"
        "ps: $($PSVersionTable.PSVersion)"
        "user: $env:USERNAME"
        "repo: $Root"
        "python: $((Get-Command python -ErrorAction SilentlyContinue).Source)"
        ""
    ) -join [Environment]::NewLine
    Set-Content -Path $log -Value $header -Encoding UTF8
    $env:CHECK_TOOLS_INNER = "1"
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath @args *>&1 |
        Tee-Object -FilePath $log -Append
    $rc = $LASTEXITCODE
    if ($null -eq $rc) { $rc = 0 }
    Copy-Item -LiteralPath $log -Destination $last -Force
    Write-Host ""
    Write-Host "Share this file with Grok (no Grok CLI needed):"
    Write-Host "  $log"
    Write-Host "  $last"
    exit $rc
}

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
} else {
    Write-Fail "arduino-cli" "PATH, %USERPROFILE%\ex-installer\arduino-cli\, or Arduino IDE"
}

# Do not use `arduino-cli lib list` alone. Arduino IDE / OneDrive Documents
# often already has the libraries while CLI's list is empty. Never uninstall.
$finder = Join-Path $PSScriptRoot "find_arduino.py"
$foundViaPy = $false
if ($py -and (Test-Path -LiteralPath $finder)) {
    $scan = & $py -u $finder 2>$null
    if ($LASTEXITCODE -eq 0 -or $scan) {
        $foundViaPy = $true
        foreach ($line in @($scan)) {
            if ($line -match '^\s*OK\s+(\S+)\s+(.*)$') {
                Write-Ok $Matches[1] $Matches[2].Trim()
            } elseif ($line -match '^\s*OK\s+lib\s+(\S+)\s+(.*)$') {
                Write-Ok ("lib " + $Matches[1]) $Matches[2].Trim()
            } elseif ($line -match '^\s*MISSING\s+lib\s+(\S+)\s+(.*)$') {
                Write-Fail ("lib " + $Matches[1]) $Matches[2].Trim()
            } elseif ($line -match '^\s*MISSING\s+(\S+)\s+(.*)$') {
                Write-Fail $Matches[1] $Matches[2].Trim()
            }
        }
    }
}
if (-not $foundViaPy) {
    # Native fallback: same folders as find_arduino.py (OneDrive Documents included).
    $homeDir2 = $env:USERPROFILE
    $libRoots = @(
        (Join-Path $homeDir2 "Arduino\libraries"),
        (Join-Path $homeDir2 "Documents\Arduino\libraries"),
        (Join-Path $homeDir2 "OneDrive\Documents\Arduino\libraries")
    )
    Get-ChildItem -Path $homeDir2 -Directory -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -like "OneDrive*" } |
        ForEach-Object {
            $libRoots += (Join-Path $_.FullName "Documents\Arduino\libraries")
            $libRoots += (Join-Path $_.FullName "Arduino\libraries")
        }
    $dataRoots = @(
        (Join-Path $homeDir2 ".arduino15"),
        (Join-Path $env:LOCALAPPDATA "Arduino15")
    )
    function Test-Lib([string]$Name) {
        foreach ($root in $libRoots) {
            if (-not (Test-Path -LiteralPath $root)) { continue }
            $direct = Join-Path $root $Name
            if (Test-Path -LiteralPath $direct) { return $direct }
            Get-ChildItem $root -Directory -ErrorAction SilentlyContinue | ForEach-Object {
                $prop = Join-Path $_.FullName "library.properties"
                if (Test-Path $prop) {
                    $n = (Select-String -Path $prop -Pattern "^name=" -ErrorAction SilentlyContinue | Select-Object -First 1)
                    if ($n -and ($n.Line.Substring(5).Trim() -eq $Name)) { return $_.FullName }
                }
            }
        }
        return $null
    }
    $avr = $null
    $esp = $null
    foreach ($d in $dataRoots) {
        $a = Join-Path $d "packages\arduino\hardware\avr"
        $e = Join-Path $d "packages\esp32\hardware\esp32"
        if ((-not $avr) -and (Test-Path $a)) { $avr = $a }
        if ((-not $esp) -and (Test-Path $e)) { $esp = $e }
    }
    if ($avr) { Write-Ok "arduino:avr" $avr } else { Write-Fail "arduino:avr" "not under Arduino15\packages (do not uninstall IDE copies)" }
    if ($esp) { Write-Ok "esp32:esp32" $esp } else { Write-Fail "esp32:esp32" "not under Arduino15\packages (do not uninstall IDE copies)" }
    foreach ($name in @("LibLCC", "ACAN2517", "ACAN2515", "M95_EEPROM", "OpenMRNLite", "ESP32Servo")) {
        $p = Test-Lib $name
        if ($p) { Write-Ok "lib $name" $p }
        else { Write-Fail "lib $name" "not in Arduino\libraries (including OneDrive Documents). Do not uninstall." }
    }
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
