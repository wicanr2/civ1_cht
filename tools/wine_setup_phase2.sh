#!/bin/bash
# Phase 2: wine prefix setup for Big5 rendering
#
# Pre-requisites:
#   - WSL Ubuntu 22.04+ with wine 6.0+ (incl wine32:i386)
#   - WINEPREFIX exists and is initialized (wineboot -i ran)
#   - Big5-capable font installed at OS level (e.g. AR PL UMing TW)
#
# Run AFTER:
#   - tools/edilzss2_decode.py decompressed CIVFONTS.FON
#   - tools/ne_font_patch_charset.py produced CIVFONTS.FON.cht
#   - patched CIVFONTS.FON copied to $WINEPREFIX/drive_c/CIV/

set -e
WINEPREFIX=${WINEPREFIX:-/root/.wine-civ1}
export WINEPREFIX

# 1. Set system ACP (ANSI Code Page) to 950 (Big5) so Win16 GDI walks DBCS pairs
wine reg add 'HKLM\System\CurrentControlSet\Control\Nls\CodePage' \
    /v ACP   /d 950 /f >/dev/null
wine reg add 'HKLM\System\CurrentControlSet\Control\Nls\CodePage' \
    /v OEMCP /d 950 /f >/dev/null
echo "[OK] ACP=950, OEMCP=950"

# 2. Install Big5 fonts at OS level (idempotent)
if ! fc-list :lang=zh-tw | grep -q 'AR PL UMing TW'; then
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        fonts-arphic-uming fonts-arphic-bsmi00lp >/dev/null 2>&1
    fc-cache -fv >/dev/null 2>&1
fi
echo "[OK] AR PL UMing TW available via fontconfig"

# 3. Set Windows-standard FontSubstitutes with charset translation
#    Format: "FaceA,charsetA" = "FaceB,charsetB"
#    GDI rewrites CreateFont(face=CIVTIMES12, charset=0) into
#    CreateFont(face='AR PL UMing TW', charset=136) automatically.
REPLACEMENT="AR PL UMing TW,136"
for face in CIVTIMES10 CIVTIMES12 CIVTIMES14 CIVTIMES18 CIVTIMES24 CIVTIMES30 CIVTIMES36; do
    wine reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes' \
        /v "${face},0"   /d "$REPLACEMENT" /f >/dev/null
    wine reg add 'HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes' \
        /v "${face},136" /d "$REPLACEMENT" /f >/dev/null
done
echo "[OK] HKLM FontSubstitutes: CIVTIMES* -> $REPLACEMENT"

# 4. Set wine Replacements as backup (some GDI paths only honor this one)
for face in CIVTIMES10 CIVTIMES12 CIVTIMES14 CIVTIMES18 CIVTIMES24 CIVTIMES30 CIVTIMES36; do
    wine reg add 'HKCU\Software\Wine\Fonts\Replacements' \
        /v "$face" /d "AR PL UMing TW" /f >/dev/null
done
echo "[OK] HKCU Wine Fonts Replacements: CIVTIMES* -> AR PL UMing TW (fallback)"

# 5. Restart wineserver so registry changes load
wineserver -k 2>/dev/null
sleep 1
echo "[OK] wineserver restarted"

echo
echo "Phase 2 wine setup complete. Run wine CIV.EXE to test."
