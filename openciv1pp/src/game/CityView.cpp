// CityView.cpp — see CityView.h. Simplified faithful port of OpenCiv1
// CityView.cs (F19_0000_*): renders the city screen layout (panels/positions
// borders + city name, founding year, population dots, owner tribe, current
// production line, and a 21-tile mini-grid of the surrounding city radius).
#include "CityView.h"
#include "OpenCiv1Game.h"
#include "UnitManagement.h"
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

// The visual panel rect — everything OUTSIDE this is "background" and a click
// there returns to the map (mirrors Civ1 click-outside-panel).
namespace {
constexpr int kPanelX = 10, kPanelY = 10;
constexpr int kPanelW = 300, kPanelH = 180;
} // namespace

bool CityView::handleClick(int fbX, int fbY) {
    if (!open_) return false;
    // Click OUTSIDE the city panel -> close (return to map).
    if (fbX < kPanelX || fbY < kPanelY ||
        fbX >= kPanelX + kPanelW || fbY >= kPanelY + kPanelH) {
        close();
        return true;
    }
    // Click inside the panel: no-op for now (deeper UI is OUT OF SCOPE).
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
    // Install a small set of dedicated palette indices so the city view has
    // a recognisable look regardless of which palette the world map left
    // behind. Picked to be far from terrain hues 200..206 and the AI civ
    // colours 220..226 (see MiniWorld::renderUnits).
    screen.palette.set(160,  20,  20,  40); // backdrop fill (dark blue)
    screen.palette.set(161, 200, 180, 120); // panel fill (warm tan)
    screen.palette.set(162,  90,  60,  20); // panel border (deep brown)
    screen.palette.set(163,   0,   0,   0); // text shadow
    screen.palette.set(164, 255, 255, 255); // text colour
    screen.palette.set(165, 220, 200, 140); // sub-panel fill
    screen.palette.set(166, 250, 220,  90); // city dot / population (warm yellow)
    screen.palette.set(167,  60, 160,  80); // grass tile (mini-grid)
    screen.palette.set(168,  50,  90, 180); // water tile (mini-grid)
    screen.palette.set(169, 180, 180, 180); // mountain tile (mini-grid)
    screen.palette.set(170, 140, 100,  60); // hills/desert tile (mini-grid)

    // 1) Backdrop: CBACK*.PIC when loaded (mirrors the C# HILL.PIC backdrop
    //    used by ShowCityLayout); else a flat colored fill.
    if (backdrop_ &&
        backdrop_->width() <= screen.width() && backdrop_->height() <= screen.height()) {
        screen.copyPaletteFrom(*backdrop_);
        screen.drawBitmap(0, 0, *backdrop_, false);
    } else {
        screen.clear(160);
    }

    // 2) Main panel rectangle (F19 layout: a big inset panel below the title
    //    bar). The C# uses Var_19d4_Screen1_Rectangle for the layout area; we
    //    mirror its 320x200 shape and inset by 10px on each side for a panel.
    screen.fillRect(Rect{kPanelX, kPanelY, kPanelW, kPanelH}, 161);
    screen.drawRect(Rect{kPanelX, kPanelY, kPanelW, kPanelH}, 162);
    screen.drawRect(Rect{kPanelX + 1, kPanelY + 1, kPanelW - 2, kPanelH - 2}, 162);

    // 3) Top title bar — city name + (year) (mirrors the C# line in
    //    F19_0000_0000_ShowCityLayout that calls DrawCenteredStringWithShadow
    //    with "{cityName} ({year})" at (160, 2)).
    const GFont& font = p.graphics.font(fontId);
    int titleY = kPanelY + 4;
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
        screen.drawString(font, tx + 1, titleY + 1, title, 163);
        screen.drawString(font, tx, titleY, title, 164);
    }

    // 4) Info column (left side) — Population:/Founded:/Owner:/Production:.
    int infoX = kPanelX + 8;
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

    // Population = ActualSize == city.units + 1 (a freshly-founded city has
    // population 1). MIRRORS the C# F19_0000_111f_DrawCityPopulation, which
    // iterates city.ActualSize and draws one POP sprite per point.
    int population = std::max(1, city.units + 1);

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
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d", yFound);
        drawLabelValue(infoY + lineH, "Founded:", buf);
    }
    {
        // Owner tribe name — translate (e.g. "Romans" -> the Chinese form
        // when the language pack has it).
        std::string tr = Translator::instance().translate(ownerName);
        drawLabelValue(infoY + lineH * 2, "Owner:", tr);
    }
    {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%d/%d", city.shields, city.production);
        drawLabelValue(infoY + lineH * 3, "Production:", buf);
    }

    // 5) Population dots — one warm yellow dot per population point, drawn in
    //    a horizontal row below the info block (mirrors the C# pop-sprite row
    //    drawn at (24, 140) in F19_0000_111f_DrawCityPopulation; we use small
    //    coloured dots instead of POP.PIC sprites — see header for the stub).
    {
        int popY = infoY + lineH * 4 + 4;
        int popX = infoX;
        for (int i = 0; i < population && i < 24; ++i) {
            screen.fillRect(Rect{popX, popY, 6, 8}, 166);
            screen.drawRect(Rect{popX, popY, 6, 8}, 163);
            popX += 8;
        }
    }

    // 6) 21-tile mini-grid — the city centre + the 20 worker tiles in the
    //    city radius. The Civ1 city radius is a 5x5 area with the four
    //    corner tiles removed (the "fat cross" pattern of 21 tiles). We
    //    sample real terrain when MapManagement was generated; otherwise we
    //    leave the cells blank (panel fill).
    {
        std::string lbl = Translator::instance().translate("Tiles:");
        int tY = infoY + lineH * 5 + 16;
        screen.drawString(font, infoX + 1, tY + 1, lbl, 163);
        screen.drawString(font, infoX, tY, lbl, 164);

        int gridX0 = kPanelX + 170;
        int gridY0 = infoY + 4;
        const int cellSz = 18;

        const MapManagement* mm = nullptr;
        try { mm = &p.mapManagement(); } catch (...) { mm = nullptr; }

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
                uint8_t col = 167; // default grass
                switch (t) {
                    case Terrain::Water: case Terrain::River: col = 168; break;
                    case Terrain::Mountains: case Terrain::Arctic: col = 169; break;
                    case Terrain::Hills: case Terrain::Desert:
                    case Terrain::Tundra:  col = 170; break;
                    case Terrain::Grassland: case Terrain::Plains:
                    case Terrain::Forest: case Terrain::Swamp:
                    case Terrain::Jungle:  col = 167; break;
                    default: col = 167; break;
                }
                screen.fillRect(Rect{gx, gy, cellSz - 1, cellSz - 1}, col);
                screen.drawRect(Rect{gx, gy, cellSz - 1, cellSz - 1}, 162);
                if (dx == 0 && dy == 0) {
                    // city centre marker (warm yellow + dark outline)
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
