#!/bin/bash
# R5 deep gameplay capture - openciv1pp side.
# Drives the build through 20-step playthrough via xdotool + native shot dumpers.
# MUST run inside openciv1pp-recorder docker, --network none, DISPLAY=:99.
set -uo pipefail

[ "${DISPLAY:-}" = ":99" ] || { echo "FAIL: DISPLAY=$DISPLAY"; exit 1; }

Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR &
XVFB_PID=$!
for i in 1 2 3 4 5; do
  xdpyinfo -display :99 >/dev/null 2>&1 && break
  sleep 1
done
xdpyinfo -display :99 >/dev/null 2>&1 || { echo "FAIL: Xvfb not up"; exit 1; }
echo "OK Xvfb up pid=$XVFB_PID"

OUT=/work/out/r5_oci
mkdir -p "$OUT"

cd /work/src
if [ ! -x build/openciv1pp ]; then
  echo "=== building openciv1pp ==="
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/tmp/cmake.log 2>&1
  cmake --build build -j"$(nproc)" >/tmp/build.log 2>&1
  if [ ! -x build/openciv1pp ]; then
    echo "BUILD FAIL"; tail -40 /tmp/build.log
    exit 1
  fi
fi
echo "OK binary at $(realpath build/openciv1pp)"

ASSETS=/work/dos/extracted/

# === Headless dumpers (PPM -> PNG) for static screens ===
hd_shot() {
  local flag="$1"; local outname="$2"
  ./build/openciv1pp --assets "$ASSETS" "$flag" "/tmp/${outname}.ppm" >/tmp/sh.log 2>&1 \
    || { echo "FAIL $flag $outname"; tail -5 /tmp/sh.log; return 1; }
  convert "/tmp/${outname}.ppm" "$OUT/${outname}_oci.png" 2>/tmp/conv.log
}

# Static-screen captures via --shotXxx
hd_shot --shotTitle  01_TITLE       || true
hd_shot --shotIntro  02_CREDITS     || true
hd_shot --shotMenu   03_MAIN_MENU   || true
hd_shot --shotWorld  08_WORLD_GEN   || true
hd_shot --shotCity   16_CITY_VIEW   || true

# === Interactive run for the wizard + play sequence ===
# We launch openciv1pp --game and drive with xdotool, taking screenshots
# at key moments via the WID.
./build/openciv1pp --assets "$ASSETS" --game >/tmp/oci.log 2>&1 &
OCI_PID=$!
sleep 3
WID=""
for _ in $(seq 1 20); do
  WID=$(xdotool search --name OpenCiv 2>/dev/null | head -1 || true)
  [ -n "$WID" ] && break
  sleep 0.5
done
[ -n "$WID" ] || { echo "FAIL no oci window"; cat /tmp/oci.log | head -30; kill $OCI_PID 2>/dev/null; exit 1; }
echo "OCI WID=$WID"
xdotool windowactivate --sync "$WID" 2>/dev/null || true

shot() {
  local name="$1"; local pre="${2:-0.5}"
  sleep "$pre"
  import -window "$WID" "$OUT/${name}_oci.png" 2>/tmp/shot.err \
    || echo "shot $name FAIL: $(cat /tmp/shot.err)"
}

press() {
  local key="$1"; local post="${2:-0.6}"
  xdotool key --window "$WID" "$key" 2>/dev/null
  sleep "$post"
}

# Step 1: TITLE (already showing)
shot 01_TITLE_live 1.0

# Step 2: CREDITS - Enter advances title; live --game flow does not show
# a separate credits stage (it jumps to MAIN_MENU). The static --shotIntro
# above already provides the credits frame; capture the post-Enter state too.
press Return 1
shot 02_CREDITS_live 0.8

# Step 3: MAIN_MENU - main menu should be visible. Enter picks first item.
shot 03_MAIN_MENU_live 0.5
press Return 1.5

# Step 4: WIZARD_DIFFICULTY - difficulty cascade
shot 04_WIZARD_DIFFICULTY 0.8
press Return 1   # accept default highlight (Chieftain)

# Step 5: WIZARD_CIVS - level of competition
shot 05_WIZARD_CIVS 0.8
press Return 1

# Step 6: WIZARD_TRIBE
shot 06_WIZARD_TRIBE 0.8
press Return 1

# Step 7: WIZARD_NAME - text entry. Try typing then Enter.
shot 07_WIZARD_NAME 0.8
xdotool type --window "$WID" --delay 60 "Claude" 2>/dev/null
sleep 0.7
shot 07b_WIZARD_NAME_TYPED 0.3
press Return 1.5

# Step 8: WORLD_GEN / STARTING confirmation
shot 08_WORLD_GEN_live 1.2
press Return 1.5
sleep 1
shot 08b_WORLD_GEN_done 0.5

# Step 9: FIRST_TURN - playing state, settler shown
shot 09_FIRST_TURN 1.0

# Step 10: DISMISS_TUTORIAL (any key) - if armed, overlay dismisses.
press space 0.6
shot 10_DISMISS_TUTORIAL 0.5

# Step 11: MOVE_SETTLER_NORTH
press Up 0.6
shot 11_MOVE_SETTLER_NORTH 0.5

# Step 12: TRY_END_TURN_PREMATURELY - Enter while moves remain.
# B5: should hold the year at 4000BC and show 你必須移動所有單位 banner.
press Return 0.8
shot 12_TRY_END_TURN_PREMATURELY 0.5

# Step 13: CONSUME_SETTLER_MVP - W skip-unit
press W 0.6
shot 13_CONSUME_SETTLER_MVP 0.4

# Step 14: END_TURN_OK - year advances to 3950BC; A4 banner may show.
press Return 0.8
shot 14_END_TURN_OK 0.6

# Step 15: FOUND_CITY - press B for build city
press B 1.0
shot 15_FOUND_CITY 0.6

# Step 16: CITY_VIEW - already covered headless; also try keyboard-open here.
# (--game has no P-for-cityview; rely on the click path)
# We use the auto-open: after build, click on city tile.
# Press M to ensure minimap, etc. Skip interactive city open.
shot 16_CITY_VIEW_live 0.3

# Step 17: CHOOSE_PRODUCTION - if cityView is open, navigate.
press Down 0.4
press Down 0.4
press Return 0.6
shot 17_CHOOSE_PRODUCTION 0.4

# Step 18: EXIT_CITY - Esc back to map
press Escape 0.6
shot 18_EXIT_CITY 0.5

# Step 19: END_TURN_2_3_4
press Return 0.6
press Return 0.6
press Return 0.6
shot 19_END_TURN_2_3_4 0.6

# Step 20: CITY_REVIEW_TURN_5
press Return 0.6
shot 20_CITY_REVIEW_TURN_5 0.6

# Cleanup
kill $OCI_PID 2>/dev/null || true
sleep 0.5
kill $XVFB_PID 2>/dev/null || true

# Convert headless PPMs into the canonical step PNGs (overwrite/supplement)
for stem in 01_TITLE 02_CREDITS 03_MAIN_MENU 08_WORLD_GEN 16_CITY_VIEW; do
  if [ -f "/tmp/${stem}.ppm" ]; then
    convert "/tmp/${stem}.ppm" "$OUT/${stem}_oci.png"
  fi
done

echo "DONE OCI captures:"
ls "$OUT" | wc -l
ls "$OUT" | sort
