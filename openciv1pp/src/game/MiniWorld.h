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

    Terrain& at(int x, int y) { return tiles_[std::size_t(y) * w_ + x]; }
    void placeUnitNearCenter();
};

} // namespace oc1
