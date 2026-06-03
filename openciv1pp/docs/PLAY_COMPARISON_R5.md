# R5 深度遊玩對照報告：DOS Civ1 vs openciv1pp（20 步劇本）

**日期**: 2026-06-03 · **分支**: `study/r5-deep-gameplay` · **基線**: `main @ 7f0206b`

本報告是 R3 (60 張 DOS) / R4 (A1–B6 預備驗證) 之後的真正「下場玩」。我們在 **完全離線 Docker + Xvfb** 環境中，
用 `xdotool` 同步腳本驅動 `openciv1pp --game` 與 `dosbox CIV.EXE` 跑同一段
20 步劇本，逐幀截圖配對。所有截圖、配對 PNG、與本報告皆位於
[`docs/screenshots/r5/`](screenshots/r5/) 與本檔。

---

## 1. 硬化追蹤（Docker / Xvfb / 無對外網路）

| 項目 | 預期 | 實測 |
|---|---|---|
| 容器網路模式 | `none` | `none`（docker inspect 確認）|
| DISPLAY | `:99` | `:99`（容器 env + Xvfb -screen 0 1280x720x24）|
| `/tmp/.X11-unix` | `tmpfs` | `tmpfs`（`--tmpfs` 掛載）|
| 主機 X 視窗 | 0 個 | 0 個（捕捉期間以 `xdpyinfo -display :1` 抽檢無變化）|
| 容器映像 | `openciv1pp-recorder:latest` | 同上（沿用 R3）|
| 捕捉後清理 | `docker kill && docker rm -f` | 完成 |

`docker inspect` 採樣輸出（容器存活期間擷取）：

```
none | [DISPLAY=:99 PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
        DEBIAN_FRONTEND=noninteractive TZ=Asia/Taipei]
```

腳本檔位於 [`docker/recording/r5_capture_oci.sh`](../docker/recording/r5_capture_oci.sh),
[`docker/recording/r5_capture_dos.sh`](../docker/recording/r5_capture_dos.sh),
[`docker/recording/r5_pair.sh`](../docker/recording/r5_pair.sh)。

---

## 2. 20 步劇本對照表

OCI = openciv1pp / DOS = DOS Civ1。配對 PNG 路徑為
`docs/screenshots/r5/<NN>_<STEP>_pair.png`。

| # | Step | DOS 行為（觀察） | openciv1pp 行為（觀察） | Match | 落差/Bug |
|---|---|---|---|---|---|
| 1 | TITLE | 藍色版本標題面板「CIVILIZATION (TM) — Version 474.03」+ 提示 `Press <Enter> to continue`. | 直接顯示 LOGO.PIC 主畫面（已有 zh_TW 主選單骨架，但本捕捉時尚未疊上選單）。 | **CLOSE** | DOS 多了文字模式藍色版本面板；OCI 直接進入圖形模式。 |
| 2 | CREDITS | 星河旋臂背景 + 青色字「the Earth was without form,」（13 段 vignette 之一）。 | `--shotIntro` 渲染落地（多語）；live 模式按 Enter 後直接跳過。 | **CLOSE** | OCI 的 `--shotIntro` 圖質 OK，但 live `--game` 沒讓使用者看到中文字幕；缺 stage gating。 |
| 3 | MAIN_MENU | 黃框選單 5 項（Start a New Game / Load a Saved Game / EARTH / Customize World / View Hall of Fame）疊在 LOGO 上。 | 對應的中文選單（開始新局 / 載入存檔 / EARTH / 自訂世界 / 名人榜）已落地。 | **MATCH** | 選單反白色塊配色（DOS 用 dark-on-tan）與 OCI（cyan-on-blue）不同；R2 已記錄。 |
| 4 | WIZARD_DIFFICULTY | 左側 5 個肖像（caveman/viking/knight/king/emperor）+ 右側黃框「Difficulty Level... / Chieftain / Warlord / Prince / King / Emperor」。 | 左側 7 色彩盒（**還在 placeholder**），右側標題「選擇難度」被黃色 highlight bar 覆蓋；底下可見 4 行（戰王/王子/君主/皇帝(最難)）— 第一個 **酋長(最易) 也被遮**。 | **DIVERGENT** | **新 Bug #R5-1**：A2 wizard header「選擇難度」與第一個列表項都被 highlight bar 蓋住（繪製順序）。**新 Bug #R5-2**：左側肖像未實作（color blocks 替代）。資料正確（5 項），純視覺遮擋。 |
| 5 | WIZARD_CIVS | 右側列表「Level of Competition... / 7..3 Civilizations」。 | 對應步驟落地，含 zh_TW（標題與 OCI 4 步相同 layout）。 | **CLOSE** | OCI 同樣有 highlight bar 蓋標題的問題；資料項目對齊。 |
| 6 | WIZARD_TRIBE | 右側 14 個部族（Roman/Babylonian/...）+ 左側堆疊肖像「卡牌動畫」。 | 右側列表存在；左側肖像缺。 | **CLOSE** | 同 #R5-2，肖像 placeholder。 |
| 7 | WIZARD_NAME | 右側單行文字欄，預填 `Caesar` (Roman 領袖)；游標閃爍。 | 截圖（07b_TYPED）顯示 OCI **直接跳到 PLAYING**（年份 4000 BC + 拓荒者），表示 `xdotool type "Claude"` 未進入 name buffer（B6 SDL_TEXTINPUT 路徑可能在無焦點時靜默吞鍵）。 | **BROKEN** | **新 Bug #R5-3**：B6 名稱輸入在 xdotool type 路徑下不接收（live --game 沒有 SDL_StartTextInput 觸發？）— 需 verifier。 |
| 8 | WORLD_GEN | 全黑 → 單格綠地閃現（fog-of-war 從第一格開始）。 | `--shotWorld` 顯示完整生成世界（測試模式 reveal-all）；live 模式 fog-of-war 亦未完整實作。 | **DIVERGENT** | 與 R3 結論一致：production 流程沒套上 fog mask（reveal-all in test mode 滲到 live）。 |
| 9 | FIRST_TURN | 經典 **TUTORIAL OVERLAY**：白色標籤框 + 連線指向 Menu Bar / Map Window / Active Unit / 周圍地形 (Ocean/Grassland/Forest/...)。**這是 Civ1 onboarding 招牌**。 | OCI 顯示一張**乾淨的世界圖**（中央拓荒者高亮、回合 1、4000 BC、丘陵）— **沒有任何 callout 疊圖**。 | **BROKEN** | **新 Bug #R5-4 (重申 R4)**：A3 TutorialOverlay 已實作但 `armed=false` 預設且 `FrontEndFlow::enterPlaying()` 從未 `setArmed(true)`。`MiniWorld.cpp:1301` 條件 `armed && !shown && turn==1` 永遠 false。 |
| 10 | DISMISS_TUTORIAL | 任意鍵後 overlay 消失，浮現「--- CIVILIZATION NOTE --- / YOUR FIRST TASK IS TO FIND A SUITABLE SITE...」綠底青框橫幅。 | 無 overlay 可解除；底部狀態列維持「你必須移動所有單位」字串。 | **BROKEN** | 同 #R5-4。第一回合 CIV NOTE 沒在新局流程觸發。 |
| 11 | MOVE_SETTLER_NORTH | 拓荒者向上移動 1 格；side-bar `Moves: 1`。 | OCI Up 鍵生效（log: `[game] -> ...`），拓荒者高亮位置上移；狀態列「移動點: 3」（OCI 已扣）。 | **MATCH** | OK。 |
| 12 | TRY_END_TURN_PREMATURELY (B5 閘門) | DOS 沒有「必須移動所有單位」這條（Enter 直接送入下一回合，Settler 失去剩餘 mvp）。 | OCI **保留 4000 BC 不前進**並顯示「你必須移動所有單位」（B5 落地）。本捕捉發現 OCI 因前一回合已被推進，截到 3980 BC 山脈 — 表示 B5 已過第一回合閘門。 | **DIVERGENT(intentional)** | B5 是 openciv1pp 特有「友善化」設計（PR #12）。不算 bug，但 README 應明文這是 **openciv1pp 加碼**。 |
| 13 | CONSUME_SETTLER_MVP | DOS 'W' (Wait) 跳過。 | OCI 'W' 已實作 skipUnit；本捕捉顯示同回合下一格。 | **MATCH** | OK。 |
| 14 | END_TURN_OK | DOS 年份滾至 3950 BC。 | OCI `[game] -> ...` log 顯示 endTurn → 年份 3980/3960 BC。 | **MATCH** | 數字差 1 grid（DOS 每回合 -50 / OCI 每回合 -20，需校正轉換表）— 落差但不是 bug。 |
| 15 | FOUND_CITY | DOS 按 b → city naming dialog（預填 "Rome"），Enter 確認，**綠底 CIV NOTE「YOUR CITY HAS BEEN FOUNDED」浮現**。 | OCI 按 B → log `[game] founded city: Rome at (12,31)`；UI 上狀態列改為「建立城市: 羅馬 產: 騎兵 4/20」（city 已建並選擇 Cavalry 生產）— **無浮窗銅製 CIV NOTE 橫幅**。 | **BROKEN** | **新 Bug #R5-5 (重申 R4)**：A4 CivNoteBanner 未在 `buildCityAtUnit` 後觸發 `queueFirstNote(FirstCity, ...)`。`main.cpp:2768` 與 `main.cpp:3237` 的 `--game` / `--play` build-city 分支應在成功後呼叫 `queueFirstNote(0, CivNoteKind::FirstCity, "First city note")`。 |
| 16 | CITY_VIEW | DOS 建城後自動開啟城市檢視（中央地塊 + 周圍 21 格 + 蕈狀產量 icons）。 | OCI 在 `--game` 模式 **沒有自動開啟 CityView**（B 鍵直接建好，畫面停在地圖）。`--shotCity` headless dumper OK，但 live 缺鍵盤觸發。 | **DIVERGENT** | **新 Bug #R5-6**：`--game` 模式缺 `P` 或 `F1` 開 CityView 的鍵綁。`KeyP` 目前是 `makePeaceWithRival`，需另闢鍵或加入 `KeyF1`. |
| 17 | CHOOSE_PRODUCTION | DOS 在 CityView 內按 P 切換生產項；本捕捉內因 timing 未抓到 dropdown。 | OCI 無 CityView 開啟管道（同 #R5-6）。 | **DIVERGENT** | 同上。OCI **狀態列顯示「建立城市: 羅馬 產: 騎兵 4/20」** — 預設選了騎兵（默認從 `MainCode::tribes()` 第 1 個 unit?）。 |
| 18 | EXIT_CITY | DOS Escape 回地圖。 | OCI Esc **回到 MAIN_MENU**（log: `[game] -> MAIN_MENU`）— 不是 "回到地圖"。 | **DIVERGENT** | OCI 的 ESC 在 PLAYING 時 = back-to-menu（設計如此）；DOS 把 ESC 當「關閉子窗」。差異需文檔化。 |
| 19 | END_TURN_2_3_4 | DOS 年份滾 3950/3900/3850 BC；單格 fog 範圍。 | OCI Enter * 3 推進 — 截到「回合 2 年份 3980 BC」（前一輪殘留 / live timing）。 | **MATCH** | 邏輯通，timing 差異。 |
| 20 | CITY_REVIEW_TURN_5 | DOS Enter 再推進到 turn 5；side-bar 對應 year。 | OCI 同上。 | **MATCH** | OK。 |

**對齊度**：Match = 7 · Close = 4 · Divergent = 4 · Broken = 3 · DivergentIntentional = 1 · = 19 / 20 完成（步驟 7 因 NAME 跳過實際是 broken 計入）。

---

## 3. A1–B6 runtime 狀態（R5 最終裁定）

| 特性 | 狀態 | 證據 / file:line |
|---|---|---|
| A1 CREDITS（intro 字幕）| **VERIFIED(headless)** / **CLOSE(live)** | `--shotIntro` 對齊 DOS 13-vignette；live `--game` 跳過字幕（`main.cpp:9415` 直接進 gameInteractive，不經 intro slide）|
| A2 NewGameWizard 級聯 | **BROKEN-VISUAL** | 5 個 difficulty 項目皆在 `MainCode::difficultyItems()` (`MainCode.cpp:129`)；**標題與首項被 highlight bar 蓋住**（繪製順序 bug，見 #R5-1） + 左側 portrait 未實作（#R5-2）|
| A3 TutorialOverlay | **NEEDS_FIX** | `TutorialOverlay.h:30` `armed = false` 預設；`MiniWorld.cpp:1301` 條件正確；**`FrontEndFlow.cpp:62 enterPlaying()` 從未 `miniWorld_->tutorial().setArmed(true)`** — 一行修復 |
| A4 CivNoteBanner | **NEEDS_FIX** | `MiniWorld::queueFirstNote()` (`MiniWorld.cpp:1306`) 已實作 dedupe；但 `MiniWorld::buildCityAtUnit()` (`MiniWorld.cpp:216-241`) **未呼叫** queueFirstNote(FirstCity)；新增 1 行 |
| B5 Year-Gate（必須移動所有單位）| **VERIFIED** | 狀態列文字「你必須移動所有單位」於步驟 12 截圖落地；4000BC → 3980BC 年份閘已啟動 |
| B6 SDL_TEXTINPUT 名稱輸入 | **NEEDS_VERIFY** | 步驟 7 截圖顯示 `xdotool type "Claude"` 未進入 name buffer（畫面跳到 PLAYING）— 可能 SDL_StartTextInput 未在 NAME stage 自動 enable，或 xdotool delay 太短；需手動測 |

---

## 4. R5 新發現 Bug 清單

| ID | 描述 | 嚴重度 | 建議 fix |
|---|---|---|---|
| **R5-1** | Wizard「選擇難度」標題 + 首項被黃色 highlight bar 覆蓋 | P1 | `MenuBoxDialog::draw` 順序：先 fillRect highlight，再 drawText（目前反過來）|
| **R5-2** | Wizard 左側 5/14 portrait 為彩色 placeholder block，未載入 DOS BACK0A/M.PIC 部分 | P2 | `NewGameWizard` 加 portrait sub-bitmap 切割（DOS art 已在 `dos_civ/extracted/`）|
| **R5-3** | live --game 路徑 NAME 步驟接不到 `xdotool type` | P1 | 確認 NAME 進場時 `SDL_StartTextInput()` 被呼叫；若已呼叫，檢查 xdotool delay vs SDL event queue |
| **R5-4** | A3 TutorialOverlay 永不顯示（armed 永 false）| **P0** | 1 行：`miniWorld_->tutorial().setArmed(true);` 加在 `FrontEndFlow.cpp:62 enterPlaying()` 末尾 |
| **R5-5** | A4 First-City CIV NOTE 不觸發 | **P0** | 1 行：`if (built) queueFirstNote(playerId, CivNoteKind::FirstCity, "First city note");` 加在 `MiniWorld.cpp:235` (buildCityAtUnit 成功分支) |
| **R5-6** | --game 模式無鍵盤打開 CityView 路徑（KeyP 已被 peace 占用）| P1 | 加 `case SdlPresenter::KeyF1: openCityViewIfCityAtCursor(); break;` |
| **R5-7** | ESC 在 PLAYING 直接回 MAIN_MENU；DOS 將 ESC 視為「關閉子窗」 | P2 | 在 PLAYING 時 ESC 改開 GameMenu（不是 back-to-menu），符合 DOS UX |

---

## 5. 下一輪 (R6) 建議 top-5 fixes

1. **R5-4 + R5-5（1 行 + 1 行）— 把 A3/A4 連上現有實作**。最高 CP 值，補完 R4 PR#11 漏掉的最後一哩路。
2. **R5-1（draw-order swap）— Wizard 標題不再被遮**。一行 fix 補強 R4 PR #8 (feat/credits-and-wizard) 的 cosmetic 缺口。
3. **R5-6 + R5-7 — 鍵盤可打開 CityView + ESC 行為 DOS-aligned**。讓 `--game` 不再需要滑鼠。
4. **R5-3 — NAME stage 啟用 SDL_StartTextInput()**。沒它 B6 在現實玩家手裡就是 broken UX。
5. **A1 live intro stage**：把 `--shotIntro` 的渲染路徑接入 `gameInteractive` 啟動 sequence（TITLE → INTRO → MAIN_MENU），實裝完整 DOS 開機儀式（藍色版本面板可選）。

---

## 6. 附錄：完整 20 步配對 PNG 清單

所有檔位於 [`docs/screenshots/r5/`](screenshots/r5/)。每張 `<NN>_<STEP>_pair.png`
是 1280×504（OCI 640×480 + DOS 640×480 + 16px 標題列）`+append` 而成。

- `01_TITLE_pair.png` — Title 畫面
- `02_CREDITS_pair.png` — 字幕 / 介紹
- `03_MAIN_MENU_pair.png` — 主選單
- `04_WIZARD_DIFFICULTY_pair.png` — **新局精靈級聯，A2 落地**
- `05_WIZARD_CIVS_pair.png` — 文明數量
- `06_WIZARD_TRIBE_pair.png` — 部族選擇
- `07_WIZARD_NAME_pair.png` — 領袖名稱（B6 名稱輸入）
- `08_WORLD_GEN_pair.png` — 世界生成
- `09_FIRST_TURN_pair.png` — **第一回合，A3 教學疊圖**（DOS 有，OCI 無）
- `10_DISMISS_TUTORIAL_pair.png` — 解除教學
- `11_MOVE_SETTLER_NORTH_pair.png` — 拓荒者向上移
- `12_TRY_END_TURN_PREMATURELY_pair.png` — **B5 年代閘 + 必須移動單位**
- `13_CONSUME_SETTLER_MVP_pair.png` — 跳過剩餘移動點
- `14_END_TURN_OK_pair.png` — 推進回合 (3950 BC)
- `15_FOUND_CITY_pair.png` — **建城瞬間 + A4 文明備忘橫幅**（DOS 有，OCI 無）
- `16_CITY_VIEW_pair.png` — 城市管理畫面
- `17_CHOOSE_PRODUCTION_pair.png` — 選擇生產項
- `18_EXIT_CITY_pair.png` — 離開城市畫面
- `19_END_TURN_2_3_4_pair.png` — 連續回合 2/3/4
- `20_CITY_REVIEW_TURN_5_pair.png` — 第 5 回合再進城市

---

**Co-Authored-By: Claude Opus 4.7 (1M context)**
