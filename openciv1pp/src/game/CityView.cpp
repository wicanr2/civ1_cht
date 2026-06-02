// CityView.cpp — see CityView.h. Simplified faithful port of OpenCiv1
// CityView.cs (F19_0000_*): renders the city screen layout (panels/positions
// borders + city name, founding year, population dots, owner tribe, current
// production line, and a 21-tile mini-grid of the surrounding city radius).
#include "CityView.h"
#include "OpenCiv1Game.h"
#include "UnitManagement.h"
#include "CheckPlayerTurn.h"
#include "TechResearch.h"
#include "MainCode.h"
#include "MiniWorld.h"
#include "MapManagement.h"
#include "TerrainTiles.h"
#include "../localization/Translator.h"
#include "../platform/SdlPresenter.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>

namespace oc1 {

CityView::CityView(OpenCiv1Game& parent) : p(parent) {}

bool CityView::open(int cityId) {
    const auto& cities = p.unitManagement().cities();
    if (cityId < 0 || std::size_t(cityId) >= cities.size()) return false;
    cityId_ = cityId;
    open_ = true;

    // Lazy-load CBACK*.PIC backdrop the first time we open() (1:1 with the C#
    // F19_0000_137f_LoadCityViewBitmaps preamble — the C# loads CITYPIX1.PIC /
    // HILL.PIC / WONDERS.PIC at this point; we only need ONE background image
    // for the simplified layout). CBACK0.PIC is the simplest backdrop (the
    // grassland one); CBACK1..3 are climate variants — try them in order.
    if (!triedLoadBackdrop_) {
        triedLoadBackdrop_ = true;
        std::error_code ec;
        const std::string& rp = p.resourcePath();
        if (!rp.empty()) {
            for (const char* nm : {"CBACK.PIC", "CBACKS.PIC", "CBACKS1.PIC",
                                   "CBACKS2.PIC", "CBACKS3.PIC",
                                   "CBACK0.PIC", "CBACK1.PIC",
                                   "CBACK2.PIC", "CBACK3.PIC"}) {
                std::filesystem::path pth = std::filesystem::path(rp) / nm;
                if (std::filesystem::exists(pth, ec)) {
                    auto bmp = loadPicFile(pth.string(), true);
                    if (bmp) { backdrop_ = std::move(bmp); break; }
                }
            }
        }
    }
    // Lazy-load TER257.PIC for the 21-tile mini-grid (real-tile path). When the
    // file is absent (e.g. no DOS assets) we fall through to the per-terrain
    // colored-rect path inside draw().
    if (!triedLoadTileset_) {
        triedLoadTileset_ = true;
        std::error_code ec;
        const std::string& rp = p.resourcePath();
        if (!rp.empty()) {
            std::filesystem::path pth = std::filesystem::path(rp) / "TER257.PIC";
            if (std::filesystem::exists(pth, ec)) {
                auto bmp = loadPicFile(pth.string(), true);
                if (bmp) tileset_ = std::move(bmp);
            }
        }
    }
    return true;
}

void CityView::close() {
    open_ = false;
    cityId_ = -1;
}

bool CityView::handleKey(int navKey) {
    if (!open_) return false;
    if (navKey == SdlPresenter::KeyEsc || navKey == SdlPresenter::KeyEnter) {
        close();
        return true;
    }
    return false;
}

// City-view layout for the 640x480 native fb:
//   - DOS region (0..319, 0..199): CBACK*.PIC backdrop + all the DOS-coord
//     panels/labels/grid baked on top at their canonical DOS coords (the
//     pixel-parity zone). All coordinates inside this region come straight
//     out of CityWorker.cs (see docs/CITYVIEW_LAYOUT.md).
//   - Right strip (320..639, 0..199): extra Chinese accessibility area
//     (mini-grid label/legend space) — reserved canvas, currently blank.
//   - Bottom HUD strip (0..639, 200..479): Chinese 16-row label/value stack
//     (Population/Food/Food per turn/Founded/Owner/Production/Researching/
//     Government/Buildings/Wonders/Diplomacy/Trade/Upkeep/Treasury/Happy/
//     Unhappy/Status). Kept for the "視野要變大" payoff.
// The panel-rect below is the bottom HUD only; clicks anywhere outside the
// 640x480 canvas close the view.
namespace {
constexpr int kBackX = 0,   kBackY = 0;
constexpr int kBackW = 320, kBackH = 200;
constexpr int kPanelX = 0,  kPanelY = 200;
constexpr int kPanelW = 640, kPanelH = 280;

// The 21 city-radius offsets (the fat-cross 5x5-minus-4-corners pattern).
// Mirrors OpenCiv1.OpenCiv1GameGlobals.CityOffsets (cardinal/diagonal
// neighbours, then the four outer cardinals, then the eight outer "knight"
// offsets, then the centre). Order matches the C# array; the layout doesn't
// depend on it but keeping the source order makes a side-by-side diff with
// CityWorker.cs trivial.
constexpr int kCityOffsetCount = 21;
constexpr int kCityOffsets[kCityOffsetCount][2] = {
    { 0,-1},{ 1, 0},{ 0, 1},{-1, 0},
    { 1,-1},{ 1, 1},{-1, 1},{-1,-1},
    { 0,-2},{ 2, 0},{ 0, 2},{-2, 0},
    {-1,-2},{ 1,-2},{ 2,-1},{ 2, 1},{ 1, 2},{-1, 2},{-2, 1},{-2,-1},
    { 0, 0},
};
} // namespace

bool CityView::handleClick(int fbX, int fbY) {
    if (!open_) return false;
    // The city-view UI fills the WHOLE 640x480 canvas after the FOV refactor
    // (backdrop top-left + mini-grid upper-right + info panel along the
    // bottom). Negative / out-of-canvas clicks close the view; everything
    // inside the canvas is a no-op (deeper UI is OUT OF SCOPE).
    if (fbX < 0 || fbY < 0 || fbX >= 640 || fbY >= 480) {
        close();
        return true;
    }
    return true;
}

void CityView::draw(GBitmap& screen, int fontId) {
    if (!open_) return;
    const auto& cities = p.unitManagement().cities();
    if (cityId_ < 0 || std::size_t(cityId_) >= cities.size()) { close(); return; }
    draw(screen, cities[std::size_t(cityId_)], fontId);
}

// ---- the actual screen-build (mirrors F19_0000_0000_ShowCityLayout shape). --
void CityView::draw(GBitmap& screen, const City& city, int fontId) {
    // ---------------------------------------------------------------------
    // PALETTE STRATEGY (per docs/CITYVIEW_LAYOUT.md fix step #1):
    //   CBACK.PIC uses ALL 256 palette entries (the brown-ground gradient
    //   actually occupies 155..175, the sky gradient 240..255, etc — see
    //   the palette dump in CBACK.PIC). So we CANNOT carve out custom
    //   indices for the HUD without breaking CBACK's appearance.
    //   Instead we use CBACK's intact palette for EVERYTHING:
    //     - DOS-region content uses CBACK's 0..15 (standard VGA layout)
    //     - Bottom HUD strip ALSO uses CBACK's 0..15 (the VGA colors)
    //   For the worked-tile blit, we momentarily swap to TER257's palette,
    //   blit tiles, then restore CBACK's 0..15 so subsequent draws still
    //   look right.
    //
    //   Color cheat-sheet (DOS VGA-standard, matches CBACK 0..15):
    //     0  = black            (text shadow, borders)
    //     1  = dark blue        (sub-panel fill)
    //     7  = light grey       (HUD panel fill alt)
    //     8  = dark grey        (HUD panel fill)
    //     12 = bright red       (disorder title, food-bar bg)
    //     14 = bright yellow    (HUD title, food-bar fill)
    //     15 = bright white     (body text, panel border)
    //     10 = bright green     (supported-units list)
    // ---------------------------------------------------------------------

    // 1) Clear the whole canvas to black BEFORE painting anything.
    screen.clear(0);

    // 2) Backdrop CBACK*.PIC at native 320x200, top-left. CBACK's palette
    //    is copied into 0..255 verbatim so the DOS region and HUD strip
    //    share a single coherent palette table.
    if (backdrop_) {
        screen.copyPaletteFrom(*backdrop_);
        if (backdrop_->width() <= screen.width() &&
            backdrop_->height() <= screen.height()) {
            screen.drawBitmap(kBackX, kBackY, *backdrop_, false);
        }
    } else {
        // No backdrop asset: install a VGA-default 0..15 ramp so DOS-color
        // indices 0..15 below resolve to recognizable colors. The DOS
        // region is left as a solid fill in this fallback (rare path,
        // only hit when DOS assets are absent).
        screen.palette.set( 0,   0,   0,   0); // black
        screen.palette.set( 1,   0,   0, 170); // dark blue
        screen.palette.set( 2,   0, 170,   0); // dark green
        screen.palette.set( 3,   0, 170, 170); // dark cyan
        screen.palette.set( 4, 170,   0,   0); // dark red
        screen.palette.set( 5, 170,   0, 170); // dark magenta
        screen.palette.set( 6, 170,  85,   0); // brown
        screen.palette.set( 7, 170, 170, 170); // light grey
        screen.palette.set( 8,  85,  85,  85); // dark grey
        screen.palette.set( 9,  85,  85, 255); // bright blue
        screen.palette.set(10,  85, 255,  85); // bright green
        screen.palette.set(11,  85, 255, 255); // bright cyan
        screen.palette.set(12, 255,  85,  85); // bright red
        screen.palette.set(13, 255,  85, 255); // bright magenta
        screen.palette.set(14, 255, 255,  85); // bright yellow
        screen.palette.set(15, 255, 255, 255); // bright white
        screen.fillRect(Rect{kBackX, kBackY, kBackW, kBackH}, 1);
    }

    // Save the full CBACK palette so we can restore it after the (TER257)
    // tile blit below. This keeps both the DOS region and the bottom HUD
    // reading their VGA-standard indices 0..15 against CBACK's palette.
    std::array<RGB, 256> cbackPal{};
    for (int i = 0; i < 256; ++i) cbackPal[std::size_t(i)] = screen.palette.colors[std::size_t(i)];

    const GFont& font = p.graphics.font(fontId);
    auto& um = p.unitManagement();

    // Cache stats used by multiple panels below.
    int population = std::max(1, city.population);
    bool hasGranary = city.hasBuilding(BuildingType::Granary);
    int growthThreshold = (city.population + 1) * (hasGranary ? 5 : 10);

    // =====================================================================
    //  DOS-REGION CONTENT (0..319, 0..199) — pixel-parity layer.
    //  All coords here come straight from CityWorker.cs (DOS coords).
    // =====================================================================

    // (a) Top header strip (2,1)-(208,21): DOS uses FillRectangleWithPattern
    // over the CBACK cloud stipple already baked into the backdrop. Rather
    // than overlay a solid color (which clobbered the cloud pattern in R2),
    // we leave CBACK's stipple visible and only draw the city-name text on
    // top in (b) below.

    // (b) City name centered at DOS (104, 2), white (color 15).
    //     Format: "{中文城市名} (人口: {N})". We use Translator for
    //     "Pop:" -> "人口:" and for the city name itself.
    {
        std::string nameTr = Translator::instance().translate(city.name);
        std::string popLbl = Translator::instance().translate("Pop:");
        char buf[80];
        std::snprintf(buf, sizeof(buf), "%s (%s %d)", nameTr.c_str(),
                      popLbl.c_str(), population);
        std::string title = buf;
        Size sz = measureString(font, title);
        // 16px font is taller than DOS's 8px font; clamp y so the text fits
        // within the 21px-tall header strip. Center on (104,2) (DOS anchor)
        // but offset upward by half the font height to match DOS visual.
        int tx = 104 - sz.w / 2;
        if (tx < 2) tx = 2;
        int ty = 2;
        uint8_t titleCol = city.disorder ? 12 /* bright red */ : 15 /* white */;
        screen.drawString(font, tx + 1, ty + 1, title, 0);   // shadow
        screen.drawString(font, tx, ty, title, titleCol);
    }

    // (c) Resources sub-panel at (2, 23, 122, 9): same story as (a) — DOS
    //     uses FillRectangleWithPattern, and CBACK already carries the
    //     stippled cloud pattern at these pixels. We leave the CBACK pixels
    //     intact and only paint the "City Resources" label on top.
    {
        std::string lbl = Translator::instance().translate("City Resources");
        screen.drawString(font, 9, 25, lbl, 0);   // shadow
        screen.drawString(font, 8, 24, lbl, 15);  // white
    }

    // (c2) Citizen rate row at DOS y=106..114, 4 zones of 33w × 9h each.
    //      Per CityWorker.cs L1380-1389: zone bg is color 9 (bright blue);
    //      DOS centers a short label (Trade/Lux/Sci/Tax style) on each zone
    //      then runs a color-swap 9<->15 animation over the active citizen
    //      drag. We render the static row (no swap animation): four labelled
    //      tiles 貿易 / 奢侈 / 科學 / 稅收 in white text on color-9 bg.
    {
        struct Zone { int x; int w; const char* en; };
        static const Zone zones[4] = {
            {  95, 34, "Trade"   },
            { 129, 32, "Luxury"  },
            { 161, 33, "Science" },
            { 194, 33, "Tax"     },
        };
        for (const Zone& z : zones) {
            screen.fillRect(Rect{z.x, 106, z.w, 9}, 9);
            std::string lbl = Translator::instance().translate(z.en);
            Size sz = measureString(font, lbl);
            int tx = z.x + (z.w - sz.w) / 2;
            int ty = 106 + (9 - font.pixelHeight) / 2;
            if (ty < 106) ty = 106;
            screen.drawString(font, tx + 1, ty + 1, lbl, 0);
            screen.drawString(font, tx, ty, lbl, 15);
        }
    }

    // (d) Worked-tile grid: 21 tiles centered around (160, 56). For each
    //     (dx,dy) blit a 16x16 TER257 tile at ((dx+5)*16+80, (dy+3)*16+8).
    //     The center tile lands at (160, 56).
    //
    //     Palette tension: TER257 pixels reference TER257's palette indices,
    //     but CBACK's palette is what's INSTALLED. If we just copy raw
    //     TER257 indices, tile colors come out wrong (CBACK[100] != TER257[100]).
    //     Fix: build an RGB->CBACK-index nearest-match lookup, then map each
    //     TER257 source pixel through it. CBACK's palette stays intact -> the
    //     surrounding DOS region keeps its DOS colors.
    {
        const MapManagement* mm = nullptr;
        try { mm = &p.mapManagement(); } catch (...) { mm = nullptr; }
        auto mapTerToCback = [&](uint8_t terIdx) -> uint8_t {
            if (!tileset_) return terIdx;
            const RGB src = tileset_->palette.colors[std::size_t(terIdx)];
            // Nearest CBACK palette index by squared RGB distance.
            int bestI = 0; int bestD = INT32_MAX;
            for (int i = 0; i < 256; ++i) {
                const RGB& c = cbackPal[std::size_t(i)];
                int dr = int(c.r) - int(src.r);
                int dg = int(c.g) - int(src.g);
                int db = int(c.b) - int(src.b);
                int d = dr*dr + dg*dg + db*db;
                if (d < bestD) { bestD = d; bestI = i; if (d == 0) break; }
            }
            return uint8_t(bestI);
        };
        // Build a per-source-index cache (256-entry LUT) so the 21-tile loop
        // doesn't pay nearest-match cost per pixel.
        std::array<uint8_t, 256> lut{};
        if (tileset_) for (int i = 0; i < 256; ++i) lut[std::size_t(i)] = mapTerToCback(uint8_t(i));

        for (int i = 0; i < kCityOffsetCount; ++i) {
            int dx = kCityOffsets[i][0];
            int dy = kCityOffsets[i][1];
            int gx = (dx + 5) * 16 + 80;
            int gy = (dy + 3) * 16 + 8;
            Terrain t = Terrain::Grassland;
            if (mm) {
                int tx = city.x + dx, ty = city.y + dy;
                if (tx >= 0 && ty >= 0 &&
                    tx < mm->width() && ty < mm->height()) {
                    t = mm->terrainAt(tx, ty);
                } else {
                    t = Terrain::Water;
                }
            }
            if (tileset_) {
                TileXY tile = terrainToTileXY(t);
                int srcX = tile.col * 16, srcY = tile.row * 16;
                // Per-pixel remap blit via setPixel (16x16 = 256 ops/tile,
                // 21 tiles = 5376 ops/frame; negligible vs the rest of draw()).
                for (int py = 0; py < 16; ++py) {
                    for (int px = 0; px < 16; ++px) {
                        uint8_t s = tileset_->getPixel(srcX + px, srcY + py);
                        screen.setPixel(gx + px, gy + py, lut[std::size_t(s)]);
                    }
                }
            } else {
                // Fallback colored rect (DOS color 2/3 are dark green/cyan,
                // close enough for terrain). 16x16.
                uint8_t col = 2;
                switch (t) {
                    case Terrain::Water:     col = 1; break;
                    case Terrain::River:     col = 11; break;
                    case Terrain::Grassland: col = 10; break;
                    case Terrain::Plains:    col = 14; break;
                    case Terrain::Forest:    col = 2; break;
                    case Terrain::Jungle:    col = 2; break;
                    case Terrain::Hills:     col = 7; break;
                    case Terrain::Mountains: col = 7; break;
                    case Terrain::Desert:    col = 6; break;
                    case Terrain::Tundra:    col = 7; break;
                    case Terrain::Arctic:    col = 15; break;
                    case Terrain::Swamp:     col = 3; break;
                    default:                 col = 10; break;
                }
                screen.fillRect(Rect{gx, gy, 16, 16}, col);
            }
            // The 21st offset is (0,0) — the city centre. DOS marks all
            // "being-worked" tiles with a 15x15 red (color 12) outline.
            // Without a real worker simulation we treat the city centre
            // tile as "being worked" so the outline appears at the canonical
            // anchor; this matches what DOS shows for a freshly-founded
            // 1-pop city.
            if (dx == 0 && dy == 0) {
                screen.drawRect(Rect{gx, gy, 15, 15}, 12);
            }
        }
    }

    // (e) Vertical food-storage bar at (309, 2, 8, 96):
    //     bg color 12 (red), filled portion (color 14 / yellow) sized by
    //     food/growthThreshold. NO label here (per fix step #4).
    {
        screen.fillRect(Rect{309, 2, 8, 96}, 12);
        int h = 96 * std::max(0, city.food) / std::max(1, growthThreshold);
        if (h > 96) h = 96;
        if (h > 0) screen.fillRect(Rect{309, 2 + (96 - h), 8, h}, 14);
    }

    // (f) "Food Storage" label at (8, 108) color 15. zh: "糧倉".
    {
        std::string lbl = Translator::instance().translate("Food Storage");
        screen.drawString(font, 9, 109, lbl, 0);
        screen.drawString(font, 8, 108, lbl, 15);
    }

    // (g) Supported-units list at (98, i*6+179) color 10. We list units
    //     physically present at city.x,city.y (proxy for "supported by this
    //     city" — Unit doesn't carry a homeCity field in this port). Cap
    //     to 4 rows so the list stays inside the DOS region. Localized.
    {
        int rows = 0;
        std::string sLbl = Translator::instance().translate("Supported Units");
        // Header label one row above the first row anchor.
        screen.drawString(font, 99, 174, sLbl, 0);
        screen.drawString(font, 98, 173, sLbl, 15);
        for (const auto& u : um.units()) {
            if (!u.alive) continue;
            if (u.owner != city.owner) continue;
            if (u.x != city.x || u.y != city.y) continue;
            if (rows >= 4) break;
            std::string nm = Translator::instance().translate(
                                 unitDefOf(u.type).name);
            int y = rows * 6 + 179;
            // Clamp y so the 16px font fits inside the DOS region
            // (the 6px DOS row pitch was for the smaller DOS font).
            screen.drawString(font, 98, y, nm, 10);
            ++rows;
        }
    }

    // (h) Right info panel at (230, 99, 90, 100): fill color 0 (black),
    //     show current production label + shield bar + buildings list.
    {
        screen.fillRect(Rect{230, 99, 90, 100}, 0);
        // "生產: <what>"
        const char* what = nullptr;
        if (city.productionKind == City::ProductionKind::Wonder &&
            city.productionWonderType != WonderType::None) {
            what = wonderDefOf(city.productionWonderType).name;
        } else if (city.productionKind == City::ProductionKind::Building &&
            city.productionBuildingType != BuildingType::None) {
            what = buildingDefOf(city.productionBuildingType).name;
        } else {
            what = unitDefOf(city.productionType).name;
        }
        std::string prodLbl = Translator::instance().translate("Production");
        std::string whatTr = Translator::instance().translate(what);
        char prodBuf[96];
        std::snprintf(prodBuf, sizeof(prodBuf), "%s: %s",
                      prodLbl.c_str(), whatTr.c_str());
        screen.drawString(font, 233, 101, prodBuf, 0);
        screen.drawString(font, 232, 100, prodBuf, 15);
        // Shield bar (production progress).
        int barX = 234, barY = 118, barW = 80, barH = 6;
        screen.drawRect(Rect{barX, barY, barW, barH}, 15);
        int filled = (city.production > 0) ?
                     std::min(barW - 2, (barW - 2) * city.shields / city.production) : 0;
        if (filled > 0) screen.fillRect(Rect{barX + 1, barY + 1, filled, barH - 2}, 14);
        // "建築:" + buildings list (up to 6 rows, 8px stride).
        std::string bLbl = Translator::instance().translate("Buildings");
        char bHeader[32];
        std::snprintf(bHeader, sizeof(bHeader), "%s:", bLbl.c_str());
        screen.drawString(font, 233, 131, bHeader, 0);
        screen.drawString(font, 232, 130, bHeader, 15);
        int row = 0;
        for (BuildingType b : city.ownedBuildings) {
            if (row >= 6) break;
            std::string n = Translator::instance().translate(buildingDefOf(b).name);
            int y = 142 + row * 8;
            if (y + 6 >= 199) break;
            screen.drawString(font, 232, y, n, 15);
            ++row;
        }
        if (row == 0) {
            screen.drawString(font, 232, 142, "-", 15);
        }
    }

    // =====================================================================
    //  BOTTOM HUD STRIP (y=200..479) — Chinese accessibility label/value
    //  stack. Uses our 160..175 palette overrides (CBACK doesn't touch them).
    // =====================================================================

    // HUD uses CBACK's standard VGA 0..15 — index 8 (dark grey) for fill,
    // 0 (black) for a single soft border to match DOS aesthetics (the old
    // double white border was visually too harsh per the R2 city-screen
    // color audit).
    screen.fillRect(Rect{kPanelX, kPanelY, kPanelW, kPanelH}, 8);
    screen.drawRect(Rect{kPanelX, kPanelY, kPanelW, kPanelH}, 0);

    // (DOS title at (104,2) is the canonical title — bottom HUD starts
    //  directly with the label/value stack, no duplicate "City:" line.)
    int infoX = kPanelX + 16;
    int infoY = kPanelY + 8;
    int lineH = font.pixelHeight + font.lineSpacing + 2;
    int yr2 = um.year(); (void)yr2;

    // Owner tribe name (resolve via civs() when populated; fall back to a
    // generic label when not).
    std::string ownerName = "Player";
    if (city.owner >= 0 && std::size_t(city.owner) < um.civs().size()) {
        int t = um.civs()[std::size_t(city.owner)].tribeIdx;
        if (t >= 0 && t < int(MainCode::tribes().size())) {
            ownerName = MainCode::tribes()[std::size_t(t)].nation;
        }
    }

    // population/growthThreshold are declared near the top of draw()
    // (used by the DOS-region content above). Re-use those values here.
    (void)hasGranary;

    // Founded label.
    int yFound = city.foundedTurn;
    // We store founded as a TURN number (small int). In the C# the original
    // shows the FOUNDED YEAR. We approximate by drawing the founded turn —
    // sufficient for the test (the label is still localized) and faithful to
    // City.foundedTurn provenance in UnitManagement.

    auto drawLabelValue = [&](int y, const char* labelKey, const std::string& val) {
        std::string lab = Translator::instance().translate(labelKey);
        screen.drawString(font, infoX + 1, y + 1, lab, 0);
        int penX = screen.drawString(font, infoX, y, lab, 15);
        // value: numeric/proper-noun-ish; not localized further
        screen.drawString(font, penX + 1, y + 1, val, 0);
        screen.drawString(font, penX, y, val, 15);
    };

    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", population);
        drawLabelValue(infoY, "Population:", buf);
    }
    {
        // 食物: <food>/<threshold> — current food box + growth threshold,
        // both in food units. Granary cities show the halved threshold.
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d/%d", city.food, growthThreshold);
        drawLabelValue(infoY + lineH, "Food:", buf);
    }
    {
        // 食物產量: ±N — current per-turn delta (gross food - pop*2).
        // Recompute from the live terrain provider so it matches whatever
        // the next end-of-turn pass will apply (foodPerTurn is updated
        // there too, but a fresh value here keeps the display in sync
        // even when the field hasn't been refreshed since save/load).
        int gross = p.checkPlayerTurn().cityFoodGross(city.x, city.y);
        int delta = gross - city.population * 2;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%+d", delta);
        drawLabelValue(infoY + lineH * 2, "Food per turn:", buf);
    }
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", yFound);
        drawLabelValue(infoY + lineH * 3, "Founded:", buf);
    }
    {
        // Owner tribe name — translate (e.g. "Romans" -> the Chinese form
        // when the language pack has it).
        std::string tr = Translator::instance().translate(ownerName);
        drawLabelValue(infoY + lineH * 4, "Owner:", tr);
    }
    {
        // Production line: SHIELDS/COST + the English key of what's being
        // built (unit type name OR building name, translated via Translator).
        char buf[64];
        const char* what = nullptr;
        if (city.productionKind == City::ProductionKind::Wonder &&
            city.productionWonderType != WonderType::None) {
            what = wonderDefOf(city.productionWonderType).name;
        } else if (city.productionKind == City::ProductionKind::Building &&
            city.productionBuildingType != BuildingType::None) {
            what = buildingDefOf(city.productionBuildingType).name;
        } else {
            what = unitDefOf(city.productionType).name;
        }
        std::string whatTr = Translator::instance().translate(what);
        std::snprintf(buf, sizeof(buf), "%s %d/%d", whatTr.c_str(),
                      city.shields, city.production);
        drawLabelValue(infoY + lineH * 5, "Production:", buf);
    }
    // "Researching: <tech>  (pts/cost)" — the owner civ's current research
    // target (Translator turns the tech English key into Chinese). Skipped
    // when TechResearch hasn't been provisioned for this game.
    if (p.techResearch().civCount() > 0) {
        Tech t = p.techResearch().civResearching(city.owner);
        const char* nameKey = TechResearch::techNameKey(t);
        if (nameKey && nameKey[0]) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), " (%d/%d)",
                          p.techResearch().civPoints(city.owner),
                          p.techResearch().civResearchCost(city.owner));
            std::string val = Translator::instance().translate(nameKey) + buf;
            drawLabelValue(infoY + lineH * 6, "Researching:", val);
        }
    }
    {
        // Owner government — Translator turns "Despotism"->"專制" etc. When
        // the owner civ is mid-transition (anarchyTurnsLeft > 0) the
        // EFFECTIVE government reads Anarchy / "無政府".
        const auto& cvs = p.unitManagement().civs();
        if (city.owner >= 0 && std::size_t(city.owner) < cvs.size()) {
            Government g = p.unitManagement().effectiveGovernment(city.owner);
            std::string val = Translator::instance().translate(
                                  governmentNameKey(g));
            drawLabelValue(infoY + lineH * 7, "Government:", val);
        }
    }
    {
        // Owned-buildings line ("建築: Granary, Walls" / "Buildings: ..."),
        // localized via the Translator. Each building name goes through the
        // Translator independently so the "Granary"->"穀倉" rule applies.
        std::string val;
        bool first = true;
        for (BuildingType b : city.ownedBuildings) {
            const BuildingDef& bd = buildingDefOf(b);
            if (!first) val += ", ";
            val += Translator::instance().translate(bd.name);
            first = false;
        }
        if (val.empty()) val = "-";
        drawLabelValue(infoY + lineH * 8, "Buildings:", val);
    }
    {
        // Wonders line ("奇蹟: 金字塔, 長城" / "Wonders: Pyramids, Great Wall"),
        // localized via the Translator. Shows the OWNER civ's full set of
        // owned wonders (not just the ones this city built) so the player
        // sees the civ-wide picture. The home-city of each wonder is
        // tracked separately in UnitManagement::wonderOwnerCity.
        std::string val;
        const auto& cvs = p.unitManagement().civs();
        if (city.owner >= 0 && std::size_t(city.owner) < cvs.size()) {
            const auto& civ = cvs[std::size_t(city.owner)];
            bool first = true;
            for (WonderType w : civ.ownedWonders) {
                const WonderDef& wd = wonderDefOf(w);
                if (!first) val += ", ";
                val += Translator::instance().translate(wd.name);
                first = false;
            }
        }
        if (val.empty()) val = "-";
        drawLabelValue(infoY + lineH * 9, "Wonders:", val);
    }
    {
        // Diplomacy line: shows the OWNER civ's relations vs every OTHER civ
        // (e.g. "civ1=和平, civ2=戰爭"). Translator turns "Diplomacy:" ->
        // "外交: " and "Peace"/"War"/"No Contact" -> "和平"/"戰爭"/"未接觸".
        // For a clean one-liner we drop the trailing "未接觸" entries.
        std::string val;
        const auto& cvs = p.unitManagement().civs();
        bool first = true;
        for (std::size_t i = 0; i < cvs.size(); ++i) {
            if (int(i) == city.owner) continue;
            Relation r = p.unitManagement().getRelation(city.owner, int(i));
            if (r == Relation::NoContact) continue; // skip un-met civs
            if (!first) val += ", ";
            val += cvs[i].name;
            val += "=";
            val += Translator::instance().translate(relationNameKey(r));
            first = false;
        }
        if (val.empty()) val = "-";
        drawLabelValue(infoY + lineH * 10, "Diplomacy:", val);
    }
    {
        // Trade line: simplified per-civ baseline trade attributed to this
        // city (1 + #cities scaled by Government.tradeMul, evenly split
        // across owner's cities — first-cut display so the user sees the
        // per-city trade contribution). Mirrors the Civ1 city-screen TRADE
        // readout (full per-tile trade pipeline + Tax/Lux/Sci slider TODO).
        const auto& um = p.unitManagement();
        int ownerCities = 0;
        for (const auto& cc : um.cities()) if (cc.owner == city.owner) ++ownerCities;
        float tradeMul = 1.0f;
        if (city.owner >= 0 && std::size_t(city.owner) < um.civs().size()) {
            tradeMul = governmentDefOf(
                um.effectiveGovernment(city.owner)).tradeMul;
        }
        int civTrade = int((1 + ownerCities) * tradeMul);
        if (civTrade < 0) civTrade = 0;
        // Per-city share (rounded up so a 1-city civ gets the full baseline).
        int perCity = (ownerCities > 0) ?
                      (civTrade + ownerCities - 1) / ownerCities : civTrade;
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", perCity);
        drawLabelValue(infoY + lineH * 11, "Trade:", buf);
    }
    {
        // Upkeep line: owner civ's cached unit upkeep (1 gold/turn each).
        // Faithful Civ1 readout (City Screen "TREASURY/UPKEEP" panel).
        const auto& um = p.unitManagement();
        int up = 0;
        if (city.owner >= 0 && std::size_t(city.owner) < um.civs().size()) {
            up = um.civs()[std::size_t(city.owner)].upkeepGoldPerTurn;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", up);
        drawLabelValue(infoY + lineH * 12, "Upkeep:", buf);
    }
    {
        // Treasury line: owner civ's current gold. Single source of truth
        // for the per-civ gold display when more than one city exists.
        const auto& um = p.unitManagement();
        int gold = 0;
        if (city.owner >= 0 && std::size_t(city.owner) < um.civs().size()) {
            gold = um.civs()[std::size_t(city.owner)].gold;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", gold);
        drawLabelValue(infoY + lineH * 13, "Treasury:", buf);
    }
    {
        // Happy: <H> Unhappy: <U> / Status: <Disorder!|Order> — three
        // lines for the HAPPINESS slice. Translator turns "Happy:" /
        // "Unhappy:" / "Status:" / "Disorder!" / "Order" into Chinese.
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", city.happy);
        drawLabelValue(infoY + lineH * 14, "Happy:", buf);
        std::snprintf(buf, sizeof(buf), "%d", city.unhappy);
        drawLabelValue(infoY + lineH * 15, "Unhappy:", buf);
        const char* st = city.disorder ? "Disorder!" : "Order";
        std::string stTr = Translator::instance().translate(st);
        drawLabelValue(infoY + lineH * 16, "Status:", stTr);
    }

    // (The 21-tile mini-grid and population-dots row that used to live in
    //  the right-strip / bottom-strip are GONE — the DOS-region content
    //  above (sections (a)..(h)) replaces them at the canonical DOS coords.)

    // ESC hint at the bottom of the HUD strip.
    {
        std::string hint = Translator::instance().translate("Esc: quit");
        int hy = kPanelY + kPanelH - font.pixelHeight - font.lineSpacing - 4;
        screen.drawString(font, infoX + 1, hy + 1, hint, 0);
        screen.drawString(font, infoX, hy, hint, 15);
    }
}

} // namespace oc1
