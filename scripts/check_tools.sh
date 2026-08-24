#!/usr/bin/env bash
# List whether required build tools are on this Linux machine.
# Does not install anything. Does not handle a Wi-Fi password.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

missing_req=0
missing_opt=0

ok() { printf "  OK       %-18s %s\n" "$1" "$2"; }
fail() {
  printf "  MISSING  %-18s %s\n" "$1" "$2"
  missing_req=1
}
warn() {
  printf "  WARN     %-18s %s\n" "$1" "$2"
  missing_opt=1
}

have_cmd() { command -v "$1" >/dev/null 2>&1; }

py=""
if have_cmd python3; then
  py=python3
elif have_cmd python; then
  py=python
fi

echo "Required tools check (Linux)  repo: $root"
echo ""
echo "=== Host Unity + DEBUG CLI (required) ==="

if have_cmd git; then
  ok git "$(git --version 2>/dev/null | head -n1)"
else
  fail git "git clone / submodule update --init --recursive"
fi

if [[ -n "$py" ]]; then
  ok python "$($py --version 2>&1) ($py)"
else
  fail python "Python 3 (python3)"
fi

if have_cmd cmake; then
  ok cmake "$(cmake --version 2>/dev/null | head -n1)"
else
  fail cmake "CMake 3.10+ (apt install cmake)"
fi

if have_cmd clang && have_cmd clang++; then
  ok clang "$(clang --version 2>/dev/null | head -n1)"
  ok "clang++" "$(clang++ --version 2>/dev/null | head -n1)"
else
  fail clang "LLVM Clang C11/C++11 (apt install clang). Not MinGW gcc."
fi

if have_cmd ninja; then
  ok ninja "$(ninja --version 2>/dev/null)"
elif have_cmd make; then
  ok make "$(make --version 2>/dev/null | head -n1)"
else
  fail generator "Ninja or GNU make (apt install ninja-build)"
fi

unity="$root/third_party/Unity/src/unity.c"
if [[ -f "$unity" ]]; then
  ok Unity "third_party/Unity/src/unity.c"
else
  fail Unity "git submodule update --init --recursive"
fi

echo ""
echo "=== Firmware (required for Mega / ESP32 sketches) ==="

cli=""
if have_cmd arduino-cli; then
  cli=arduino-cli
elif [[ -x "$HOME/ex-installer/arduino-cli/arduino-cli" ]]; then
  cli="$HOME/ex-installer/arduino-cli/arduino-cli"
elif [[ -x "$HOME/bin/arduino-cli" ]]; then
  cli="$HOME/bin/arduino-cli"
fi

if [[ -n "$cli" ]]; then
  ok arduino-cli "$("$cli" version 2>/dev/null | head -n1)"
  cores="$("$cli" core list 2>/dev/null || true)"
  if echo "$cores" | grep -q 'arduino:avr'; then
    ok "arduino:avr" "Mega 2560 core installed"
  else
    fail "arduino:avr" "arduino-cli core install arduino:avr"
  fi
  if echo "$cores" | grep -qi 'esp32:esp32'; then
    ok "esp32:esp32" "D1 R32 core installed"
  else
    fail "esp32:esp32" "arduino-cli core install esp32:esp32"
  fi
  libs="$("$cli" lib list 2>/dev/null || true)"
  for name in LibLCC ACAN2517 ACAN2515 M95_EEPROM OpenMRNLite ESP32Servo; do
    if echo "$libs" | grep -qi "$name"; then
      ok "lib $name" "Arduino Library Manager"
    else
      fail "lib $name" "arduino-cli lib install $name"
    fi
  done
else
  fail arduino-cli "https://arduino.github.io/arduino-cli/latest/installation/"
  fail "arduino:avr" "needed once arduino-cli is installed"
  fail "esp32:esp32" "needed once arduino-cli is installed"
fi

echo ""
echo "=== Optional quality / coverage / wrap ==="

if have_cmd llvm-cov && have_cmd llvm-profdata; then
  ok llvm-cov "$(llvm-cov --version 2>/dev/null | head -n1)"
  ok llvm-profdata "host coverage (python scripts/run_coverage.py)"
else
  warn llvm-cov "apt install llvm  (python scripts/run_coverage.py)"
fi

if [[ -n "$py" ]] && "$py" -c "import lizard" >/dev/null 2>&1; then
  ok lizard "python module (scripts/run_lizard.py)"
else
  warn lizard "pipx install lizard  (or pip install lizard)"
fi

if have_cmd cppcheck; then
  ok cppcheck "$(cppcheck --version 2>/dev/null | head -n1)"
else
  warn cppcheck "apt install cppcheck  (scripts/run_cppcheck.sh)"
fi

if have_cmd clang-tidy; then
  ok clang-tidy "$(clang-tidy --version 2>/dev/null | head -n1)"
else
  warn clang-tidy "apt install clang-tidy  (scripts/run_clang_tidy.sh)"
fi

if have_cmd oclint; then
  ok oclint "$(oclint --version 2>/dev/null | head -n1)"
else
  warn oclint "Linux only; skip if not installed (scripts/run_oclint.sh)"
fi

if [[ -n "$py" ]] && "$py" -c "import cryptography" >/dev/null 2>&1; then
  ok cryptography "Python AES-GCM for Wi-Fi wrap"
else
  warn cryptography "apt install python3-cryptography  (host wrap only)"
fi

if [[ -n "$py" ]] && "$py" -c "import serial" >/dev/null 2>&1; then
  ok pyserial "scripts/collect_hw_ids.py --port"
else
  warn pyserial "apt install python3-serial  (ID harvest only)"
fi

echo ""
if [[ "$missing_req" -ne 0 ]]; then
  echo "Required tools are missing. See docs/REQUIRED_TOOLS.txt"
  exit 1
fi
if [[ "$missing_opt" -ne 0 ]]; then
  echo "Host/firmware tools OK. Optional items listed as WARN above."
  echo "Details: docs/REQUIRED_TOOLS.txt"
  exit 0
fi
echo "All required and optional tools found."
echo "Details: docs/REQUIRED_TOOLS.txt"
exit 0
