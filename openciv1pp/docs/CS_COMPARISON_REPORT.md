# C# OpenCiv1 vs openciv1pp — side-by-side study

Date: 2026-06-03
Run mode: 100% Docker (`--network none --env DISPLAY=:99 --tmpfs /tmp/.X11-unix`)
Image:    `openciv1-cs-cmp:latest` (built from `docker/cs_compare/Dockerfile`)
Driver:   `docker/cs_compare/compare.sh`

## 1. Build status

### 1.1 openciv1pp (C++/SDL2, this repo)

| step    | result |
|---------|--------|
| cmake configure (Release, container) | OK |
| cmake build (33 CXX TUs)             | OK — single `openciv1pp` binary |
| `--shot{Title,Intro,Menu,City,World}`| OK — five 640x480 PPM → PNG (DOS-faithful 42–131 unique RGB) |

### 1.2 C# OpenCiv1 (`~/civ1_cht/OpenCiv1/`)

| step    | result |
|---------|--------|
| Target  | `net8.0`, WinExe, single csproj (Avalonia 11.3.14 + SkiaSharp 2.88.8) |
| .NET SDK in image | 8.0.421 |
| `dotnet restore`  | OK once NuGet cache pre-warmed at image build (offline-run constraint) — produces NU1605 warning: `Avalonia.Skia 11.3.14 -> SkiaSharp (>= 2.88.9)`, project pins 2.88.8 |
| `dotnet build -c Debug`   | **OK** (1 NU1605 warning, 4 CS0414 unused-field warnings, 0 errors) |
| `dotnet build -c Release` | **FAIL** — `src/Game/OpenCiv1Game.cs(107,54): error CS0103: The name 'Assembly' does not exist in the current context` (the `#else` branch of a `#if DEBUG` block; `using System.Reflection;` missing) |

### 1.3 Reproducible build/restore offline

Because the hardened run uses `--network none`, all NuGet packages are
fetched in a separate prewarm step at **image build** time (where Docker
allows network) using `docker/cs_compare/prewarm.csproj`. The runtime
restore then uses `--source $NUGET_PACKAGES --no-cache`.

## 2. Runtime status (Xvfb :99 inside container)

| step | result |
|---------|--------|
| Xvfb :99 (1280x720x24) | OK |
| `dotnet OpenCiv1.dll` (Debug build) | OK — Avalonia main window appears |
| `xdotool search --name OpenCiv1` | OK — `WID=2097164`, name `OpenCiv1` |
| Window size captured | 644x404 |
| Keyboard injection (`xdotool key`) | OK — `Return`/`Down`/`Escape`/`b` all advance state |
| Software-renderer fallback (`AVALONIA_RENDER_MODE=software`) | Not needed |

The C# Avalonia binary launches and renders inside a containerised Xvfb
without GL acceleration. The same `--network none` flags that block NuGet
do **not** block Xvfb (purely local Unix socket) nor the Avalonia render
loop, confirming the hardening is correctly scoped.

## 3. Per-screen comparison

All PNGs at `docs/cs_compare/`. Composites (PP left | CS right) at
`docs/cs_compare/sidebyside/`.

| screen | openciv1pp (left) | C# OpenCiv1 (right, /work/out/oci_cs) | observations |
|--------|--------------------|--------------------------------------|--------------|
| **Title** | `pp_title.png` — Civ logo + 5-line zh_TW menu (`開始新局 / 載入存檔 / 地球 / 自訂世界 / 名人榜`) at 640x480, 42 unique RGB (DOS palette) | `01_title.png` — **credits roll "HARRY TEASLEY"** in studio fade, 644x404, 24 RGB (mostly black + gold) | C# app boots into the studio credits intro first, not the title menu. We never reach the actual title with the menu inside our 10-step script. PP captures the menu state directly with `--shotMenu`. |
| **Intro** | `polish_intro_birth_zh.png` — civilization "birth" slide with zh_TW caption `學會用火...`, 131 RGB | `04_intro.png` — difficulty picker dialog with **CJK glyphs partially rendering** (`酋長(最易)` style labels visible but overlapping/garbled), portraits, English heading. 186 RGB | C# embeds a zh_TW resource (`Resources/Localization/zh_TW.json`) but glyph layout is broken: characters overlap horizontally, kerning collapses. PP renders CJK cleanly via FreeType MONO. |
| **Menu** | `pp_menu.png` — main start menu, full zh_TW | `05_menu.png` — `Level of Competition...` dialog, **English only** (`7 Civilizations` etc.) | The C# localization layer covers some dialog titles (intro screen) but not the gameplay setup menu. zh_TW JSON is incomplete or not wired through. |
| **World** | `pp_world.png` — DOS-faithful tile map with zh_TW status bar (`回合 1 年份: 3980 BC 丘陵 拓荒者 / 城市:6 文明:8 政府:專制 …`) | `07_loop_2.png` — `Pick your tribe...` dialog (English: Roman/Babylonian/German/…), portraits, **map not yet reached** | The C# app's keyboard-driven flow stops at tribe pick under our scripted Return/Down sequence; reaching the actual world map needs additional interactions (set difficulty, set count, set tribe, name leader). PP `--shotWorld` jumps directly to the rendered world. |
| **City** | `polish_city_zh.png` — city view with zh_TW labels (`巴比倫 / 城市資源 / 糧倉 / 貿易 / 科學 / 供養單位 / 人口:1 食物:1/20 食物產量:+1 ...`) | `08_b.png` — same `Pick your tribe...` menu (key `b` did not transition us out of the menu) | Same flow-blockage as world. |

### 3.1 Palette / colour fidelity

| | resolution | unique RGB (typical) |
|---|---|---|
| openciv1pp `pp_*.png`        | 640x480 | 42 (title/menu), 61 (city), 75 (world), 131 (intro slide) |
| openciv1pp `polish_*_zh.png` | 640x480 | identical numbers — confirms `--shot*` is the same path that produced our published polish set |
| C# OpenCiv1 `01_title.png`–`10_esc.png` | 644x404 | 24–186 |
| C# OpenCiv1 root screenshot (`99_root.png`) | 1280x720 | 296 — includes Xvfb background pixels |

Both renderers are well under VGA's 256 colours. C# OpenCiv1 emits a
larger native window (644x404 client area + Avalonia chrome) so a true
pixel-for-pixel comparison requires resizing — `compare.sh` rescales both
sides to 640x400 for the composites in `sidebyside/`.

## 4. Top 5 actionable gaps to fix in openciv1pp

These are gaps where the C# port **is doing something openciv1pp is not**.
None of them block the current openciv1pp polish set, but they are useful
parity targets for the next milestone.

1. **Studio credits intro** — C# OpenCiv1 plays the "Harry Teasley / Sid
   Meier / Bruce Shelley" fade-in credits roll before the title screen.
   openciv1pp jumps straight to the title menu. Decide whether we want to
   replay it (we have `BIRTH*.PIC/PAL` already used for the birth slides;
   the credits use the title-screen fade and TXT data). If yes, add a
   pre-title scene in `MainIntro.cpp` driven by `STORY.TXT` / `CREDITS.TXT`.

2. **Difficulty picker dialog with portrait grid** — C# OpenCiv1's
   "difficulty" screen shows the five king portraits (chieftain → emperor)
   in a 2×3 mosaic with a list dialog on the right. openciv1pp's
   `MenuBoxDialog` only renders the list; the portrait grid is not drawn.
   Asset is `KING00.PIC … KING13.PIC`. This is a cosmetic upgrade — add
   `KING00..04` rendering next to the `MenuBoxDialog` for the difficulty
   selection only.

3. **"Pick your tribe" with English defaults** — openciv1pp shows a
   localized tribe list. C# OpenCiv1 shows the English vector
   (`Roman, Babylonian, …`). openciv1pp's tribe list is already CJK-correct;
   this is a **C# bug**, not ours. Document and move on.

4. **NU1605 SkiaSharp downgrade in C# csproj** — `Avalonia.Skia 11.3.14`
   transitively requires SkiaSharp ≥ 2.88.9 but the project pins 2.88.8.
   This forced our prewarm step to use `NoWarn=NU1605`. Not actionable on
   our side, but worth filing upstream.

5. **`#if DEBUG / #else` Assembly path bug in C# OpenCiv1** — line 107 of
   `src/Game/OpenCiv1Game.cs` calls `Assembly.GetExecutingAssembly()` in
   the `#else` (Release) branch without `using System.Reflection;`. This
   makes `dotnet build -c Release` fail on Linux. Worth filing upstream;
   we worked around it by building Debug.

(The reverse list — what openciv1pp does that C# OpenCiv1 doesn't — is
much longer: full zh_TW polish, FreeType MONO CJK font, real DOS palette
fidelity, headless `--shot*` modes for CI, `--play` with real DOS tiles,
all already documented in the main README.)

## 5. Files produced

```
docker/cs_compare/
  Dockerfile          (.NET 8 SDK + Avalonia runtime deps + Xvfb + ImageMagick + NuGet prewarm)
  prewarm.csproj      (one-shot project so image build can pre-fetch Avalonia/SkiaSharp packages
                       — required because runtime uses --network none)
  compare.sh          (Xvfb start, openciv1pp shots, dotnet restore/build/run, xdotool drive,
                       side-by-side composites, palette stats)
docs/cs_compare/
  oci_pp/             5 openciv1pp PNGs from --shot* + 5 published polish_*_zh.png references
  oci_cs/             14 C# OpenCiv1 PNGs captured via xdotool/ImageMagick:
                       01_title, 02_after_title, 03_post, 04_intro, 05_menu, 06_newgame,
                       07_loop_{1..4}, 08_b, 09_ret, 10_esc, 99_root (whole Xvfb screen)
  sidebyside/         5 composites: title, intro, menu, city, world
  palette.txt         per-PNG unique-RGB stats (Python PIL)
  logs/               cs_dotnet_info, cs_restore, cs_build (Debug),
                       cs_build_release (Release fail log for the record),
                       pp_shot_{title,intro,menu,city,world} stdout,
                       compare_stdout.log (mirror of in-container stdout)
docs/CS_COMPARISON_REPORT.md   (this file)
```

## 6. Hardening verification

The run honoured every rule in the operating contract:

* No host bash invocation of any X-using binary (no Xvfb, no
  `dotnet *.dll`, no `./build/openciv1pp`, no xdotool, no import, no
  ffmpeg x11grab from the host shell).
* Both `docker run` invocations included
  `--network none --env DISPLAY=:99 --tmpfs /tmp/.X11-unix`.
* Live verification: `docker inspect $CID --format '{{.HostConfig.NetworkMode}}'`
  returned `none` while the comparison container was running.
* Container exited cleanly; image kept, container reaped via `--rm`.
* The two attempts that previously leaked to the host display did **not**
  recur — only Xvfb inside the container saw any X traffic.

## 7. Reproducing this report

```bash
docker build -t openciv1-cs-cmp:latest docker/cs_compare/
docker run --rm --network none --env DISPLAY=:99 --tmpfs /tmp/.X11-unix \
  -v /home/anr2/civ1_cht/civ1_cht/openciv1pp:/work/openciv1pp \
  -v /home/anr2/civ1_cht/OpenCiv1:/work/OpenCiv1 \
  -v /home/anr2/civ1_cht/dos_civ:/work/dos:ro \
  -v /home/anr2/civ1_cht/civ1_cht/openciv1pp/docs/cs_compare:/work/out \
  openciv1-cs-cmp:latest \
  bash /work/openciv1pp/docker/cs_compare/compare.sh
```

Whole pipeline (including container build) finishes in ~2 minutes after
the first NuGet warm-up.
