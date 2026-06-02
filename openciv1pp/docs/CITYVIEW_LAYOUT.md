# CityView DOS Layout Reference

## Source
- `~/civ1_cht/OpenCiv1/src/Game/CodeObjects/CityWorker.cs` lines 261-300, 1380-1400, 1817, 2162-2200, 2244-2280, 2477, 2569-2640, 2946-2960, 4727-4940 (literal DOS asm transliteration; every coordinate is from the 1991 binary)
- `~/civ1_cht/dos_civ/extracted/CBACK.PIC` — the city screen background art (320×200, panels BAKED IN)
- `~/civ1_cht/dos_civ/extracted/CITYPIX1.PIC` / `CITYPIX2.PIC` / `CITYPIX3.PIC` — terrain/climate variants for the city map area
- `~/civ1_cht/dos_civ/extracted/POP.PIC` — citizen face sprites for top/citizen row
- `~/civ1_cht/dos_civ/extracted/WONDERS.PIC`, `WONDERS2.PIC` — wonder icons (right panel)

## Native resolution
**320×200**. All coordinates below are DOS pixel coords. The full city screen is `CBACK.PIC` blitted at (0,0); the panel frames and divider lines are **baked into CBACK** — code only fills the dynamic content (text, sprites, bars) in known slots.

## Per-region table (extracted from CityWorker.cs)

| Region | x1,y1 | x2,y2 (or w,h) | Color/Asset | C# source |
|---|---|---|---|---|
| Screen clear | 0,0 | w=320,h=200 | color 0 (black) | `FillRectangle(0,0,320,200,0)` L261 |
| CBACK backdrop blit | 0,0 | w=320,h=200 | CBACK.PIC | (lazy-loaded in CityView.cs) |
| Top header strip (patterned) | 2,1 | 208,21 (end) | pattern | `FillRectangleWithPattern(2,1,208,21)` L264 |
| City name centered | 104,2 | — | white (15) | `DrawCenteredString(name+"(Pop:N)", 104, 2, 15)` L293 |
| Worked tiles area (patterned) | 127,23 | 208,104 (end) | pattern | `FillRectangleWithPattern(127,23,208,104)` L296 |
| Worked tiles grid | each cell (mapX-mapViewX)*16+80, (mapY-mapViewY)*16+8 | 16,16 per cell | TER257 sprites | L302-329; mapViewX=city.X-5, mapViewY=city.Y-3 |
| Worked-tile selection frame | same | 15,15 outline | color 12 (red) | L328-329 |
| Citizen rate indicator row | 95-128 / 129-160 / 161-193 / 194-226 at y=106-114 | 33w × 9h each (4 zones) | text + bg color 9 | L1380-1389 `DrawFilledRectangleWithCenteredText` |
| Citizen face slots (TAX/LUX/SCI) | 33*Var_2496+96, 107 | 32w × 7h | color-swap 9↔15 | L1392 `ReplaceColor` |
| WONDERS label | 190, local+5 | — | white (15) | L1817 `DrawString("WONDERS",190,...,15)` |
| Right panel divider line top | 231,0 → 250,0 | — | color 0/1 | L2162-2165 `DrawLine` |
| Vertical food-storage bar | 309,2 | 8 × 96 | color-swap 14↔12 | L2174 `ReplaceColor(309,2,8,96,...)` |
| "Food Storage" label | 8,108 | — | white (15) | L2244 |
| "City Resources" label | 8, local_fc | — | white (15) | L2276 |
| Resources sub-panel (left) | 2,23 | 122,9 (w×h) | color 1 | L2273 `FillRectangle(2,23,122,9,1)` |
| Supported units list (left-bottom) | 98, i*6+179 | rows of 6px | color 10 | L2477 `DrawString(name, 98, i*6+179, 10)` |
| Right info panel (clear/refresh) | 230,99 | 90 × 100 | color 0 | L2638 `FillRectangle(230,99,90,100,0)` |
| Selection blink bar | 252, (i+offset)*6+3 | 56 × 7 | color 1↔9 | L2946-2956 |
| Unit sprite cells | xPos,yPos | 12 × 14 | color-swap 5↔12 | L4727 `ReplaceColor(xPos,yPos,12,14,5,12)` |
| Building list zone1 (right) | (xSrc&1)*19+161, 100 | 18 × 10 | sprite | L4776 |
| Building list zone2 (right) | (xSrc/5)*19+161, (xSrc%5)*10+50 | 18 × 10 | 5-row stack | L4782 |

## Mapping to 640×480 openciv1pp

Per user directive **像素縮小沒有關係 但是視野範圍要變大**:
- Render the entire DOS city screen at **NATIVE 320×200 in the top-left** of the 640×480 canvas — pixels stay crisp, no upscale.
- The freed **right strip 320..639 × 0..199** and **bottom strip 0..639 × 200..479** are available for an extra Chinese info HUD (label/value rows, building list with Chinese names, etc.) — but the DOS region 0..319 × 0..199 must be **pixel-identical** to DOS.
- The right vertical food bar (309, 2, 8, 96) MUST stay at x=309 — do not push it into the 320..639 strip.

## Divergences from current `src/game/CityView.cpp`

**HIGH severity (DOS-baked layout missing)**
1. We **clear** the canvas to color 160 (custom dark blue) at line 171 before blitting CBACK — this is fine for the right/bottom HUD strips but means the DOS-region (0..319 × 0..199) has our custom palette overlay instead of CBACK's, until CBACK overwrites it. Acceptable, but `copyPaletteFrom(*tileset_)` at line 195 then **clobbers CBACK's palette with TER257's** — this is why the screenshot looks washed-out vs DOS. Fix: install CBACK's palette LAST (after TER257), or install TER257 palette only at the worked-tile slot area.
2. **No city-name centered text at (104, 2)** in DOS coords. Our title at `titleY = kPanelY + 6 = 206` is in the bottom HUD strip, not the DOS top header — visible difference from DOS.
3. **No citizen rate row** (Tax/Lux/Sci indicator) at y=106-114. We don't draw it at all.
4. **No worked-tile grid at DOS coords** (127,23)..(208,104). Our `kGridX = 340, kGridY = 16` puts it in the RIGHT strip, not on top of CBACK.
5. **No vertical food-storage bar at (309, 2, 8, 96)** — completely absent.
6. **No "Food Storage"/"City Resources" labels at (8, 108)/(8, ~118)** in DOS coords.
7. **No supported-units list at (98, i*6+179)** in DOS coords.
8. **No right info panel at (230, 99, 90, 100)** for buildings/production list.

**MEDIUM severity (layout mismatch but functionally ok)**
9. `kGridX=340 kGridY=16` mini-grid is invented for 640×480 — DOS draws on top of CBACK. We should remove this and instead draw real worked tiles at DOS coords.
10. The 280px tall bottom HUD strip with 10 label/value rows is openciv1pp-only (Chinese accessibility). Keep it — but ALSO restore the DOS in-CBACK content above.

**LOW severity (640×480 inventions to KEEP as Chinese HUD)**
11. Bottom panel (0,200,640,280) with Chinese label/value stack — keep, it's the "視野範圍要變大" payoff.
12. `kGridX=340` region — repurpose for a larger Chinese building list or thumbnail grid.

## Fix plan for `src/game/CityView.cpp`

The implementation agent (Task #6) should:

1. **Install CBACK palette LAST** (after TER257, after installCityViewPalette). The CBACK colors must dominate the (0..319, 0..199) region.
2. **Draw city name centered at DOS (104, 2)** with `DrawTools::F0_1182_00b3_DrawCenteredStringToScreen0`-equivalent. Use font 2 (per L273 `Var_aa.FontID = 2`). Text format: `"{NAME} (Pop: {populated})"` — but localize to `{中文城市名} (人口: {N})`. Color 15 (white).
3. **Restore the worked-tile grid at DOS coords (128, 24)..(208, 104)**:
   - For each (dx, dy) in city offset pattern (the 21-tile "fat plus"), compute terrain at (city.x+dx, city.y+dy) and blit TER257 tile 16×16 at ((dx+5)*16+80, (dy+3)*16+8). Actually with mapViewX=city.X-5, mapViewY=city.Y-3, drawing at ((cx-mapViewX)*16+80, (cy-mapViewY)*16+8) where (cx,cy) = (city.x+dx, city.y+dy) reduces to ((dx+5)*16+80, (dy+3)*16+8). The center tile at (dx=0,dy=0) lands at (160, 56).
   - Draw 15×15 red outline (color 12) on tiles being worked.
4. **Draw vertical food-storage bar at (309, 2, 8, 96)**:
   - Background is color 12, filled portion (per food/growthThreshold) is color 14.
5. **Draw "Food Storage" label at (8, 108)** color 15. Localized: "糧倉".
6. **Draw "City Resources" sub-panel at (2, 23, w=122, h=9)** fill color 1, label "City Resources" → "城市資源" at (8, ~28) color 15.
7. **Draw supported-units list at (98, i*6+179)** color 10. Each row is the unit type name (e.g. "Settlers" → "拓荒者") in font 0 or 1 (compact 6px tall). Localized.
8. **Draw right info panel content at (230, 99, 90, 100)** color 0 fill, then list buildings & current production with shield bar. Localized.
9. **Keep** the bottom HUD strip y=200..480 with the existing 10-row Chinese label/value stack — that's the "視野要變大" content. But DROP the title there (DOS title at (104,2) replaces it).
10. **Remove** `kGridX=340 kGridY=16` mini-grid block — DOS grid lives inside CBACK.

## Annotated PNGs

(Generation skipped in this analysis pass — the implementation agent will regenerate `polish_city_zh.png` after the rewrite and we can diff visually then. The DOS reference is already at `docs/screenshots/dos_cback.png`.)

## Estimated effort
+150 / -80 LOC in `CityView.cpp` + ~5 new zh_TW.json entries ("城市資源", "糧倉" / "Food Storage" already present, "Pop:", "Supported Units").
