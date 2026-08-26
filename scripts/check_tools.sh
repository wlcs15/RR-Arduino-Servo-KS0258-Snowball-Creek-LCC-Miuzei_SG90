#!/usr/bin/env bash
# List whether required build tools are on this Linux machine.
# Does not install anything. Does not handle a Wi-Fi password.
#
# Writes local/check_tools-YYYYMMDD-HHMMSS-<host>.log (and check_tools-last.log)
# so a machine without the Grok CLI can still share the result.
set -u

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [[ -z "${CHECK_TOOLS_INNER:-}" ]]; then
    mkdir -p "$root/local"
    ts="$(date +%Y%m%d-%H%M%S)"
    host="$(hostname -s 2>/dev/null || hostname 2>/dev/null || echo unknown)"
    host="${host//[^A-Za-z0-9._-]/_}"
    log="$root/local/check_tools-${ts}-${host}.log"
    last="$root/local/check_tools-last.log"
    {
        echo "=== check_tools log (share this file with Grok; Grok CLI not required) ==="
        echo "file: $log"
        echo "time: $(date -Is 2>/dev/null || date)"
        echo "host: $(hostname 2>/dev/null || echo unknown)"
        echo "os: $(uname -a 2>/dev/null || echo unknown)"
        echo "user: ${USER:-unknown}"
        echo "repo: $root"
        echo "git: $(git -C "$root" describe --tags --always --dirty 2>/dev/null || echo n/a)"
        echo "python: $(command -v python3 2>/dev/null || command -v python 2>/dev/null || echo none)"
        echo
    } >"$log"
    set +e
    CHECK_TOOLS_INNER=1 "$0" "$@" 2>&1 | tee -a "$log"
    rc=${PIPESTATUS[0]}
    set -e
    cp -f "$log" "$last"
    echo
    echo "Share this file with Grok (no Grok CLI needed):"
    echo "  $log"
    echo "  $last"
    exit "$rc"
fi

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
else
  fail arduino-cli "https://arduino.github.io/arduino-cli/latest/installation/"
fi

# Filesystem first (Arduino IDE / OneDrive / ~/.arduino15). Never uninstall.
if [[ -n "$py" && -f "$root/scripts/find_arduino.py" ]]; then
  while IFS= read -r line; do
    if [[ "$line" =~ ^[[:space:]]*OK[[:space:]]+lib[[:space:]]+([^[:space:]]+)[[:space:]]+(.*)$ ]]; then
      ok "lib ${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    elif [[ "$line" =~ ^[[:space:]]*OK[[:space:]]+([^[:space:]]+)[[:space:]]+(.*)$ ]]; then
      case "${BASH_REMATCH[1]}" in
        arduino:avr|esp32:esp32) ok "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" ;;
      esac
    elif [[ "$line" =~ ^[[:space:]]*MISSING[[:space:]]+lib[[:space:]]+([^[:space:]]+)[[:space:]]+(.*)$ ]]; then
      fail "lib ${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}"
    elif [[ "$line" =~ ^[[:space:]]*MISSING[[:space:]]+([^[:space:]]+)[[:space:]]+(.*)$ ]]; then
      case "${BASH_REMATCH[1]}" in
        arduino:avr|esp32:esp32) fail "${BASH_REMATCH[1]}" "${BASH_REMATCH[2]}" ;;
      esac
    fi
  done < <("$py" -u "$root/scripts/find_arduino.py")
else
  fail "arduino:avr" "python scripts/find_arduino.py (does not uninstall)"
  fail "esp32:esp32" "python scripts/find_arduino.py (does not uninstall)"
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
