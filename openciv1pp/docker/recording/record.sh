#!/bin/bash
# Record side-by-side comparison of openciv1pp vs DOS Civ1.
# Runs entirely inside the openciv1pp-recorder Docker container.
# Expects:
#   /work/src  -> openciv1pp source tree   (rw, for ./build)
#   /work/dos  -> DOS Civ1 assets          (ro)
#   /work/out  -> output dir for videos    (rw)
set -euo pipefail
cd /work/src

# 1. Build openciv1pp inside the container
echo "=== [1/8] Build openciv1pp ==="
rm -rf build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ls -la build/openciv1pp

# 2. Start Xvfb on display :99 (1280x720x24)
echo "=== [2/8] Start Xvfb on :99 ==="
Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR &
XVFB_PID=$!
export DISPLAY=:99
# wait for Xvfb to come up
for _ in $(seq 1 20); do
  if xdpyinfo -display :99 >/dev/null 2>&1; then break; fi
  sleep 0.25
done
xdpyinfo -display :99 >/dev/null
echo "Xvfb up (pid=$XVFB_PID)"

# Helper: kill any running instance of $1 silently
killq() { kill "$1" 2>/dev/null || true; wait "$1" 2>/dev/null || true; }

# 3. Record openciv1pp (30 s)
echo "=== [3/8] Record openciv1pp ==="
ffmpeg -y -loglevel error -video_size 1280x720 -framerate 15 -f x11grab -i :99.0 \
       -t 35 -pix_fmt yuv420p -c:v libx264 -preset veryfast \
       /work/out/openciv1pp_raw.mp4 &
FFMPEG_PID=$!

# Launch openciv1pp in interactive --game mode against the DOS assets
./build/openciv1pp --assets /work/dos/extracted/ --game >/tmp/oci.log 2>&1 &
OCI_PID=$!
sleep 3

# Wait for an SDL window to appear & focus it before sending keys.
OCI_WIN=""
for _ in $(seq 1 20); do
  OCI_WIN=$(xdotool search --name 'OpenCiv\|openciv\|Civ' 2>/dev/null | head -1 || true)
  if [ -n "$OCI_WIN" ]; then break; fi
  sleep 0.5
done
if [ -n "$OCI_WIN" ]; then
  xdotool windowactivate --sync "$OCI_WIN" 2>/dev/null || true
  echo "openciv1pp window id=$OCI_WIN"
fi

# Drive through TITLE -> MAIN_MENU -> DIFFICULTY -> TRIBE -> NAME -> STARTING -> PLAYING.
# Bind each key to the window so it's delivered even without keyboard focus.
KEY_TARGET=()
if [ -n "$OCI_WIN" ]; then KEY_TARGET=(--window "$OCI_WIN"); fi
xdotool key "${KEY_TARGET[@]}" --delay 800 Return || true   # TITLE -> MAIN_MENU
sleep 3
xdotool key "${KEY_TARGET[@]}" --delay 800 Return || true   # MAIN_MENU pick "開始新局" -> DIFFICULTY
sleep 4
xdotool key "${KEY_TARGET[@]}" --delay 700 Return || true   # DIFFICULTY default -> TRIBE
sleep 5
xdotool key "${KEY_TARGET[@]}" --delay 700 Return || true   # TRIBE default -> NAME
sleep 5
xdotool key "${KEY_TARGET[@]}" --delay 700 Return || true   # NAME accept -> STARTING
sleep 4
xdotool key "${KEY_TARGET[@]}" --delay 700 Return || true   # STARTING -> PLAYING
sleep 3
# Hold on the playing screen for a few seconds (do nothing) so the viewer can see it.

killq "$FFMPEG_PID"
kill "$OCI_PID" 2>/dev/null || true
sleep 1
echo "openciv1pp recording done"
ls -la /work/out/openciv1pp_raw.mp4 || true

# 4. Prepare DOS Civ1 working dir (writable copy of assets)
echo "=== [4/8] Stage DOS Civ1 working dir ==="
mkdir -p /tmp/civcontainer
# Prefer zip if present (cleaner state); fall back to extracted dir.
if [ -f /work/dos/CIVILIZATION.zip ]; then
  unzip -oq /work/dos/CIVILIZATION.zip -d /tmp/civcontainer
else
  cp -r /work/dos/extracted/* /tmp/civcontainer/
fi
ls /tmp/civcontainer | head

# 5. dosbox conf
cat > /tmp/dosbox.conf <<'EOF'
[sdl]
output=surface
fullscreen=false
[render]
aspect=true
[cpu]
core=auto
cycles=auto
[autoexec]
mount c /tmp/civcontainer
c:
CIV.EXE
exit
EOF

# 6. Record DOS Civ1 (30 s)
echo "=== [5/8] Record DOS Civ1 via DOSBox ==="
ffmpeg -y -loglevel error -video_size 1280x720 -framerate 15 -f x11grab -i :99.0 \
       -t 35 -pix_fmt yuv420p -c:v libx264 -preset veryfast \
       /work/out/dos_raw.mp4 &
FFMPEG_PID=$!

dosbox -conf /tmp/dosbox.conf >/tmp/dosbox.log 2>&1 &
DOS_PID=$!
sleep 6

# Wait for DOSBox window & focus it.
DOS_WIN=""
for _ in $(seq 1 20); do
  DOS_WIN=$(xdotool search --name 'DOSBox' 2>/dev/null | head -1 || true)
  if [ -n "$DOS_WIN" ]; then break; fi
  sleep 0.5
done
DOS_TGT=()
if [ -n "$DOS_WIN" ]; then
  xdotool windowactivate --sync "$DOS_WIN" 2>/dev/null || true
  DOS_TGT=(--window "$DOS_WIN")
  echo "DOSBox window id=$DOS_WIN"
fi

# Drive through DOS title screens.
# Version splash needs Return; then "Select graphics mode" expects a digit (1=VGA).
xdotool key "${DOS_TGT[@]}" --delay 900 Return || true
sleep 2
xdotool key "${DOS_TGT[@]}" --delay 700 1 || true     # graphics mode VGA
sleep 3
xdotool key "${DOS_TGT[@]}" --delay 700 1 || true     # sound mode "No sounds please"
sleep 5
# Past the menu: difficulty / tribe / city-name prompts use digits + Return.
xdotool key "${DOS_TGT[@]}" --delay 700 Return || true
sleep 3
xdotool key "${DOS_TGT[@]}" --delay 700 Return || true
sleep 3
xdotool key "${DOS_TGT[@]}" --delay 700 Return || true
sleep 4
xdotool key "${DOS_TGT[@]}" --delay 500 space space || true
sleep 3

killq "$FFMPEG_PID"
kill "$DOS_PID" 2>/dev/null || true
sleep 1
echo "DOS recording done"
ls -la /work/out/dos_raw.mp4 || true

# 7. Side-by-side: scale both to 640x400, hstack
echo "=== [6/8] Build side-by-side comparison.mp4 ==="
ffmpeg -y -loglevel error -i /work/out/openciv1pp_raw.mp4 -i /work/out/dos_raw.mp4 \
  -filter_complex '[0:v]scale=640:400,setsar=1[a];[1:v]scale=640:400,setsar=1[b];[a][b]hstack[v]' \
  -map '[v]' -c:v libx264 -pix_fmt yuv420p -preset veryfast \
  /work/out/comparison.mp4

# 8. Compact GIF for README (target a few MB)
echo "=== [7/8] Build comparison.gif ==="
ffmpeg -y -loglevel error -i /work/out/comparison.mp4 \
  -vf "fps=10,scale=960:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse" \
  /work/out/comparison.gif

# 9. Cleanup
echo "=== [8/8] Cleanup ==="
kill "$XVFB_PID" 2>/dev/null || true
echo "DONE: comparison.mp4 + comparison.gif written to /work/out/"
ls -la /work/out/
