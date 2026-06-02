---
name: project-civ1-cht
description: 1993 Sid Meier 文明帝國 1 Windows 版繁中化專案；wine 上跑通，REPO + README 已建在 D:\03_game_tmp\civ1_cht
metadata:
  node_type: memory
  type: project
  originSessionId: 24e94b82-cd08-439d-8145-b7a9c1682a7c
---

# Civilization for Windows (1993 MicroProse) 繁體中文化

**Repo**: https://github.com/wicanr2/civ1_cht
**本機工作目錄 (Windows)**: `D:\03_game_tmp\civ1_cht\`
**本機工作目錄 (Linux/WSL)**: clone 後當前 cwd
**安裝來源**: 使用者自備 (1993 MicroProse 原版安裝檔)
**解壓產物**: `build/extracted/` (setup script 產出)
**Wine prefix**: `~/.wine-civ1` (win32, Win 3.1 mode)

## 安裝到另一台 Linux 機（從 git clone 開始）

```bash
git clone https://github.com/wicanr2/civ1_cht.git
cd civ1_cht
# 把 1993 Civ for Windows 原版安裝檔拷貝到 orig/
mkdir -p orig && cp /path/to/civ1win/* orig/
# 一鍵建環境
bash tools/setup_wsl_wine.sh
# Run
export WINEPREFIX=~/.wine-civ1
cd $WINEPREFIX/drive_c/CIV && wine CIV.EXE
```

## 起源 (1991 DOS / 1993 Windows)

- Sid Meier + Bruce Shelley，MicroProse 1991 DOS、1993 Windows port
- 14 文明，21 個 wonder，7 個 government，7000 BC–AD 2050
- 4X 鼻祖之一；Civ for Windows 是 1993 Win 3.1 NE 程式，封裝 EDI Install Pro

## Phase 0 (完成 2026-05-24)

### EDILZSS2 格式破解
MicroProse 自家壓縮（也用在 Master of Magic、M1 Tank Platoon II 等）：
```
0x00-0x07: magic "EDILZSS2"
0x08-0x17: 16-byte filename buffer (null-terminated, junk-padded)
0x18:      separator byte (always 0x00)
0x19+:     LZSS stream
  - LSB-first control byte (8 bits)
  - bit=1 -> literal (1 byte)
  - bit=0 -> back-ref (2 bytes): [offset_lo:8][len-3:4 | offset_hi:4]
    offset = ((b2 & 0xF0) << 4) | b1   (12-bit)
    length = (b2 & 0x0F) + 3
  - window 4096 bytes, init 0x20 (space)
  - wpos_init = N - 18 (classic Storer-Szymanski)
```
Python decoder: `tools/edilzss2_decode.py` (repo 內)
Encoder: 尚未寫（Phase 4 才需要）

### CIV.EXE 構造 (832,512 bytes, decompressed)
- Win16 NE, 133 segments
- Imports: KERNEL, USER, **GDI**, WIN87EM, MMSYSTEM, COMMDLG (全 by ordinal, iname table 為空)
- Resources: RT_CURSOR x16, RT_ICON x1, RT_MENU x1, **RT_DIALOG x24**, RT_ACCEL x1
- **No RT_STRING resources** — 遊戲文字全 inline plain ASCII 在 data segments
- 字型 face: `CIVTIMES12`, `CIVTIMES24`（CIVFONTS.FON 內 RT_FONT）

### CIVFONTS.FON
244,224 bytes Win16 NE 字型 library，標準 RT_FONT 資源。

### CIVDATA*.RSC (Civdata0-4)
含 MIDI (`MThd`/`MTrk`)、Photoshop-edited bitmaps (`8BIM`)、`Civ Data N` label。
**裡面沒有遊戲文字**（strings 看起來都是壓縮過的 binary），不用 RE。

### Wine 6.0.3 bring-up confirmed
- Ubuntu 22.04 WSL2 + wine32:i386 + wine64
- prefix Win 3.1 mode
- 直接跑 `wine CIV.EXE` 開出 512×320 視窗
- 截圖看到 "Designed by SID MEIER with BRUCE SHELLEY" intro 動畫
- **256 色 palette 正常，沒花掉** — wine 內建 emulation 足夠
- **不需要 PG-cht 那種 shim.dll bypass**（PG 是因為 WinG；Civ1 1993 pre-WinG 純 GDI）

## Phase 1 (完成 2026-05-24)

### RT_DIALOG patch — 24 dialogs / 54 instances / 34 unique
- Parser `tools/ne_dialog_extract.py` 解 Win16 DLGTEMPLATE + DLGITEMTEMPLATE
- 23 個 `"CIVDIALOG"` 視窗 class **絕對不可譯**（程式註冊的 class name）
- Translation: `data/dialog_translations.json` 34 條 English→Big5 映射
- Patcher `tools/ne_dialog_patch.py` 用 cp950 encode + ASCII space pad

### 關鍵雷區 — Win16 dialog walker 約束
DLGITEMTEMPLATE 字串 null-terminated，緊接的 `createInfoSize` byte 位置由 walker 決定。**短 Big5 不能直接塞**——walker 撞前面的 null，下個 byte 當 createInfoSize → 跳過 N bytes → parse 整個 dialog 爆掉。
**Fix**: 譯文不夠長補 ASCII space (0x20) 到**完全等於原 slot byte 長度**。Button 右側多空格通常被 rect 裁掉看不到。

### Phase 1 verify (wine 6.0.3)
- CIV.EXE.cht 832,512 bytes（與原版相同）
- wine 載入成功，intro 動畫照樣播放 → NE 結構 / resource walker 未壞
- Big5 字模顯示要等 Phase 2 (CIVFONTS Big5 glyph) 才能正確 — Phase 1 只 stage Big5 bytes

## Phase 2 (完成 2026-05-24, infra layer)

### CIVFONTS.FON 構造（21 RT_FONT）
- **14 個文明裝飾字**: CIVBABYLON / CIVZULU / CIVEGYPT / CIVENGLISH / CIVGREEK / CIVRUSSIAN / CIVGERMAN / **CIVCHINESE (chop-suey 風 Latin, 不是中文)** / CIVFRENCH / CIVROMAN / CIVINDIAN / CIVAMERICAN / CIVAZTEC / CIVMONGOL — 各 14pt bitmap ANSI charset dfLastChar=0xD5，文明命名橫幅用
- **7 個 UI 字 (CIVTIMES family)**: CIVTIMES10/12/14/18/24/30/36 — dialog/text 實際用，multi-size bitmap ANSI charset dfLastChar=0xFF (含 Latin-1)

### 策略 pivot
原計畫 A（直接換 CIVFONTS RT_FONT bitmap 成 Big5 點陣）不可行——Win16 .FNT v2 `dfFirstChar`/`dfLastChar` 是 uint8，原生 DBCS 支援不直觀。
**改走 C**: font substitution + dfCharSet 標記。

### Two-layer fix
1. **CIVFONTS.FON RT_FONT dfCharSet 0x00 → 0x88 (CHINESEBIG5_CHARSET)** for 7 CIVTIMES fonts，工具 `tools/ne_font_patch_charset.py`。位元偏移 dfCharSet 在每個 FONTINFO 的 offset 0x55。
2. **`HKLM\Software\Microsoft\Windows NT\CurrentVersion\FontSubstitutes`** 註冊 `CIVTIMES12,0 = AR PL UMing TW,136`（charset 136=0x88）。GDI 在 CreateFont 時 face name + charset 一起 rewrite。
3. **OS 層 Big5 字型**: `fonts-arphic-uming` + `fonts-arphic-bsmi00lp`
4. **Setup 全自動化**: `tools/wine_setup_phase2.sh`

### Phase 2 視覺驗證限制
intro 滾動字幕 ("JEFFERY L. BRIGGS"、"HARRY TEASLEY") **Phase 2 前後幾乎一樣** → 強烈指這些字是 **pre-rendered BMP** (1993 常見) 不走 GDI TextOut。視覺驗證 Big5 字型實效**自然延後到 Phase 3** 後當真正 dialog/menu 觸發 (走 DrawText) 時才能確認。

## Phase 3 (完成 2026-05-24, Batch A)

### Scope discovery
CIV.EXE 全掃 927 ASCII runs (≥6B null-terminated)，dedupe 790 unique，4 桶：
- **prose** (532): user-facing 遊戲文字 → 翻譯目標
- **short_word** (155): UI 標籤
- **api_or_symbol** (64): 內部 lookup key (CIVDIALOG, BUILDCITY, ...) — **不可譯**
- **filename** (39): *.wav *.fon *.rsc — **不可譯**

### Batch A 翻譯 (48 unique / 50 位置)
主選單、難度、退出確認、回顧、外交、貿易、太空船、戰爭，14 個領袖名 (Solomon→智者所羅門、Augustus→奧古斯都大帝、...)，事件訊息 (Senate/Global Warming/conquest/...)。

### Patcher 差異 vs Phase 1
- Phase 1 (RT_DIALOG): walker 後緊接 createInfoSize byte → 必須補 **ASCII space** 到原長
- Phase 3 (inline data segment): 純 C string strlen → 補 **NULL** 乾淨
- 兩者都受 byte-length ≤ 原長約束

### 雷區
patcher filter 起初用 `if not k.startswith('_')` 把翻譯 key `_Start a New Game_...` 跟 metadata key `_comment` 一起濾掉。改成「metadata = `_` 開頭且尾段全小寫」修掉。

### 視覺驗證限制 (同 Phase 2 一面牆)
title screen 等待 input 進主選單，但 WSLg + winevdm input pipeline 完全不通 — xdotool 對 wine 視窗 click/key 全被吞 (內層 / desktop 包裝 / root 都試了)。**翻譯後的主選單視覺驗證得等 Phase 5 Win10 portable 實機跑**。

Byte-level 已證明: hex dump @ 0xbb35b 為正確 Big5 cp950 `_開始新局_載入存檔_...`，1567 bytes modified, 832,512 size 一致。

## Phase 5 (完成 2026-05-24, build artifact 5.21 MB)

### Build artifact
**`Civilization-CHT-portable.exe`** — 單檔 5.21 MB Win10 portable，含 otvdm + patched 遊戲。（local 產物，不入 repo）

### otvdm 選擇理由
Civ1 1993 是 Win16 NE，**Win10 64-bit drop 了 WoW16** 跑不動原生。otvdm (github.com/otya128/winevdm) 1.5MB 開源 Win16-on-Win10 reimpl，420k+ 下載，最成熟方案。

### Bundle 結構
```
Civilization-CHT-portable.exe (7z SFX, GUIMode=1)
├─ Civilization-CHT.bat   (注入 font subst + 跑 otvdmw)
├─ game/                  (5.75 MB, Phase 1/2/3 patched 全套)
└─ otvdm/                 (3.88 MB, Win16 runtime)
```

### Launcher 關鍵 trick
透過 otvdm 的 `EnableRegistryRedirection` (default on)，在 `HKCU\Software\otvdm\HKEY_LOCAL_MACHINE\...\FontSubstitutes` 注入 `CIVTIMES*,0 = MingLiU,136` (Big5)，**完全 portable 不污染 Win10 系統 registry**。MingLiU 是 Win10 內建 Big5 字。

### Build 腳本
`tools/build_portable_sfx.ps1` 全自動：7z LZMA max + UTF-8 BOM config (Title/BeginPrompt/RunProgram/GUIMode) + 7z.sfx 串接。

### Phase 5 BLOCKER (2026-05-24 dinner suspend)
otvdm v0.9.0 跟 1993 Civ for Windows 之間有確定 bug：
- 用 cmd start launch `otvdmw.exe CIV.EXE` + 加 font subst → 5 秒 SEGV (IP:06AA / access=0x0000D460 / 三次 register state 全等 = deterministic)
- 不加 font subst → 60s 沒掛但**ORIG 也 SEGV** (`test_0_orig.bat` 黑畫面 → SEGV)
- 移除 wavs workaround → 沒救
- **Root cause**: cracyc 自己引入的 `sndPlaySound` bug，**修正在 cracyc/winevdm master HEAD `cd84ae2` (2025-11-30, PR #1536)**，但 v0.9.0 release 不含
- otya128/winevdm 沒 release artifacts 在 GH Actions
- cracyc fork build artifact `otvdm-github-fullbuild-81` 已過期 (90 day retention)
- 上游 Civ1 issues: #1545 (CLOSED Dec 2025 = sndPlaySound fix) + #1480 (OPEN)

### Phase 5 DECISION (Option C)
**放棄 Win10 portable 路線**。改 ship 模式：
- Linux/WSL2 + wine 6.0.3 = 100% 可用 (Phase 0 已驗證)
- 將 setup 文件補完，作為 Win 用戶官方建議路線
- 等以後 otvdm v0.9.1+ release 含 #1545 fix 再回頭做 Win10 portable
- Stretch goal: 自 build cracyc master (需 VS 2017，root 沒 .sln 要找 build system)

### 下週繼續從這裡
1. README 補「Win10 user → 走 WSL+wine 路線」+ 詳細指引 ✅ (已補)
2. tools/setup_wsl_wine.sh 一鍵裝 ✅ (已建)
3. (stretch) 試 build cracyc/winevdm `cd84ae2` from source，build 出來重啟 Phase 5 portable
4. 主要工作回到 **Batch B/C/D/E 補翻譯** — inline string 還有 484 條 prose + 145 條 short_word 沒譯

## Batch B-E + Phase 4/6 計畫
3. CIV.EXE inline string in-place patch — 1 週 dev + 1-2 週翻譯 / QA
4. EDILZSS2 compressor (optional) — 1-2 天
5. Win10 portable SFX (wine32 + game bundle，類似 wing-portable-sfx 模式) — 2-3 天
6. 256-color shim — **預期不需要**，實機 Win10 出問題再做

## 與 PG-cht 的關聯
- 共用「wine portable SFX」打包路線
- CIV1 **不需要** WING32 patch（PG-cht 的 0xA55 jnz→nop）— Civ1 純 GDI
- 共用 Big5 字型 12×16 路線
- inline string slot-length 約束跟 PG 一樣，但 Civ1 slots 更寬鬆（C strings vs Pascal）

## Related skills
- `wing-portable-sfx` — 最終 portable 打包技術（reuse）
- `panzer-general-cht` — inline string patch 經驗（reuse）
- `panzer-general-wine` — wine 啟動環境（reuse）
- 本 skill: `civ1-cht`（EDILZSS2 spec + 整套工作流）

## 螢幕截圖 (Phase 0-3)
- `docs/screenshots/01_intro_sid_meier.png` — wine 跑出來的 SID MEIER intro，CIVTIMES 字型確認正常
- `docs/screenshots/02_intro_after_phase1.png` — Phase 1 patch 完
- `docs/screenshots/03_intro_jeffery_briggs.png` / `04_intro_harry_teasley_after_phase2.png` — Phase 2 字幕對比
- `docs/screenshots/05_intro_music_by_phase3.png` — Phase 3 build 跑 intro
- `docs/screenshots/06_title_screen_phase3.png` — WSLg input 卡點

---

## Track B — C++/SDL2 原生重寫（openciv1pp/，2026-05-26~28）

把 OpenCiv1（1991 DOS Civ 的 C#/Avalonia 重寫，MIT）改寫成 **C++17 + SDL2** 並原生中文化。原始碼 `openciv1pp/`。**注意**：與 Track A（1993 Win16 binary patch）是不同路線、不同 binary；共用的是翻譯內容（zh_TW.json 由 Track A 的 data/*.json 經 build_zh_table.py 產生）。

### 已完成且驗證（ctest 16/16，-Wall -Wextra 零警告）
- **引擎**：VCPU（全 x86 指令集+C/Z/S/D/O 旗標，方法名對齊 VCPU.cs）、GBitmap（palette framebuffer + 繪圖原語）、GDriver（多螢幕/字型/螢幕合成/F0_VGA_* 入口）、`.pic` codec（RLE+LZW+18bit palette，像素級 round-trip）、`.txt` section 載入、Translator+FreeType MONO CJK（DrawString chokepoint 注入）、SdlPresenter（視窗+鍵盤）。
- **8 個 CodeObjects 移植**：DrawTools / ImageTools / LanguageTools / CommonTools / MenuBoxDialog(+nav) / TextBoxDialogs / GameMenus + FrontEndFlow 串接。
- **可立即執行**：`./build/openciv1pp --menuflow` = 鍵盤導覽的全中文 主選單→難度→開始 流程。截圖在 openciv1pp/docs/screenshots/。

### 關鍵技術雷區
- CJK 必須用 FreeType **MONO**（hinted 1-bit）非灰階門檻——palette 無 alpha，門檻會把細筆畫砍成斜碎片。
- UTF-8 三路徑 drawString：<0x80 ASCII 點陣字 / 0x80-0xFF 行內換色碼 / ≥0x100 CJK（FreeType）。
- 翻譯在 GDriver.drawString / DrawTools 注入（非 GBitmap 層）——CodeObject 文字必須走這兩條才會中文化。
- ctest cwd 是 build/：localization 測試靠 `set_tests_properties(WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})` 才找得到 assets/zh_TW.json。

### 移植模式（其餘 ~30 個 CodeObject 照做）
讀 C# CodeObject → 在 src/game/ 開對應類別取 `OpenCiv1Game&` → 用 cpu/graphics/var_aa 照抄、保留 F0_* 名 → 加 `--xxxtest`（含 translate-on vs -off 像素差證明中文）→ 註冊進 CMake foreach + `--test` 聚合器。建議用 sub-agent 委派（避免主上下文 prompt too long）。

### 剩餘（通往完整可玩）
模擬主體 ~30+ CodeObjects（地圖/城市/單位/戰鬥/AI/回合，含 Segment_25fb 359KB、CityWorker 158KB 等）+ boot path（MainCode/StartGameMenu/MainIntro）+ 使用者自備正版 Civ1 DOS 資產（.pic/.pal/.txt）。多週長征。
