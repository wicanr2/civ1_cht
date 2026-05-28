#include "MiniWorld.h"
#include "MapManagement.h"
#include "OpenCiv1Game.h"
#include "UnitManagement.h"
#include "CheckPlayerTurn.h"
#include "../resource/PicLoader.h"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>

namespace oc1 {

namespace {

// A small integer hash (deterministic, seed-mixed) used to build value noise.
// No floating point so the map is bit-reproducible across platforms.
uint32_t hash2(int x, int y, uint32_t seed) {
    uint32_t h = seed;
    h ^= uint32_t(x) * 0x9E3779B1u;
    h ^= uint32_t(y) * 0x85EBCA77u;
    h *= 0xC2B2AE3Du;
    h ^= h >> 15;
    h *= 0x27D4EB2Fu;
    h ^= h >> 13;
    return h;
}

// Smoothed value noise in [0,255]: bilinear-ish blend of lattice samples at a
// fixed cell size. Integer-only.
int valueNoise(int x, int y, uint32_t seed, int cell) {
    int gx = x / cell, gy = y / cell;
    int fx = x % cell, fy = y % cell;
    auto n = [&](int ix, int iy) { return int(hash2(ix, iy, seed) & 0xFF); };
    int n00 = n(gx, gy),     n10 = n(gx + 1, gy);
    int n01 = n(gx, gy + 1), n11 = n(gx + 1, gy + 1);
    int top = n00 + (n10 - n00) * fx / cell;
    int bot = n01 + (n11 - n01) * fx / cell;
    return top + (bot - top) * fy / cell;
}

} // namespace

MiniWorld::MiniWorld(int w, int h, uint32_t seed)
    : w_(w), h_(h), tiles_(std::size_t(w) * h, Terrain::Water) {
    // Two octaves of value noise -> an elevation field, thresholded to terrain.
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            int e = (valueNoise(x, y, seed, 6) * 2 + valueNoise(x, y, seed ^ 0x5bd1e995u, 3)) / 3;
            // moisture field for grass/plains/desert split
            int m = valueNoise(x, y, seed ^ 0xA53Cu, 5);
            Terrain t;
            if (e < 90)       t = Terrain::Water;
            else if (e < 130) t = (m < 110 ? Terrain::Desert : Terrain::Plains);
            else if (e < 165) t = (m < 120 ? Terrain::Plains : Terrain::Grassland);
            else if (e < 195) t = Terrain::Forest;
            else if (e < 220) t = Terrain::Hills;
            else              t = Terrain::Mountains;
            at(x, y) = t;
        }
    }
    // Place the unit at the map centre, nudged onto the nearest non-water tile.
    unitX_ = w_ / 2;
    unitY_ = h_ / 2;
    if (terrainAt(unitX_, unitY_) == Terrain::Water) {
        for (int r = 1; r < std::max(w_, h_); ++r) {
            bool found = false;
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    int nx = unitX_ + dx, ny = unitY_ + dy;
                    if (nx >= 0 && ny >= 0 && nx < w_ && ny < h_ &&
                        terrainAt(nx, ny) != Terrain::Water) {
                        unitX_ = nx; unitY_ = ny; found = true;
                    }
                }
            if (found) break;
        }
    }
}

bool MiniWorld::useRealGenerator(MapManagement& mm, uint32_t seed) {
    mm.generate(int32_t(seed));
    w_ = mm.width();
    h_ = mm.height();
    tiles_.assign(std::size_t(w_) * h_, Terrain::Water);
    for (int y = 0; y < h_; ++y)
        for (int x = 0; x < w_; ++x)
            at(x, y) = mm.terrainAt(x, y);
    usesRealGenerator_ = true;
    // Place the unit at the map centre, nudged onto the nearest non-water tile.
    placeUnitNearCenter();
    return true;
}

void MiniWorld::placeUnitNearCenter() {
    unitX_ = w_ / 2;
    unitY_ = h_ / 2;
    if (terrainAt(unitX_, unitY_) == Terrain::Water) {
        for (int r = 1; r < std::max(w_, h_); ++r) {
            bool found = false;
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    int nx = unitX_ + dx, ny = unitY_ + dy;
                    if (nx >= 0 && ny >= 0 && nx < w_ && ny < h_ &&
                        terrainAt(nx, ny) != Terrain::Water) {
                        unitX_ = nx; unitY_ = ny; found = true;
                    }
                }
            if (found) break;
        }
    }
}

Terrain MiniWorld::terrainAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return Terrain::Water;
    return tiles_[std::size_t(y) * w_ + x];
}

// terrainNameKey is provided as an inline forwarder in MiniWorld.h that calls
// oc1::terrainNameKey (defined in TerrainTiles.cpp). No definition needed here.

uint8_t MiniWorld::terrainColor(Terrain t) {
    // Custom indices (installed by draw()): distinct, readable map colours used
    // ONLY in the colored-rect fallback (no real tileset). The 7 indices match
    // the original MiniWorld terrains; the additional Civ1 terrains
    // (Tundra/Arctic/Swamp/Jungle/River) fall through to Water for the
    // fallback view — the real tileset draws them faithfully via TER257.
    switch (t) {
        case Terrain::Water:     return 200;
        case Terrain::Grassland: return 201;
        case Terrain::Plains:    return 202;
        case Terrain::Forest:    return 203;
        case Terrain::Hills:     return 204;
        case Terrain::Mountains: return 205;
        case Terrain::Desert:    return 206;
        default:                 return 200;
    }
}

bool MiniWorld::loadTileset(const std::string& dosAssetDir) {
    if (dosAssetDir.empty()) return false;
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string terPath = (fs::path(dosAssetDir) / "TER257.PIC").string();
    if (!fs::exists(terPath, ec)) return false;
    std::unique_ptr<GBitmap> ter = loadPicFile(terPath, true);
    if (!ter) return false;
    tileset_ = std::move(ter);

    // SP257.PIC is optional (unit sprites); SPRITES.PIC is a fallback name.
    for (const char* name : {"SP257.PIC", "SPRITES.PIC"}) {
        const std::string p = (fs::path(dosAssetDir) / name).string();
        if (fs::exists(p, ec)) {
            std::unique_ptr<GBitmap> sp = loadPicFile(p, true);
            if (sp) { sprites_ = std::move(sp); break; }
        }
    }
    return true;
}

bool MiniWorld::screenToTile(int fbX, int fbY, int& tileX, int& tileY) const {
    // Inverse of the draw() viewport: top-left of the visible map area is (0,0)
    // in framebuffer coords; HUD bar sits BELOW the map (y >= lastViewH_).
    if (lastTileSize_ <= 0) return false;
    if (fbX < 0 || fbY < 0 || fbX >= lastViewW_ || fbY >= lastViewH_) return false;
    int tx = lastCamX_ + fbX / lastTileSize_;
    int ty = lastCamY_ + fbY / lastTileSize_;
    if (tx < 0 || ty < 0 || tx >= w_ || ty >= h_) return false;
    tileX = tx; tileY = ty;
    return true;
}

bool MiniWorld::handleMouseClick(int fbX, int fbY) {
    int tx = 0, ty = 0;
    if (!screenToTile(fbX, fbY, tx, ty)) return false; // HUD / off-map: no move
    int dx = (tx > unitX_) - (tx < unitX_); // sign in {-1,0,+1}
    int dy = (ty > unitY_) - (ty < unitY_);
    if (dx == 0 && dy == 0) return false; // click on the unit itself
    return moveUnit(dx, dy);
}

void MiniWorld::attachGame(OpenCiv1Game& g) {
    game_ = &g;
    auto& um = g.unitManagement();
    um.setMapBounds(w_, h_);
    um.setTerrainProvider([this](int x, int y) { return this->terrainAt(x, y); });
}

bool MiniWorld::buildCityAtUnit(std::string& outName, int playerId) {
    if (!game_) return false;
    auto& um = game_->unitManagement();
    um.setMapBounds(w_, h_);
    if (um.buildCity(unitX_, unitY_, playerId, turn_, outName)) {
        lastActionKey_ = "Build City";
        lastCityName_ = outName;
        return true;
    }
    return false;
}

void MiniWorld::renderCities(GBitmap& screen) const {
    if (!game_) return;
    const auto& cities = game_->unitManagement().cities();
    if (cities.empty()) return;

    const int tileSize = lastTileSize_;
    const int camX = lastCamX_, camY = lastCamY_;
    const int cols = (lastViewW_ + tileSize - 1) / tileSize;
    const int rows = (lastViewH_ + tileSize - 1) / tileSize;
    // Distinct palette entries for the city marker (rect + outline + glyph).
    screen.palette.set(211, 250, 220, 90);   // city fill (warm yellow)
    screen.palette.set(212,  50,  30, 10);   // city outline (dark)
    for (const auto& c : cities) {
        int rx = c.x - camX, ry = c.y - camY;
        if (rx < 0 || ry < 0 || rx >= cols || ry >= rows) continue;
        int px = rx * tileSize, py = ry * tileSize;
        if (sprites_) {
            // SP257.PIC city sprite — first city graphic (col 0, row 1 of the
            // 16x16 sprite grid). Transparent (index 0).
            screen.drawBitmap(px, py, *sprites_,
                              Rect{0, 16, 16, 16}, true);
        } else {
            int inset = std::max(1, tileSize / 6);
            screen.fillRect(Rect{px + inset, py + inset,
                                 tileSize - 2 * inset,
                                 tileSize - 2 * inset}, 211);
            screen.drawRect(Rect{px + inset, py + inset,
                                 tileSize - 2 * inset,
                                 tileSize - 2 * inset}, 212);
            // A small "+" cross inside for visual distinction from the unit.
            int cx = px + tileSize / 2, cy = py + tileSize / 2;
            screen.drawLine(cx - inset, cy, cx + inset, cy, 212);
            screen.drawLine(cx, cy - inset, cx, cy + inset, 212);
        }
    }
}

void MiniWorld::endTurn() {
    ++turn_;
    // When a host game is attached, run the per-turn housekeeping pass
    // (per-civ city loop -> shields/units; then GameData.Year advance).
    if (game_) game_->checkPlayerTurn().processEndOfTurn();
}

bool MiniWorld::setUnitPosition(int x, int y) {
    int cx = std::clamp(x, 0, w_ - 1);
    int cy = std::clamp(y, 0, h_ - 1);
    unitX_ = cx;
    unitY_ = cy;
    return (cx == x && cy == y);
}

bool MiniWorld::moveUnit(int dx, int dy) {
    int nx = std::clamp(unitX_ + dx, 0, w_ - 1);
    int ny = std::clamp(unitY_ + dy, 0, h_ - 1);
    if (nx == unitX_ && ny == unitY_) return false;
    unitX_ = nx;
    unitY_ = ny;
    return true;
}

void MiniWorld::draw(GDriver& gd, int fontId, int tileSize) const {
    GBitmap& fb = gd.screen(GDriver::MainScreen);

    const bool useTiles = (tileset_ != nullptr);
    // With real tiles, install TER257's palette so the Civ1 colours show, then
    // render the 16x16 tiles. The HUD/marker indices (207-210) are re-set AFTER
    // the palette copy so HUD text + the unit marker still read regardless.
    if (useTiles) {
        fb.copyPaletteFrom(*tileset_);
        tileSize = 16; // real tiles are 16x16
    } else {
        // Fallback: install the synthetic terrain palette colours.
        fb.palette.set(200,  40,  60, 160); // Water
        fb.palette.set(201,  70, 170,  60); // Grassland
        fb.palette.set(202, 150, 170,  70); // Plains
        fb.palette.set(203,  30, 110,  40); // Forest
        fb.palette.set(204, 150, 120,  70); // Hills
        fb.palette.set(205, 120, 120, 120); // Mountains
        fb.palette.set(206, 200, 180, 110); // Desert
    }
    fb.palette.set(207, 255, 255, 255); // grid / HUD text
    fb.palette.set(208,  20,  20,  30); // HUD bar background
    fb.palette.set(209, 230,  40,  40); // unit marker
    fb.palette.set(210,   0,   0,   0); // grid lines (dark)

    const int hudH = 56; // room for 3 HUD text lines (turn/help/cities)
    const int viewW = fb.width();
    const int viewH = fb.height() - hudH;
    const int cols = viewW / tileSize;
    const int rows = viewH / tileSize;

    // Camera: centre on the unit, clamped to map edges.
    int camX = unitX_ - cols / 2;
    int camY = unitY_ - rows / 2;
    camX = std::clamp(camX, 0, std::max(0, w_ - cols));
    camY = std::clamp(camY, 0, std::max(0, h_ - rows));

    // Cache the viewport math so screenToTile() / handleMouseClick() invert
    // exactly this mapping.
    lastTileSize_ = tileSize;
    lastCamX_ = camX; lastCamY_ = camY;
    lastViewW_ = viewW; lastViewH_ = viewH;

    fb.clear(208);

    for (int ry = 0; ry < rows; ++ry) {
        for (int rx = 0; rx < cols; ++rx) {
            int mx = camX + rx, my = camY + ry;
            int px = rx * tileSize, py = ry * tileSize;
            Terrain t = (mx < w_ && my < h_) ? terrainAt(mx, my) : Terrain::Water;
            if (useTiles) {
                // Faithful TER257 base tile from TerrainTiles.h (1:1 with
                // Array_b886[terrain,0] in OpenCiv1 StartGameMenu).
                TileXY tile = terrainToTileXY(t);
                fb.drawBitmap(px, py, *tileset_,
                              Rect{tile.col * 16, tile.row * 16, 16, 16}, false);
            } else {
                fb.fillRect(Rect{px, py, tileSize, tileSize}, terrainColor(t));
                fb.drawRect(Rect{px, py, tileSize, tileSize}, 210); // grid
            }
            if (mx == unitX_ && my == unitY_) {
                if (useTiles && sprites_) {
                    // Settlers sprite: SP257 tile (col 0, row 10). Drawn
                    // transparently (index 0 = transparent) over the terrain.
                    fb.drawBitmap(px, py, *sprites_,
                                  Rect{0, 160, 16, 16}, true);
                } else {
                    // distinct unit marker: filled inset square
                    int inset = std::max(2, tileSize / 4);
                    fb.fillRect(Rect{px + inset, py + inset,
                                     tileSize - 2 * inset, tileSize - 2 * inset}, 209);
                }
            }
        }
    }

    // Cities pass: AFTER terrain (and unit), BEFORE the HUD bar. Visually the
    // city marker REPLACES the Settlers sprite on its tile (Civ1 behaviour:
    // the Settlers is consumed when the city is founded).
    renderCities(fb);

    // ---- bottom HUD bar (all Chinese, via the translating drawString) ----
    const int hudY = fb.height() - hudH;
    fb.fillRect(Rect{0, hudY, fb.width(), hudH}, 208);
    fb.drawLine(0, hudY, fb.width() - 1, hudY, 207);

    const GFont& font = gd.font(fontId);
    int lineH = font.pixelHeight + font.lineSpacing;
    int tx = 4, ty = hudY + 2;

    // Line 1: "回合 N (年份: <year>)"  +  terrain name  +  unit name.
    int penX = gd.drawString(GDriver::MainScreen, font, tx, ty, "Turn", 207);
    char num[16];
    std::snprintf(num, sizeof(num), " %d  ", turn_);
    penX = fb.drawString(font, penX, ty, num, 207); // numbers: no translation needed
    // Year string: GameData.Year < 0 -> "<n> BC", > 0 -> "<n> AD". Mirrors
    // Segment_1238.cs F0_1238_*_FormatYear ("{Math.Abs(Year)} { (Year>=0)?AD:BC }").
    if (game_) {
        penX = gd.drawString(GDriver::MainScreen, font, penX, ty, "Year:", 207);
        int yr = game_->unitManagement().year();
        char yb[24];
        std::snprintf(yb, sizeof(yb), "%d ", yr < 0 ? -yr : yr);
        penX = fb.drawString(font, penX, ty, yb, 207);
        penX = gd.drawString(GDriver::MainScreen, font, penX, ty,
                             yr < 0 ? "BC" : "AD", 207);
        penX = fb.drawString(font, penX, ty, "  ", 207);
    }
    penX = gd.drawString(GDriver::MainScreen, font, penX, ty,
                         terrainNameKey(terrainAt(unitX_, unitY_)), 207);
    penX = fb.drawString(font, penX, ty, "  ", 207);
    gd.drawString(GDriver::MainScreen, font, penX, ty, unitName_, 207);

    // Line 2: help line.
    gd.drawString(GDriver::MainScreen, font, tx, ty + lineH,
                  "Arrow keys: move", 207);
    int hx = gd.getDrawStringSize(fontId, "Arrow keys: move").w;
    int penX2 = tx + hx + 8;
    penX2 = gd.drawString(GDriver::MainScreen, font, penX2, ty + lineH,
                          "Enter: end turn  Esc: quit", 207);

    // Line 3: "城市:" count, then (when set) the last action + city name. The
    // city count is always shown so the HUD changes the moment a city is
    // founded; the Translator turns "Cities:" -> "城市: " and "Build City"
    // -> "建立城市" at this single drawString chokepoint.
    int line3Y = ty + lineH * 2;
    std::size_t nCities = game_ ? game_->unitManagement().cityCount() : 0;
    int penX3 = gd.drawString(GDriver::MainScreen, font, tx, line3Y,
                              "Cities:", 207);
    char cnum[16];
    std::snprintf(cnum, sizeof(cnum), " %zu", nCities);
    penX3 = fb.drawString(font, penX3, line3Y, cnum, 207);
    if (!lastActionKey_.empty()) {
        penX3 = fb.drawString(font, penX3, line3Y, "   ", 207);
        penX3 = gd.drawString(GDriver::MainScreen, font, penX3, line3Y,
                              lastActionKey_, 207);
        if (!lastCityName_.empty()) {
            penX3 = fb.drawString(font, penX3, line3Y, ": ", 207);
            // City name itself: also translated when the Translator has an
            // entry for it (e.g. "Capital" -> "首都").
            gd.drawString(GDriver::MainScreen, font, penX3, line3Y,
                          lastCityName_, 207);
        }
    }
    // Production line: when at least one city is founded, append the FIRST
    // city's shields/production threshold (the "current city" placeholder
    // until per-city focus is wired). Mirrors the HUD line CityWorker draws
    // for the active city ("Production: SHIELDS/COST").
    if (nCities > 0) {
        const auto& c0 = game_->unitManagement().cities()[0];
        penX3 = fb.drawString(font, penX3, line3Y, "   ", 207);
        penX3 = gd.drawString(GDriver::MainScreen, font, penX3, line3Y,
                              "Production:", 207);
        char pbuf[32];
        std::snprintf(pbuf, sizeof(pbuf), " %d/%d", c0.shields, c0.production);
        fb.drawString(font, penX3, line3Y, pbuf, 207);
    }
}

} // namespace oc1
