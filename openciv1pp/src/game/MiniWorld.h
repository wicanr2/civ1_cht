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
class OpenCiv1Game;  // fwd-decl (optional host: enables UnitManagement::buildCity)

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

    // Place the unit at an explicit map cell. Clamps to [0,w)x[0,h). Used by
    // the integrated --game flow to drop the Settlers on a chosen starting tile
    // (selected by FrontEndFlow after useRealGenerator). Returns true if the
    // coords were in bounds (i.e. the unit was placed exactly there); false if
    // they had to be clamped.
    bool setUnitPosition(int x, int y);

    // End-of-turn: increments the turn counter AND, when a host game is
    // attached, calls CheckPlayerTurn::processEndOfTurn() (per-civ city pass:
    // shield accumulation + threshold-triggered unit production, then advances
    // GameData.Year via the ported Segment_1238 year-step ladder). Headless
    // (no host) endTurn() is still a simple turn_++ — the playtest covers that.
    void endTurn();

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
    // return false (no movement). When the click HITS a tile with a founded
    // city AND a host game is attached, the city view is opened instead and
    // the unit does not move.
    bool handleMouseClick(int fbX, int fbY);

    // Tile-coords variant of handleMouseClick — exposed so the integrated
    // game loop / tests can drive the click logic without going through the
    // framebuffer math. Returns the same true/false as handleMouseClick.
    bool handleMapClick(int tileX, int tileY);

    // Returns the cityId at (tileX,tileY) when a city is there, else -1.
    int cityIdAt(int tileX, int tileY) const;

    // ---- real-asset tileset (optional) ----
    bool loadTileset(const std::string& dosAssetDir);
    bool hasTileset() const { return tileset_ != nullptr; }
    int  tilesetWidth()  const { return tileset_ ? tileset_->width()  : 0; }
    int  tilesetHeight() const { return tileset_ ? tileset_->height() : 0; }

    // ---- rendering ----
    void draw(GDriver& gd, int fontId, int tileSize = 12) const;

    // Render every founded city as a marker on its map tile. Called by draw()
    // when a host game has been attached (attachGame), AFTER terrain and BEFORE
    // the unit marker. Public for direct test access. Uses the SP257 city
    // sprite when available, else a distinct colored rect with a "C" overlay.
    void renderCities(GBitmap& screen) const;

    // Render all multi-civ units (the AI civs' Settlers + the human Settlers
    // when the host game has been attached and setupCivs/addUnit have been
    // called). Each unit is drawn at its tile with the OWNER civ's distinct
    // palette colour (kCivMarkerIndex from UnitManagement.cpp). When no host
    // game is attached, this is a no-op and the legacy single-unit draw path
    // still renders the (unitX_, unitY_) marker by itself.
    void renderUnits(GBitmap& screen) const;

    static uint8_t terrainColor(Terrain t);

    // ---- city management (host-game wire-up) ----
    // Attach the OpenCiv1Game host so MiniWorld can call its UnitManagement
    // (e.g. for the B-key build-city action). Also installs a terrain provider
    // (closes over `this`) on UnitManagement so it can validate Water/Arctic.
    void attachGame(OpenCiv1Game& g);
    OpenCiv1Game* game() const { return game_; }

    // Player-driven Build City action (B key). Returns true if a city was
    // founded; outName receives the name. No-op (returns false) when no host
    // game is attached or the current tile is invalid (Water/Arctic).
    bool buildCityAtUnit(std::string& outName, int playerId = 0);

    // Last action message to show on the HUD second line (English key, run
    // through the Translator at draw time).
    const std::string& lastActionKey() const { return lastActionKey_; }
    void setLastActionKey(std::string k) { lastActionKey_ = std::move(k); }
    const std::string& lastCityName() const { return lastCityName_; }

private:
    int w_, h_;
    std::vector<Terrain> tiles_;
    int unitX_ = 0, unitY_ = 0;
    int turn_ = 1;
    std::string unitName_ = "Settlers";

    std::unique_ptr<GBitmap> tileset_;
    std::unique_ptr<GBitmap> sprites_;

    OpenCiv1Game* game_ = nullptr;        // optional host (for UnitManagement)
    std::string lastActionKey_;           // shown on HUD line 2 when non-empty
    std::string lastCityName_;            // last founded city's resolved name

    bool usesRealGenerator_ = false;

    // Cached viewport math from the last draw() — used by screenToTile() to
    // invert the same camera/tile-size mapping. mutable so the const draw()
    // can update it. Default values match the headless "draw never called"
    // case: tileSize=16, camX/camY = top-left, viewW/H = full screen.
    mutable int lastTileSize_ = 16;
    mutable int lastCamX_ = 0, lastCamY_ = 0;
    mutable int lastViewW_ = 320, lastViewH_ = 200 - 56;

    Terrain& at(int x, int y) { return tiles_[std::size_t(y) * w_ + x]; }
    void placeUnitNearCenter();
};

} // namespace oc1
