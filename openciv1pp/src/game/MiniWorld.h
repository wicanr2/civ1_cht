// MiniWorld.h — a small, self-contained playable game world.
//
// NOTE(port): This is NOT a faithful port of a Civ1 CodeObject. It is an
// original, minimal "playable slice" built on top of the verified engine
// (GDriver/GBitmap/Translator) to demonstrate an interactive game loop with a
// scrollable tile map, a movable unit, turn advancement and a Chinese HUD. The
// map is generated procedurally from a seed (no copyrighted assets). All text
// goes through the existing translating drawString chokepoint so the HUD shows
// 繁體中文 when the Translator is enabled.
#pragma once
#include "../graphics/GBitmap.h"
#include "../graphics/GDriver.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oc1 {

enum class Terrain : uint8_t {
    Ocean = 0, Grassland, Plains, Forest, Hills, Mountains, Desert, Count
};

class MiniWorld {
public:
    // Build a W x H world deterministically from a seed.
    MiniWorld(int w, int h, uint32_t seed);

    // ---- pure / testable API ----
    int width()  const { return w_; }
    int height() const { return h_; }
    Terrain terrainAt(int x, int y) const;
    // English key for a terrain (the Translator looks this up). e.g. "Ocean".
    static const char* terrainNameKey(Terrain t);

    int unitX() const { return unitX_; }
    int unitY() const { return unitY_; }
    int turn()  const { return turn_; }
    const std::string& unitName() const { return unitName_; }

    // Move the unit by (dx,dy), clamped to map bounds. Returns true if the
    // position actually changed. (Ocean is enterable — kept simple.)
    bool moveUnit(int dx, int dy);

    // Advance the turn counter.
    void endTurn() { ++turn_; }

    // ---- real-asset tileset (optional) ----
    // Load TER257.PIC (terrain tilesheet) and, if present, SP257.PIC (unit
    // sprites) from a DOS-assets directory. Applies TER257's palette to the
    // render screen so the real Civ1 colours show. Returns true if TER257 loaded
    // (then draw() uses real tiles); false leaves the colored-rect fallback in
    // place. Safe to call when the assets are absent (returns false, no throw).
    bool loadTileset(const std::string& dosAssetDir);
    bool hasTileset() const { return tileset_ != nullptr; }
    // Dimensions of the loaded terrain tilesheet (0 if not loaded). For tests.
    int tilesetWidth()  const { return tileset_ ? tileset_->width()  : 0; }
    int tilesetHeight() const { return tileset_ ? tileset_->height() : 0; }

    // ---- rendering ----
    // Draw a viewport of tiles centred on the unit + a bottom Chinese HUD.
    // tileSize is the pixel size of each tile (e.g. 12). Camera clamps to edges.
    // When a tileset is loaded the map is rendered with the real Civ1 16x16
    // tiles (tileSize is forced to 16); otherwise it falls back to colored rects.
    void draw(GDriver& gd, int fontId, int tileSize = 12) const;

    // Palette index used to colour a terrain (also installed into the screen
    // palette by draw()).
    static uint8_t terrainColor(Terrain t);

private:
    int w_, h_;
    std::vector<Terrain> tiles_;
    int unitX_ = 0, unitY_ = 0;
    int turn_ = 1;
    std::string unitName_ = "Settlers";

    std::unique_ptr<GBitmap> tileset_;   // TER257.PIC (terrain), 320x200 grid of 16x16
    std::unique_ptr<GBitmap> sprites_;   // SP257.PIC (units), optional

    Terrain& at(int x, int y) { return tiles_[std::size_t(y) * w_ + x]; }

    // (col,row) of the 16x16 tile in TER257 for a terrain (eyeballed, see .cpp).
    static void terrainTile(Terrain t, int& col, int& row);
};

} // namespace oc1
