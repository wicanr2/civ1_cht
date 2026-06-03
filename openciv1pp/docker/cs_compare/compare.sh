#!/bin/bash
# Hardened comparison runner. Container-only. NEVER run outside docker.
# Avoid `set -uo pipefail` — many of our pipes intentionally tolerate failure
# (`find | grep | head -1` returns nonzero when nothing matches), and a stray
# pipefail combined with subshell `$(...)` was silently killing later phases.
set +e

[ "${DISPLAY:-}" = ":99" ] || { echo "FAIL: DISPLAY=$DISPLAY (must be :99 inside container)"; exit 1; }

mkdir -p /work/out/logs
# Write a persistent stdout log without `exec > >(tee ...)`. That construct was
# triggering early SHELL TERMINATION after the first `$(find|grep|head -1)`
# pipeline — most likely tee's process substitution interacting badly with
# pipefail when grep was SIGPIPE'd. We just `tee -a` at strategic points
# instead, and rely on the container-attached stdout for the live log.
LOG=/work/out/logs/compare_stdout.log
: > "$LOG"
log() { echo "$@" | tee -a "$LOG"; }

Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR &
XVFB_PID=$!
for i in 1 2 3 4 5; do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 1; done
xdpyinfo -display :99 >/dev/null 2>&1 || { echo "FAIL: Xvfb did not start"; exit 1; }
echo "[ok] Xvfb :99 up (PID=$XVFB_PID)"

mkdir -p /work/out/oci_pp /work/out/oci_cs /work/out/sidebyside /work/out/logs

# -----------------------------------------------------------------------------
# Phase A: openciv1pp screenshots
# We rebuild openciv1pp inside container (clean env) and run --shot* modes.
# Output PPM is converted to PNG. We also copy any pre-existing polish_*.png
# from host so the comparison has both sources.
# -----------------------------------------------------------------------------
echo "[phase A] openciv1pp"
cd /work/openciv1pp
# Skip rebuild if a recent container build already exists and all 5 shots are present
ALL_SHOTS=1
for n in pp_title pp_intro pp_menu pp_city pp_world; do
  [ -f /work/out/oci_pp/$n.png ] || ALL_SHOTS=0
done
if [ "$ALL_SHOTS" = "1" ] && [ -x build_docker/openciv1pp ]; then
  echo "[skip] openciv1pp already built + all shots present"
elif [ ! -x build_docker/openciv1pp ]; then
  rm -rf build_docker
  cmake -S . -B build_docker -DCMAKE_BUILD_TYPE=Release > /work/out/logs/pp_cmake.log 2>&1 \
    && cmake --build build_docker -j 2 > /work/out/logs/pp_build.log 2>&1
fi

if [ -x build_docker/openciv1pp ]; then
  echo "[ok] openciv1pp built in container"
  BIN=./build_docker/openciv1pp
  ASSETS=/work/dos/extracted
  set +e
  $BIN --assets $ASSETS --shotTitle /work/out/oci_pp/pp_title.ppm  > /work/out/logs/pp_shot_title.log 2>&1
  $BIN --assets $ASSETS --shotIntro /work/out/oci_pp/pp_intro.ppm  > /work/out/logs/pp_shot_intro.log 2>&1
  $BIN --assets $ASSETS --shotMenu  /work/out/oci_pp/pp_menu.ppm   > /work/out/logs/pp_shot_menu.log 2>&1
  $BIN --assets $ASSETS --shotCity  /work/out/oci_pp/pp_city.ppm   > /work/out/logs/pp_shot_city.log 2>&1
  $BIN --assets $ASSETS --shotWorld /work/out/oci_pp/pp_world.ppm  > /work/out/logs/pp_shot_world.log 2>&1
  set -e
  for f in /work/out/oci_pp/*.ppm; do
    [ -f "$f" ] && convert "$f" "${f%.ppm}.png" 2>/dev/null && rm "$f"
  done
else
  echo "[warn] openciv1pp container build failed — falling back to host polish_*.png"
fi

# Always also copy pre-existing host polish_*.png as reference set
cp /work/openciv1pp/docs/screenshots/polish_title_zh.png        /work/out/oci_pp/polish_title_zh.png       2>/dev/null || true
cp /work/openciv1pp/docs/screenshots/polish_intro_birth_zh.png  /work/out/oci_pp/polish_intro_birth_zh.png 2>/dev/null || true
cp /work/openciv1pp/docs/screenshots/polish_menu_zh.png         /work/out/oci_pp/polish_menu_zh.png        2>/dev/null || true
cp /work/openciv1pp/docs/screenshots/polish_city_zh.png         /work/out/oci_pp/polish_city_zh.png        2>/dev/null || true
cp /work/openciv1pp/docs/screenshots/polish_world_zh.png        /work/out/oci_pp/polish_world_zh.png       2>/dev/null || true

echo "[oci_pp dir]"; ls -la /work/out/oci_pp/

# -----------------------------------------------------------------------------
# Phase B: build & run C# OpenCiv1
# -----------------------------------------------------------------------------
echo "[phase B] OpenCiv1 (C#)"
cd /work/OpenCiv1
dotnet --info > /work/out/logs/cs_dotnet_info.log 2>&1 || true
head -10 /work/out/logs/cs_dotnet_info.log

# Restore + build using prewarmed NuGet cache (image build pre-fetched packages).
# `--network none` blocks api.nuget.org, so we must point restore at the local cache.
# NOTE: -c Release fails on Linux because src/Game/OpenCiv1Game.cs line 107
# (#else branch) references `Assembly.GetExecutingAssembly()` without
# `using System.Reflection;`. Debug branch is well-formed. We build Debug.
NUGET_LOCAL=${NUGET_PACKAGES:-/root/.nuget/packages}
echo "[nuget cache] $NUGET_LOCAL ($(ls -1 $NUGET_LOCAL 2>/dev/null | wc -l) pkgs)"
dotnet restore --source "$NUGET_LOCAL" --no-cache > /work/out/logs/cs_restore.log 2>&1
RESTORE_RC=$?
echo "[restore rc=$RESTORE_RC] tail:"
tail -10 /work/out/logs/cs_restore.log

dotnet build -c Debug --no-restore > /work/out/logs/cs_build.log 2>&1
BUILD_RC=$?
echo "[build rc=$BUILD_RC] tail:"
tail -25 /work/out/logs/cs_build.log
# Also produce a -c Release log for the comparison report (expected to fail).
dotnet build -c Release --no-restore > /work/out/logs/cs_build_release.log 2>&1 || true

CS_DLL=$(find . -path "*/bin/Release/net*/*.dll" 2>/dev/null | grep -iE "OpenCiv1\.dll$" | head -1)
if [ -z "$CS_DLL" ]; then
  CS_DLL=$(find . -path "*/bin/Release/net*/*.dll" 2>/dev/null | head -1)
fi
echo "[CS_DLL] $CS_DLL"

# Prefer Debug dll since we built Debug; widen search to all configs.
CS_DLL=$(find . -path "*/bin/Debug/net*/*.dll"   2>/dev/null | grep -iE "OpenCiv1\.dll$" | head -1)
[ -z "$CS_DLL" ] && CS_DLL=$(find . -path "*/bin/Release/net*/*.dll" 2>/dev/null | grep -iE "OpenCiv1\.dll$" | head -1)
echo "[CS_DLL re-resolved] $CS_DLL"

if [ -n "$CS_DLL" ] && [ -f "$CS_DLL" ] && [ "$BUILD_RC" = "0" ]; then
  # OpenCiv1 expects original DOS assets at ~/Dos/Civ1/ on Linux
  mkdir -p /root/Dos/Civ1
  # copy is safer than symlink in case the app walks
  cp -r /work/dos/extracted/* /root/Dos/Civ1/ 2>/dev/null || true
  ls /root/Dos/Civ1/ | head -5

  # First try GPU/default render
  echo "[run] dotnet $CS_DLL"
  dotnet "$CS_DLL" > /work/out/logs/cs_run.log 2>&1 &
  APP_PID=$!
  sleep 12

  WID=""
  for nm in "OpenCiv1" "Open Civ1" "Civ1" "Civilization" "MainWindow" "Avalonia"; do
    WID=$(xdotool search --name "$nm" 2>/dev/null | head -1)
    [ -n "$WID" ] && { echo "[ok] window matched name=$nm WID=$WID"; break; }
  done

  if [ -z "$WID" ]; then
    echo "[warn] no window — try AVALONIA_RENDER_MODE=software"
    kill $APP_PID 2>/dev/null; wait $APP_PID 2>/dev/null
    AVALONIA_RENDER_MODE=software dotnet "$CS_DLL" > /work/out/logs/cs_run_sw.log 2>&1 &
    APP_PID=$!
    sleep 12
    for nm in "OpenCiv1" "Open Civ1" "Civ1" "Civilization" "MainWindow" "Avalonia"; do
      WID=$(xdotool search --name "$nm" 2>/dev/null | head -1)
      [ -n "$WID" ] && { echo "[ok] sw window matched name=$nm WID=$WID"; break; }
    done
  fi

  if [ -n "$WID" ]; then
    shot()  { local n=$1; local d=${2:-0}; [ "$d" != "0" ] && sleep "$d"; import -window "$WID" "/work/out/oci_cs/$n.png" 2>>/work/out/logs/cs_import.log; }
    press() { xdotool key --window "$WID" "$1"; sleep "${2:-0.6}"; }

    shot 01_title 0
    press Return 2; shot 02_after_title 0
    press Return 2; shot 03_post 0
    press Return 2; shot 04_intro 0
    press Down 1; press Return 2; shot 05_menu 0
    press Return 3; shot 06_newgame 0
    for i in 1 2 3 4; do sleep 2; shot "07_loop_$i" 0; done
    press b 2; shot 08_b 0
    press Return 2; shot 09_ret 0
    press Escape 2; shot 10_esc 0
    # Whole-screen fallback (root window) in case import on WID is blank
    import -window root /work/out/oci_cs/99_root.png 2>>/work/out/logs/cs_import.log || true
  else
    echo "[fail] no Avalonia window appeared even with software renderer"
    # Still grab a root screenshot so we can see whatever surfaced
    import -window root /work/out/oci_cs/99_root_nowin.png 2>>/work/out/logs/cs_import.log || true
  fi
  kill $APP_PID 2>/dev/null || true
  wait $APP_PID 2>/dev/null || true
else
  echo "[fail] C# build did not produce a usable dll"
fi

echo "[oci_cs dir]"; ls -la /work/out/oci_cs/ 2>/dev/null || true

# -----------------------------------------------------------------------------
# Phase C: side-by-side composites
# -----------------------------------------------------------------------------
echo "[phase C] side-by-side"
mk() {
  local a=$1 b=$2 out=$3
  if [ ! -f "$a" ] || [ ! -f "$b" ]; then
    echo "[skip] $out (missing $a or $b)"
    return
  fi
  convert "$a" -resize 640x400 /tmp/_a.png
  convert "$b" -resize 640x400 /tmp/_b.png
  convert /tmp/_a.png /tmp/_b.png +append "$out"
  echo "[ok] $out"
}

# Prefer container-built pp shots, fall back to polish_*
for stem in title intro menu city world; do
  case $stem in
    title) pp=/work/out/oci_pp/pp_title.png;  alt=/work/out/oci_pp/polish_title_zh.png;        cs=/work/out/oci_cs/01_title.png ;;
    intro) pp=/work/out/oci_pp/pp_intro.png;  alt=/work/out/oci_pp/polish_intro_birth_zh.png;  cs=/work/out/oci_cs/04_intro.png ;;
    menu)  pp=/work/out/oci_pp/pp_menu.png;   alt=/work/out/oci_pp/polish_menu_zh.png;         cs=/work/out/oci_cs/05_menu.png ;;
    city)  pp=/work/out/oci_pp/pp_city.png;   alt=/work/out/oci_pp/polish_city_zh.png;         cs=/work/out/oci_cs/08_b.png ;;
    world) pp=/work/out/oci_pp/pp_world.png;  alt=/work/out/oci_pp/polish_world_zh.png;        cs=/work/out/oci_cs/07_loop_2.png ;;
  esac
  src=$pp
  [ -f "$src" ] || src=$alt
  mk "$src" "$cs" "/work/out/sidebyside/${stem}.png"
done

# -----------------------------------------------------------------------------
# Phase D: palette stats
# -----------------------------------------------------------------------------
echo "[phase D] palette stats"
python3 - <<'PY' > /work/out/palette.txt 2>&1
import os
from PIL import Image
out = []
for d in ('/work/out/oci_pp', '/work/out/oci_cs'):
    if not os.path.isdir(d):
        continue
    for f in sorted(os.listdir(d)):
        if not f.lower().endswith('.png'):
            continue
        p = os.path.join(d, f)
        try:
            im = Image.open(p).convert('RGB')
            cols = im.getcolors(im.width * im.height) or []
            out.append(f"{os.path.basename(d)}/{f}: {im.width}x{im.height}, {len(cols)} unique RGB")
        except Exception as e:
            out.append(f"{os.path.basename(d)}/{f}: ERROR {e}")
print("\n".join(out))
PY
cat /work/out/palette.txt | head -40

# Cleanup
kill $XVFB_PID 2>/dev/null || true
wait $XVFB_PID 2>/dev/null || true
echo "DONE"
echo "[oci_pp final]";       ls /work/out/oci_pp/       2>/dev/null
echo "[oci_cs final]";       ls /work/out/oci_cs/       2>/dev/null
echo "[sidebyside final]";   ls /work/out/sidebyside/   2>/dev/null
