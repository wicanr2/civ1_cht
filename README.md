# Civilization for Windows 繁體中文化專案

> *Sid Meier's Civilization* (1991 DOS / **1993 MicroProse Windows port**) — 繁體中文化  
> Win16 NE 直接 patch ✦ Big5 字型嵌入 CIVFONTS.FON ✦ wine 跨平台執行 ✦ EDILZSS2 自家壓縮格式破解

![Phase 0 intro](docs/screenshots/01_intro_sid_meier.png)
*Phase 0 bring-up：wine 6.0.3 (WSL Ubuntu 22.04) 上跑出 1993 Civilization for Windows 的 intro 動畫，黃色 CIVTIMES 字型與 256 色 palette 正常顯示。*

---

## 目錄

1. [專案狀態](#status)
2. [起源 — 1991 Civilization 從何而來](#origin)
3. [Civilization 系列簡史](#series)
4. [為何要漢化 Civ1 Windows 版？](#why)
5. [快速開始 (Phase 0)](#quick-start)
6. [實機截圖](#screenshots)
7. [Technical Deep Dive — EDILZSS2 格式破解](#edilzss2)
8. [CIV.EXE 結構分析](#civ-exe)
9. [Phase 規劃](#phases)
10. [1993 台灣 Civ 玩家文化](#taiwan-1993)
11. [License & Credits](#credits)

---

<a name="status"></a>
## 🎯 專案狀態

| Phase | 任務 | 狀態 |
|------|------|------|
| **0** | EDILZSS2 RE + wine 6.0.3 bring-up | ✅ **2026-05-24 完成** |
| **1** | RT_DIALOG (24 個) Big5 patch — 54 instances / 34 unique | ✅ **2026-05-24 完成** |
| 2 | CIVFONTS.FON glyph 換 Big5 12×16 點陣 | 📋 規劃中 |
| 3 | CIV.EXE inline string 數百條 in-place 翻譯 | 📋 規劃中 |
| 4 | EDILZSS2 compressor（重打包 .EX$） | ⏸ 可選 |
| 5 | Win10 portable SFX（wine32 + game bundle） | 📋 規劃中 |
| 6 | 256-color shim | ⏸ 預期不需要 |

---

<a name="origin"></a>
## 📜 起源 — 1991 Civilization 從何而來

1991 年，**Sid Meier** 與 **Bruce Shelley** 在 MicroProse 共同設計了一款後來重新定義策略遊戲的作品：*Sid Meier's Civilization*。當時 Sid Meier 已經因 *F-19 Stealth Fighter*（1988）、*Pirates!*（1987）、*Railroad Tycoon*（1990）站穩名聲；Bruce Shelley 則是從 Avalon Hill 過來的桌遊設計師（後來 1995 創 Ensemble 做出 *Age of Empires*）。

### 三條血脈匯流

| 來源 | 貢獻給 Civ1 的元素 |
|------|------|
| **Civilization (Avalon Hill, 1980)** — Francis Tresham 設計的桌遊 | 文明命名、科技樹概念、貿易、起源年代 7000 BC |
| **Empire (Walter Bright, 1977)** | 6-sided tile map、城市生產回合、戰爭單位 |
| **SimCity (Maxis, 1989)** | 城市內部建築 / 民意 / 細部管理 |

把這三條融在一起，Sid Meier 創造了一個玩家從「**一個拓荒者（Settler）出發、到 AD 2050 抵達 Alpha Centauri**」的單機 4X 史詩——而這正是「**4X 類型 (eXplore, eXpand, eXploit, eXterminate)**」這個名詞被定義之前的原型作品。

### 為什麼 1993 Windows 版重要？

DOS 版（1991）紅遍全球，但**真正讓 Civ 走進台灣家庭**的是 1993 Windows port：
- Win 3.1 是當時台灣家用 PC 的主流環境（中文 Windows 3.1 1993 上市）
- 滑鼠+下拉選單比 DOS 鍵盤指令更親民
- 解析度從 320×200 升到 640×480 (`CIVTIMES` font 16/24px 兩種 size)
- 1994-1995 倚天中文系統 + Win 3.1 中文版可以**英文 Civ 上跑**（看英文遊戲），但無法直接中文化（NE 二進位格式手動 hex patch 難度極高）

**三十多年後，這個專案要把 1993 Win 版 Civ1 完整中文化。**

---

<a name="series"></a>
## 🏛️ Civilization 系列簡史

| 作品 | 年份 | 開發 | 重大改變 |
|------|------|------|------|
| **Civilization** | 1991 (DOS) / **1993 (Win)** | MicroProse / Sid Meier + Bruce Shelley | 系列鼻祖；本專案目標 |
| Civ II | 1996 | MicroProse / Brian Reynolds + Jeff Briggs | 等角視圖、多人 |
| Civ III | 2001 | Firaxis | 文化邊界、戰略資源 |
| Civ IV | 2005 | Firaxis / Soren Johnson | 宗教、Civic 制度、Python 模組化 |
| Civ V | 2010 | Firaxis / Jon Shafer | 一格一兵、六邊形 hex map |
| Civ VI | 2016 | Firaxis | 區域 District、英雄領袖 |
| Civ VII | 2025 | Firaxis | Age 切換、混搭文明 |

**1993 Win 版位居系列原點**——之後三十年所有 Civ 機制都是它的演化。

---

<a name="why"></a>
## ✨ 為何要漢化 Civ1 Windows 版？

1993 年的中文化版本市面上**沒出過**——MicroProse 從未官方繁中，台灣當年只有民間少量手抄攻略本。Civ1 的核心體驗——歷史人物對話、科技樹說明、奇蹟描述、城市建議——全是英文。

對 1993-1995 年用倚天中文系統的台灣玩家，遊戲跑得起來但讀不懂：

```
"You must attack the evil:"
"We have signed a peace treaty with the [Civ]"
"The Top Five Cities in the World"
"Overruled by the Senate. Action canceled."
```

這些是 CIV.EXE 內 inline plaintext 字串，靠純記憶硬刷的世代留下很多誤譯（例如把 Senate 翻成「議院」而不是更貼切的「參議院」）。

**這個專案要把 1993 Civ for Windows 完整中文化，不只翻譯，還要讓它在 2026 年的 Windows 10 + wine portable 環境下單檔可雙擊執行**——三十年的等待，現在補回來。

---

<a name="quick-start"></a>
## ⚡ 快速開始 (Phase 0：原版 + wine)

目前 Phase 0 階段，這個 repo 只包含 RE 文檔與工具，**翻譯尚未開始**。如果你想復現 wine bring-up：

### 需要準備

- **WSL2 Ubuntu 22.04** (Windows 10/11)
- **wine 6.0.3+** (含 `wine32:i386` for Win16 NE)
- **1993 Civilization for Windows 原版安裝檔**（自行取得合法拷貝；本 repo 不提供）

### 三步驟

```bash
# 1. 安裝 wine 32-bit (for Win16 NE)
sudo dpkg --add-architecture i386 && sudo apt update
sudo apt install -y wine wine32:i386 wine64

# 2. 用本 repo 的 Python 工具解開 EDILZSS2 壓縮檔
python3 tools/edilzss2_decode.py /path/to/CIV.EX\$ ./CIV.EXE
python3 tools/edilzss2_decode.py /path/to/CIVFONTS.FO\$ ./CIVFONTS.FON
# ... 其餘 .HL$ .RS$ .WA$ 同樣處理

# 3. 跑 wine
export WINEARCH=win32 WINEPREFIX=~/.wine-civ1
wine reg add 'HKCU\Software\Wine' /v Version /d win31 /f
wine ./CIV.EXE
```

---

<a name="screenshots"></a>
## 📸 實機截圖

### Phase 0：原版 wine 跑通

![SID MEIER intro](docs/screenshots/01_intro_sid_meier.png)
*1993 Civ for Windows 開場：「Designed by SID MEIER with BRUCE SHELLEY」 — CIVTIMES 字型 + 星雲背景 + 256 色 palette 全部正常。截圖來源：wine 6.0.3 / WSL Ubuntu 22.04 / 2026-05-24。*

### Phase 1：RT_DIALOG patch 完成後

![intro after Phase 1 patch](docs/screenshots/02_intro_after_phase1.png)
*54 個 RT_DIALOG 字串 patched 成 Big5 後，intro 動畫照樣播放——證明 NE 結構未被破壞、resource table walker 正常。dialog 文字本身的 Big5 字模要等 Phase 2 (CIVFONTS Big5 glyph) 才能正確顯示。*

---

<a name="edilzss2"></a>
## 🔬 Technical Deep Dive — EDILZSS2 格式破解

MicroProse 1992-1995 年自家壓縮格式 **EDILZSS2**，沒有公開規格、`cabextract` / `7z` / wine `expand.exe` 都不認識（wine 的 expand 只懂 SZDD）。本專案 spike 階段反推得出完整 spec：

### 檔頭結構

```
偏移       長度    內容
0x00       8       Magic "EDILZSS2"
0x08       16      Filename buffer（null-terminated，後面為 compressor 記憶體洩漏 junk）
0x18       1       Separator byte（恆為 0x00）
0x19+      ?       LZSS 壓縮流（吃到 EOF）
```

### LZSS 變體規格

| 屬性 | 值 |
|------|------|
| Window size | 4096 bytes |
| Window 初值 | 0x20（空白） |
| 起始 wpos | N − 18 = 4078（古典 Storer-Szymanski） |
| Control byte bit order | **LSB-first**（bit 0 先吃） |
| bit = 1 | literal（讀 1 byte） |
| bit = 0 | back-ref（讀 2 bytes） |
| Back-ref encoding | `[offset_lo:8] [length−3:4 | offset_hi:4]` |
| Offset | `((b2 & 0xF0) << 4) | b1`（12-bit） |
| Length | `(b2 & 0x0F) + 3`（3..18） |

### 破解過程關鍵 insight

1. CIV.EX$ 在 offset 0x19 觀察到 `0xFF` byte（所有 bit 都是 1），緊跟 8 bytes `4D 5A 57 00 01 00 00 00` —— 正是 MZ header 開頭。**這 8 bytes literal 即為控制位元解讀的鐵證**（literal = bit set）。
2. 解出後的 stream 在 offset ~30 出現 `\r\nThis is a Windows program.` —— Microsoft Win16 NE 標準 DOS stub 訊息 → **變體完全正確**。
3. 第一版 decoder 早期 EOF 處理錯誤（`break` 跳出內層 for loop 後外層 while 還繼續，造成 `data[idx]` index 超界、被 try/except 吞掉 → 看起來只解出 51KB）。修掉 → 完整解出 832KB CIV.EXE。

### 5 個檔的解壓結果

| 原檔 (.$ 壓縮) | 解出 (uncompressed) | 格式 |
|------|------|------|
| CIV.EX$ (335 KB) | CIV.EXE (832 KB) | Win16 NE executable |
| CIVFONTS.FO$ (68 KB) | CIVFONTS.FON (244 KB) | Win16 NE bitmap font library |
| CIVHELP.HL$ (33 KB) | CIVHELP.HLP (73 KB) | Microsoft WinHelp 3.x |
| READ.ME$ (3 KB) | READ.ME (5.7 KB) | Plain ASCII |
| Civdata0.rs$ (122 KB) | Civdata0.RSC (181 KB) | 自家 resource container |

工具：`tools/edilzss2_decode.py`（本 repo Phase 0 deliverable）。

---

<a name="civ-exe"></a>
## 🧬 CIV.EXE 結構分析

| 項目 | 內容 |
|------|------|
| 格式 | Microsoft Win16 NE (1993, for Windows 3.1) |
| 大小 | 832,512 bytes (decompressed) |
| Segments | 133 |
| Imports | **KERNEL, USER, GDI, WIN87EM, MMSYSTEM, COMMDLG**（全 by-ordinal） |
| Resources | RT_CURSOR×16, RT_ICON×1, RT_MENU×1, **RT_DIALOG×24**, RT_ACCEL×1, RT_GROUP_CURSOR×16, RT_GROUP_ICON×1 |
| **RT_STRING** | **0**（字串全 inline 在 data segments） |
| 自訂字型 face | `CIVTIMES12`, `CIVTIMES24` (in CIVFONTS.FON) |

### 為何 GDI 匯入是關鍵

CIV.EXE 匯入 `GDI` 模組 → 文字繪製走 `TextOutA` / `DrawText` 等標準 Win16 GDI API → **不需要 hook 自家 blit routine**，這把字型替換從「Win16 + 自家 blit + byte-pair fake CJK」的地獄路（如 MM3 / EOB1 走的路）降級為「換 CIVFONTS.FON 內 RT_FONT bitmap」的天堂路。

### 為何字串全 inline 是約束

CIV.EXE 沒有 RT_STRING resources → 不能像 Win32 程式那樣動 string table id，必須**逐條 in-place patch**。slot length 受限（譯文不能比原文長太多 byte），但 Big5 通常比英文短（「和約」2 chars vs `peace treaty` 12 chars），實務上夠用。

---

<a name="phase1-done"></a>
## ✅ Phase 1 完成記錄（2026-05-24）

24 個 `RT_DIALOG` 全部反組譯、字串抽出、Big5 patched。

### 工作流程

```
CIV.EXE
  ↓ ne_dialog_extract.py
civ_dialogs.json (24 dialogs / 78 strings / 35 unique)
  ↓ 過濾 class="CIVDIALOG" 不可譯
54 translatable instances / 34 unique English strings
  ↓ data/dialog_translations.json (人工翻譯)
  ↓ ne_dialog_patch.py (Big5 cp950 encode + ASCII space pad)
CIV.EXE.cht (832,512 bytes, identical to original size)
  ↓ wine smoke test
✅ NE 結構未壞，intro 正常播放
```

### 關鍵技術 — Win16 dialog walker 約束

Win16 `DLGITEMTEMPLATE` 中字串是 **null-terminated**，緊接的 `createInfoSize` byte 位置由 walker 走到 null 後的下一個位置決定。**如果直接把短 Big5 字串塞進原 slot**：

```
原: "Cancel\0" (7 bytes: 6 text + 1 null)  → walker idx 走到 7
新: "取消\0"   (5 bytes: 4 text + 1 null)  → walker idx 走到 5
                                              然後讀 byte 5 = 'l' (0x6C)
                                              當成 createInfoSize=108
                                              跳過 108 bytes → 解析爆掉
```

解法：Big5 譯文不足 slot 時，**以 ASCII space (0x20) 補滿到原 slot byte 長度**。這樣 walker 走到 null 的位置不變，後續 field 解析正確。

### 翻譯範例

| 英文 | slot | Big5 | 補空格 |
|------|------|------|------|
| `OK` | 2B | `確` | (剛好) |
| `Cancel` | 6B | `取消` | + 2 spaces |
| `Open Game` | 9B | `開啟` | + 5 spaces |
| `What shall we build in ` | 23B | `在此城建造甚麼? ` | + 8 spaces |
| `Are you sure you want` | 21B | `您確定要` | + 13 spaces |

button 右側多空格通常被 dialog rect 裁掉看不到。Static text 多空格在 left-aligned mode 也不影響。

### Phase 1 deliverables

- [`tools/ne_dialog_extract.py`](tools/ne_dialog_extract.py) — Win16 RT_DIALOG parser
- [`tools/ne_dialog_extract_v2.py`](tools/ne_dialog_extract_v2.py) — 翻譯候選 worksheet generator
- [`tools/ne_dialog_patch.py`](tools/ne_dialog_patch.py) — Big5 patcher with ASCII-space padding
- [`data/dialog_translations.json`](data/dialog_translations.json) — 34 unique English → Big5 mapping

---

<a name="phases"></a>
## 🗺️ 未來 Phase 規劃

### Phase 2：CIVFONTS.FON 換 Big5 字模（3-5 天，下一步）

CIVFONTS.FON 是標準 Win16 NE 字型庫，內含 RT_FONT resources。計畫：
- 保留 face name `CIVTIMES12` / `CIVTIMES24`（程式可能 hardcode 找這 face）
- 換掉 ASCII 32-127 對應的 glyph，補上 Big5 全集
- 字模來源：WenQuanYi Bitmap Song 或 NWP 16×16 點陣

### Phase 3：CIV.EXE inline string 全翻譯（1 週 dev + 1-2 週 QA）

- 工具 scan CIV.EXE data segment，列出所有 ≥4 字 ASCII 字串候選
- 人工 / LLM 翻譯
- in-place patch：原 byte 範圍內塞 Big5 + null pad
- slot length policy: Big5 字串 ≤ 原 ASCII byte 數
- 14 個領袖名（Caesar, Hammurabi, Frederick, Ramses, Lincoln, Alexander, Gandhi, Stalin, Shaka, Napoleon, Montezuma, Mao, Elizabeth, Genghis）一一對應中文

### Phase 4：EDILZSS2 compressor（1-2 天，可選）

把翻譯後 CIV.EXE 重新壓回 CIV.EX$，讓使用者可以走原 SETUP.EXE 安裝流程。或者跳過，直接 ship 預解開檔。

### Phase 5：Win10 portable SFX（2-3 天）

Bundle `wine32 + prefix + 翻譯後 CIV` → 單一 .exe（7z self-extractor）。使用者雙擊即玩，不需自己裝 wine。技術直接複用既有 `wing-portable-sfx` skill（最初為 Panzer General 開發）。

### Phase 6：256-color shim（預期不需要，0-3 天）

如果 Win10 native + wine portable 環境出現 palette 顯示問題，需要 patch CIV.EXE 或 ship 客製 DLL。**目前 wine 6.0.3 on Linux 證實不需要任何 shim**——CIV1 1993 沒用 WinG，純 GDI，wine palette emulation 已涵蓋。

---

<a name="taiwan-1993"></a>
## 🇹🇼 1993 台灣 Civ 玩家文化

1993 年 Civ for Windows 在台灣的處境：
- **管道**：少數電腦資訊店、進口商；多數玩家透過軟體公司打折版或拷貝流通
- **環境**：DOS 6.2 + 倚天中文系統 / 中文 Windows 3.1（剛上市，1993 年中）
- **攻略**：軟體世界、電腦玩家雜誌偶有 Civ 攻略；多以 DOS 1991 版為主，Win 版很少專文
- **討論社群**：BBS（中山美麗之島、台大椰林、中央松濤）的「戰略遊戲版」是當時唯一線上討論場域

**1993 沒有民間 Civ 中文化 patch 流通**——技術門檻太高（hex edit Win16 NE 字型 + 字串 in-place + Big5 escape）。

> 30 多年後的這個專案，可以說是補上 1993 年沒有的那個「**中文化 Civ for Windows**」。

---

<a name="credits"></a>
## 📄 License & Credits

### Code & Tools
- `tools/edilzss2_decode.py` — 本專案 MIT License
- 未來 patch scripts — MIT License

### Translations
- 中文翻譯文字 — **CC BY-SA 4.0**

### Original Game
- *Sid Meier's Civilization for Windows* © 1993 MicroProse Software, Inc.
- 本專案**不包含**遊戲原始 binary / assets；使用者必須自備合法拷貝
- 商標權屬 Take-Two Interactive / Firaxis Games

### Honorable Mentions
- **Sid Meier** & **Bruce Shelley** — 1991 Civilization 設計
- **Francis Tresham** — 1980 Avalon Hill *Civilization* 桌遊（科技樹概念來源）
- **1993-1995 台灣 BBS 戰略遊戲版討論者** — 早期非正式中文討論的開拓者

---

*本 README 為 Phase 0 階段版本（2026-05-24）。每個 phase 完成後會更新進度表與新增截圖。*
