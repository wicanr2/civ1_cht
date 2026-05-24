#!/bin/bash
# Civilization for Windows (1993) 繁中化 — 一鍵 Linux/WSL2 開發環境建置
#
# 用途: 在 Ubuntu 20.04/22.04 (含 WSL2) 上從零建立完整開發 + 測試環境。
#       裝完之後可以直接 apply patches + 跑 wine 看 Civ1 中文版。
#
# 用法:
#   1. git clone https://github.com/wicanr2/civ1_cht.git
#   2. cd civ1_cht
#   3. 把 1993 Civ for Windows 原版安裝檔放到 ./orig/
#      (檔案: CIV.EX$, CIVFONTS.FO$, CIVHELP.HL$, Civdata0-4.rs$, *.wa$ ...)
#   4. bash tools/setup_wsl_wine.sh
#
# 不會做的事 (使用者必須自己準備):
#   - 取得 1993 Civilization for Windows 原版安裝檔 (著作權, repo 不含)
#   - sudo 密碼 (script 會用到 sudo apt)

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$REPO_ROOT"

echo "=============================================="
echo " Civ1 CHT — Linux/WSL2 開發環境一鍵建置"
echo " Repo: $REPO_ROOT"
echo "=============================================="
echo

# ---------- Step 1: 系統套件 ----------
echo "[1/6] 安裝系統套件 (wine + i386 + python3 + Big5 字型)..."
if ! dpkg --print-foreign-architectures 2>/dev/null | grep -q i386; then
    sudo dpkg --add-architecture i386
fi
sudo apt-get update -qq
sudo apt-get install -y \
    wine wine32:i386 wine64 \
    python3 python3-pip \
    fonts-arphic-uming fonts-arphic-bsmi00lp \
    cabextract xxd unzip \
    >/dev/null
echo "    [OK] apt packages installed"

WINE_VER=$(wine --version 2>/dev/null || echo "unknown")
echo "    wine version: $WINE_VER"
echo

# ---------- Step 2: 檢查原版檔 ----------
echo "[2/6] 檢查原版安裝檔..."
ORIG_DIR="$REPO_ROOT/orig"
mkdir -p "$ORIG_DIR"

# 大小寫不敏感找原版檔 (Linux fs case-sensitive, 但安裝光碟有時 mixed case)
find_orig() {
    local pattern="$1"
    find "$ORIG_DIR" -maxdepth 1 -iname "$pattern" 2>/dev/null | head -1
}

CIV_EXS=$(find_orig 'CIV.EX$')
FONT_FOS=$(find_orig 'CIVFONTS.FO$')

if [ -z "$CIV_EXS" ] || [ -z "$FONT_FOS" ]; then
    echo
    echo "❌ 原版安裝檔不齊全 (在 $ORIG_DIR/ 找不到 CIV.EX\$ 或 CIVFONTS.FO\$)"
    echo
    echo "   請把 1993 Civ for Windows 安裝光碟 / 軟碟內容拷貝到:"
    echo "   $ORIG_DIR/"
    echo
    echo "   至少需要: CIV.EX\$, CIVFONTS.FO\$"
    echo "   建議連同: CIVHELP.HL\$, Civdata0-4.rs\$, *.wa\$, READ.ME\$"
    echo
    echo "   都放好之後再跑一次本 script。"
    exit 2
fi
echo "    [OK] 找到 CIV.EX\$ ($CIV_EXS)"
echo "    [OK] 找到 CIVFONTS.FO\$ ($FONT_FOS)"
echo

# ---------- Step 3: 解 EDILZSS2 ----------
echo "[3/6] 解開 EDILZSS2 壓縮檔到 build/extracted/..."
EXTRACT_DIR="$REPO_ROOT/build/extracted"
mkdir -p "$EXTRACT_DIR"

decode_one() {
    local src="$1" dst="$2"
    if [ -f "$src" ]; then
        python3 "$REPO_ROOT/tools/edilzss2_decode.py" "$src" "$dst" 2>&1 | tail -3
        if [ -f "$dst" ]; then
            local sz=$(stat -c%s "$dst")
            echo "    [OK] $(basename "$src") -> $(basename "$dst") ($sz bytes)"
        fi
    fi
}

decode_one "$CIV_EXS"      "$EXTRACT_DIR/CIV.EXE"
decode_one "$FONT_FOS"     "$EXTRACT_DIR/CIVFONTS.FON"

HELP_HLS=$(find_orig 'CIVHELP.HL$' || true)
README_MES=$(find_orig 'READ.ME$' || true)
[ -n "$HELP_HLS" ]   && decode_one "$HELP_HLS"   "$EXTRACT_DIR/CIVHELP.HLP"
[ -n "$README_MES" ] && decode_one "$README_MES" "$EXTRACT_DIR/READ.ME"

# Civdata0-4
for n in 0 1 2 3 4; do
    rsc=$(find_orig "Civdata${n}.rs\$" || true)
    [ -n "$rsc" ] && decode_one "$rsc" "$EXTRACT_DIR/Civdata${n}.RSC"
done

# WAVs (各領袖語音, *.wa$)
shopt -s nullglob nocaseglob
for src in "$ORIG_DIR"/*.wa\$; do
    base=$(basename "$src")
    out_base="${base%.*}.wav"
    decode_one "$src" "$EXTRACT_DIR/$out_base"
done
shopt -u nocaseglob
echo

# ---------- Step 4: Apply patches ----------
echo "[4/6] Apply Phase 1/2/3 patches..."
PATCH_DIR="$REPO_ROOT/build/patched"
mkdir -p "$PATCH_DIR"

# Phase 1 prep: 抽 RT_DIALOG catalogue 到 build/civ_dialogs.json
DIALOG_JSON="$REPO_ROOT/build/civ_dialogs.json"
if [ -f "$EXTRACT_DIR/CIV.EXE" ]; then
    python3 "$REPO_ROOT/tools/ne_dialog_extract.py" \
        "$EXTRACT_DIR/CIV.EXE" "$DIALOG_JSON" >/dev/null 2>&1 || true
    [ -f "$DIALOG_JSON" ] && echo "    [OK] extracted RT_DIALOG catalogue -> civ_dialogs.json"
fi

# Phase 1: RT_DIALOG Big5 patch
if [ -f "$EXTRACT_DIR/CIV.EXE" ] && [ -f "$DIALOG_JSON" ]; then
    python3 "$REPO_ROOT/tools/ne_dialog_patch.py" \
        "$EXTRACT_DIR/CIV.EXE" \
        "$DIALOG_JSON" \
        "$REPO_ROOT/data/dialog_translations.json" \
        "$PATCH_DIR/CIV.EXE.p1" >/dev/null 2>&1 && \
        echo "    [OK] Phase 1 (RT_DIALOG) -> CIV.EXE.p1" || \
        echo "    [WARN] Phase 1 patch 失敗"
fi

# Phase 2: CIVFONTS.FON dfCharSet patch
if [ -f "$EXTRACT_DIR/CIVFONTS.FON" ]; then
    python3 "$REPO_ROOT/tools/ne_font_patch_charset.py" \
        "$EXTRACT_DIR/CIVFONTS.FON" \
        "$PATCH_DIR/CIVFONTS.FON.cht" >/dev/null 2>&1 && \
        echo "    [OK] Phase 2 (CIVFONTS dfCharSet) -> CIVFONTS.FON.cht" || \
        echo "    [WARN] Phase 2 patch 失敗"
fi

# Phase 3: inline string patch (疊在 Phase 1 output 上)
if [ -f "$PATCH_DIR/CIV.EXE.p1" ]; then
    python3 "$REPO_ROOT/tools/inline_string_patch.py" \
        "$PATCH_DIR/CIV.EXE.p1" \
        "$REPO_ROOT/data/inline_translations.json" \
        "$PATCH_DIR/CIV.EXE.p3" --apply >/dev/null 2>&1 && \
        echo "    [OK] Phase 3 (inline strings) -> CIV.EXE.p3" || \
        echo "    [WARN] Phase 3 patch 失敗"
fi
echo

# ---------- Step 5: Wine prefix init + font subst ----------
echo "[5/6] 初始化 wine prefix..."
export WINEARCH=win32
export WINEPREFIX="${WINEPREFIX:-$HOME/.wine-civ1}"

if [ ! -d "$WINEPREFIX" ]; then
    wineboot -i >/dev/null 2>&1 || true
    echo "    [OK] wine prefix initialized ($WINEPREFIX)"
else
    echo "    [SKIP] wine prefix 已存在 ($WINEPREFIX)"
fi

# 設 win31 模式
wine reg add 'HKCU\Software\Wine' /v Version /d win31 /f >/dev/null 2>&1 || true

# 跑 Phase 2 wine setup (ACP=950 + FontSubstitutes)
bash "$REPO_ROOT/tools/wine_setup_phase2.sh" 2>&1 | tail -10 || \
    echo "    [WARN] wine_setup_phase2.sh 出錯（可手動補跑）"
echo

# ---------- Step 6: Deploy 到 wine prefix ----------
echo "[6/6] 部署 patched 遊戲到 $WINEPREFIX/drive_c/CIV/..."
GAME_DIR="$WINEPREFIX/drive_c/CIV"
mkdir -p "$GAME_DIR"

# patched binaries 優先；如果 patch 沒成功，退回原版 (debug 用)
if [ -f "$PATCH_DIR/CIV.EXE.p3" ]; then
    cp "$PATCH_DIR/CIV.EXE.p3" "$GAME_DIR/CIV.EXE"
    echo "    [OK] CIV.EXE = Phase 1+3 patched"
elif [ -f "$PATCH_DIR/CIV.EXE.p1" ]; then
    cp "$PATCH_DIR/CIV.EXE.p1" "$GAME_DIR/CIV.EXE"
    echo "    [OK] CIV.EXE = Phase 1 patched (Phase 3 失敗，退回 p1)"
else
    cp "$EXTRACT_DIR/CIV.EXE" "$GAME_DIR/CIV.EXE"
    echo "    [WARN] CIV.EXE = 原版 (patches 都失敗)"
fi

if [ -f "$PATCH_DIR/CIVFONTS.FON.cht" ]; then
    cp "$PATCH_DIR/CIVFONTS.FON.cht" "$GAME_DIR/CIVFONTS.FON"
    echo "    [OK] CIVFONTS.FON = Phase 2 patched"
else
    cp "$EXTRACT_DIR/CIVFONTS.FON" "$GAME_DIR/CIVFONTS.FON"
    echo "    [WARN] CIVFONTS.FON = 原版"
fi

# 原版 assets (未 patch)
[ -f "$EXTRACT_DIR/CIVHELP.HLP" ] && cp "$EXTRACT_DIR/CIVHELP.HLP" "$GAME_DIR/"
for n in 0 1 2 3 4; do
    [ -f "$EXTRACT_DIR/Civdata${n}.RSC" ] && cp "$EXTRACT_DIR/Civdata${n}.RSC" "$GAME_DIR/"
done
cp "$EXTRACT_DIR"/*.wav "$GAME_DIR/" 2>/dev/null || true

echo "    [OK] 部署完成"
echo
echo "    $GAME_DIR 內容:"
ls -la "$GAME_DIR" | sed 's/^/      /' | head -15
echo

# ---------- 完成 ----------
echo "=============================================="
echo " ✅ 環境建置完成"
echo "=============================================="
echo
echo " 啟動 Civ1 繁中版:"
echo "   export WINEPREFIX=$WINEPREFIX"
echo "   cd $GAME_DIR"
echo "   wine CIV.EXE"
echo
echo " 注意 (踩過的雷):"
echo "   - 純 Linux desktop (X11/Wayland) — 完整可玩"
echo "   - WSLg 環境 — 視窗會出，但 keyboard/mouse pipeline 對 CIV1 不通"
echo "     (xdotool send key/click 全被 CIV.EXE 吞)，只能看 intro 不能進主選單"
echo "   - 主選單視覺驗證 (中文): 純 Linux host or 別走 WSLg"
echo
echo " 下步: 看 README.md / docs/PROJECT_MEMORY.md / docs/SKILL.md"
