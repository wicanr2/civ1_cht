#include "MiniWorld.h"
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
    : w_(w), h_(h), tiles_(std::size_t(w) * h, Terrain::Ocean) {
    // Two octaves of value noise -> an elevation field, thresholded to terrain.
    for (int y = 0; y < h_; ++y) {
        for (int x = 0; x < w_; ++x) {
            int e = (valueNoise(x, y, seed, 6) * 2 + valueNoise(x, y, seed ^ 0x5bd1e995u, 3)) / 3;
            // moisture field for grass/plains/desert split
            int m = valueNoise(x, y, seed ^ 0xA53Cu, 5);
            Terrain t;
            if (e < 90)       t = Terrain::Ocean;
            else if (e < 130) t = (m < 110 ? Terrain::Desert : Terrain::Plains);
            else if (e < 165) t = (m < 120 ? Terrain::Plains : Terrain::Grassland);
            else if (e < 195) t = Terrain::Forest;
            else if (e < 220) t = Terrain::Hills;
            else              t = Terrain::Mountains;
            at(x, y) = t;
        }
    }
    // Place the unit at the map centre, nudged onto the nearest non-ocean tile.
    unitX_ = w_ / 2;
    unitY_ = h_ / 2;
    if (terrainAt(unitX_, unitY_) == Terrain::Ocean) {
        for (int r = 1; r < std::max(w_, h_); ++r) {
            bool found = false;
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    int nx = unitX_ + dx, ny = unitY_ + dy;
                    if (nx >= 0 && ny >= 0 && nx < w_ && ny < h_ &&
                        terrainAt(nx, ny) != Terrain::Ocean) {
                        unitX_ = nx; unitY_ = ny; found = true;
                    }
                }
            if (found) break;
        }
    }
}

Terrain MiniWorld::terrainAt(int x, int y) const {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return Terrain::Ocean;
    return tiles_[std::size_t(y) * w_ + x];
}

const char* MiniWorld::terrainNameKey(Terrain t) {
    switch (t) {
        case Terrain::Ocean:     return "Ocean";
        case Terrain::Grassland: return "Grassland";
        case Terrain::Plains:    return "Plains";
        case Terrain::Forest:    return "Forest";
        case Terrain::Hills:     return "Hills";
        case Terrain::Mountains: return "Mountains";
        case Terrain::Desert:    return "Desert";
        default:                 return "Ocean";
    }
}

uint8_t MiniWorld::terrainColor(Terrain t) {
    // Custom indices (installed by draw()): distinct, readable map colours.
    switch (t) {
        case Terrain::Ocean:     return 200;
        case Terrain::Grassland: return 201;
        case Terrain::Plains:    return 202;
        case Terrain::Forest:    return 203;
        case Terrain::Hills:     return 204;
        case Terrain::Mountains: return 205;
        case Terrain::Desert:    return 206;
        default:                 return 200;
    }
}

// Map each terrain enum to a (col,row) 16x16 tile in TER257.PIC. These were
// picked by eyeballing the real tilesheet (320x200, 20 cols x 12 rows; top-left
// origin). Not perfect Civ1 fidelity — just visually-sensible base tiles:
//   row 0  = desert (tan dotted)        row 3  = forest (dense green)
//   row 4  = grassland/plains (green)   row 5  = mountains/hills (grey rocky)
//   row 11 = ocean (solid blue)
void MiniWorld::terrainTile(Terrain t, int& col, int& row) {
    switch (t) {
        case Terrain::Ocean:     col = 0; row = 11; break; // solid blue water
        case Terrain::Grassland: col = 0; row = 4;  break; // bright green
        case Terrain::Plains:    col = 1; row = 4;  break; // green (lighter variant)
        case Terrain::Forest:    col = 0; row = 3;  break; // dense green blobs
        case Terrain::Hills:     col = 1; row = 5;  break; // grey rocky
        case Terrain::Mountains: col = 0; row = 5;  break; // grey rocky (peak)
        case Terrain::Desert:    col = 0; row = 0;  break; // tan dotted
        default:                 col = 0; row = 11; break;
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
        fb.palette.set(200,  40,  60, 160); // Ocean
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

    const int hudH = 36;
    const int viewW = fb.width();
    const int viewH = fb.height() - hudH;
    const int cols = viewW / tileSize;
    const int rows = viewH / tileSize;

    // Camera: centre on the unit, clamped to map edges.
    int camX = unitX_ - cols / 2;
    int camY = unitY_ - rows / 2;
    camX = std::clamp(camX, 0, std::max(0, w_ - cols));
    camY = std::clamp(camY, 0, std::max(0, h_ - rows));

    fb.clear(208);

    for (int ry = 0; ry < rows; ++ry) {
        for (int rx = 0; rx < cols; ++rx) {
            int mx = camX + rx, my = camY + ry;
            int px = rx * tileSize, py = ry * tileSize;
            Terrain t = (mx < w_ && my < h_) ? terrainAt(mx, my) : Terrain::Ocean;
            if (useTiles) {
                int col, row;
                terrainTile(t, col, row);
                fb.drawBitmap(px, py, *tileset_,
                              Rect{col * 16, row * 16, 16, 16}, false);
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

    // ---- bottom HUD bar (all Chinese, via the translating drawString) ----
    const int hudY = fb.height() - hudH;
    fb.fillRect(Rect{0, hudY, fb.width(), hudH}, 208);
    fb.drawLine(0, hudY, fb.width() - 1, hudY, 207);

    const GFont& font = gd.font(fontId);
    int lineH = font.pixelHeight + font.lineSpacing;
    int tx = 4, ty = hudY + 2;

    // Line 1: "回合 N"  +  current tile terrain name  +  unit name.
    int penX = gd.drawString(GDriver::MainScreen, font, tx, ty, "Turn", 207);
    char num[16];
    std::snprintf(num, sizeof(num), " %d   ", turn_);
    penX = fb.drawString(font, penX, ty, num, 207); // numbers: no translation needed
    penX = gd.drawString(GDriver::MainScreen, font, penX, ty,
                         terrainNameKey(terrainAt(unitX_, unitY_)), 207);
    penX = fb.drawString(font, penX, ty, "  ", 207);
    gd.drawString(GDriver::MainScreen, font, penX, ty, unitName_, 207);

    // Line 2: help line.
    gd.drawString(GDriver::MainScreen, font, tx, ty + lineH,
                  "Arrow keys: move", 207);
    int hx = gd.getDrawStringSize(fontId, "Arrow keys: move").w;
    gd.drawString(GDriver::MainScreen, font, tx + hx + 8, ty + lineH,
                  "Enter: end turn  Esc: quit", 207);
}

} // namespace oc1
