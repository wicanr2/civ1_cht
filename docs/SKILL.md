---
name: civ1-cht
description: 接續 1993 Sid Meier 文明帝國 1 Windows 版 (MicroProse Civilization for Windows) 繁體中文化專案。當使用者提到 Civ1、Civilization 1、文明帝國 1、CIV.EXE、CIVFONTS.FON、CIVTIMES12/24、CIVDIALOG、EDILZSS2、EDI Install Pro、MicroProse 1993 安裝器，或想擴充 Civ1 翻譯 (Batch B/C/D/E)、打包 Win10 portable、回頭做 256-color shim、處理 14 文明領袖名 (Solomon/Augustus/Charlemagne/Jefferson/...)、24 個 RT_DIALOG patch、或 inline string slot-length 約束時觸發。**主動觸發**: 即使使用者只說「繼續做 Civ1」「補譯」「打包文明」也要套用。也涵蓋同類技術 — MicroProse 1992-1995 自家 EDILZSS2 壓縮格式 (Master of Magic、M1 Tank Platoon II 等都用)、Win16 NE inline ASCII 字串 in-place patch、Win16 RT_DIALOG walker null-terminated 約束、wine font subst CHINESEBIG5_CHARSET 路線。
---

# 文明帝國 1 (1993 Civilization for Windows) 繁體中文化

接續 2026 年 5 月 24 日進行中的 1993 MicroProse Civilization for Windows 繁中化專案。**先讀完此 SKILL.md 確認專案現況**，再決定下一步。

## 安裝到另一台機

```bash
# 1. 確認 ~/.claude/skills/ 目錄存在
mkdir -p ~/.claude/skills/civ1-cht

# 2. 從 repo clone 或拷貝 docs/SKILL.md
cp docs/SKILL.md ~/.claude/skills/civ1-cht/SKILL.md

# 3. 把 project memory 也丟進 user-level memory
mkdir -p ~/.claude/projects/$(echo "$PWD" | sed 's|/|-|g')/memory/
cp docs/PROJECT_MEMORY.md ~/.claude/projects/$(echo "$PWD" | sed 's|/|-|g')/memory/project_civ1_cht.md
```

下次 Claude session 提到 "Civ1 / 文明帝國 1 / CIV.EXE / EDILZSS2" 時就會自動 trigger 這個 skill。

## 路徑與 repo

| 項目 | 位置 |
|------|------|
| **GitHub repo** | https://github.com/wicanr2/civ1_cht |
| **Local work dir (Windows)** | `D:\03_game_tmp\civ1_cht\` |
| **Local work dir (Linux/WSL)** | repo clone 處 |
| **原版安裝來源** | 使用者自備 1993 MicroProse 安裝光碟 |
| **解壓產物** | `build/extracted/` (setup script 產出) |
| **Wine prefix** | `~/.wine-civ1` (win32, Win 3.1 mode) |

## Phase 進度

| Phase | 任務 | 狀態 |
|------|------|------|
| 0 | EDILZSS2 RE + wine bring-up | ✅ |
| 1 | RT_DIALOG (24) Big5 patch — 54 instances / 34 unique | ✅ |
| 2 | CIVFONTS dfCharSet + FontSubstitutes | ✅ infra |
| **3** | inline string Batch A (48) — 主選單/難度/14 領袖 | ✅ Batch A |
| 3+ | Batch B (civ adj + wonder/unit) / C (event msg) / D (advisor) / E (format str) | 📋 |
| 4 | EDILZSS2 compressor (重打包 .EX$) | ⏸ optional |
| 5 | Win10 portable SFX | ⚠️ otvdm BLOCKER, 已 fallback WSL+wine |
| 6 | 256-color shim | ⏸ 預期不需要 |

## EDILZSS2 格式 (已完整破解)

MicroProse 1992-1995 自家壓縮，**也用在 Master of Magic、M1 Tank Platoon II 等同期作品**。

```
偏移       長度    內容
0x00       8       Magic "EDILZSS2"
0x08       16      Filename buffer (null-terminated, junk-padded uninit mem)
0x18       1       Separator byte (always 0x00)
0x19+      ?       LZSS stream
```

LZSS 變體：
- Window 4096 bytes, 初值 0x20，wpos_init = N − 18 (古典 Storer-Szymanski)
- Control byte **LSB-first**，bit=1 → literal (1 byte)，bit=0 → back-ref (2 bytes)
- Back-ref: `[offset_lo:8] [length−3:4 | offset_hi:4]`
- Offset = `((b2 & 0xF0) << 4) | b1` (12-bit)
- Length = `(b2 & 0x0F) + 3` (3..18)
- 整 stream 吃到 EOF，沒有 explicit terminator

**Decoder**: `tools/edilzss2_decode.py`（repo 內）
**Encoder**: 尚未寫（Phase 4 才需要 — 不重打回 .EX$ 也可直接 ship 解開檔）

### Decoder bug 雷區
早期版本 `for bit in range(8)` 內 `if idx >= len: break` 只跳出內層，外層 while 繼續 `data[idx]` → IndexError 被 try/except 吞 → 看起來只解出 51KB (應該 832KB)。修法：在內層 break 改成 return，徹底結束。

## CIV.EXE 構造

- **832,512 bytes** decompressed Win16 NE
- 133 segments
- Imports: KERNEL, USER, **GDI**, WIN87EM, MMSYSTEM, COMMDLG (全 by-ordinal，iname table 0 bytes)
- Resources: RT_CURSOR×16, RT_ICON×1, RT_MENU×1, **RT_DIALOG×24**, RT_ACCEL×1, RT_GROUP_*
- **No RT_STRING**: 遊戲文字全 inline plain ASCII 在 data segments (~b7000-bc000 區段)
- 自訂字型 face: `CIVTIMES12` / `CIVTIMES24` (in CIVFONTS.FON)
- 自訂 dialog class: `CIVDIALOG` (program-registered，**絕對不可譯**)

## CIVFONTS.FON 構造

244 KB Win16 NE 字型 library，21 個 RT_FONT 分兩家族：

**14 個文明裝飾字** (14pt bitmap ANSI charset, dfLastChar=0xD5)：
- CIVBABYLON / CIVZULU / CIVEGYPT / CIVENGLISH / CIVGREEK / CIVRUSSIAN / CIVGERMAN / **CIVCHINESE** (chop-suey 風 Latin **不是中文**) / CIVFRENCH / CIVROMAN / CIVINDIAN / CIVAMERICAN / CIVAZTEC / CIVMONGOL
- 各文明命名橫幅紋飾用

**7 個 UI 字 (CIVTIMES family)** (multi-size bitmap ANSI charset, dfLastChar=0xFF)：
- CIVTIMES10 / 12 / 14 / 18 / 24 / 30 / 36
- dialog/text 實際使用

**dfCharSet 位元偏移**：每個 FONTINFO 結構 offset 0x55。

## Phase 1: RT_DIALOG patcher 雷區

Win16 DLGITEMTEMPLATE 字串 null-terminated，後緊接 `createInfoSize` byte (walker 依賴位置)：

```
原: "Cancel\0" (7B: 6 text + null)  → walker 走到 idx 7 讀 createInfoSize
壞: "取消\0"  (5B: 4 text + null)  → walker 走到 idx 5 讀 byte 'l' (0x6C)
                                       當 createInfoSize=108 → 跳 108 bytes → 整 dialog 解析爆掉
```

**Fix**: 短 Big5 譯文必須補 **ASCII space (0x20) 到完全等於原 slot 長度**。button 右側多空格通常被 rect 裁掉看不到。

工具: `tools/ne_dialog_extract.py` + `tools/ne_dialog_patch.py`

## Phase 2: 字型替換策略 (Approach C)

原計畫 A (重打 RT_FONT bitmap 成 Big5 點陣) 不可行 — Win16 .FNT v2 `dfFirstChar`/`dfLastChar` uint8，原生 DBCS 支援不直觀。

**改走 C: 三層配合**

1. **CIVFONTS.FON dfCharSet 0x00 → 0x88 (CHINESEBIG5_CHARSET)** 對 7 個 CIVTIMES 字 (`tools/ne_font_patch_charset.py`)
2. **Wine FontSubstitutes registry**：
   ```
   HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes
     CIVTIMES12,0 = AR PL UMing TW,136
     (...所有 7 個 CIVTIMES sizes)
   ```
3. **OS Big5 字型**: `fonts-arphic-uming` (AR PL UMing TW) + `fonts-arphic-bsmi00lp` (1990s Arphic Big5)

全部自動化在 `tools/wine_setup_phase2.sh`。

## Phase 3: inline string patcher 對比 Phase 1

| 面向 | Phase 1 (RT_DIALOG) | Phase 3 (inline) |
|------|------|------|
| Walker | DLGITEMTEMPLATE 緊接 createInfoSize byte | 純 C strlen |
| Pad | ASCII space (`0x20`) — 必須等於原長 | NULL (`0x00`) — 乾淨 |
| Length | 必須相等 | ≤ 原長 |

工具: `tools/inline_string_extract.py` + `inline_string_triage.py` + `inline_string_patch.py`
翻譯字典: `data/inline_translations.json` (Batch A 已有 48 條)

### Patcher metadata filter 雷區
JSON 中 metadata key (`_comment`, `_batch_a`) 跟翻譯 key (`_Start a New Game_...`) 都以 `_` 開頭。
原本 `if not k.startswith('_')` 把翻譯 key 也濾掉。
**Fix**：metadata = `_` 開頭且尾段全小寫；翻譯 key 通常含大寫 / 空格 / 標點。

## 14 領袖名 → 14 文明對應 (Civ1)

| Civ | 領袖 (English) | 中文 |
|---|---|---|
| Babylonians | Solomon the Wise | 智者所羅門 |
| Romans | Emperor Augustus | 奧古斯都大帝 |
| Germans | King Charlemagne | 查理曼大帝 |
| Egyptians | Ramses (?) | 拉美西斯 (待補) |
| Americans | Thomas Jefferson / Lincoln | 湯瑪斯傑佛遜 / 林肯 |
| Greeks | Alexander (the Great) | 亞歷山大 (待補) |
| Indians | Gandhi (Mahatma) | 甘地 (待補) |
| Russians | Stalin / Lenin | 史達林 / 列寧 |
| Zulus | Shaka | 沙卡 (待補) |
| French | Napoleon / Charles De Gaulle | 拿破崙 / 戴高樂將軍 |
| Aztecs | Montezuma | 蒙特蘇瑪 (待補) |
| Chinese | Mao Tse Tung | 毛澤東 (待補) |
| English | Elizabeth / Churchill / Chamberlain | 伊麗莎白 / 邱吉爾首相 / 張伯倫首相 |
| Mongols | Genghis Khan | 成吉思汗 (待補) |
| Japanese | Shogun Tokugawa | 德川幕府將軍 |
| Vikings | Eric the Red | 紅鬍子艾瑞克 |
| Turks | Sulayman the Magnificent | 蘇萊曼大帝 |
| German Kaiser | Kaiser Wilhelm | 威廉皇帝 |
| French | Louis the XVI | 路易十六 |
| Roman | Caesar (in WAV file: Ceas.wa$) | 凱撒 (待補) |

語音檔 (`*.wa$`) 對應領袖：Alex/Ceas/Eliz/Fred/Gand/Geng/Hama/Linc/Mao/Mont/Napo/Rams/Shak/Stal。

## Wine bring-up 標準 setup (Linux/WSL)

一鍵腳本: `tools/setup_wsl_wine.sh`。或手動：

```bash
# 1. 裝 wine 32-bit (Win16 NE 需要)
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install -y wine wine32:i386 wine64 cabextract fonts-arphic-uming fonts-arphic-bsmi00lp

# 2. 解 EDILZSS2 (5 個 .EX$/.FO$/.HL$/.RS$/.RM$)
python3 tools/edilzss2_decode.py /path/to/CIV.EX\$ ./CIV.EXE
python3 tools/edilzss2_decode.py /path/to/CIVFONTS.FO\$ ./CIVFONTS.FON
# ...

# 3. Init wine prefix
export WINEARCH=win32 WINEPREFIX=~/.wine-civ1
wineboot -i
wine reg add 'HKCU\Software\Wine' /v Version /d win31 /f

# 4. Phase 2 setup (font subst)
bash tools/wine_setup_phase2.sh

# 5. Run
cp CIV.EXE CIVFONTS.FON Civdata*.RSC *.wav $WINEPREFIX/drive_c/CIV/
cd $WINEPREFIX/drive_c/CIV
wine CIV.EXE
```

## WSL 環境的 input 限制 (踩過的雷)

WSLg + winevdm 之間的 keyboard/mouse input pipeline 對 CIV.EXE **完全不通**。xdotool send key/click 到 wine 視窗 (內層、desktop 包裝、root) 都被 CIV 吞掉，game 一直停在 title screen 不進主選單。

**繞道**：純 Linux desktop (X11/Wayland host) 可正常輸入；或在 WSL 內驗證走 byte 級 hex dump 證明 Big5 在位。

## ⚠️ otvdm v0.9.0 + Civ1 已知 BLOCKER

**v0.9.0 (2023-09 latest release) 跟 Civ1 不相容**：
- cmd start launch CIV.EXE + 加 font subst → 5 秒 SEGV `IP:06AA access=0x0000D460` (deterministic)
- ORIG CIV.EXE 不加任何 patch 也 SEGV (test_0_orig 黑畫面 → crash)
- Workaround 移除 wavs → 還是掛
- **Root cause**: cracyc 引入的 `sndPlaySound` bug
- **Fix 在 cracyc/winevdm master HEAD `cd84ae2` (2025-11-30, PR #1536)** 但 v0.9.0 不含
- otvdm CI 沒 release artifacts；cracyc 個人 build 已過期 (90d)
- 上游 issue: #1545 (CLOSED Dec 2025) + #1480 (OPEN)

**目前決定 (Phase 5 Option C)**：放棄 Win10 portable，主推 WSL+wine 路線。Win10 portable 等以後 otvdm 新 release 或自己 build cracyc master 才能恢復。

**重啟 Win10 portable 條件 (任一達成)**：
1. otya128/winevdm 出 v0.9.1+ 含 sndPlaySound fix
2. 找到 cracyc master 的可用 build artifact (forks / mirrors)
3. 自己用 VS 2017+ build cracyc master `cd84ae2`

## 跟 PG-cht / 其他 Sid Meier 風格遊戲的關聯

- **共用 `wing-portable-sfx` 打包路線** (Win10 portable)，但 CIV1 1993 pre-WinG **不需要 WING32 patch** (PG-cht 的 0xA55 jnz→nop)
- **共用 Big5 font subst** (AR PL UMing TW + dfCharSet=0x88)
- **共用 inline string slot-length** 約束 (跟 PG 類似但 Civ1 C strings 比 PG Borland Pascal 寬鬆)

## 已知翻譯 (引用慣例)

Batch A 主要術語（`data/inline_translations.json`）：

| 英文 | 譯文 | 註 |
|---|---|---|
| Senate | 參議院 | (不是「議院」，台灣慣例) |
| Wonder | 奇蹟 | |
| Wonder of the World | 世界奇蹟 | |
| Civilization | 文明 | (game title) |
| Despotism | 君主獨裁 | |
| Republic | 共和 | |
| Democracy | 民主 | |
| Anarchy | 無政府 | |
| Settler | 拓荒者 | |
| Capital | 首都 | |
| Marketplace | 市集 | |
| Cathedral | 大教堂 | |
| Library | 圖書館 | |

擴充 Batch B/C/D/E 時保持術語一致。

## 主動觸發場景

即使使用者只說以下任一也要套用此 skill：
- 「繼續做 Civ1 / Civ1 補譯」
- 「Civ1 打包 portable」
- 「Civ1 加 X 翻譯」
- 「Civ1 跑不起來」
- 「文明 1 中文化」

當使用者談到任何 **MicroProse 1992-1995 同期遊戲** 的 EDILZSS2 解壓或 Win16 NE 字串 patch 時也適用。
