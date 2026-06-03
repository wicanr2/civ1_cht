#!/bin/bash
# R5 deep gameplay capture - DOS Civ1 side.
# Drives dosbox running CIV.EXE through the 20-step playthrough via xdotool.
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

OUT=/work/out/r5_dos
mkdir -p "$OUT"

# Stage Civ1
mkdir -p /tmp/civ
cd /tmp/civ
if [ -f /work/dos/CIVILIZATION.zip ]; then
  unzip -oq /work/dos/CIVILIZATION.zip -d /tmp/civ || true
fi
if [ ! -f /tmp/civ/CIV.EXE ] && [ -d /work/dos/extracted ]; then
  cp -r /work/dos/extracted/* /tmp/civ/ 2>/dev/null || true
fi

cat > /tmp/dosbox.conf <<'EOF'
[sdl]
output=surface
fullscreen=false
[render]
aspect=true
[cpu]
core=auto
cycles=fixed 6000
[autoexec]
mount c /tmp/civ
c:
CIV.EXE
exit
EOF

dosbox -conf /tmp/dosbox.conf >/tmp/dos.log 2>&1 &
DOSPID=$!
sleep 6

WID=""
for _ in $(seq 1 20); do
  WID=$(xdotool search --name DOSBox 2>/dev/null | head -1 || true)
  [ -n "$WID" ] && break
  sleep 0.5
done
[ -n "$WID" ] || { echo "FAIL no DOSBox window"; cat /tmp/dos.log | head -30; exit 1; }
echo "DOS WID=$WID"
xdotool windowactivate --sync "$WID" 2>/dev/null || true

shot() { local n="$1"; local p="${2:-0.5}"; sleep "$p"; import -window "$WID" "$OUT/${n}_dos.png" 2>/tmp/se.err || echo "shot $n fail: $(cat /tmp/se.err)"; }
press(){ local k="$1"; local p="${2:-0.6}"; xdotool key --window "$WID" "$k" 2>/dev/null; sleep "$p"; }

# Boot: splash -> Return -> graphics -> sound -> input.
shot 00_BOOT 1.2
press Return 1.5
# Step 1: TITLE = "select graphics mode" splash; capture, then 1=VGA
shot 01_TITLE 0.5
press 1 1.2
press Return 1.5
# sound prompt
press 1 1.2
press Return 1.5
# input device
press 2 1.2
press Return 2.5

# Step 2: CREDITS (intro slideshow). Capture a representative mid-frame.
shot 02_CREDITS 1.0
press space 1.5
press space 1.5
press Return 1.5

# Step 3: MAIN_MENU
shot 03_MAIN_MENU 0.5
press Return 1.5

# Step 4: WIZARD_DIFFICULTY
shot 04_WIZARD_DIFFICULTY 0.5
press Return 1   # picks default Chieftain

# Step 5: WIZARD_CIVS
shot 05_WIZARD_CIVS 0.5
press Return 1

# Step 6: WIZARD_TRIBE - capture, pick Roman = 1
shot 06_WIZARD_TRIBE 0.5
press 1 0.6
press Return 1

# Step 7: WIZARD_NAME - shot first, then type
shot 07_WIZARD_NAME 0.5
xdotool type --window "$WID" --delay 80 "Claude" 2>/dev/null
sleep 0.7
shot 07b_WIZARD_NAME_TYPED 0.3
press Return 2.5

# Step 8: WORLD_GEN
shot 08_WORLD_GEN 1.5
sleep 3
shot 08b_WORLD_GEN_done 0.5

# Step 9: FIRST_TURN
shot 09_FIRST_TURN 1.5

# Step 10: DISMISS_TUTORIAL (DOS shows CIV NOTE; any key dismisses)
press Return 0.8
shot 10_DISMISS_TUTORIAL 0.5

# Step 11: MOVE_SETTLER_NORTH
press Up 0.6
shot 11_MOVE_SETTLER_NORTH 0.5

# Step 12: TRY_END_TURN - DOS Civ1 doesn't have a year-gate; Settler is auto-selected
press Return 0.6
shot 12_TRY_END_TURN_PREMATURELY 0.5

# Step 13: skip wait
press space 0.6
shot 13_CONSUME_SETTLER_MVP 0.4

# Step 14: END_TURN_OK
press Return 0.8
shot 14_END_TURN_OK 0.6

# Step 15: FOUND_CITY - DOS Civ1 "b" = build city
press b 0.8
press Return 1.2
shot 15_FOUND_CITY 0.4

# Step 16: CITY_VIEW (DOS auto-opens; pressing F1 also opens city report)
shot 16_CITY_VIEW 0.5

# Step 17: CHOOSE_PRODUCTION (DOS - within city view, P toggles production)
press p 0.5
shot 17_CHOOSE_PRODUCTION 0.4
press Return 0.4

# Step 18: EXIT_CITY
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
kill $DOSPID 2>/dev/null || true
sleep 0.5
kill $XVFB_PID 2>/dev/null || true

echo "DONE DOS captures:"
ls "$OUT" | wc -l
ls "$OUT" | sort
