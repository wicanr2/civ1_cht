# Civilization 1 繁體中文化專案

> *Sid Meier's Civilization* (1991 DOS) — **C++17 / SDL2 原生重寫** 並內建繁體中文化
> 子專案：[`openciv1pp/`](openciv1pp/) ✦ Track B（主線）

![openciv1pp vs DOS Civ1 side-by-side](openciv1pp/docs/videos/comparison.gif)

*左：本專案 (`openciv1pp`)，右：1991 DOS 原版透過 DOSBox 執行。錄製完全在 Docker + Xvfb 容器內進行，不在主機桌面開窗。*

---

## 目錄

1. [專案狀態](#status)
2. [openciv1pp — C++/SDL2 主線（Track B）](#openciv1pp)
3. [快速開始](#quick-start)
4. [實機截圖](#screenshots)
5. [起源 — 1991 Civilization 從何而來](#origin)
6. [Civilization 系列簡史](#series)
7. [為何要漢化 Civ1？](#why)
8. [1993 台灣 Civ 玩家文化](#taiwan-1993)
9. [Track A — 1993 Windows civ.exe NE patch（歷史紀錄）](#track-a)
10. [License & Credits](#credits)

---

<a name="status"></a>
## 🎯 專案狀態

**Track B (`openciv1pp`)** — 主線，2026-06 持續推進：

| 階段 | 內容 | 狀態 |
|------|------|------|
| **引擎地基** | VCPU + GBitmap + GDriver + .pic codec + .txt loader + Translator + FreeType MONO CJK + SDL2 + 鍵鼠輸入 | ✅ 全綠 |
| **CodeObject 移植** | 22+ 個（DrawTools / ImageTools / LanguageTools / CommonTools / MenuBoxDialog / TextBoxDialogs / GameMenus / FrontEndFlow / MainCode / MainIntro / MapManagement / UnitManagement / CheckPlayerTurn / CityView / TechResearch / Government / GameLoadAndSave / MiniWorld / Civilopedia / HallOfFame …）| ✅ 全綠 |
| **Civ1 機制** | 8 文明（+野蠻人）/ 10 單位 / 15 科技 / 5 政府 / 8 建築 / 7 奇蹟（含 Apollo）/ 戰鬥 / 食物 / 黃金 / 快樂 / 外交 / 稅率 / 道路 / Goodie Huts | ✅ 全綠 |
| **勝利條件** | 太空競賽（Apollo + 結構/元件/模組 → 抵達半人馬座阿爾法）+ Hall of Fame | ✅ 全綠 |
| **中文化** | 358+ zh_TW.json 條目；DrawTools / GDriver chokepoint 自動翻譯；UI 全中文 | ✅ 全綠 |
| **驗證** | **54/54 ctest** 連跑 5 輪無 flake；`-Wall -Wextra` 零警告 | ✅ |
| **影片** | Docker + Xvfb 容器內錄製，side-by-side .mp4 + .gif | ✅ |
| **整局實機從頭玩到勝利** | `--game` 啟動可用；500-turn 確定性 smoke test 待補 | ⏳ |

**Track A (`tools/`, `data/`)** — 1993 Windows civ.exe NE 二進位 patch 路線，**已封存**：Phase 1/2/3/5 完成（48 條 inline 翻譯 + 54 個 RT_DIALOG patch + dfCharSet + 5.21 MB Win10 portable build）。詳見 [`docs/TRACK_A_README.md`](docs/TRACK_A_README.md)。

---

<a name="openciv1pp"></a>
## 🛠️ openciv1pp — C++/SDL2 主線

把 [OpenCiv1](https://codeberg.org/rhorvat/OpenCiv1)（MIT，1991 DOS Civ 的 C# 重寫）逐步改寫成 **C++17 + SDL2** 並把中文化做進引擎（palette 層合成 CJK 字模，與顯示後端無關）。原始碼在 [`openciv1pp/`](openciv1pp/)，詳見其 [README](openciv1pp/README.md)。

### 結構分層

```
遊戲邏輯 (CodeObjects)                ← 22+ 已移植，移植 SOP 自動化
   │ 透過 OpenCiv1Game 殼（VCPU + GDriver + CRectangle 脈絡）
   │ 呼叫
VCPU (暫存器/旗標/分段記憶體/全指令集) ← --selftest 通過
   │ 寫入
palette framebuffer (GBitmap)
   │ ├─ ASCII：GFont 點陣字
   │ └─ CJK ：CjkGlyphCache (FreeType MONO)  ← 中文化在 palette 層完成
   │ 翻譯：Translator (zh_TW.json, 358+ 條)
   │ 轉 RGBA
顯示後端 SdlPresenter (SDL2)          ← 與邏輯/中文化完全解耦

資源：.pic 載入器 (RLE+LZW+18bit palette) ← --restest 像素級 round-trip
```

**關鍵設計**：中文字在 palette 層就合成完畢，所以顯示後端可隨意更換，中文化不受影響。

### 已完成 Civ1 機制

- **8 文明**（巴比倫/羅馬/德意志/埃及/美利堅/希臘/印度 + 第 8 野蠻人）
- **10 單位**（Settlers / Militia / Phalanx / Cavalry / Trireme / Legion / Knight / Catapult / Musketeers / Cannon）+ veteran / fortified / 道路移動加成
- **15 科技**樹 + 5 政府（含 Anarchy 過渡）
- **8 建築**（Granary / Barracks / Walls / Temple / Marketplace / Library / Cathedral / Aqueduct）
- **7 奇蹟**（Pyramids / Hanging Gardens / Great Wall / Colossus / Magellan / Lighthouse / Oracle / Apollo Program）
- **戰鬥**：地形/Fortify/Walls/Great Wall/老兵 全堆疊
- **食物 + 人口成長 + Aqueduct**、**黃金 + 維護費 + 破產**、**快樂/民變**
- **外交**（NoContact/Peace/War）/ **Tax/Lux/Sci 滑桿**
- **AI**：建首都/擴張/挑最佳單位/逼近敵人/宣戰/Fortify/野蠻人入侵
- **Goodie Huts** 隨機獎勵 / **道路** 移動加成
- **太空競賽勝利**（Apollo → 10 結構 + 8 元件 + 4 模組 → launch → 20 turn 後抵達半人馬座阿爾法）
- **Civilopedia**（8 大類 × 88 條）/ **Hall of Fame**（JSON 持久化、分數公式、本地化欄位）
- **存讀檔**（v16 format，向下相容 v1-v15）

---

<a name="quick-start"></a>
## ⚡ 快速開始

```bash
# 依賴
sudo apt install build-essential cmake libsdl2-dev libfreetype-dev fonts-arphic-uming

# 建置
cd openciv1pp && cmake -S . -B build && cmake --build build

# 跑遊戲（鍵盤可操作、全中文）
./build/openciv1pp --game --assets /path/to/civ1_dos_extracted/

# 跑全 54 個自測
ctest --test-dir build
```

DOS 資產（`.pic/.pal/.txt`）為著作權物，不在 repo；使用者必須自備合法拷貝。沒有 DOS 資產也能跑大部分 ctest（用程式內 fallback 渲染）。

詳細 dependencies 與 build option 見 [`openciv1pp/README.md`](openciv1pp/README.md)。

### 把 Claude skill + memory 帶到另一台機（可選）

```bash
mkdir -p ~/.claude/skills/civ1-cht
cp docs/SKILL.md ~/.claude/skills/civ1-cht/SKILL.md

PROJ_KEY=$(echo "$PWD" | sed 's|/|-|g')
mkdir -p ~/.claude/projects/$PROJ_KEY/memory/
cp docs/PROJECT_MEMORY.md ~/.claude/projects/$PROJ_KEY/memory/project_civ1_cht.md
```

---

<a name="screenshots"></a>
## 📸 實機截圖

### openciv1pp 主畫面 vs DOS Civ1

![openciv1pp vs DOS Civ1 side-by-side](openciv1pp/docs/videos/comparison.gif)

完整 MP4：[`openciv1pp/docs/videos/comparison.mp4`](openciv1pp/docs/videos/comparison.mp4)

### 細部對照

| 畫面 | openciv1pp（中文）| DOS Civ1 (1991) |
|---|---|---|
| 主標題 | [polish_title_zh.png](openciv1pp/docs/screenshots/polish_title_zh.png) | [dos_title.png](openciv1pp/docs/screenshots/dos_title.png) |
| 開場 BIRTH | [polish_intro_birth_zh.png](openciv1pp/docs/screenshots/polish_intro_birth_zh.png) | [dos_birth.png](openciv1pp/docs/screenshots/dos_birth.png) |
| 城市管理 (320×200 區) | [polish_city_dos_region_zh.png](openciv1pp/docs/screenshots/polish_city_dos_region_zh.png) | [dos_cback.png](openciv1pp/docs/screenshots/dos_cback.png) |
| 主選單 | [polish_menu_zh.png](openciv1pp/docs/screenshots/polish_menu_zh.png) | （同 LOGO backdrop） |
| 世界地圖 | [polish_world_zh.png](openciv1pp/docs/screenshots/polish_world_zh.png) | （runtime 合成） |

完整像素級對照分析：[`openciv1pp/docs/DOS_PARITY_REPORT_R2.md`](openciv1pp/docs/DOS_PARITY_REPORT_R2.md) 與 [`openciv1pp/docs/CITYVIEW_LAYOUT.md`](openciv1pp/docs/CITYVIEW_LAYOUT.md)。

---

<a name="origin"></a>
## 📜 起源 — 1991 Civilization 從何而來

1991 年，**Sid Meier** 與 **Bruce Shelley** 在 MicroProse 共同設計了一款後來重新定義策略遊戲的作品：*Sid Meier's Civilization*。

### 三條血脈匯流

| 來源 | 貢獻給 Civ1 的元素 |
|------|------|
| **Civilization (Avalon Hill, 1980)** — Francis Tresham 設計的桌遊 | 文明命名、科技樹概念、貿易、起源年代 7000 BC |
| **Empire (Walter Bright, 1977)** | 6-sided tile map、城市生產回合、戰爭單位 |
| **SimCity (Maxis, 1989)** | 城市內部建築 / 民意 / 細部管理 |

把這三條融在一起，Sid Meier 創造了一個玩家從「**一個拓荒者 (Settler) 出發、到 AD 2050 抵達 Alpha Centauri**」的單機 4X 史詩——而這正是「**4X 類型 (eXplore, eXpand, eXploit, eXterminate)**」這個名詞被定義之前的原型作品。

---

<a name="series"></a>
## 🏛️ Civilization 系列簡史

| 作品 | 年份 | 開發 | 重大改變 |
|------|------|------|------|
| **Civilization** | **1991 (DOS)** / 1993 (Win) | MicroProse / Sid Meier + Bruce Shelley | 系列鼻祖；**本專案目標（1991 DOS 版重寫）** |
| Civ II | 1996 | MicroProse / Brian Reynolds + Jeff Briggs | 等角視圖、多人 |
| Civ III | 2001 | Firaxis | 文化邊界、戰略資源 |
| Civ IV | 2005 | Firaxis / Soren Johnson | 宗教、Civic 制度、Python 模組化 |
| Civ V | 2010 | Firaxis / Jon Shafer | 一格一兵、六邊形 hex map |
| Civ VI | 2016 | Firaxis | 區域 District、英雄領袖 |
| Civ VII | 2025 | Firaxis | Age 切換、混搭文明 |

---

<a name="why"></a>
## ✨ 為何要漢化 Civ1？

**Civilization 系列從未推出過官方中文版**——1991 DOS 原版、1993 Windows 版、乃至後續所有版本，MicroProse 與後來的 Firaxis 都從未在華語市場發行繁中或簡中本。當年玩家邊查字典邊玩、口耳相傳記下文明歷史與外交辭令——也因此留下不少術語的口頭翻譯版本（例如 Senate 常被叫成「議院」而不是更貼切的「參議院」）。

**三十多年後，這個專案要把 1991 DOS Civ1 從根重寫成 C++/SDL2 並完整中文化**——保留 1991 原始邏輯的 byte-for-byte 忠實度（透過 OpenCiv1 的 VCPU 轉寫），同時在 palette 合成層注入中文字模，讓繁體中文成為原生顯示。

---

<a name="taiwan-1993"></a>
## 🇹🇼 1993 台灣 Civ 玩家文化

1993 年 Civ for Windows 在台灣的處境：
- **管道**：少數電腦資訊店、進口商；多數玩家透過軟體公司打折版或拷貝流通
- **環境**：DOS 6.2 + 倚天中文系統 / 中文 Windows 3.1（剛上市，1993 年中）
- **攻略**：軟體世界、電腦玩家雜誌偶有 Civ 攻略；多以 DOS 1991 版為主，Win 版很少專文
- **討論社群**：BBS（中山美麗之島、台大椰林、中央松濤）的「戰略遊戲版」是當時唯一線上討論場域

**1993 沒有民間 Civ 中文化 patch 流通**——技術門檻太高（hex edit Win16 NE 字型 + 字串 in-place + Big5 escape）。

> 30 多年後的這個專案，補上 1993 年沒有的那個「**完整中文化 Civ1**」——而且是從 1991 DOS 原始版本的邏輯重寫，不是 patch 1993 Windows port。

---

<a name="track-a"></a>
## 🗄️ Track A — 1993 Windows civ.exe NE patch（歷史紀錄）

> **狀態：封存**。Phase 1/2/3/5 已完成；現主線改走 Track B (`openciv1pp`)。Track A 工具與翻譯資料仍保留於 repo (`tools/`, `data/`, `launcher/`)。

**Track A 路線**：直接 patch 1993 MicroProse Civilization for Windows 的 Win16 NE 二進位檔，把 inline ASCII 字串改成 Big5、CIVFONTS.FON 的 dfCharSet 改成 0x88、以 otvdm 在 Win10 上跑單檔 portable。

**Track A 已交付**：
- **EDILZSS2 解壓器**（[`tools/edilzss2_decode.py`](tools/edilzss2_decode.py)） — MicroProse 自家壓縮格式完整逆推
- **RT_DIALOG patcher**（[`tools/ne_dialog_patch.py`](tools/ne_dialog_patch.py)） — 54 個 RT_DIALOG / 34 unique 字串 Big5 化（Phase 1）
- **CIVFONTS dfCharSet patcher**（[`tools/ne_font_patch_charset.py`](tools/ne_font_patch_charset.py)）+ FontSubstitutes 註冊（Phase 2）
- **Inline string patcher**（[`tools/inline_string_patch.py`](tools/inline_string_patch.py)） — Batch A 48 條核心翻譯（主選單、難度、14 領袖、外交訊息）（Phase 3）
- **Win10 portable SFX build**（[`launcher/Civilization-CHT.bat`](launcher/Civilization-CHT.bat) + [`tools/build_portable_sfx.ps1`](tools/build_portable_sfx.ps1)）— 5.21 MB 單檔 7z SFX with otvdm（Phase 5）

**Track A 已知阻塞**：otvdm v0.9.0 release（2023-09）的 `sndPlaySound` bug 撞 Civ1 → SEGV deterministic。修正在 cracyc/winevdm master `cd84ae2`（2025-11）已 merge，等下次 release 才能消化。WSL2 + wine 6.0.3 路線 100% 可跑。

詳細 Phase 0-5 紀錄（EDILZSS2 格式破解、CIV.EXE 結構分析、字節級 patch 過程、ne_dialog walker 約束、Win16 GDI 字型替換、Win10 portable 打包）見 [`docs/TRACK_A_README.md`](docs/TRACK_A_README.md)。

---

<a name="credits"></a>
## 📄 License & Credits

### Code & Tools
- `openciv1pp/` C++/SDL2 引擎 — **MIT License**
- `tools/` Track A patcher 系列 — **MIT License**

### Translations
- 中文翻譯文字（zh_TW.json + dialog_translations.json + inline_translations.json）— **CC BY-SA 4.0**

### Original Game
- *Sid Meier's Civilization* © 1991 MicroProse Software, Inc.
- *Sid Meier's Civilization for Windows* © 1993 MicroProse Software, Inc.
- 本專案**不包含**遊戲原始 binary / assets；使用者必須自備合法拷貝
- 商標權屬 Take-Two Interactive / Firaxis Games

### Source projects
- [OpenCiv1](https://codeberg.org/rhorvat/OpenCiv1) by Robert Horvat — 1991 DOS Civilization C# 轉寫，MIT License。openciv1pp 在此基礎上做 C++/SDL2 改寫 + 原生中文化。

### Honorable Mentions
- **Sid Meier** & **Bruce Shelley** — 1991 Civilization 設計
- **Francis Tresham** — 1980 Avalon Hill *Civilization* 桌遊（科技樹概念來源）
- **1993-1995 台灣 BBS 戰略遊戲版討論者** — 早期非正式中文討論的開拓者

---

*本 README 主線為 Track B `openciv1pp`（C++/SDL2 原生重寫 + 中文化）。Track A（1993 Windows civ.exe NE patch）歷史紀錄詳見 [`docs/TRACK_A_README.md`](docs/TRACK_A_README.md)。最後更新 2026-06。*
