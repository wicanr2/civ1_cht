// MiniWorld.h — a small, playable game world wired to either:
//   (a) the faithful Civ1 world-generator (MapManagement::generateMap, ported
//       from OpenCiv1's F7_0000_0012_GenerateMap), OR
//   (b) the original procedural-noise terrain (kept as a fallback so the play
//       loop still works when MapManagement isn't supplied).
//
// In both modes the terrain is drawn with the FAITHFUL TER257.PIC base tile
// returned by TerrainTiles.h's terrainToTileXY(Terrain) — replacing the older
// eyeballed (col,row) guesses. The HUD/turn/unit loop are unchanged.
//
// The Terrain enum now uses the canonical 12-value Civ1 enum from
// TerrainTiles.h (Water/River/etc.) — 1:1 with OpenCiv1's TerrainTypeEnum.
#pragma once
#include "../graphics/GBitmap.h"
#include "../graphics/GDriver.h"
#include "TerrainTiles.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oc1 {

class MapManagement; // fwd-decl (optional source of the terrain grid)

class MiniWorld {
public:
    // Build a W x H world deterministically from a seed using the original
    // procedural-noise generator (kept as a fallback / no-deps default).
    MiniWorld(int w, int h, uint32_t seed);

    // Build a world from a MapManagement instance whose terrain grid has
    // already been populated (e.g. via generateMap()). The world copies the
    // (80x50 by default) grid and uses it for rendering. The unit is placed
    // on the nearest non-water tile to the map centre.
    MiniWorld(const MapManagement& mm, uint32_t unitSeed = 0);

    // Re-populate this world's terrain grid from the faithful Civ1 world
    // generator (MapManagement::generate(seed)). Resizes to MapManagement's
    // 80x50 if needed. After this the unit is placed at the nearest non-water
    // tile to the map centre, and `usesRealGenerator()` returns true. The
    // value-noise terrain (set by the (w,h,seed) ctor) is REPLACED.
    bool useRealGenerator(MapManagement& mm, uint32_t seed);
    bool usesRealGenerator() const { return usesRealGenerator_; }

    // ---- pure / testable API ----
    int width()  const { return w_; }
    int height() const { return h_; }
    Terrain terrainAt(int x, int y) const;
    static const char* terrainNameKey(Terrain t) { return oc1::terrainNameKey(t); }

    int unitX() const { return unitX_; }
    int unitY() const { return unitY_; }
    int turn()  const { return turn_; }
    const std::string& unitName() const { return unitName_; }

    bool moveUnit(int dx, int dy);
    void endTurn() { ++turn_; }

    // ---- mouse input (hit-test + dispatch) ----
    // Inverse of the draw() viewport math: convert framebuffer pixel coords to
    // map tile coords. Uses the same camera/tile-size used by the last draw()
    // (cached so the inverse is consistent regardless of how it was called).
    // Returns false (and leaves tileX/tileY at whatever was passed in) for
    // clicks that fall outside the map viewport (e.g. on the HUD bar).
    bool screenToTile(int fbX, int fbY, int& tileX, int& tileY) const;

    // Move the unit ONE step toward the clicked tile. Delta sign of
    // tileX-unitX / tileY-unitY, clamped to {-1,0,1} on each axis (so this
    // composes with the existing moveUnit(dx,dy) step model). Returns true if
    // the unit actually moved. Clicks on the HUD or on the unit's own tile
    // return false (no movement).
    bool handleMouseClick(int fbX, int fbY);

    // ---- real-asset tileset (optional) ----
    bool loadTileset(const std::string& dosAssetDir);
    bool hasTileset() const { return tileset_ != nullptr; }
    int  tilesetWidth()  const { return tileset_ ? tileset_->width()  : 0; }
    int  tilesetHeight() const { return tileset_ ? tileset_->height() : 0; }

    // ---- rendering ----
    void draw(GDriver& gd, int fontId, int tileSize = 12) const;

    static uint8_t terrainColor(Terrain t);

private:
    int w_, h_;
    std::vector<Terrain> tiles_;
    int unitX_ = 0, unitY_ = 0;
    int turn_ = 1;
    std::string unitName_ = "Settlers";

    std::unique_ptr<GBitmap> tileset_;
    std::unique_ptr<GBitmap> sprites_;

    bool usesRealGenerator_ = false;

    // Cached viewport math from the last draw() — used by screenToTile() to
    // invert the same camera/tile-size mapping. mutable so the const draw()
    // can update it. Default values match the headless "draw never called"
    // case: tileSize=16, camX/camY = top-left, viewW/H = full screen.
    mutable int lastTileSize_ = 16;
    mutable int lastCamX_ = 0, lastCamY_ = 0;
    mutable int lastViewW_ = 320, lastViewH_ = 200 - 36;

    Terrain& at(int x, int y) { return tiles_[std::size_t(y) * w_ + x]; }
    void placeUnitNearCenter();
};

} // namespace oc1
