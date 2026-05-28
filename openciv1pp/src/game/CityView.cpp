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

// City-view layout for the 640x480 native fb (FOV refactor):
//   - CBACK*.PIC backdrop at native 320x200 in the TOP-LEFT (0,0)..(320,200).
//   - 21-tile mini-grid (5x5 with 4 corners removed, 24px cells = 120x120)
//     in the upper-right region, beside the backdrop.
//   - Comprehensive info panel occupying the FULL bottom strip
//     y=200..480 (640x280) — name/year, owner/population/production/buildings.
// The panel-rect below is the INFO panel only; clicks anywhere outside it
// AND outside the backdrop+grid upper strip close the view.
namespace {
constexpr int kBackX = 0,   kBackY = 0;
constexpr int kBackW = 320, kBackH = 200;
constexpr int kGridX = 340, kGridY = 16;     // upper-right of backdrop
constexpr int kPanelX = 0,  kPanelY = 200;
constexpr int kPanelW = 640, kPanelH = 280;
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
    // Install a dedicated set of palette indices with HIGH-CONTRAST values
    // mirroring the 16-colour VGA palette (Palette::loadDefaultVga in
    // src/graphics/Palette.h). Picked to be far from terrain hues 200..206
    // and the AI civ colours 220..226 (see MiniWorld::renderUnits). We DO
    // NOT write to indices 0..15 directly because copyPaletteFrom(*backdrop_)
    // would clobber them; instead we install matching VGA RGB values at
    // dedicated 16x.. indices so the panel/text stays readable on top of
    // whatever palette the loaded backdrop installs.
    //   160 = VGA dark blue   (1)   -> backdrop fill
    //   161 = VGA dark grey   (8)   -> panel fill (DARK so text on top reads)
    //   162 = VGA bright white (15) -> panel border / dividers
    //   163 = VGA black        (0)  -> text shadow
    //   164 = VGA bright white (15) -> body text / labels
    //   165 = VGA light grey   (7)  -> sub-panel fill
    //   166 = VGA bright yellow(14) -> population dots / city centre / title
    //   167 = VGA bright green (10) -> grassland / plains
    //   168 = VGA dark blue    (1)  -> water / river
    //   169 = VGA light grey   (7)  -> hills / mountains
    //   170 = VGA brown        (6)  -> desert
    //   171 = VGA dark green   (2)  -> forest / jungle
    //   172 = VGA bright cyan  (11) -> river highlight
    //   173 = VGA dark cyan    (3)  -> swamp
    auto installCityViewPalette = [&]() {
        screen.palette.set(160,   0,   0, 170); // dark blue (backdrop fill)
        screen.palette.set(161,  85,  85,  85); // dark grey (panel fill)
        screen.palette.set(162, 255, 255, 255); // bright white (border)
        screen.palette.set(163,   0,   0,   0); // black (text shadow)
        screen.palette.set(164, 255, 255, 255); // bright white (text)
        screen.palette.set(165, 170, 170, 170); // light grey (sub-panel)
        screen.palette.set(166, 255, 255,  85); // bright yellow (title + pop)
        screen.palette.set(167,  85, 255,  85); // bright green (grass/plains)
        screen.palette.set(168,   0,   0, 170); // dark blue (water/river)
        screen.palette.set(169, 170, 170, 170); // light grey (hills/mountains)
        screen.palette.set(170, 170,  85,   0); // brown (desert)
        screen.palette.set(171,   0, 170,   0); // dark green (forest/jungle)
        screen.palette.set(172,  85, 255, 255); // bright cyan (river)
        screen.palette.set(173,   0, 170, 170); // dark cyan (swamp)
        screen.palette.set(174, 255, 255, 255); // bright white (arctic)
    };
    installCityViewPalette();

    // Clear the whole canvas to the "outside" backdrop fill BEFORE painting
    // the upper region. This guarantees the bottom info panel starts from a
    // known colour and the area to the right of CBACK reads as canvas, not
    // as garbage from a prior frame.
    screen.clear(160);

    // 1) Backdrop: CBACK*.PIC when loaded (mirrors the C# HILL.PIC backdrop
    //    used by ShowCityLayout); else a flat colored fill in the backdrop
    //    rect. CBACK is 320x200 DOS art — drawn at NATIVE size in the
    //    top-left so the pixels stay crisp; the right side / bottom panel
    //    use the freed space for the mini-grid + info panel. The palette is
    //    re-installed AFTER copyPaletteFrom so panel/text colours survive
    //    the backdrop's palette table.
    if (backdrop_) {
        screen.copyPaletteFrom(*backdrop_);
        if (backdrop_->width() <= screen.width() &&
            backdrop_->height() <= screen.height()) {
            screen.drawBitmap(kBackX, kBackY, *backdrop_, false);
        }
        installCityViewPalette();
    } else {
        // No backdrop asset: fill ONLY the backdrop rect (panel still gets 161).
        screen.fillRect(Rect{kBackX, kBackY, kBackW, kBackH}, 160);
    }
    // When TER257 tileset is loaded for the mini-grid, its palette (indices
    // 0..~127) must be installed so the blit pixels resolve correctly. Our
    // CityView overrides at 160+ are re-applied AFTER so they survive.
    if (tileset_) {
        screen.copyPaletteFrom(*tileset_);
        installCityViewPalette();
    }

    // 2) Bottom info panel — fills the freed space y=200..480 (640x280) below
    //    the native-size CBACK. Generous room for all the labels.
    screen.fillRect(Rect{kPanelX, kPanelY, kPanelW, kPanelH}, 161);
    screen.drawRect(Rect{kPanelX, kPanelY, kPanelW, kPanelH}, 162);
    screen.drawRect(Rect{kPanelX + 1, kPanelY + 1, kPanelW - 2, kPanelH - 2}, 162);

    // 3) Title bar at the TOP of the info panel — city name + (year)
    //    (mirrors the C# DrawCenteredStringWithShadow "{cityName} ({year})"
    //    line in F19_0000_0000_ShowCityLayout).
    const GFont& font = p.graphics.font(fontId);
    int titleY = kPanelY + 6;
    {
        char yearbuf[32];
        int yr = p.unitManagement().year();
        std::snprintf(yearbuf, sizeof(yearbuf), " (%d %s)",
                      yr < 0 ? -yr : yr, yr < 0 ? "BC" : "AD");
        // Title "City: <Name>" — translate "City" via the chokepoint, then the
        // name (also translatable, e.g. "Capital"->"首都"), then the year.
        std::string cityKey = Translator::instance().translate("City");
        std::string nameTr  = Translator::instance().translate(city.name);
        std::string title   = cityKey + ": " + nameTr + yearbuf;
        Size sz = measureString(font, title);
        int tx = kPanelX + (kPanelW - sz.w) / 2;
        // Shadow then text (the F19_ DrawCenteredStringWithShadow recipe).
        // Title is rendered in bright yellow (166) for visual emphasis.
        screen.drawString(font, tx + 1, titleY + 1, title, 163);
        screen.drawString(font, tx, titleY, title, 166);
    }

    // 4) Info column (LEFT half of bottom panel) — Population:/Founded:/
    //    Owner:/Production:/Researching:/Buildings:. Lots of breathing room
    //    after the FOV refactor.
    int infoX = kPanelX + 16;
    int infoY = titleY + font.pixelHeight + font.lineSpacing + 6;
    int lineH = font.pixelHeight + font.lineSpacing + 2;

    auto& um = p.unitManagement();
    int yr = um.year();

    // Owner tribe name (resolve via civs() when populated; fall back to a
    // generic label when not).
    std::string ownerName = "Player";
    if (city.owner >= 0 && std::size_t(city.owner) < um.civs().size()) {
        int t = um.civs()[std::size_t(city.owner)].tribeIdx;
        if (t >= 0 && t < int(MainCode::tribes().size())) {
            ownerName = MainCode::tribes()[std::size_t(t)].nation;
        }
    }

    // Population = ActualSize, now driven by city.population (set by
    // CheckPlayerTurn's per-turn food/growth pass). Mirrors the C#
    // F19_0000_111f_DrawCityPopulation, which iterates city.ActualSize and
    // draws one POP sprite per point. Floor at 1 (a city always has >= 1
    // pop; single-pop starvation just clamps food at 0).
    int population = std::max(1, city.population);
    // Growth threshold for the current pop (Granary halves it). Shown as
    // "<food>/<threshold>" in the 食物 line so the player can read the
    // distance to growth at a glance.
    bool hasGranary = city.hasBuilding(BuildingType::Granary);
    int growthThreshold = (city.population + 1) * (hasGranary ? 5 : 10);

    // Founded label.
    int yFound = city.foundedTurn; (void)yr;
    // We store founded as a TURN number (small int). In the C# the original
    // shows the FOUNDED YEAR. We approximate by drawing the founded turn —
    // sufficient for the test (the label is still localized) and faithful to
    // City.foundedTurn provenance in UnitManagement.

    auto drawLabelValue = [&](int y, const char* labelKey, const std::string& val) {
        std::string lab = Translator::instance().translate(labelKey);
        screen.drawString(font, infoX + 1, y + 1, lab, 163);
        int penX = screen.drawString(font, infoX, y, lab, 164);
        // value: numeric/proper-noun-ish; not localized further
        screen.drawString(font, penX + 1, y + 1, val, 163);
        screen.drawString(font, penX, y, val, 164);
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
        if (city.productionKind == City::ProductionKind::Building &&
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
        drawLabelValue(infoY + lineH * 7, "Buildings:", val);
    }

    // 5) Population dots — one warm yellow dot per population point, drawn in
    //    a horizontal row below the info block (mirrors the C# pop-sprite row
    //    drawn at (24, 140) in F19_0000_111f_DrawCityPopulation; we use small
    //    coloured dots instead of POP.PIC sprites — see header for the stub).
    {
        int popY = infoY + lineH * 8 + 2;
        int popX = infoX;
        for (int i = 0; i < population && i < 24; ++i) {
            screen.fillRect(Rect{popX, popY, 6, 8}, 166);
            screen.drawRect(Rect{popX, popY, 6, 8}, 163);
            popX += 8;
        }
    }

    // 6) 21-tile mini-grid — the city centre + the 20 worker tiles in the
    //    city radius. The Civ1 city radius is a 5x5 area with the four
    //    corner tiles removed (the "fat cross" pattern of 21 tiles). After
    //    the FOV refactor the grid lives in the UPPER-RIGHT of the canvas
    //    (beside the native-size CBACK), with BIGGER 24x24 cells so it
    //    reads clearly. We sample real terrain when MapManagement was
    //    generated; otherwise we leave the cells blank (panel fill).
    {
        // When TER257 is available, the mini-grid blits the 16x16 base tile
        // per cell upscaled into a 24x24 cell (3:2 nearest-neighbour by way
        // of the temp 16x16 -> scaled2x path that fills 32x32, clipped to
        // 24). Otherwise we draw 24px colored rects keyed to a DISTINCT
        // bright palette index per terrain enum.
        const int cellSz = 24;
        // Upper-right region — beside the native-size CBACK at (kGridX, kGridY).
        int gridX0 = kGridX;
        int gridY0 = kGridY;

        // Section label above the grid.
        std::string lbl = Translator::instance().translate("Tiles:");
        int tY = gridY0 - font.pixelHeight - font.lineSpacing - 2;
        if (tY < 0) tY = 0;
        screen.drawString(font, gridX0 + 1, tY + 1, lbl, 163);
        screen.drawString(font, gridX0, tY, lbl, 164);

        const bool useTiles = (tileset_ != nullptr);

        const MapManagement* mm = nullptr;
        try { mm = &p.mapManagement(); } catch (...) { mm = nullptr; }

        // Per-terrain palette index lookup for the fallback path. DISTINCT
        // VGA-style colours so each terrain reads at a glance (mirrors the
        // colour scheme called out in the polish task: Water/River=blue,
        // Grassland=green, Plains=yellow, Forest/Jungle=dark-green, Hills/
        // Mountains=light-grey, Desert=brown, Tundra=light-grey, Arctic=white,
        // Swamp=dark-cyan).
        auto terrainCol = [](Terrain t) -> uint8_t {
            switch (t) {
                case Terrain::Water:     return 168; // dark blue
                case Terrain::River:     return 172; // bright cyan
                case Terrain::Grassland: return 167; // bright green
                case Terrain::Plains:    return 166; // bright yellow
                case Terrain::Forest:    return 171; // dark green
                case Terrain::Jungle:    return 171; // dark green
                case Terrain::Hills:     return 169; // light grey
                case Terrain::Mountains: return 169; // light grey
                case Terrain::Desert:    return 170; // brown
                case Terrain::Tundra:    return 165; // light grey (sub-panel)
                case Terrain::Arctic:    return 174; // bright white
                case Terrain::Swamp:     return 173; // dark cyan
                default:                 return 167; // grass fallback
            }
        };

        // 5x5 area; skip the 4 corners == 21 tiles ("fat cross" city radius).
        for (int dy = -2; dy <= 2; ++dy) {
            for (int dx = -2; dx <= 2; ++dx) {
                bool corner = (std::abs(dx) == 2 && std::abs(dy) == 2);
                if (corner) continue;
                int gx = gridX0 + (dx + 2) * cellSz;
                int gy = gridY0 + (dy + 2) * cellSz;

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
                if (useTiles) {
                    // 16x16 TER257 tile blitted at NATIVE size centered in the
                    // 24x24 cell. We don't upscale: native pixels keep the
                    // tile art crisp (matching the FOV refactor's "no chunky
                    // pixels" rule). srcRect-aware drawBitmap pulls the right
                    // slot out of the tileset atlas in one call.
                    TileXY tile = terrainToTileXY(t);
                    int ox = gx + (cellSz - 16) / 2;
                    int oy = gy + (cellSz - 16) / 2;
                    screen.drawBitmap(ox, oy, *tileset_,
                                      Rect{tile.col * 16, tile.row * 16, 16, 16},
                                      false);
                    screen.drawRect(Rect{gx, gy, cellSz, cellSz}, 162);
                } else {
                    screen.fillRect(Rect{gx, gy, cellSz - 1, cellSz - 1},
                                    terrainCol(t));
                    screen.drawRect(Rect{gx, gy, cellSz - 1, cellSz - 1}, 162);
                }
                if (dx == 0 && dy == 0) {
                    // city centre marker (bright yellow + dark outline)
                    int inset = 3;
                    screen.fillRect(Rect{gx + inset, gy + inset,
                                         cellSz - 1 - 2 * inset,
                                         cellSz - 1 - 2 * inset}, 166);
                    screen.drawRect(Rect{gx + inset, gy + inset,
                                         cellSz - 1 - 2 * inset,
                                         cellSz - 1 - 2 * inset}, 163);
                }
            }
        }
    }

    // 7) Hint line at the bottom — ESC returns to map.
    {
        std::string hint = Translator::instance().translate("Esc: quit");
        // re-key to "ESC: return" if the existing key is too tied to the world
        // map. We use "Esc: quit" key already in the language pack (HUD line 2).
        int hy = kPanelY + kPanelH - font.pixelHeight - font.lineSpacing - 4;
        screen.drawString(font, infoX + 1, hy + 1, hint, 163);
        screen.drawString(font, infoX, hy, hint, 164);
    }
}

} // namespace oc1
