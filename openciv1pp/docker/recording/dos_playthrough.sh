#!/bin/bash
# Hardened DOS Civ1 playthrough capture. Runs ONLY inside the
# openciv1pp-recorder Docker container with --network none and DISPLAY=:99.
# Captures every distinct UI state from boot through the first few turns.
set -uo pipefail

# === Hardening gate: refuse to run unless inside the prepared container ===
if [ "${DISPLAY:-}" != ":99" ]; then
  echo "FAIL: DISPLAY=$DISPLAY (expected :99). Refusing to run."
  exit 1
fi

Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR &
XVFB_PID=$!
for i in 1 2 3 4 5 6 7 8; do
  xdpyinfo -display :99 >/dev/null 2>&1 && break
  sleep 1
done
if ! xdpyinfo -display :99 >/dev/null 2>&1; then
  echo "FAIL: Xvfb did not start on :99"
  kill $XVFB_PID 2>/dev/null || true
  exit 1
fi
echo "Xvfb up on :99, pid=$XVFB_PID"

# === Stage Civ1 game dir (writable copy) ===
mkdir -p /tmp/civ
cd /tmp/civ
if [ -f /work/dos/CIVILIZATION.zip ]; then
  unzip -oq /work/dos/CIVILIZATION.zip -d /tmp/civ || true
fi
# Fall back / supplement with extracted dir if zip empty or missing files
if [ ! -f /tmp/civ/CIV.EXE ] && [ -d /work/dos/extracted ]; then
  cp -r /work/dos/extracted/* /tmp/civ/ 2>/dev/null || true
fi
echo "civ dir contents (first 10):"
ls /tmp/civ | head -10

# === dosbox config ===
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

# === launch dosbox in background ===
mkdir -p /work/out/dos_play
dosbox -conf /tmp/dosbox.conf > /tmp/dos.log 2>&1 &
DOSPID=$!
sleep 6

WID=$(xdotool search --name DOSBox 2>/dev/null | head -1)
if [ -z "$WID" ]; then
  echo "FAIL: no DOSBox window found"
  cat /tmp/dos.log | head -40
  kill $DOSPID 2>/dev/null || true
  kill $XVFB_PID 2>/dev/null || true
  exit 1
fi
echo "DOSBox WID=$WID"
xdotool windowactivate --sync "$WID" 2>/dev/null || true

shot() {
  local name="$1"
  local pre="${2:-0.5}"
  sleep "$pre"
  import -window "$WID" "/work/out/dos_play/${name}.png" 2>/tmp/shot.err \
    || echo "shot $name failed: $(cat /tmp/shot.err)"
}

press() {
  local key="$1"
  local post="${2:-0.6}"
  xdotool key --window "$WID" "$key" 2>/dev/null
  sleep "$post"
}

# ===== Boot ceremony =====
# Splash: "CIVILIZATION (TM) Version 474.03 ..." - press Return to advance
shot 01_initial_boot 1.0
press Return 2

# "Select graphics mode:" 1=VGA, 2=MCGA, 3=EGA, 4=Tandy
shot 02_graphics_prompt 0
press 1 1
shot 03_graphics_vga 0
press Return 2

# "Select sound option:" 1=No sounds, 2=Adlib, 3=Sound Blaster, 4=Roland MT-32 etc.
shot 04_sound_prompt 0
press 1 1
shot 05_sound_none 0
press Return 2

# "Select input device:" 1=Mouse, 2=Keyboard only, 3=Joystick
shot 06_input_prompt 0
press 2 1
shot 07_input_kb 0
press Return 3

# After setup: MicroProse logo / intro animation begins.
shot 08_intro_start 0
sleep 2; shot 09_intro_a 0
sleep 2; shot 10_intro_b 0
sleep 2; shot 11_intro_c 0
sleep 2; shot 12_intro_d 0
# Skip remaining intro
press space 2
shot 13_after_space 0
press space 2
shot 14_after_space2 0
press Return 2
shot 15_title_or_menu 0

# Main menu: "1. Start a new game" etc.
press Return 2
shot 16_after_return 0
press 1 2
shot 17_after_1 0
press Return 2
shot 18_after_return2 0

# Difficulty (1-5): 3 = Prince
shot 19_difficulty 0
press 3 1
shot 20_diff_picked 0
press Return 2

# Number of civilizations
shot 21_num_civs 0
press Return 2
shot 22_num_picked 0

# Tribe select (Roman = 1)
shot 23_tribe_select 0
press 1 1
shot 24_tribe_picked 0
press Return 2

# Name entry
shot 25_name_entry 0
xdotool type --window "$WID" "ClaudeAgent" 2>/dev/null
sleep 1
shot 26_name_typed 0
press Return 3

# World generation
shot 27_worldgen_start 0
for i in 1 2 3 4 5 6; do
  sleep 2
  shot "28_worldgen_$i" 0
done
shot 29_worldgen_done 0

# First in-game view (settler on map)
sleep 3
shot 30_first_view 0

# Movement: try moving the settler
press Right 1; shot 31_right 0
press Down 1;  shot 32_down 0
press Left 1;  shot 33_left 0
press Up 1;    shot 34_up 0

# Found city: 'b' for "build city" in DOS Civ1
press b 2; shot 35_b_settle 0
# Confirm city dialog (Return or 'y')
press Return 2; shot 36_city_founded 0

# End turn (Enter or space at "End of Turn" prompt)
press Return 2
shot 37_end_turn_1_a 0
sleep 2
shot 38_end_turn_1_b 0

# Subsequent turns: keep pressing Return / space to advance
for i in 1 2 3 4 5; do
  press Return 1
  sleep 1
  shot "39_loop_${i}_a" 0
  press space 1
  shot "39_loop_${i}_b" 0
done

# F10 menu (game menu)
press F10 2
shot 40_F10_menu 0
press Escape 2
shot 41_after_esc 0

# More turn looping
for i in 6 7 8 9 10; do
  press Return 1
  sleep 1
  shot "42_turn_${i}" 0
done

# Done
kill $DOSPID 2>/dev/null || true
sleep 1
kill $XVFB_PID 2>/dev/null || true
echo "DONE"
echo "captured files:"
ls /work/out/dos_play | wc -l
ls /work/out/dos_play | head -60
