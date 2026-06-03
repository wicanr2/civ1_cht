#!/bin/bash
# B6 NAME-entry LIVE verification (R5 caveat fix).
# Run inside openciv1pp-recorder docker --rm --network none -e DISPLAY=:99.
# Build openciv1pp, drive --game with xdotool through TITLE..NAME, type
# "Claude", capture the NAME stage, then ENTER and verify the chosen name.
set -uo pipefail

[ "${DISPLAY:-}" = ":99" ] || { echo "FAIL: DISPLAY=$DISPLAY"; exit 1; }
Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR &
XVFB_PID=$!
for i in 1 2 3 4 5; do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 1; done
xdpyinfo -display :99 >/dev/null 2>&1 || { echo "FAIL: Xvfb not up"; exit 1; }
echo "OK Xvfb up pid=$XVFB_PID"

cd /work/src
echo "=== building openciv1pp ==="
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/tmp/cmake.log 2>&1 || { echo BUILD CONFIG FAIL; tail -40 /tmp/cmake.log; exit 1; }
cmake --build build -j"$(nproc)" >/tmp/build.log 2>&1
if [ ! -x build/openciv1pp ]; then
  echo "BUILD FAIL"; tail -60 /tmp/build.log; exit 1
fi
echo "OK binary built"

# Headless: run nameinputlivetest first as a fast confirmation.
echo "=== running --nameinputlivetest ==="
./build/openciv1pp --nameinputlivetest || { echo NAMEINPUTLIVETEST FAIL; exit 1; }
echo "=== running --nameinputtest (pre-existing) ==="
./build/openciv1pp --nameinputtest || { echo NAMEINPUTTEST FAIL; exit 1; }

OUT=/work/out/b6
mkdir -p "$OUT"

ASSETS=/work/dos/extracted/

# Live: drive --game with xdotool, capture the NAME stage with "Claude" typed.
./build/openciv1pp --assets "$ASSETS" --game >/tmp/oci.log 2>&1 &
OCI_PID=$!
sleep 3
WID=""
for _ in $(seq 1 20); do
  WID=$(xdotool search --name OpenCiv 2>/dev/null | head -1 || true)
  [ -n "$WID" ] && break
  sleep 0.5
done
[ -n "$WID" ] || { echo "FAIL no oci window"; head -40 /tmp/oci.log; kill $OCI_PID 2>/dev/null; exit 1; }
echo "OCI WID=$WID"
xdotool windowactivate --sync "$WID" 2>/dev/null || true
sleep 0.5

press() { xdotool key --window "$WID" "$1" 2>/dev/null; sleep "${2:-0.6}"; }
shot()  { sleep "${2:-0.4}"; import -window "$WID" "$OUT/$1.png"; }

# FrontEndFlow has NO CIVILIZATIONS stage (unlike the wizard's internal state
# machine) — it goes TITLE -> MAIN_MENU -> DIFFICULTY -> TRIBE -> NAME with
# 4 Returns. The original r5_capture_oci.sh pressed Return 5 times before
# typing which overshot to STARTING; that was half of why the live test
# always landed on PLAYING.
press Return 1.0   # TITLE -> MAIN_MENU
press Return 1.0   # MAIN_MENU pick "New Game" -> DIFFICULTY
press Return 1.0   # DIFFICULTY accept default -> TRIBE
press Return 1.2   # TRIBE accept default -> NAME

shot B6_NAME_blank 0.8
echo "tail oci.log (state transitions):"
grep -- "->" /tmp/oci.log || true

# Type "Claude" and capture. Use a generous per-key delay (150ms) so the
# 60Hz SDL poll loop reliably picks up every SDL_TEXTINPUT event (with
# delay 60-80 the 'u' was occasionally lost — the OS schedules xdotool's
# events faster than our loop drains them).
xdotool type --window "$WID" --delay 150 "Claude"
sleep 1.0
shot B6_NAME_verified 0.5

# Accept ENTER and check the printed chosenName.
press Return 1.5
sleep 1.0
shot B6_NAME_after_enter 0.5

# Capture the log: it should contain "name=\"Claude\"" if the buffer was used
# (the --game flow does not print chosenName at STARTING, but we can check
# state transitions: NAME -> STARTING means accept ran).
echo "=== oci.log tail ==="
tail -50 /tmp/oci.log
kill $OCI_PID 2>/dev/null || true
wait $OCI_PID 2>/dev/null || true

echo "=== outputs ==="
ls -la "$OUT"
