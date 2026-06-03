# 開發經驗紀錄（Lessons Learned）

> 本文件累積 openciv1pp（Track B）開發過程中**踩到的雷、學到的模式、可複製的工作流**。比 SKILL.md 更具體：SKILL 是「這個專案是什麼」，本文是「開發者實戰要怎麼做」。每個小節按議題分類，皆可獨立讀取。最新更新 2026-06。

---

## 目錄

1. [Sub-agent 委派工作流](#delegation)
2. [Docker-only 測試規則（含 X11 洩漏教訓）](#docker)
3. [Branch / PR 工作流](#branch-pr)
4. [CodeObject 移植 SOP](#codeobject)
5. [CityView 修復案例研究](#cityview)
6. [DOS Civ1 + C# OpenCiv1 對照方法](#comparison)
7. [zh_TW.json 維護](#i18n)
8. [ctest 演進與健康度](#ctest)
9. [Save format 演進](#saveformat)
10. [失敗模式速查](#failure-modes)

---

<a name="delegation"></a>
## 1. Sub-agent 委派工作流

主上下文容易 prompt-too-long（讀 .pic / .cs 大檔尤其爆），所以每個 CodeObject 移植或大改動派**獨立 sub-agent**。

### 委派 prompt 必含
- 完整工作目錄路徑（agent 從零開始不知道）
- `git checkout main && git pull origin main` 起手
- **完整 scope**（檔案、新增測試、翻譯字串）一次給足
- **build + ctest + commit + push + open PR** 全包進 agent 自己做
- 明確說「**不可開遊戲視窗**」，純 headless ctest
- 結尾報告格式（commit hash / ctest count / line delta / 200 字內）
- 失敗處置：3 次 fix 不成功 `git checkout -- . && git clean -fd` 還原誠實回報

### 常見失敗模式
- **API socket error 早死**：通常 14-58 tool uses 之間。對策：在 prompt 加 hard cap，並設計成「部分結果也能 commit」(`git add` 成功部分、honest fail 報告)。
- **Sub-agent 越權**：上一個 agent 的 working tree 殘留改動，新 agent 若 `git add -A` 會誤 commit 他人工作。對策：明寫「先 `git status`，發現他人改動 → stash → 你 commit → restore」。
- **Push race**：兩個 agent 同時 push 同 branch。對策：規定 `git pull --rebase` 重試 ≤ 3 次後失敗。

### Bulk parallelization 配方
獨立 scope 同時派 3 個 agent + run_in_background=true：
```
T13: A1+A2 視覺前端  → feat/credits-and-wizard
T14: A3+A4 in-game hint → feat/tutorial-and-civnote
T15: B5+B6 機制/輸入 → fix/year-gate-and-textinput
```
彼此不衝突 IFF 在不同檔案；衝突僅在 `assets/zh_TW.json`（小增量好 rebase）跟 `CMakeLists.txt`（foreach test 列）。

---

<a name="docker"></a>
## 2. Docker-only 測試規則（含 X11 洩漏教訓）

### 鐵律：任何會碰 X11 / SDL window / DOSBox / 螢幕擷取 / GUI 自動化的工作 **必須在 Docker container 內**跑

### 為什麼

本機曾發生：
- 子 agent 把 `dosbox -conf ...` 跑在 host shell（不是 container 內），host `DISPLAY=:1` 繼承下去，**dosbox 視窗跳到用戶桌面前景**
- `Xephyr` 退路會開「可見的」nested X window — **禁用 Xephyr**
- 容器內的 Xvfb 啟動慢，script 沒 fail-fast，DISPLAY=:99 沒對齊 → 工具走 host display
- TaskStop 只殺 agent harness，**docker container 自己活著** — 必須 `docker ps -aq | xargs docker kill`

### 強制 docker run 旗標

```bash
docker run --rm \
  --network none \              # ← 切網路，杜絕 X TCP 連線
  --env DISPLAY=:99 \           # ← 明確覆寫，不繼承 host 的 :1
  --tmpfs /tmp/.X11-unix \      # ← 不分享 host X socket
  -v <repo>:<mount>[:ro] \
  <image> bash <container_script>
```

### 容器內 script 第一行必 fail-fast

```bash
set -uo pipefail
[ "${DISPLAY:-}" = ":99" ] || { echo "FAIL: DISPLAY=$DISPLAY"; exit 1; }
Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR &
for i in 1 2 3 4 5; do xdpyinfo -display :99 >/dev/null 2>&1 && break; sleep 1; done
xdpyinfo -display :99 >/dev/null 2>&1 || { echo "FAIL: Xvfb"; exit 1; }
```

### 啟動後即時驗證隔離

```bash
sleep 5
CID=$(docker ps -q | head -1)
docker inspect $CID --format '{{.HostConfig.NetworkMode}} {{.Config.Env}}'
# 期望: "none ... DISPLAY=:99 ..."
# 否則: docker kill $CID && exit 1
```

### 退場清理

```bash
docker ps -aq | xargs -r docker kill
docker ps -aq | xargs -r docker rm -f
```
**每次** sub-agent 結束都要做（無論成功失敗）。container 殘留 = 下次 host 探測誤判 + volume mount root-owned 檔卡 stash。

### 純 headless（無 X）也可在 host 跑

- `cmake --build build -j` ✓
- `ctest --test-dir build` ✓ — 測試本身要設計成「不開窗」
- 純 Bash / Python / git ✓

### 已建立的 Docker images

| Image | 用途 | Dockerfile |
|---|---|---|
| `openciv1pp-recorder:latest` | 錄影 / DOS playthrough | `openciv1pp/docker/recording/Dockerfile` |
| `openciv1-cs-cmp:latest` | C# OpenCiv1 build + 對照 | `openciv1pp/docker/cs_compare/Dockerfile` |

兩個 image 都包含：Xvfb + ffmpeg + xdotool + ImageMagick + fonts-noto-cjk。差別在 C# 版多 `.NET 8 SDK + Avalonia runtime libs`。

詳細規則記於 user-level memory：`~/.claude/projects/-home-anr2-civ1-cht/memory/docker-only-testing.md`。

---

<a name="branch-pr"></a>
## 3. Branch / PR 工作流

### 鐵律：**禁止直接 push 到 `main`**

Claude Code classifier 會阻擋。所有改動走「feature branch → PR → merge」。

### 標準流程

```bash
git checkout main && git pull origin main
git checkout -b <type>/<short-desc>
# ... 改動 ...
git add <specific-files>
git commit -m "<scope>: <message>" \
  --message "Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>"
git push -u origin <type>/<short-desc>
gh pr create --base main --head <type>/<short-desc> \
  --title "<title>" --body "<body>

🤖 Generated with [Claude Code](https://claude.com/claude-code)"
```

### Branch naming convention

| Prefix | 用途 |
|---|---|
| `feat/` | 新功能 |
| `fix/` | bug 修復 |
| `docs/` | 文件 |
| `study/` | 學習研究、不必合並的對照資料 |
| `skill/` | SKILL.md / memory 規則更新 |

### Merge 後清理

```bash
gh pr merge <N> --merge   # merge commit (preserves granular history)
git push origin --delete <branch>    # remote
git branch -D <branch>               # local
git fetch --prune                    # remove stale refs
```

### Co-Authored-By trailer

每個 commit 必含：
```
Co-Authored-By: Claude Opus 4.7 (1M context) <noreply@anthropic.com>
```
PR body 結尾必含：
```
🤖 Generated with [Claude Code](https://claude.com/claude-code)
```

### 大型 sub-project（如 Track B）整批合並

Track B 的 45+ commits 一次走 PR #1 進 main，之後增量再開個別 PR。模式：先建大底子 PR、merge、之後每個 CodeObject / 修復獨立 PR。

---

<a name="codeobject"></a>
## 4. CodeObject 移植 SOP

從 `~/civ1_cht/OpenCiv1/src/Game/CodeObjects/<Name>.cs` 轉寫到 `openciv1pp/src/game/<Name>.{h,cpp}`：

1. **不修改 C# source** — READ-ONLY 參考。
2. 開 C++ class 取 `OpenCiv1Game&`，**保留 `F0_*` 命名** 對齊 C#。
3. 用 cpu / graphics / var_aa 照抄狀態存取。
4. **文字必走 DrawTools / GDriver.drawString** chokepoint（會自動翻譯）。
5. 加 `--xxxtest` （ASCII off vs CJK on 像素差證明翻譯生效）→ 註冊 CMakeLists `foreach` + `main.cpp` `--test` 聚合器。
6. 雷區：
   - **CJK glyph 用 FreeType `FT_LOAD_TARGET_MONO`**，不要灰階+門檻（曾產生破碎斜線片）
   - **ctest 靠 `WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}`** 找 assets/zh_TW.json 相對路徑
   - **palette ordering**：CBACK 自帶 palette 必須最後安裝，否則 TER257 把 CBACK 的棕地索引蓋掉 → 整片發灰
7. 委派 sub-agent 帶上現有 C++ API 速查 + DrawTools/ImageTools usage 範例。

### 已移植 22+ 個 CodeObjects 範例
DrawTools / ImageTools / LanguageTools / CommonTools / MenuBoxDialog / TextBoxDialogs / GameMenus / FrontEndFlow / MainCode / MainIntro / MapManagement / UnitManagement / CheckPlayerTurn / CityView / TechResearch / Government / GameLoadAndSave / MiniWorld / Civilopedia / HallOfFame / TerrainTiles / ...

剩餘大宗：完整 in-game UI 修飾、CityWorker（4970 lines C#）、Civilopedia 全量、HallOfFame 細節、save format v17+。

---

<a name="cityview"></a>
## 5. CityView 修復案例研究

用戶反饋 「進入城鎮管理的畫面 跟 dos 版差很多 顏色怪怪的」 → 兩階段拆解。

### 階段 1：座標分析
從 `~/civ1_cht/OpenCiv1/src/Game/CodeObjects/CityWorker.cs` （DOS 逐行轉寫）grep 每個 `FillRectangle` / `DrawString` / `ReplaceColor`，建表：
```
| 元素 | DOS 座標 | 顏色 | 對應 C# line |
|------|---------|------|-------------|
| 城市名置中 | (104, 2) | 15 white | L293 |
| 工作地塊區 | (127,23)..(208,104) | pattern | L296 |
| 中央格 5×5 of 16×16 | 中心 (160, 56) | TER257 sprites | L302 |
| 糧倉垂直條 | (309, 2, 8, 96) | 12/14 swap | L2174 |
| 供養單位 | (98, i*6+179) | 10 | L2477 |
| 右資訊面板 | (230, 99, 90, 100) | 0 black | L2638 |
```
寫成 `docs/CITYVIEW_LAYOUT.md` 給實作 agent。

### 階段 2：實作
派 agent 照表重寫 `CityView::draw()`。**+349/-250 行**，DOS 區嚴格對齊。

### 顏色根因：palette stomp
本以為 CBACK 的 palette 完整載入，但 `screen.palette.set(0..15, ...)` 在 `copyPaletteFrom(*backdrop_)` **之後** 又把 0..15 蓋成「Windows 3.1 VGA 標準」。**錯！** CBACK 本身的 index 0..15 是它自己 .PAL 的值（巧合就是 VGA 標準，但棕地漸層在 155..175，被我們在 160..175 的 HUD overrides 撞到）。

**修法**：drop 那一段 `palette.set(0..15)`、`copyPaletteFrom` 後不再改、TER257 tile blit 改成「per-pixel RGB-nearest remap into CBACK palette」（LUT 256 entries pre-computed）。

**結果**：DOS-region 33% pixel diff vs raw CBACK（diff 是疊上去的中文+精靈），CBACK 雲彩+棕地完整透出。

### 教訓
DOS 1991 用 256-color VGA palette，每個 .PIC 自帶 palette。**code 中用的「色號」是針對當前 palette table** — 不能假設 0..15 是 VGA。要確認 CBACK index 9 (例) 真的是什麼顏色，要 dump `cbackPal[9]`。

---

<a name="comparison"></a>
## 6. DOS Civ1 + C# OpenCiv1 對照方法

用戶質疑 openciv1pp 跟原版差別大 → 三層對照：

### 層 1：靜態 .PIC 對照
最低成本。把 DOS 的 LOGO.PIC / BIRTH3.PIC / CBACK.PIC 用我們的 PicLoader decode 成 PNG，跟 openciv1pp 的 `--shotTitle` / `--shotIntro` / `--shotCity` 截圖 pixel-diff。
- **TITLE / INTRO BIRTH 已驗證 0/64000 pixels diff** = PIXEL_PERFECT
- 限制：只能比靜態畫面，不能比 runtime 合成的 CityView / World

### 層 2：DOS 實機 via DOSBox
Docker + Xvfb + dosbox-staging + xdotool 自動驅動。記錄每個 UI 狀態：
- 開機儀式（graphics → sound → input 三 prompt）
- 標題 / 主選單 / 難度精靈 / 部落選擇 / 命名 / 世界生成
- 第一回合 + 城市建立 + 城市畫面 + 結束回合

**60 PNG + 報告**：`docs/DOS_CIV1_PLAYTHROUGH_NOTES.md`

### 層 3：C# OpenCiv1 並排
Docker + .NET 8 SDK + Avalonia + Xvfb。Build C# 版本然後 xdotool 驅動。產 side-by-side 對照圖。

**14 PNG + 報告**：`docs/CS_COMPARISON_REPORT.md`

### 關鍵發現
- **openciv1pp 中文化覆蓋率 > C# OpenCiv1**（C# 還是英文 + 部分亂碼）
- **openciv1pp 缺**：製作組鳴謝 / 新局精靈級聯視覺 / 首回合教學疊圖 / CIVILIZATION NOTE 橫幅
- **C# upstream bug 發現**：`OpenCiv1Game.cs:107` Release build 缺 `using System.Reflection;`

---

<a name="i18n"></a>
## 7. zh_TW.json 維護

### 結構
`openciv1pp/assets/zh_TW.json` 單一 flat JSON：
```json
{
  "English source string": "中文翻譯",
  ...
}
```
~370+ 條目（本 session 開頭 ~283 → 結束 ~370）。

### 翻譯規則
- **Chokepoint 注入**：`GDriver::drawString` / `DrawTools::F0_1182_*` 自動 lookup table
- **TRIM key 後 lookup**：DOS 字串常含 hotkey marker `\x8f<ch>`，`MenuBoxDialog::splitMenuItems` 先剝去再丟翻譯
- **無譯回退**：找不到 key 就原文輸出（不會壞遊戲）

### 容易漏的字串桶
- 14 文明領袖+國族+民族三元組（MainCode.cpp:138-152）
- GameMenus Orders/Options 子選單
- TerrainTiles 各地形
- 「EARTH」「Pick your tribe...」「Player」「(HELP AVAILABLE)」這類分散小字
- `"Esc: quit"` 之類介面 hint

### 翻譯 sweep 步驟
1. grep 整個 src/ 找 raw English literal 通過 drawString
2. 對照 zh_TW.json 列缺失
3. 一批 add + commit
4. 跑 ctest 確認沒 break

詳細缺漏清單見 `docs/DOS_PARITY_REPORT.md` / `docs/DOS_PARITY_REPORT_R2.md`。

---

<a name="ctest"></a>
## 8. ctest 演進與健康度

| Session 階段 | ctest 數 |
|---|---|
| 開頭 | 17 |
| Track B 大底完成 | 50 |
| Barbarians / Apollo / Wonders | 52 |
| Civilopedia + HoF | 54 |
| 本次三個 agent A1-B6 後（預估）| 60 |

### 測試健康度（R3 報告）
- `ctest --repeat until-fail:5` → 270/270 sequential PASS，**0 flake**
- 全 54 個測試 < 0.10 秒；總時長 < 1 秒
- `cmake --build` -Wall -Wextra 零警告

### 測試命名
`<feature>test`，例：barbtest / spacetest / civtest / halltest / cityviewtest。

### 測試骨架
`src/main.cpp` 內 `int <name>test() { ... return 0; }` + dispatcher。`CMakeLists.txt` foreach 列表登記。`set_tests_properties WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}` 確保 assets 路徑解析。

### 不可 break 規則
新 sub-agent 完成前**必須** ctest N/N 全綠。若新增測試導致舊測試 fail，先修舊測試的退化或回滾自己改動。

---

<a name="saveformat"></a>
## 9. Save format 演進

| Version | 新增欄位 | Branch |
|---|---|---|
| v1 | 初版 | - |
| v15 | civbarb | barbarians |
| v16 | civapollo, civship | Apollo + Space Race |
| v17（規劃中）| tutorial, notesfired | T14 agent |

### 向下相容
每次 bump 都要 v1..v(N-1) 仍可 load（缺欄位預設值）。`GameLoadAndSave.cpp` 讀檔走 line-by-line `if startsWith("xxx ")` switch，不認識的行忽略。

### `savetest` 一定要每版 round-trip
新 field 加完務必 save → load → 比對 → assert。

---

<a name="failure-modes"></a>
## 10. 失敗模式速查

| 症狀 | 根因 | 修法 |
|---|---|---|
| 遊戲視窗跳到桌面 | sub-agent 在 host shell 跑 X-binary（`dosbox` / `openciv1pp --shot*` / `dotnet *.dll`） | 強制包 `docker run --network none --env DISPLAY=:99 --tmpfs /tmp/.X11-unix` |
| TaskStop 後仍看到視窗 | container 還在跑 | `docker ps -aq \| xargs docker kill && docker ps -aq \| xargs docker rm -f` |
| stash 失敗 root-owned files | 之前 docker run 沒清乾淨 root 寫的 volume | container 內加 `--user $(id -u):$(id -g)` 或退場時 `chown -R $(id -u) /work/out` |
| Sub-agent API socket error 早死 | 上下文太大 / 網路抖動 | 縮 scope、加 hard cap tool calls、寫成「部分結果也能 commit」 |
| Push race | 兩 agent 同 branch | 規定 `git pull --rebase` 重試 ≤ 3 次 |
| 直接 push main 被擋 | Claude classifier | 走 feature branch + PR |
| CJK glyph 破碎斜線 | FreeType 灰階 + 硬門檻 | 用 `FT_LOAD_TARGET_MONO` 1-bit |
| CityView 整片發灰 | TER257 palette stomp CBACK | per-pixel RGB-nearest remap 進 CBACK palette |
| MenuBox 中文沒翻譯 | hotkey marker `\x8f<ch>` 黏在 label 後沒剝 | `splitMenuItems` 先剝再翻譯 |
| ctest WORKING_DIRECTORY 找不到 assets | 預設 cwd 是 build/ | CMake foreach 後 `set_tests_properties ... WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}` |
| WaitTimer 無效 | C# 是 ms × 12，我們忘記乘 | `SDL_Delay(max(waitTime * 12, 1))` |
| 兩份 README 矛盾 | 過去多次重構留下 stale 版本 | 主 repo README + sub-project README 分層；歷史內容歸檔到 `docs/<TRACK>_README.md` |

---

## 附錄：本 session 重大里程碑

| Date | 里程碑 | 對應 PR |
|---|---|---|
| 2026-05 | Track B 引擎地基 + 8 個 CodeObjects | PR #1 |
| 2026-06 | Side-by-side 影片對照（Docker+Xvfb 首次） | PR #2 |
| 2026-06 | Docker-only 測試規則永久化 | PR #3 |
| 2026-06 | README 重構 → openciv1pp 主體；Track A 歸檔 | PR #4 |
| 2026-06 | DOS Civ1 實機 playthrough 學習筆記 | PR #5 |
| 2026-06 | C# OpenCiv1 並排對照 | PR #6 |
| 2026-06 | A1+A2 製作組+新局精靈 / A3+A4 教學+NOTE / B5+B6 年代閘+TEXTINPUT | PR #7-9（規劃中）|

---

*文件起源 2026-06，由本 session 累積經驗整合而成。後續發現的新模式請追加到對應小節。*
