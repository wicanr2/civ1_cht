# DOS Civ1 Playthrough Notes (2026-06-03)

**Source captures**: `docs/screenshots/dos_play/01_*.png` … `42_*.png` — 60 PNGs
recorded by `docker/recording/dos_playthrough.sh` running inside the
`openciv1pp-recorder:latest` container with `--network none`, `DISPLAY=:99`,
`--tmpfs /tmp/.X11-unix`, dosbox driving the original `CIV.EXE` from
`/home/anr2/civ1_cht/dos_civ/CIVILIZATION.zip`. Host X server (`:1`) was
verified untouched throughout the run.

**openciv1pp baseline**: branch `track-b-cpp-sdl2-port` (per
`docs/DOS_PARITY_REPORT_R2.md`); static-screen parity already reaches
PIXEL_PERFECT on TITLE/INTRO and CONVERGED on MENU/CITY. The gap this report
addresses is the *interactive flow* — how DOS Civ1 *feels* across boot,
new-game setup, world-gen, and the first turns — and what openciv1pp does not
yet replicate.

---

## 1. Summary

DOS Civ1 has a very particular onboarding shape that nothing else in the
genre uses today: a **two-stage hardware preflight** (text-mode setup banner
on a blue panel + four numbered prompts), then a **cinematic creation myth**
(galaxy → "the Earth was without form, and void." → continents form → fire
→ tools → 7/13 vignettes), then a **declarative menu cascade**
(`Difficulty → Civilizations → Tribe → Name`) where each step paints a new
column of "decision metaphor" portraits on the left while the right column
holds the choice list. Once in-game, the very first turn shows a **labelled
schematic overlay** identifying every UI region (Map Window, Menu Bar,
Active Unit, terrain types) — a tutorial that fires exactly once and then
never again. Tutorial hints continue to pop up as a green
`--- CIVILIZATION NOTE ---` banner whenever the player encounters a new
mechanic for the first time.

openciv1pp today reproduces the **art** of these screens nearly pixel-perfect
(TITLE/INTRO/MENU/CITY ROIs hit 0-pixel diff in R2) but does **not**
reproduce the *flow*: the boot-setup screen is missing entirely, the
new-game wizard is a single-list flow rather than the DOS cascade with
left-column portrait stack, the first-turn schematic-overlay tutorial is
absent, and the in-context green `CIVILIZATION NOTE` banner has no analog.
The captured DOS playthrough also exposes timing quirks (e.g. the `4000 BC`
year holds for several "press-Enter" pulses until the Settler actually
moves, then ticks to 3960 BC, 3920 BC, etc.) and input quirks (Settlers'
"Build City" is `b` — the binding worked in DOS at screenshot `35_b_settle`,
but only after the unit had moves) that openciv1pp's current turn loop does
not visibly model.

---

## 2. DOS UI flow (captured order)

| # | Phase | Frames | Key DOS observation |
|---|---|---|---|
| A | Splash + graphics select | `01_initial_boot`, `02_graphics_prompt` | Blue title panel `CIVILIZATION (TM) - Version 474.03 / From those civilized guys at MPS Labs.` over BLACK background. Single-line text prompt below: `Select graphics mode: _` with numbered list 1) VGA (256 color) … 4) Tandy 1000. Cursor blinks at end of `:` prompt. |
| B | Sound select | `04_sound_prompt`, `05_sound_none` | Same banner stays; prompt text swaps to `Select sound mode:` with **six** options 1) No sounds please 2) IBM sounds 3) Tandy sounds 4) AdLib/Sound Blaster 5) Roland MT-32 MIDI board 6) Custom sound driver. The choice persists on screen as it's typed. |
| C | Input device | `06_input_prompt`, `07_input_kb` | `1. Mouse and Keyboard / 2. Keyboard only` — only two options, no banner above the list (banner is overwritten). After acceptance the screen clears to BLACK. |
| D | Cinematic intro | `08–12_intro_*` | Loads to a STARRY GALAXY backdrop (red-pink swirl + asteroids). Text appears centred-bottom in CYAN: `the Earth was without form,` then `and void.` paced ~2s apart. Per `polish_intro_birth_zh.png` reference, DOS proceeds through 13 vignettes (`7/13` shown in openciv1pp), each one a different evolution image (galaxy → continents → fire → tools …). Space/Return skips. |
| E | Title screen | `13_after_space`, `14_after_space2`, `15_title_or_menu` | `Sid Meier's CIVILIZATION` logo (gold-red 3D letters) on BLACK background, NO menu box yet. Pressing Return brings up the menu over the same logo. |
| F | Main menu | `16_after_return` | Same title screen + tan/brown bordered list box: `Start a New Game / Load a Saved Game / EARTH / Customize World / View Hall of Fame`. Top item is highlighted in dark-on-tan inverted bar. |
| G | Difficulty | `19_difficulty`, `20_diff_picked` | **Five portrait thumbnails stacked vertically along the LEFT edge** (caveman, viking, knight, king-on-horse, emperor-on-throne) + a YELLOW-bordered list box on the right: `Difficulty Level… / Chieftain (easiest) / Warlord / Prince / King / Emperor (toughest)`. Selection highlight is cyan-on-blue. |
| H | Civilizations | `21_num_civs`, `22_num_picked` | The chosen difficulty portrait **stays on the left**; the right box title becomes `Level of Competition…` with `7 Civilizations / 6 / 5 / 4 / 3`. Visual continuity: only one portrait now, in the slot that matches your difficulty pick. |
| I | Tribe pick | `23_tribe_select`, `24_tribe_picked` | The portrait card now has a **stacked-pile drop shadow** (multiple yellow card-outlines fanned slightly) indicating "tribes available". Right box: `Pick your tribe…` with 14 tribes (Roman, Babylonian, German, Egyptian, American, Greek, Indian, Russian, Zulu, French, Aztec, Chinese, English, Mongol). |
| J | Leader name | `25_name_entry` | The left portrait now shows the *chosen tribe's portrait* (in our run: Elizabeth I, the English) with the word `English` painted underneath. Right box: `Your Name…` with a single-line cyan-bordered text field pre-filled with the canonical historical leader name (Elizabeth I). Cursor blinks on first character. |
| K | Birth narration | `36_city_founded` (mis-named — this is the leader-birth card) | Full-screen WHITE background framed by a **Greek-key meander** in green/red. Portrait at top centre, body text below: `blizabeth I, you have risen / to become leader of the / English. May your reign / be long and prosperous. / The English have knowledge / of Irrigation, Mining, / Bronze Working, Iron Working, / and Roads.` Notes: (a) DOS lower-cases the first letter when text-entered name is empty/canonical, (b) starting techs vary by tribe (English get Bronze/Iron Working). |
| L | First in-game view (TUTORIAL OVERLAY) | `37_end_turn_1_a`, `38_end_turn_1_b` | **One-shot schematic** that labels every UI region with white callout boxes connected by white lines to actual UI elements: `Menu Bar` (top — GAME ORDERS ADVISORS WORLD CIVILOPEDIA), `Map Window` (top-left mini-map), `Active Unit` (left side-bar text block), terrain labels `Ocean / Plains / Forest / Desert / Plains / Grassland / Grassland` arranged around the visible map tile. Side-bar text: `4000 BC ♥ / 50€ 0.5.5 / English / Settlers / Moves: 0 / NONE / (Plains)`. The Settler tile is highlighted with a pink/magenta selection box. |
| M | Tutorial CIV-NOTE banner | `39_loop_1_a` | A green-on-cyan banner appears OVER the map: `--- CIVILIZATION NOTE --- / YOUR FIRST TASK IS TO FIND A SUITABLE / SITE AND FOUND YOUR CAPITAL CITY. / THE BEST CITY SITES ARE NEAR RIVERS, / GRASSLANDS, COASTAL AREAS, OR AREAS / NEAR SPECIAL RESOURCES.` (Note: this appears only after the player presses Return for the first time — i.e. the tutorial overlay is replaced by a tactical hint.) Side-bar `Moves: 1` indicates the turn started. |
| N | Subsequent turns | `39_loop_*`, `42_turn_*` | After the CIV-NOTE banner is dismissed, the in-game UI settles into its normal state: side-bar with `3960 BC`, `3920 BC` ticking down per turn. The tile is no longer outlined in pink (selection cleared). The right two-thirds of the screen is BLACK because only one tile is explored — the rest of the world is fog. |
| O | F10 / game menu | `40_F10_menu` | F10 dimmed (or blanked) most of the screen to nearly all BLACK with only a tiny `green + magenta` smudge near centre (Settler tile, the only revealed map). This suggests DOS Civ1 either pops a **modal** menu we didn't see (because xdotool sent F10 to dosbox which DOSBox might have eaten as fullscreen-toggle/menu-hotkey), or the game intentionally blanks the screen. `41_after_esc` recovers to the normal in-game view, confirming the blank was a modal overlay. |

---

## 3. Per-screen comparison vs openciv1pp

Classifications: **MATCH** = visually & flow identical, **CLOSE** = art matches
but flow/timing differs, **DIVERGENT** = different layout or content,
**MISSING** = no analog in openciv1pp today.

| DOS screen | openciv1pp current state | Class | Suggested change |
|---|---|---|---|
| Splash blue banner + graphics select (`01–03`) | None — openciv1pp starts directly into the SDL window at title art | **MISSING** | Optional pre-window banner is anachronistic, but adding a `--retroBoot` flag that paints the blue-banner text-mode screen for ~2s before SDL grabs the screen would set the period mood. Skip if no time. |
| Sound mode select (`04–05`) | None | **MISSING** | Same as above; or fold into a one-time first-launch config screen mimicking the DOS palette. |
| Input device select (`06–07`) | None — SDL2 always accepts both mouse and keyboard | **MISSING** | Skip — not worth the visual cost. |
| Cinematic intro (`08–12`) | `polish_intro_birth_zh.png` — the per-vignette page (`學會用火... 7/13`) is implemented | **CLOSE** | DOS shows text **at bottom of the same galaxy image** with no page counter. openciv1pp shows text **below the image** with a `7/13` counter. Move text into image area (bottom band, cyan colour `#5BD8FF` per DOS palette) and drop the page counter (or hide it behind a `--showPageNumbers` debug flag). |
| Title screen pre-menu (`13–15`) | `polish_title_zh.png` — title is on a BLUE background w/ centred black-rectangle for the logo | **DIVERGENT** | DOS title screen has BLACK background with the gold logo floating; openciv1pp wraps the logo in a black inset on blue. Change background to solid BLACK to match DOS. |
| Main menu (`16`) | `polish_menu_zh.png` — tan-bordered menu box positioned BELOW the logo, items: `開始新局 / 載入存檔 / EARTH / 自訂世界 / 名人榜` | **MATCH** (translation done) | Already pixel-perfect art per R2 report. Verify the highlight bar is dark-on-tan inverted (not cyan-on-blue from later screens) — DOS uses *different* highlight colours on main-menu vs sub-menus. |
| Difficulty cascade (`19–20`) | Unknown / not in captured screenshots; codebase grep shows `CheckPlayerTurn.cpp`, `CityView.cpp` but no `DifficultyScreen.*` | **MISSING (flow)** | Implement the **left-column portrait + right-column list** layout. Five portraits stacked: caveman/viking/knight/king/emperor at exact DOS y-positions. Yellow border (color 14) on the list box. Selection highlight: cyan text on solid blue (color 1) bar. Title: `難度… / 部落酋長(最易) / 軍閥 / 王子 / 國王 / 皇帝(最難)`. |
| Civilizations count (`21–22`) | None visible | **MISSING** | Reuse the same right-side list box layout from Difficulty. Left side: keep ONLY the portrait of the difficulty chosen (visual continuity matters). Options: 3-7 civilizations. Title: `競爭等級…`. |
| Tribe pick (`23–24`) | None visible | **MISSING** | Same layout again, but the left portrait card becomes a **stacked pile** (4-5 cards fanned with slight offsets). Right box title: `挑選部族…` with 14 entries. The stack effect signals "many tribes to choose from" — important visual cue. |
| Leader name entry (`25–26`) | None visible | **MISSING** | After tribe pick, the left portrait swaps to the tribe's canonical leader and the tribe name appears underneath in white text. Right side: `您的姓名…` text-entry box pre-filled with canonical name (Augustus for Roman, Elizabeth I for English, etc.). Cyan border on input box. **Bug we hit**: xdotool's `type` did not deliver into this field — likely because the input box uses BIOS keyboard polling rather than the X event queue. Worth checking that openciv1pp uses SDL2 `SDL_StartTextInput()` / `SDL_TEXTINPUT` events here so xdotool & IME both work. |
| Birth narration (`36_city_founded`) | None visible — but the existing `polish_intro_birth_zh.png` is on a DIFFERENT screen (cosmic vignette, not the leader-birth card) | **MISSING** | Greek-key meander border (green outer, red inner — implement as a tiled 8x8 pattern bitmap). Portrait centred top. Body text in monospace black on white, line-broken to ~24 chars/line. Tribe-specific starting tech list. Reference DOS art: `BIRTH0.PIC`/`BIRTH1.PIC` already in `dos_civ/extracted/`. |
| World generation animation (`27–29`) | `multi_civ_world_zh.png` + `realgen_civ1_world_zh.png` already implemented | **CLOSE** | DOS captures show the world-gen produces a BLACK screen with single highlighted tile (i.e. heavily fog-of-war'd from the start). openciv1pp's `multi_civ_world_zh.png` shows the **whole world** revealed in test mode — make sure the actual production flow only shows the starting tile area (3-5 tiles around the Settler). Add the fog-of-war black-out for unexplored areas. |
| First-view TUTORIAL OVERLAY (`37–38`) | None — openciv1pp jumps straight to game | **MISSING (HIGH PRIORITY)** | Implement a `FirstSessionTutorialOverlay` that draws white callout boxes connected by white lines to UI regions: top-bar (`GAME ORDERS ADVISORS WORLD CIVILOPEDIA`), mini-map, side-panel, active-unit text. Trigger condition: `firstGameEver` flag in settings file; dismiss on any key. This is the **single most iconic Civ1 onboarding moment** and the most "feel" wins per implementation hour. |
| CIVILIZATION NOTE banner (`39_loop_1_a`) | None | **MISSING (HIGH PRIORITY)** | A reusable modal text banner: green (color 10) background, cyan (color 11) border, white text. Triggered on context-specific firsts (first turn → "find a suitable site", first city → "produce a unit", first encounter → "diplomacy", etc.). Each banner has a unique ID stored in save file so it never re-fires. DOS Civ1's text strings are in `CIV.EXE` data segment; could be extracted with `strings CIV.EXE`. |
| Side-bar HUD (`37` left side) | `polish_world_zh.png` bottom strip — different layout: horizontal bottom strip in CJK | **DIVERGENT** | DOS uses a **vertical LEFT-side panel** with mini-map at top, then `4000 BC ♥`, `50€ 0.5.5`, civ name, unit type, `Moves: 0`, current order, terrain. openciv1pp shows the same info in a horizontal bottom strip (`回合 1 年份: 4000 BC 丘陵 拓荒者 ...`). The vertical left-panel layout is the DOS signature; consider switching. Note: R2 report's CityView already uses DOS-coord overlays, so the same machinery applies to PlayMap. |
| Year ticker (`37`→`39`→`42_turn_*`) | Year ticker not shown in captured openciv1pp shots, but `回合 1` is present in `polish_world_zh.png` | **CLOSE** | DOS shows BOTH `4000 BC` and the implicit turn number. openciv1pp shows `回合 1` (turn number) + `年份: 4000 BC`. DOS doesn't show "回合 1" explicitly. Minor. |
| Settler "B" → Build City (`35_b_settle`) | Implementation status unclear from screenshots | **UNKNOWN** | DOS binding: `b` while a Settler is selected → confirm dialog → city is founded. Verify openciv1pp matches this exact binding (not Shift+B, not 'F' for found). |
| F10 game menu (`40`) | Unknown | **UNKNOWN** | Could not verify what DOS pops up under F10 from our capture (DOSBox may have eaten the keystroke). Worth replaying with `xdotool key F1` instead — F1 in DOS Civ1 is the in-game help. |

---

## 4. Animation / timing observations

1. **Hardware setup pacing**: each of the three prompts (`01→04→06`) waits
   for a digit press + Return. No animation, no fade. openciv1pp can skip
   this entirely.

2. **Galaxy intro pacing**: `08→12` show ~2 second pauses between text
   updates (`the Earth was without form,` → `and void.`). The galaxy image
   itself is *static*; it does NOT pan, zoom, or rotate. openciv1pp's
   `polish_intro_birth_zh.png` shows the same static-image-with-text pattern,
   good.

3. **Title screen reveal**: `13_after_space` to `14_after_space2` are
   identical (39636 vs 39685 bytes) — the title logo fades in instantly,
   no progressive draw. The MENU appears on the FIRST Return press without
   any transition (`15_title_or_menu` vs `16_after_return` differ by only a
   border highlight). openciv1pp matches this with `polish_title_zh.png`.

4. **Difficulty → Civilizations transition**: portrait stays in slot,
   right-box swaps content. No fade, no slide. Snap cuts.

5. **Tribe pick stack effect**: the LEFT card on `23_tribe_select` has 4-5
   yellow-bordered shadow lines fanning slightly to the right, suggesting
   a "deck of cards" metaphor. This is a static effect, drawn into the same
   bitmap — not animated.

6. **World-gen during `27→29`**: the screen first shows the leader card,
   then snap-cuts to the in-game view. DOS does NOT animate continents
   forming (that's only the intro). openciv1pp's `multi_civ_world_zh.png`
   shows a fully-generated world — same behaviour.

7. **First turn year holds at 4000 BC**: across `37→38` the side-bar
   shows `4000 BC` even though we pressed Enter. DOS only advances the
   year when **all** units have ended their turn. Pressing Enter on an
   active Settler with `Moves: 0` doesn't end the turn — it just
   acknowledges. By `39_loop_1_a` the side-bar finally says `Moves: 1`,
   meaning DOS auto-restored the Settler's moves on turn rollover.
   The year ticks to 3960 BC by `39_loop_3_b` (3 turns elapsed), then
   3920 BC by `42_turn_6`. **Implication**: openciv1pp's turn loop needs
   to gate year-advance on "all units done OR explicit End Turn", not on
   each Enter press.

8. **CIV-NOTE banner timing**: the banner stays on screen for ~1 second
   after pressing Return; it does not require a separate dismiss. Visible
   in `39_loop_1_a` but gone by `39_loop_1_b`.

9. **Selection box on Settler**: visible as a pink/magenta outline in
   `37` and `38`, gone by `39`. This means DOS draws a selection indicator
   on the active unit ONLY while the unit has pending moves and the player
   has not yet given an order.

---

## 5. Top 10 prioritized fixes for openciv1pp

Ordered by **feel impact ÷ implementation cost**.

1. **Implement the new-game wizard cascade** (Difficulty → Civilizations →
   Tribe → Name) with the **left-column portrait + right-column yellow
   bordered list** layout. This is the most defining UX shape of DOS Civ1's
   onboarding and is currently MISSING. Use 5 difficulty portraits + 14
   tribe portraits already present in `dos_civ/extracted/` (BACK*A.PIC).

2. **Add the first-session TUTORIAL OVERLAY** (`37_end_turn_1_a`).
   White callout boxes pointing at every UI region. Trigger once via a
   `~/.openciv1pp/state.ini` `tutorialShown=0/1` flag. Dismiss on any key.

3. **Add the green CIVILIZATION NOTE banner system**. Reusable modal
   text box: green bg, cyan border, white text. Strings stored in
   `assets/strings/civ_notes_zh_TW.txt`, with EN fallback. Each
   note triggers on a context-specific first-time event.

4. **Fix the turn loop year-advance gate**. Currently any Enter likely
   ticks the year; DOS only ticks when all units' moves are exhausted.
   Add a `ShouldAdvanceYear()` check in `CheckPlayerTurn.cpp`.

5. **Switch in-game HUD to vertical left-side panel**. Mini-map at top,
   then year/treasury, civ name, unit details, current order, terrain
   under cursor. Drop the horizontal bottom-strip layout from
   `polish_world_zh.png`.

6. **Add the leader-birth card** with Greek-key meander border, tribe
   portrait, and tribe-specific starting tech list. Art assets:
   `BIRTH0.PIC`/`BIRTH1.PIC` already extracted.

7. **Implement fog-of-war for fresh games**. The captured DOS world is
   nearly all BLACK around the Settler — only the 3x3 tile area around
   the unit is visible. openciv1pp's playmap should default to fully
   fogged with reveal-radius=1 around units/cities.

8. **Move intro vignette text into the image area**. DOS shows
   `the Earth was without form, and void.` *over* the galaxy image at
   bottom (cyan colour). openciv1pp renders it BELOW the image in
   white. Move text to overlay at y=image_bottom-20 in cyan
   (palette index 11).

9. **Title screen background to solid BLACK**. Current
   `polish_title_zh.png` uses BLUE; DOS uses BLACK. Single-line change
   in TitleScene clear colour.

10. **Verify SDL_StartTextInput is on at the name-entry field**.
    xdotool's `type` failed to deliver into our captured DOS run
    (`Elizabeth I` stayed put instead of becoming `ClaudeAgent`) — DOS
    relies on BIOS keyboard polling and that's fine for it, but
    openciv1pp on SDL2 needs `SDL_StartTextInput()` for both
    automation and IME (relevant for the Traditional Chinese release).

---

## 6. Inventory of captured PNGs

All paths under `/home/anr2/civ1_cht/civ1_cht/openciv1pp/docs/screenshots/dos_play/`.

| File | Phase | Notes |
|---|---|---|
| `01_initial_boot.png` | Splash + graphics select | initial banner + `Select graphics mode:` |
| `02_graphics_prompt.png` | (same) | duplicate frame — pre-input |
| `03_graphics_vga.png` | (same) | `1` typed; cursor advanced past the `1` |
| `04_sound_prompt.png` | Sound select | `Select sound mode:` 6-option list |
| `05_sound_none.png` | (same) | after `1` typed |
| `06_input_prompt.png` | Input device | `1. Mouse and Keyboard / 2. Keyboard only` |
| `07_input_kb.png` | (same) | post-Enter, screen blanked (192 bytes — nearly pure black) |
| `08_intro_start.png` … `12_intro_d.png` | Cinematic intro | starry galaxy; all identical 6361 bytes — text overlay not yet captured at this delay |
| `13_after_space.png` | Title fade in | `Sid Meier's CIVILIZATION` logo on black |
| `14_after_space2.png` | (same) | identical content |
| `15_title_or_menu.png` | Title with cosmic intro behind | `the Earth was without form,` — actually shows the intro text we wanted (sequence is slightly off from `08–12` due to script timing) |
| `16_after_return.png` | Title + intro text `and void.` | identical motif |
| `17_after_1.png` | (same) | `1` keypress while on intro skipped to next vignette |
| `18_after_return2.png` | (same) | continuation |
| `19_difficulty.png` | DIFFICULTY screen | 5 left portraits + right list — first choice highlighted: Chieftain |
| `20_diff_picked.png` | (same) | `3` pressed — list still shows Chieftain highlighted; DOS may have ignored the digit on this prompt |
| `21_num_civs.png` | LEVEL OF COMPETITION | single left portrait + right `7/6/5/4/3 Civilizations` list |
| `22_num_picked.png` | TRIBE PICK (pre-show) | left portrait now has stacked card pile |
| `23_tribe_select.png` | TRIBE PICK | right box `Pick your tribe…` with 14 tribes |
| `24_tribe_picked.png` | (same) | `1` pressed but stayed on Roman highlight (Return needed) |
| `25_name_entry.png` | NAME ENTRY | left card now `English` with Elizabeth I portrait; right box `Your Name… / Elizabeth I` |
| `26_name_typed.png` | (same) | `ClaudeAgent` xdotool-type did NOT make it into the box — see Top-10 #10 |
| `27_worldgen_start.png` | WORLD-GEN bridge | identical to leader card, suggests world-gen was instantaneous |
| `28_worldgen_1.png` … `28_worldgen_6.png` | (same) | nothing animated; world-gen appears done already |
| `29_worldgen_done.png` | (same) | leader card still showing — pressing Return didn't advance |
| `30_first_view.png` | (same) | same frame |
| `31_right.png`, `32_down.png`, `33_left.png`, `34_up.png` | (same) | arrow-key presses did NOT advance from the name-entry screen — confirms input was stuck at name prompt |
| `35_b_settle.png` | NAME field after `b` | the `b` got typed into the name box, lower-casing the leading E → `blizabeth I` |
| `36_city_founded.png` | LEADER-BIRTH CARD | the `Return` after `b` finally advanced — Greek-key border, tribe portrait, body text `blizabeth I, you have risen … of Irrigation, Mining, Bronze Working, Iron Working, and Roads.` |
| `37_end_turn_1_a.png` | TUTORIAL OVERLAY | first in-game view with white callout labels for every UI region |
| `38_end_turn_1_b.png` | (same) | identical |
| `39_loop_1_a.png` | CIV-NOTE banner | green banner: `YOUR FIRST TASK IS TO FIND A SUITABLE SITE …` |
| `39_loop_1_b.png` | banner dismissed | now plain in-game view; `Moves: 1` showing |
| `39_loop_2_a.png` … `39_loop_5_b.png` | turn loop | year stable at 3960 BC, then 3920 BC |
| `40_F10_menu.png` | F10 attempted | screen blanked to nearly all black — DOSBox may have intercepted F10 |
| `41_after_esc.png` | F10 closed | back to normal in-game view |
| `42_turn_6.png` … `42_turn_10.png` | turn loop continued | year still 3920 BC, side-bar unchanged — captures end of session |

---

## 7. Container hardening trace

This run was executed under the mandatory hardening rules to prevent X
leakage to host:

- `docker run --rm --network none --env DISPLAY=:99 --tmpfs /tmp/.X11-unix`
- inside container: `Xvfb :99 -screen 0 1280x720x24 -nolisten tcp +extension RANDR`
- post-launch check confirmed `NetworkMode=none`, `DISPLAY=:99`,
  parent of `Xvfb` and `dosbox` PIDs = the container's bash, not host.
- Host's actual X server (`/usr/lib/xorg/Xorg vt2 ... :1`) was running
  throughout but received no connections from our container.
- Final cleanup: `docker ps -aq | xargs -r docker kill && docker ps -aq | xargs -r docker rm -f`.

No X traffic reached the host display. Script source:
`docker/recording/dos_playthrough.sh`.
