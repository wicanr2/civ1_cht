#!/bin/bash
# R5 side-by-side pair builder. Reads /work/out/r5_oci/*_oci.png and
# /work/out/r5_dos/*_dos.png; upscales DOS x2 to match OCI 640x480; joins.
set -uo pipefail

OCI_DIR=/work/out/r5_oci
DOS_DIR=/work/out/r5_dos
OUT_DIR=/work/out/r5_pair
mkdir -p "$OUT_DIR"

# List of canonical 20 steps. Some have suffix variants (_live, _live2, etc.);
# we accept the canonical _oci / _dos name.
STEPS=(
  "01_TITLE"
  "02_CREDITS"
  "03_MAIN_MENU"
  "04_WIZARD_DIFFICULTY"
  "05_WIZARD_CIVS"
  "06_WIZARD_TRIBE"
  "07_WIZARD_NAME"
  "08_WORLD_GEN"
  "09_FIRST_TURN"
  "10_DISMISS_TUTORIAL"
  "11_MOVE_SETTLER_NORTH"
  "12_TRY_END_TURN_PREMATURELY"
  "13_CONSUME_SETTLER_MVP"
  "14_END_TURN_OK"
  "15_FOUND_CITY"
  "16_CITY_VIEW"
  "17_CHOOSE_PRODUCTION"
  "18_EXIT_CITY"
  "19_END_TURN_2_3_4"
  "20_CITY_REVIEW_TURN_5"
)

# Helper - find first matching file, accepting _live suffix fallbacks.
pick_one() {
  local dir="$1"; local stem="$2"; local side="$3"
  if [ -f "$dir/${stem}_${side}.png" ]; then echo "$dir/${stem}_${side}.png"; return; fi
  # try with _live suffix
  if [ -f "$dir/${stem}_live_${side}.png" ]; then echo "$dir/${stem}_live_${side}.png"; return; fi
  echo ""
}

# Create a missing placeholder
missing_png() {
  local out="$1"; local label="$2"
  convert -size 640x480 xc:'#202028' \
    -gravity center -font /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    -pointsize 28 -fill '#ff8080' \
    -annotate +0+0 "MISSING\n$label" "$out" 2>/dev/null || \
    convert -size 640x480 xc:'#202028' -gravity center \
      -pointsize 28 -fill white -annotate +0+0 "MISSING $label" "$out"
}

for stem in "${STEPS[@]}"; do
  oci="$(pick_one "$OCI_DIR" "$stem" oci)"
  dos="$(pick_one "$DOS_DIR" "$stem" dos)"
  oci_use="/tmp/${stem}_oci_norm.png"
  dos_use="/tmp/${stem}_dos_norm.png"
  if [ -n "$oci" ]; then
    convert "$oci" -resize 640x480 -background black -gravity center -extent 640x480 "$oci_use"
  else
    missing_png "$oci_use" "OCI $stem"
  fi
  if [ -n "$dos" ]; then
    # DOS is typically 640x400 from dosbox SDL surface; resize to 640x480 letterboxed.
    convert "$dos" -resize 640x480 -background black -gravity center -extent 640x480 "$dos_use"
  else
    missing_png "$dos_use" "DOS $stem"
  fi
  # Add captions
  convert "$oci_use" -gravity North -background '#1a1a2a' -splice 0x24 \
    -font /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    -pointsize 16 -fill '#80d0ff' -annotate +0+4 "openciv1pp" "$oci_use" 2>/dev/null || true
  convert "$dos_use" -gravity North -background '#1a1a2a' -splice 0x24 \
    -font /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc \
    -pointsize 16 -fill '#ffd080' -annotate +0+4 "DOS Civ1" "$dos_use" 2>/dev/null || true
  convert "$oci_use" "$dos_use" +append "$OUT_DIR/${stem}_pair.png"
done

echo "DONE pairs:"
ls "$OUT_DIR" | wc -l
ls "$OUT_DIR" | sort
