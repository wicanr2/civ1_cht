# openciv1pp — OpenCiv1 C++ / SDL2 繁中化改寫

把 [OpenCiv1](https://codeberg.org/rhorvat/OpenCiv1)（C#/.NET + Avalonia 的 1991 DOS Civilization 重寫）逐步改寫成 **C++ + SDL2**，並內建繁體中文化。

本 repo 目前是**已驗證的地基（foundation）**：渲染管線、字型、中文化、SDL2 顯示、VCPU 核心都已能編譯執行。8 萬行遊戲邏輯（CodeObjects）為後續增量移植目標。

## 為什麼這樣分層

OpenCiv1 的遊戲邏輯大量是 x86 組語逐指令轉寫，跑在一個軟體 VCPU + 分段記憶體模型上。直接全量直譯成 C++ 是數月工程，所以策略是**先把可重用、難度集中的「殼層」做穩**：

```
遊戲邏輯 (CodeObjects)                ← 增量移植中：DrawTools ✅；其餘 ~40 待移植
   │ 透過 OpenCiv1Game 殼（VCPU + GDriver + CRectangle 脈絡）← ✅ harness 完成
   │ 呼叫
VCPU (暫存器/旗標/分段記憶體/全指令集) ← ✅ 完成；--selftest 通過
   │ 寫入
palette framebuffer (GBitmap)         ← ✅ 完成
   │ ├─ ASCII：GFont 點陣字
   │ └─ CJK ：CjkGlyphCache (FreeType MONO)  ← 中文化在 palette 層完成
   │ 翻譯：Translator (zh_TW.json)            ← ✅ 完成
   │ 轉 RGBA
顯示後端 SdlPresenter (SDL2)          ← ✅ 完成；與邏輯/中文化完全解耦

資源：.pic 載入器 (RLE+LZW+18bit palette) ← ✅ 完成；--restest 像素級 round-trip
```

**關鍵設計**：中文字在 palette 層就合成完畢，所以顯示後端可隨意更換，中文化不受影響。

## 依賴

- C++17、CMake ≥ 3.16
- SDL2（`libsdl2-dev`）
- FreeType2（`libfreetype-dev`）
- 一個 CJK TTF/TTC：自動搜尋 `OPENCIV1_CJK_FONT`、`assets/zh_font.ttf`、AR PL UMing、Noto Sans CJK、細明體…

```bash
sudo apt install build-essential cmake libsdl2-dev libfreetype-dev fonts-arphic-uming
```

## 編譯 / 執行

```bash
cmake -S . -B build && cmake --build build

cd .                      # 需從 repo 根目錄執行（會讀 assets/zh_TW.json）
./build/openciv1pp                 # 開 SDL2 視窗顯示繁中示範畫面
./build/openciv1pp --english       # 關閉翻譯（顯示原文）
./build/openciv1pp --dump out.ppm  # 離屏渲染一張 frame（無需 GUI，CI 友善）
./build/openciv1pp --pic FILE.pic  # 載入真實 .pic 資產 -> FILE.pic.ppm
./build/openciv1pp --makepic O.pic # 把示範畫面匯出成 .pic（exporter / e2e 測試）
./build/openciv1pp --gfxdraw O.ppm # GBitmap 繪圖原語視覺場景
./build/openciv1pp --gddraw O.ppm  # GDriver 螢幕間 blit tiling 視覺場景
```

### 自我測試（CI 友善，無需 GUI / 無需資產）

```bash
./build/openciv1pp --test    # 一鍵跑全部，任一失敗回傳非零（CI 用）
# 或個別：
for t in selftest restest gfxtest gdtest paltest; do ./build/openciv1pp --$t; done
```

| 測試 | 涵蓋 |
|------|------|
| `--selftest` | VCPU 全指令集 + C/Z/S/D/O 旗標語意 |
| `--restest`  | `.pic` codec（RLE+LZW）encode→decode 像素級 round-trip |
| `--gfxtest`  | GBitmap 繪圖原語（setPixel 模式 / line / fill / rect / replaceColor / drawBitmap 透明+裁切）|
| `--gdtest`   | GDriver 螢幕管理（drawImage 螢幕間 blit / screenToBitmap 擷取 / drawBitmapToScreen）|
| `--paltest`  | 18-bit palette color-struct load/save round-trip |

`--dump` 讓整條渲染管線可在**無頭環境驗證**：輸出 PPM，可轉 PNG 檢視中文是否正確上字。

## 中文化資料

`assets/zh_TW.json` 由 `OpenCiv1/tools/build_zh_table.py` 從姊妹專案 `civ1_cht`（1993 Win16 版繁中化）人工校過的譯文產生。新增翻譯：編輯 `civ1_cht/data/*.json` → 重跑該 script → 覆蓋 `assets/zh_TW.json`。查無對照的字串會 fallthrough 成英文，部分覆蓋永遠安全。

字型用 FreeType **MONO（hinted 1-bit）** 渲染而非灰階門檻——palette framebuffer 只能存開/關像素，hinted 點陣在小字級（細明體 Light）下遠比 thresholded AA 銳利。

## 目錄

| 路徑 | 內容 |
|------|------|
| `src/vcpu/` | 16-bit x86 VCPU：暫存器/flags/分段記憶體/**全指令集**（每個 CodeObject 的根基）。方法名對齊 `VCPU.cs`（`ADD_UInt16` 等），CodeObject 移植近乎逐行 |
| `src/graphics/` | `GBitmap`（palette framebuffer）、`GFont`、`CjkGlyphCache`、`Palette` |
| `src/resource/` | `Compression`（RLE+LZW）、`PicLoader`（.pic blocks → GBitmap + palette） |
| `src/localization/` | `Translator` + 極簡 JSON parser（零外部相依） |
| `src/platform/` | `SdlPresenter`（唯一觸碰 SDL2 之處） |
| `src/main.cpp` | 進入點 + bring-up 示範 + 測試模式 |

## 移植路線圖

- [x] **VCPU 補全** — 全指令集 + C/Z/S/D/O 旗標語意，逐一對拍 `VCPU.cs`；`--selftest` 通過。
- [x] **資源載入器** — RLE + LZW codec + `.pic` block 解析（18-bit palette / 8-bit / 4-bit packed）；`--restest` 像素級 round-trip。
- [x] **GBitmap 繪圖原語** — setPixel(mode)/drawLine/fillRect/drawRect/replaceColor/drawBitmap(透明+負偏移裁切)；`--gfxtest` 像素級驗證。
- [x] **GDriver 螢幕層** — 多螢幕 + drawImage(螢幕間 blit) + screenToBitmap(擷取) + drawBitmapToScreen；`--gdtest`。
- [x] **Palette color-struct** — 18-bit 位元組格式 load/save；`--paltest`。
- [ ] **更多資源** — `.map`、`.txt`、字型 `font.cv`（替換目前 FreeType 啟動的 ASCII bootstrap）。
- [x] **移植 harness** — `OpenCiv1Game` 殼（VCPU + GDriver + `CRectangle` 脈絡 + 字型註冊 + 文字量測）；確立可重複的 C#→C++ 移植模式。
- [~] **CodeObjects 增量移植** — **DrawTools** 已移植（文字排版：置中/陰影/`GetStringWidth`/`DrawTextBlock` 自動換行）；`--drawtest` + `--drawscene` 驗證。其餘 ~40 個依相依順序（開機→主選單→地圖→城市）逐檔移植，每步 `--dump` 比對畫面。
  - **注意：實際執行遊戲需使用者自備正版 Civ1 DOS 資產**（`.pic/.pal/.txt`，著作權，repo 不含）。
- [ ] **輸入/存檔/音效**。

## License

改寫程式碼 MIT（同 OpenCiv1）。中文譯文 CC BY-SA 4.0（同 civ1_cht）。執行需自備合法的原版遊戲資源檔。
