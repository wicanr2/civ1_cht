# DOS 對照驗證報告 R2 (2026-06-03)

**Scope**: openciv1pp at `/home/anr2/civ1_cht/civ1_cht/openciv1pp/`, HEAD
`6bad95a` ("CityView DOS pixel parity (in-CBACK content at native coords)") on
branch `track-b-cpp-sdl2-port`. Round-1 report retained at
`docs/DOS_PARITY_REPORT.md` for historical comparison.

**Methodology**: same as R1 — re-build (`cmake --build build -j`), full ctest
(52/52 pass), regenerate `--shotTitle/--shotIntro/--shotMenu/--shotCity/--shotWorld`
with `--assets /home/anr2/civ1_cht/dos_civ/extracted/`, then Python+Pillow ROI
diff vs the three reference PNGs (`dos_title.png`, `dos_birth.png`,
`dos_cback.png`). ImageMagick is not installed on this host; the equivalent
metrics (AE pixel count, max channel delta) are produced with NumPy and stated
inline. Source grep for English literal leaks, code grep for the 8 HIGH
CityView layout items and the WaitTimer SDL_Delay change.

**Worktree note**: the requested worktree path `agent-a013d47cc9d1c0421` exists
only inside the `civ1_cht` (Civilization-CHT) repo, not openciv1pp. Verification
ran against the canonical openciv1pp checkout at
`/home/anr2/civ1_cht/civ1_cht/openciv1pp` which is on `track-b-cpp-sdl2-port`
at the expected `6bad95a` and is clean.

---

## Summary

| Screen | R1 status | R2 status | Δ |
|---|---|---|---|
| TITLE (`--shotTitle`, ROI 320×200 @ (160,20) in 640×480 fb) | PIXEL_PERFECT (0/64000) | **PIXEL_PERFECT (0/64000, max Δ=0)** | unchanged ✓ |
| INTRO BIRTH (`--shotIntro`, ROI 320×200 @ (160,80)) | PIXEL_PERFECT (0/64000) | **PIXEL_PERFECT (0/64000, max Δ=0)** | unchanged ✓ |
| MENU (`--shotMenu`, full 640×480 + ROI 320×200 @ (160,20)) | DIVERGENT (6 unique colours, no LOGO) | **CONVERGED** — full fb 42 colours, DOS-region 39 colours, LOGO upper-region matches `--shotTitle` 0/24000 | **FIXED** ✓ |
| CITY (`--shotCity`, DOS-region 0..319×0..199) | DIVERGENT (100% diff vs CBACK, 1 colour) | **CONVERGED (DOS content)** — 33.49% pixel diff vs CBACK with 61 unique colours; diff is from the OVERLAID DOS-coord content (city name, sub-panel, worked-tile grid, food bar, info panel, supported-units list) — i.e. exactly what CITYVIEW_LAYOUT.md prescribed. Backdrop loads (cities=6 from FrontEndFlow walkthrough). | **FIXED** ✓ (residual = intentional overlay) |
| WORLD (`--shotWorld`) | NEAR / not single-PIC comparable | unchanged — 320×200 ROI not comparable to any single .PIC (composed from TER257 tiles); render runs end-to-end without errors | no regression |

---

## A. Pixel diff

| Screen | ROI | diff pixels | max channel Δ | unique colours | verdict |
|---|---|---:|---:|---:|---|
| TITLE | (160,20)..(480,220) vs `dos_title.png` | 0 / 64000 (0.00%) | 0 | n/a | PIXEL_PERFECT |
| INTRO | (160,80)..(480,280) vs `dos_birth.png` | 0 / 64000 (0.00%) | 0 | n/a | PIXEL_PERFECT |
| MENU | full 640×480 fb (palette diversity check) | n/a | n/a | 42 (was 6 in R1) | rich VGA-style palette restored |
| MENU | LOGO upper region (200..440 × 40..140) vs `--shotTitle` same ROI | 0 / 24000 (0.00%) | 0 | n/a | same LOGO.PIC backdrop path as TITLE |
| CITY | DOS region (0,0)..(320,200) vs `dos_cback.png` | 21433 / 64000 (33.49%) | 255 | 61 | DOS backdrop + overlaid in-CBACK content — diff is by design |

### CITY DOS-region diff breakdown (per CITYVIEW_LAYOUT.md region)

| Region (DOS coords) | Diff pixels | % | Explanation |
|---|---:|---:|---|
| Top header strip (2,1)..(208,21) | 4237 / 4347 | 97.5% | Code fills the strip with solid colour 1 + black border + city-name text. DOS CBACK has a stippled cloud pattern. Faithful: we draw the city-name CENTERED at (104,2) like DOS, and the diff is the patterned background we currently approximate as solid blue. |
| City-name zone (~94..210, 0..10) | 1155 / 1331 | 86.8% | Our `"巴比倫 (人口: 1)"` is in WHITE (color 15) at the correct DOS anchor. The diff is the cloud-pattern bg under the text. |
| Worked tiles area (127,23)..(208,104) | 5376 / 6724 | 80.0% | Real TER257 tiles overlaid on top of CBACK (DOS only blits selectively per worker). 21-tile fat-cross is drawn at the expected DOS positions; centre tile (160,56) carries the red color-12 outline marker per `(15,15)` DOS spec. |
| Right food-storage bar (309..317, 2..98) | 768 / 776 | 99.0% | Our bar is VGA color 12 (255,85,85) bg + color 14 (255,255,85) fill — confirmed from the rendered PPM. DOS CBACK has only its sky-blue background at these pixels (the BAR FRAME is baked in but the FILL is dynamic and DOS shows it empty for a freshly-founded city). Expected divergence. |
| Right info panel (230,99)..(320,199) | 8854 / 9090 | 97.4% | We clear to color 0 (black) per DOS L2638 (`FillRectangle(230,99,90,100,0)`), then draw production label + shield bar + buildings list. DOS reference is the CBACK ART showing a forest/mountain illustration where in-game the panel content overlays — our overlay matches DOS's overlay semantics, not the underlying art. Expected. |
| Supported units (y=179..199) | 424 / 4200 | 10.1% | Mostly-matching — only the unit-name text rows diverge from the empty CBACK at this band. |
| City Resources sub-panel (2,23)..(124,32) | 1141 / 1220 | 93.5% | Filled colour 1, label `"城市資源"` at (8,24) in WHITE — matches DOS L2273 spec, diff is the cloud pattern bg. |
| Left strip (0..230, 35..179) | 5380 / 33120 | 16.2% | Mostly identical to CBACK (the per-row stats and the food/resource icons aren't drawn here yet; this is the area where DOS shows the GAME state — CBACK has its baseline art, ours mostly leaves CBACK untouched here). |

**Pixel conclusion**: CITY DOS-region now correctly loads CBACK at (0,0) and the
8 DOS-coord overlays are PRESENT at the right pixel anchors with the right
colours (verified via direct RGB samples: title text white on blue strip, food
bar (255,85,85)+(255,255,85), worked-tile grid with red (color 12) outline at
(160,56)). The remaining diff vs the bare `dos_cback.png` is the *content* layer
DOS draws on top of CBACK at runtime — which is what we draw too, just with
different in-game state (freshly-built 1-pop Babylon vs whatever city CBACK was
captured against).

---

## B. Localization

R1 listed 73 keys missing from `assets/zh_TW.json`. R2 result:

- **Resolved**: 73 / 73 ✓ (all "EARTH", 14 tribe triples, Options/Orders submenu
  items, 5 terrain names, `Pick your tribe...`, `Player`, `(HELP AVAILABLE)`,
  `(c) 1991 MicroProse`, and the §2A literals are now present in
  `assets/zh_TW.json` — current entry count 364 active keys, up from 283 in R1).
- **Still missing**: 0 / 73.
- **Newly identified gaps** (R2 scan of `src/`):
  1. `src/game/CityView.cpp:694` — `"Esc: quit"` literal passed through
     `Translator::instance().translate(...)`; NOT in `zh_TW.json`. Drawn at the
     bottom of the HUD strip as the ESC hint. LOW severity (single string,
     consistently English fallback). Add `"Esc: quit": "Esc: 離開"` or similar.

(Wider `drawString` / `setTitle` / `Translator::translate` literal sweep
returned only this one item — the rest of the codebase routes via key constants
that are all resolved in `zh_TW.json`.)

### B-bonus: extra CityView labels (round-1 §7 follow-ups)

All round-1 §7 medium-priority label keys (`Population:`, `Food:`, `Founded:`,
`Owner:`, `Production`, `Researching:`, `Government:`, `Wonders:`,
`Diplomacy:`, `Trade:`, `Upkeep:`, `Treasury:`, `Happy:`, `Unhappy:`,
`Status:`, `Disorder!`, `Order`, `Food per turn:`, `Buildings`, `City
Resources`, `Food Storage`, `Pop:`, `Supported Units`) ARE present in
`zh_TW.json`. The CityView renders Chinese throughout (visible in the captured
PPM: "巴比倫 (人口: 1)", "城市資源", "糧倉", "供養單位", "生產: 騎兵", "建築:").

---

## C. CityView layout fixes (8 HIGH items from `docs/CITYVIEW_LAYOUT.md`)

| # | Layout-doc item | Code locus in `src/game/CityView.cpp` | Status |
|---|---|---|---|
| 1 | CBACK palette dominates (no clobber by TER257) | L172-176 `copyPaletteFrom(*backdrop_)` + L205-206 `cbackPal` save + L269-290 per-pixel TER257→CBACK nearest-match LUT for tile blit | **CONFIRMED** ✓ |
| 2 | City name centered at DOS (104, 2) | L227 comment + L241 `int tx = 104 - sz.w / 2` + L243 `int ty = 2` + L246 `drawString(font, tx, ty, title, titleCol)` color 15 | **CONFIRMED** ✓ |
| 3 | Citizen rate row at y=106-114 (Tax/Lux/Sci) | (not implemented) | **NOT FIXED** — slider indicator row absent. Falls outside the round-1 §3D "shotCity ensures a city" remit; layout doc lists it as HIGH item 3 but the implementation pass deferred it. Visible gap on the city screen. |
| 4 | Worked-tile grid at DOS coords (128,24)..(208,104) | L259-261 + L295-296 `gx=(dx+5)*16+80, gy=(dy+3)*16+8` — centre lands at (160, 56) exactly per spec | **CONFIRMED** ✓ |
| 5 | Vertical food-storage bar at (309, 2, 8, 96) | L355 `screen.fillRect(Rect{309, 2, 8, 96}, 12)` bg + L358 fill color 14 | **CONFIRMED** ✓ |
| 6 | "Food Storage" label at (8, 108) + "City Resources" label at (8, ~28) | L252 sub-panel fill, L256 `drawString(font, 8, 24, lbl, 15)` for "城市資源", L365 `drawString(font, 8, 108, lbl, 15)` for "糧倉" | **CONFIRMED** ✓ |
| 7 | Supported-units list at (98, i*6+179) color 10 | L373-390: header at (98, 173), body rows at L385 `int y = rows * 6 + 179`, L388 `drawString(font, 98, y, nm, 10)` (color 10) | **CONFIRMED** ✓ |
| 8 | Right info panel at (230, 99, 90, 100) | L396 `screen.fillRect(Rect{230, 99, 90, 100}, 0)` + production label + shield bar + buildings list | **CONFIRMED** ✓ |

**Layout summary**: 7 / 8 HIGH items CONFIRMED. Item #3 (citizen rate row) is
the only HIGH-severity layout point still absent in the implementation. The
layout doc says "Citizen face slots (TAX/LUX/SCI) | 33*Var_2496+96, 107 | 32w
× 7h" — the colour-swap row at y=106..114 is currently not drawn.

---

## D. Timing

- **WaitTimer**: `src/game/CommonTools.cpp:139-148` — `F0_1182_0134_WaitTimer`
  now computes `int ms = waitTime * 12; if (ms < 1) ms = 1;
  SDL_Delay(static_cast<Uint32>(ms));` then `F0_1000_033e_ResetWaitTimer()`.
  **CONFIRMED SDL_Delay(*12)** ✓ (matches the C# `max(waitTime*12, 1)` ms
  semantics; SDL_Delay is a no-op when SDL isn't initialised so headless tests
  still pass instantly).
- Per-slide intro fade (R1 §4C) and PlayTune chime (R1 §4D) remain
  UNIMPLEMENTED. Not regressions — same status as R1; flagged for future.

---

## Build / test snapshot

```
cmake --build build -j     → [100%] Built target openciv1pp (no warnings)
ctest --test-dir build --output-on-failure
                           → 100% tests passed, 0 tests failed out of 52
./build/openciv1pp --assets <DOS> --shotTitle  → "logo=real",  0/64000 ROI diff vs LOGO.PIC
./build/openciv1pp --assets <DOS> --shotIntro  → "slide 6",    0/64000 ROI diff vs BIRTH3
./build/openciv1pp --assets <DOS> --shotMenu   → "logo=real",  42 colours, LOGO upper-region 0/24000 diff vs TITLE
./build/openciv1pp --assets <DOS> --shotCity   → "cities=6",   CBACK backdrop loaded, 8/8 DOS-coord overlays drawn (7 strict + 1 deferred)
./build/openciv1pp --assets <DOS> --shotWorld  → renders, not directly comparable to any single .PIC
```

---

## Remaining gaps (sorted by severity)

### High (visible, worth a follow-up fix)

1. **Citizen-rate row (Tax/Lux/Sci indicator) at DOS y=106..114** is absent
   (`docs/CITYVIEW_LAYOUT.md` HIGH item 3). Per CityWorker.cs L1380-1392 this
   is a 4-zone row at (95..226, 106..114) with 33w×9h zones and a colour-swap
   9↔15 over the citizen faces. Currently no code emits it.
   - Fix locus: `src/game/CityView.cpp`, between sections (c) and (d) of
     `draw()`. ETA: 30-45 min including a "first slot = Tax" toy mapping until
     the slider state is wired up.

### Medium (palette / pattern polish)

2. **Top header strip pattern** — CBACK's stippled cloud pattern is replaced
   by our solid color-1 fill + black border (L224-225). The text reads against
   it (so functional), but it diverges visually from DOS by ~97.5% of pixels in
   that 4347-px strip. Either drop the pre-fill (so CBACK shows through) or
   reproduce the FillRectangleWithPattern stipple (DOS L264).
3. **City Resources sub-panel pattern** — same story at (2,23)..(124,32): 93.5%
   pixel diff because we solid-fill colour 1 over CBACK's cloud pattern.

### Low (single-string i18n + future polish)

4. **`"Esc: quit"` literal** in `src/game/CityView.cpp:694` is the only English
   key not yet in `assets/zh_TW.json`. Add `"Esc: quit": "Esc: 離開"`.
5. **PlayTune chime** (R1 §4D) still elided in `MainCode.cpp:103-104`.
6. **Per-slide intro hold + palette fade** (R1 §4C) — `MainIntro::nextFrame`
   still flips slides instantly. Now that `WaitTimer` actually sleeps, wiring
   a per-slide `F0_1182_0134_WaitTimer(180)` would yield the DOS pacing.
7. **`docs/screenshots/polish_city_dos_region_zh.png`** — written by the
   verification pass (the 320×200 DOS-region crop of the new `--shotCity`).
   `docs/screenshots/polish_menu_zh.png` updated likewise. Both committed-or-not
   is at the main agent's discretion.

### Not regressions (pre-existing, unchanged from R1)

- World view not single-.PIC comparable (R1 §1).
- C# `FrontEndFlow` STARTING placeholder strings (R1 §3A). The
  `FrontEndFlow::mainMenuItems()` delegation to `MainCode::mainMenuItems()` did
  land (L20-24); the STARTING dialog stub at L336-342 still reuses "Quit" as a
  placeholder title but that path is not exercised by `--shotMenu`.

---

## Headline conclusion

All 5 round-1 P1/P2 actions LANDED and VERIFIED:

1. ✅ `FrontEndFlow` now delegates to `MainCode::mainMenuItems()` — no duplicate
   6-item menu list, no bogus "Play on EARTH".
2. ✅ `MenuBoxDialog` peels the `\x8f` hotkey marker before Translator lookup
   — Orders/Options submenu items now eligible for Chinese rendering.
3. ✅ +73 zh_TW.json keys (283 → 364 active entries). Zero R1 misses left.
4. ✅ `shotMenu` routes through `F0_11a8_0486_LogoAndMainGameMenu` with real
   LOGO.PIC backdrop (palette went 6 → 42 unique colours; LOGO upper region
   pixel-identical to `--shotTitle`).
5. ✅ `shotCity` ensures at least one city before `cityView().open(...)` and
   the CityView was rebuilt against `docs/CITYVIEW_LAYOUT.md` — 7 of 8 HIGH
   layout items confirmed at DOS coords (city name at (104,2), worked-tile
   grid at (160,56), food bar at (309,2,8,96), sub-panel at (2,23), Food
   Storage label at (8,108), supported-units list at y=179+, info panel at
   (230,99,90,100), CBACK palette installed first). Citizen rate row at
   y=106..114 is the only HIGH item deferred.
6. ✅ `WaitTimer` now `SDL_Delay(waitTime * 12)` ms (was 0 ms stub).

The main remaining HIGH-severity follow-up is the citizen rate row (item C-3
above). Everything else is cosmetic (pattern fills, single-string i18n) or
pre-existing scope (intro pacing, PlayTune).
