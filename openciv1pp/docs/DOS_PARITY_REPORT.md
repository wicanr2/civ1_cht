# DOS 對照差異報告 (2026-06-02)

**Scope**: openciv1pp at `/home/anr2/civ1_cht/civ1_cht/openciv1pp/` (track-b-cpp-sdl2-port)
vs DOS Civ1 assets at `/home/anr2/civ1_cht/dos_civ/extracted/` (107 .PIC, 38 .PAL, 13 .TXT).

**Methodology**:
- Phase 1 (build): `cmake --build build -j` → OK, `--test` full suite: ALL PASS.
- Phase 2 (pixel diff): used the binary's `--shotTitle/--shotIntro/--shotMenu/--shotWorld/--shotCity` (640×480 PPM dumps) and `--pic <f.PIC>` (native 320×200 PPM of DOS art) plus Pillow/numpy ROI compare.
- Phase 3 (text audit): grep for English literals in `src/game/*.cpp`, cross-referenced `assets/zh_TW.json` (283 active entries).
- Phase 4 (layout): compared menu coordinates against C# reference at `~/civ1_cht/OpenCiv1/src/Game/CodeObjects/`.
- Phase 5 (timing): read `CommonTools::F0_1182_0134_WaitTimer` and intro stepping.

**CLI gotcha**: `--shotTitle <out>` returns immediately after parsing — `--assets <dir>` MUST be passed BEFORE `--shotTitle` on the command line, otherwise `assetsDir` is still null when the shot fires.

---

## 1. 像素差異

| Screen | Status | Pixels diff | Max Δ | Note |
|---|---|---|---|---|
| TITLE / LOGO ROI @ (160,20) 320×200 inside 640×480 fb | **PIXEL_PERFECT** | 0 / 64000 | 0 | `--shotTitle --assets …` loads `LOGO.PIC`, blits native, palette matches byte-for-byte |
| INTRO / BIRTH3 ROI @ (160,80) 320×200 inside 640×480 fb (default slide 6) | **PIXEL_PERFECT** | 0 / 64000 | 0 | `MainIntro::nextFrame` cursor=6 → slides[6] = BIRTH3 (cursor 0..2 are LOGO/PLANET1/PLANET2, then BIRTH0..BIRTH8); off-by-3 vs the "BIRTH3 = slide index 6" comment in `main.cpp:2837` is INTENDED but worth a docstring fix |
| MENU (`--shotMenu`) vs same logo backdrop | **DIVERGENT** | ~298 560 / 307 200 ≈ 97% non-LOGO pixels in bottom strip | 255 | `shotMenu()` (`main.cpp:2906`) calls `clear(1)` then draws a free-floating MenuBoxDialog with NO `LOGO.PIC` load — only 6 colours total in the frame. Diverges from DOS where the menu draws over the LOGO/intro residual. `shotTitle` is the correct path. |
| WORLD (`--shotWorld`) | NEAR / partial | 75 colours, dark bg, has minimap | n/a | Real tileset path runs (`flow.setAssetDir`); not directly comparable to a single DOS .PIC because the gameplay world is composed from `TER257.PIC` tiles, not a single screen. ROI vs any single .PIC is meaningless. **Action**: capture a DOS `wine`-running screenshot for reference (currently un-pixel-comparable). |
| CITY (`--shotCity`) | **DIVERGENT** | 64 000 / 64 000 ≈ 100% diff at expected `kBackX=0,kBackY=0` ROI | 255 | `CityView::open()` only loads CBACK when a valid city exists; in `shotCity()` (`main.cpp:2884`) the `cid` lookup loops `unitManagement().cities()` and falls through to `cid = 0` even when the player has none. The chosen city likely has owner==0 (no city, the `cid` defaults to 0 even when `cities()` is empty). Result: `backdrop_` never loads, `draw()` falls back to `fillRect(…, 160)` solid colour over (0,0)..(320,200). Top-left ROI is solid magenta/grey, not CBACK. |
| CBACK.PIC (raw `--pic`) | reference | n/a | n/a | DOS art decodes cleanly to 320×200 with 39 unique colours |
| BIRTH0..BIRTH8, LOGO, CASTLE0, DIFFS, CUSTOM, PLANET1 (raw `--pic`) | reference | n/a | n/a | PicLoader decodes every tested asset at 320×200 |

**Pixel-diff conclusion**: TITLE and INTRO are pixel-exact when `--assets` is provided, demonstrating the rendering pipeline (PicLoader + GBitmap + palette + drawBitmap) is byte-faithful. The CITY shot and the MENU shot are *configuration* divergences, not rendering bugs — the entry points fail to load their backdrop assets at all.

---

## 2. 中文化漏網

### 2A. UNLOCALIZED_LITERAL (literals that reach the screen but cannot be tr()'d as-written)

- `src/game/GameMenus.cpp:73,92,140,144,145,149,153,160-209,222-223,251-252,274-275` — every `\x8f`+hotkey suffix (`" Build Road \x8fr\n"`, `" Sentry \x8fs\n"`, etc.) is concatenated AFTER the translatable label. The C# splits the menu on `\n` and DrawTools routes each LINE through Translator; the line still contains the trailing hotkey marker, so no exact NOR trimmed match in `zh_TW.json` will hit. The Orders/Options menus will render with the English label even when `Translator.enabled == true`.
  - Fix locus: either (a) strip the `\x8fX` suffix BEFORE Translator (split label vs hotkey at menu-build time, re-join after translation) inside MenuBoxDialog, or (b) add full-literal keys including the marker to `zh_TW.json`.
- `src/game/FrontEndFlow.cpp:22,335-336` — STARTING placeholder uses literal `"Quit"` as title and `"Start a New Game"` as message (placeholder reuse). Translator covers them but the *semantic* result is "離開 / 開始新局" — wrong message. Should use a real "starting new game…" string.
- `src/game/CityView.cpp:244` — `std::string ownerName = "Player";` — passed into the panel labels; `"Player"` is not in `zh_TW.json`.
- `src/game/CityView.cpp:283,290` and surrounding lines — label literals `"Population:"`, `"Food:"`, etc. — these reach `drawLabelValue()` which does eventually call `drawString` so they CAN be translated, but they are not in `zh_TW.json`.
- `src/game/MenuBoxDialog.cpp:147` — literal `"(HELP AVAILABLE)"`. Translatable in principle, missing key.
- `src/game/MainCode.cpp:240,242` — `"Pick your tribe..."` literal used both as menu title and as name-dialog title. Not in `zh_TW.json`.
- `src/main.cpp:58` — demo string `"(c) 1991 MicroProse"` reaches the screen via `gd.drawString` (translated chokepoint). Not in `zh_TW.json`.

### 2B. MISSING_TRANSLATION (keys referenced by code, missing from `zh_TW.json`)

73 missing keys identified by structured scan. The high-impact buckets:

- **EARTH** (main menu item; `MainCode.cpp:24`) — present-but-as `"Play on EARTH"` only. DOS uses `" EARTH"` per `MainCode.cs:191`. The Translator will trim and lookup "EARTH"; needs its own key.
- **14 tribe leader/nationality/people triples** (`MainCode.cpp:138-152`): Caesar, Roman, Romans, Hammurabi, Babylonian, Babylonians, Frederick, German, Germans, Ramesses, Egyptian, Egyptians, Abe Lincoln, American, Americans, Alexander, Greek, Greeks, M.Gandhi, Indian, Indians, Stalin, Russian, Russians, Shaka, Zulu, Zulus, Napoleon, French, Montezuma, Aztec, Aztecs, Mao Tse Tung, Chinese, Elizabeth I, English, Genghis Khan, Mongol, Mongols.
- **GameMenus Options submenu**: Options:, Instant Advice, AutoSave, End of Turn, Animations, Sound, Enemy Moves, Encyclopedia Text, Palace, Debug saves.
- **GameMenus Orders submenu**: No Orders, Add to City, Found New City, Build RailRoad, Build Mines, Clean up Pollution, Build Fortress, Wait, Sentry, GoTo, Pillage, Home City, Unload, Disband Unit.
- **TerrainTiles** (`TerrainTiles.cpp:39-51`): Tundra, Arctic, Swamp, Jungle, River.
- Plus: `Pick your tribe...`, `Player`, `(HELP AVAILABLE)`, `(c) 1991 MicroProse`.

(Tech names from `TechResearch.cpp` and many city/terrain names ARE present in `zh_TW.json` already — Alphabet/Pottery/Writing/Currency/Iron Working/Monarchy/Democracy/Construction/Mathematics/Feudalism/Gunpowder/Metallurgy/Map Making/Mysticism plus Ocean/Grassland/Plains/Forest/Hills/Mountains/Desert all hit.)

### 2C. Staged-but-unreachable (NOT stale, NOT a bug)

`zh_TW.json` has 80+ entries for CodeObjects that exist in the C# (`Overlay_22.cs`, `CityWorker.cs`, `MeetWithKing.cs`, `Encyclopedia.cs`, `HallOfFame.cs`, `GameReplay.cs`, etc.) but are NOT yet ported to C++. Examples: `"Charles De Gaulle"`, `"CIVIL DISORDER"`, `"Establish Embassy"`, `"Will you?_Keep moving_Establish trade route"`, `"Your civilization has conquered the entire planet!"`. These will become reachable as more CodeObjects are ported; they are correctly translated and should stay.

### 2D. Translation chokepoint OK

`GDriver::drawString(int, GFont&, x, y, text, color)` and `GDriver::drawString(CRectangle&, x, y, text)` both pass through `Translator::instance().translate(text)` (`GDriver.h:141, 161`). `getDrawStringSize` measures the TRANSLATED string for correct CJK widths (`GDriver.h:156`). `Translator::translate` does exact then `trim()`'d fallback so leading-space DOS-style items like `" Tax Rate"` correctly hit `"Tax Rate"` in `zh_TW.json` (`Translator.cpp:144-148`). This chokepoint is correct; the gaps above are payload, not pipeline.

---

## 3. 排版偏移

### 3A. FrontEndFlow vs MainCode inconsistency (CRITICAL)

There are **two divergent main-menu definitions**:

| | DOS (C# `MainCode.cs:191`) | openciv1pp `MainCode.cpp:21-28` | openciv1pp `FrontEndFlow.cpp:22-23` |
|---|---|---|---|
| Item 0 | `" Start a New Game"` | `"Start a New Game"` | `"Start a New Game"` |
| Item 1 | `" Load a Saved Game"` | `"Load a Saved Game"` | `"Load a Saved Game"` |
| Item 2 | `" EARTH"` | `"EARTH"` | **`"Play on EARTH"`** |
| Item 3 | `" Customize World"` | `"Customize World"` | `"Customize World"` |
| Item 4 | `" View Hall of Fame"` | `"View Hall of Fame"` | `"View Hall of Fame"` |
| Item 5 | (none — 5 items) | (none — 5 items) | **`"Quit"`** |
| Coordinates | `(100, 140)` on 320×200 | `(220, 250)` on 640×480 | `(60, 40)` on 640×480 |
| windowFrame | true | true | true |

- `MainCode.cpp` is the authentic port (matches DOS items + intended FOV scaling). `FrontEndFlow.cpp` is the stub used by `--menu`/`--menuflow`/most `--*test` modes and by `shotMenu`.
- **Action**: have `FrontEndFlow::mainMenuItems()` delegate to `MainCode::mainMenuItems()` instead of duplicating with its own 6-entry list; have `FrontEndFlow::draw()` State::MAIN_MENU call `mainCode().F0_11a8_0486_LogoAndMainGameMenu(...)` like State::TITLE does; remove the (60,40) hardcode.

### 3B. Difficulty menu

- DOS: `(160, 35)` on 320×200, `windowFrame=false`, first item `"Difficulty Level..."` is a TITLE row (not selectable), then 5 difficulty options with leading-space (`StartGameMenu.cs:line "Difficulty Level...\n Chieftain (easiest)..."`).
- openciv1pp `MainCode.cpp:200`: `(240, 70)` on 640×480, `windowFrame=true`, first item title included — close to faithful, but the windowFrame flip changes the visual frame and the C# false-frame path uses `backgroundColor = getPixel(screen, x, y)` (transparent over the existing screen), whereas openciv1pp's true-frame path fills colour 7 and outlines black.
- openciv1pp `FrontEndFlow.cpp:302`: `(60, 40)` on 640×480, `windowFrame=true`, NO title row.
- **Action**: same as 3A — route DIFFICULTY through `MainCode::F0_11a8_087c_NewGameMenu` and reproduce DOS's `windowFrame=false` background-sampling behaviour.

### 3C. ShotMenu does NOT load LOGO.PIC

- `shotMenu()` (`main.cpp:2906-2923`) calls `clear(1)` then `MenuBoxDialog::F0_2d05_0031_ShowMenuBox(items, 30, 20, true, false)`. Result: 6-colour solid blue background with a small box at (30, 20). 100% diff vs `shotTitle`'s LOGO region.
- DOS parity is `shotTitle` (which DOES load LOGO and draw the menu at MainCode's (220, 250)). The `shotMenu` mode is effectively a no-context smoke test; either delete it or make it route through `MainCode::F0_11a8_0486_LogoAndMainGameMenu` so it parallels `shotTitle`.

### 3D. CityView backdrop

- `CityView::draw` correctly attempts `backdrop_` at (kBackX=0, kBackY=0), but `CityView::open` lazy-loads CBACK only on the FIRST call. `shotCity()` builds a fresh OpenCiv1Game, founds AI cities via `processEndOfTurn`, then calls `cityView().open(cid)` where `cid` defaults to 0. If `cities()[0].owner == 0` (no city actually founded by the player path), the open succeeds with the dummy zeroth slot and `backdrop_` never loads (no city → conditional in `open()` returns false before the load block? — verify `CityView.cpp:25-27`: `if (cityId < 0 || std::size_t(cityId) >= cities.size()) return false;` — so the load only fails if the cities array is empty).
- Empirical: shotCity result has 100% diff at (0,0)..(320,200) ROI vs CBACK.PIC, suggesting either the `for(nm : {"CBACK.PIC", ...})` loop never executed (cities() empty so `open()` returned early before reaching the loader) OR `loadPicFile` returned null. The condition needs verification with a debug print.
- **Action**: in `shotCity()` (`main.cpp:2884`), confirm a city exists before calling `open()`; if AI fails to found a city, fall back to manually instantiating one with terrain=Grassland so the backdrop loader runs.

### 3E. Translator key trailing-marker problem

- Items like `" No Orders \x8fspace"` (built in `GameMenus.cpp:140`) reach Translator with the `\x8f` marker attached → no match. The DOS C# has the SAME concatenated form (`GameMenus.cs:386` builds with `\x8f` too) but the original game never localized — it always rendered English. For a faithful Chinese port, MenuBoxDialog must split label vs hotkey, translate the label, then re-join — OR the translation table must include marker-suffixed keys (impractical).
- **Action**: in `MenuBoxDialog.cpp` (after `splitMenuItems`, before `getDrawStringSize`), detect a final ` \x8f<char>` suffix, peel it off into `optionChars`, translate the label, then draw label-then-hotkey.

---

## 4. 動畫/時序

### 4A. WaitTimer is a no-op

- C# `F0_1182_0134_WaitTimer(waitTime)` sleeps `max(waitTime*12, 1)` ms then DoEvents. openciv1pp (`CommonTools.cpp:138-143`) explicitly stubs the sleep — `(void)waitTime; F0_1000_033e_ResetWaitTimer();` — every wait is 0 ms.
- Specific divergences from `~/civ1_cht/OpenCiv1/src/Game/CodeObjects/MapInitAndIntro.cs`:
  - `MapInitAndIntro.cs:1341` `WaitTimer(5)` → 60 ms in DOS, **0 ms** in openciv1pp.
  - `MapInitAndIntro.cs:1367` `WaitTimer(10)` → 120 ms in DOS, **0 ms**.
  - `MapInitAndIntro.cs:1373` `WaitTimer(5)` → 60 ms, **0 ms**.
  - `MapInitAndIntro.cs:1459` `WaitTimer(180)` → 2160 ms (long pause), **0 ms**.
- **Status**: TIMING_DIVERGENT. Acceptable for headless tests; visually breaks interactive title/intro pacing.
- **Action**: in `--menu`, `--menuflow`, `--intro` interactive paths, wire `WaitTimer` to `SDL_Delay(waitTime * 12)` (gated by an `is-interactive` flag so headless tests stay fast).

### 4B. Palette-cycle slot

- `CommonTools::F0_1000_044a_CyclePaletteTimer` advances one position per tick, gated by `speedCount` per slot. The `speed` field comes from the C#; the port preserves the gating correctly (verified by `--commontest`). NOT a divergence.

### 4C. MainIntro stepping

- `MainIntro::nextFrame` walks slides 0..12 (LOGO, PLANET1, PLANET2, BIRTH0..BIRTH8, LOGO-credits-stub) one per call. C# `F2_0000_0bd7` loops and waits `WaitTimer(5)` per palette-fade step. The port has no fade and no per-slide pause. Headless `--shotIntro` is fine; interactive intro flips slides instantly.
- **Status**: TIMING_DIVERGENT for the per-slide hold AND the per-row palette fade (which `F0_1000_04d4_TransformPaletteToColor` mediates — verify if ported in CommonTools).

### 4D. PlayTune / Sound timer

- `MainCode.cpp:103-104` openly elides `PlayTune(1, 0)`. DOS plays a chime after the menu selection. Not a visual divergence; the report flags it for completeness.

---

## 5. 修補優先順序

### 高 (high — blocks correctness, visually obvious, small diff)

1. **Unify the two main-menu definitions** (`FrontEndFlow.cpp:21-24, 285-300`). Delete FrontEndFlow's hardcoded items and route `State::MAIN_MENU` through `MainCode::mainMenuItems()` + `MainCode::F0_11a8_0486_LogoAndMainGameMenu()`. This single change fixes: extra "Quit" item, wrong "Play on EARTH" string, wrong (60,40) menu coordinates, and missing LOGO.PIC backdrop in `--menu`/`--menuflow`. ETA: 30 min.
2. **MenuBoxDialog hotkey-marker split** (`MenuBoxDialog.cpp` around lines 80-105 + 156-184). Peel `\x8f<char>` suffix before measure/translate/draw; render the localized label, then the hotkey glyph. Unlocks ALL Orders/Options/Game/World/Advisor/Encyclopedia menu localizations at once. ETA: 1-2 hours.
3. **Add the 14 tribe leader/nationality/people triples + the 30 in-game menu items to `assets/zh_TW.json`**. After fix #2 lands, these become reachable. ETA: copy from the existing `~/civ1_cht/dos_civ/extracted/CIV.EXE` Big5 patch payload (Phase 3 Batch A in `civ1_cht`'s memory) — already translated there.
4. **shotMenu loads LOGO** (`main.cpp:2906-2923`). Replace the bespoke `clear(1) + ShowMenuBox(...,30,20)` with a call to `MainCode::F0_11a8_0486_LogoAndMainGameMenu(0, nullptr)`. ETA: 5 min.
5. **shotCity ensures a city exists** (`main.cpp:2884`). After `processEndOfTurn`, assert `unitManagement().cities().size() > 0` AND `cities()[0].owner > 0`; otherwise manually plant a city so CBACK loads. ETA: 15 min.

### 中 (medium — correctness drift, not blocking the docs comparison)

6. **`STARTING` state placeholder strings** (`FrontEndFlow.cpp:335-336`). Use proper "starting new game" message keys (and translate them).
7. **Translation key gaps**: `"EARTH"` (separate from `"Play on EARTH"`), `"Pick your tribe..."`, `"Player"`, `"(HELP AVAILABLE)"`, `"(c) 1991 MicroProse"`, `"Population:"`, `"Food:"`. ETA: 10 min to add.
8. **Difficulty menu windowFrame flip** (`MainCode.cpp:200`). DOS uses `windowFrame=false` (transparent over the current screen). The current `true` adds a fill+border that diverges from the DOS look.
9. **TerrainTiles English literals** (`TerrainTiles.cpp:39-51`) — add Tundra/Arctic/Swamp/Jungle/River to `zh_TW.json`. ETA: 5 min.
10. **CityView open() backdrop fallback when CBACK.PIC missing**: emit a coloured-rect placeholder with the CBACK palette installed, so the panel below renders against a known palette instead of bleeding the VGA default.

### 低 (low — polish / interactive-only)

11. **WaitTimer is no-op** (`CommonTools.cpp:138-143`). Add an `interactive` mode flag (or a delegate) so headless tests stay 0 ms but `--menu`/`--menuflow`/`--intro` use `SDL_Delay(waitTime*12)`. Per-slide hold + palette fade in `MainIntro` benefits.
12. **Comment fix**: `main.cpp:2837` reads "default BIRTH3 = index 6" — the slides list places BIRTH3 at index 6 because the leading 3 are LOGO/PLANET1/PLANET2. Either rename the comment to "default slide index 6 (= BIRTH3 after LOGO/PLANET1/PLANET2)" or expose a named alias.
13. **Stale-key linter**: add a CI check that warns when `zh_TW.json` keys appear without any matching source literal (after stripping `_`-prefixed metadata). The 80+ "staged" entries listed in §2C are NOT stale per se — they're pre-translated for unported CodeObjects — so the linter should be advisory, not failing.

---

## Appendix A: build/test snapshot

```
cmake --build build -j     → Built target openciv1pp [100%]
./build/openciv1pp --test  → SELFTEST/RESTEST/GFXTEST/GDTEST/COMPOSITETEST/PALTEST/
                              DRAWTEST/IMGTEST/LANGTEST/TXTTEST/MENUTEST/NAVTEST/
                              COMMONTEST/TEXTBOXTEST/FLOWTEST/GAMEMENUTEST: ALL PASS
./build/openciv1pp --assets <DOS> --shotTitle  → logo=real, 0/64000 ROI diff vs LOGO.PIC
./build/openciv1pp --assets <DOS> --shotIntro  → slide 6 = BIRTH3, 0/64000 ROI diff
./build/openciv1pp --assets <DOS> --shotCity   → 100% ROI diff (CBACK not loaded)
./build/openciv1pp --assets <DOS> --shotMenu   → no LOGO backdrop (6 unique colours)
./build/openciv1pp --assets <DOS> --shotWorld  → 75 colours, valid render, not single-PIC comparable
```

## Appendix B: source locations for the 11 fixes

| Fix | Files to touch |
|---|---|
| 1. Unify main menu | `src/game/FrontEndFlow.cpp:20-24, 39-49, 275-309, 285` |
| 2. Hotkey split    | `src/game/MenuBoxDialog.cpp:10-28 (splitMenuItems), 80-105, 156-184` |
| 3. Add translations | `assets/zh_TW.json` (43 new keys) |
| 4. shotMenu wired   | `src/main.cpp:2906-2923` |
| 5. shotCity safe    | `src/main.cpp:2884-2903`; `src/game/CityView.cpp:25-51` |
| 6. STARTING strings | `src/game/FrontEndFlow.cpp:327-339` |
| 7. UI label keys    | `assets/zh_TW.json` |
| 8. Difficulty frame | `src/game/MainCode.cpp:189-201` |
| 9. Terrain keys     | `assets/zh_TW.json`; `src/game/TerrainTiles.cpp:39-51` |
| 10. CityView fallback | `src/game/CityView.cpp:180-189` |
| 11. WaitTimer        | `src/game/CommonTools.cpp:138-143` + an interactive flag |

## Appendix C: NOT changed (worktree audit only)

This report is read-only. Nothing in `/home/anr2/civ1_cht/civ1_cht/openciv1pp/src/` or `assets/` was modified. The worktree at `.claude/worktrees/agent-a41795403bf5c1f5d` is for measurement only; the only file written under the main repo is THIS report at `docs/DOS_PARITY_REPORT.md`.
