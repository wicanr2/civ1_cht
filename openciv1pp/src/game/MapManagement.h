// MapManagement.h — ported CodeObject (OpenCiv1 MapManagement.cs + the
// world-generator entry F7_0000_0012_GenerateMap from MapInitAndIntro.cs).
//
// This C++ port covers the FAITHFUL Civ1 world-generator:
//   - 80x50 cell grid
//   - Stage 1: "cloud" continents (random-walk blobs, Plains/Hills/Mountains
//              stacked where blobs overlap)
//   - Stage 2: temperature pass (Desert/Plains/Tundra/Arctic distribution by Y)
//   - Stage 3: two horizontal climate sweeps (Grassland/Forest, Jungle/Swamp,
//              Desert->Plains, ...)
//   - Stage 4: planet-age erosion (Forest->Jungle, Hills->Mountains, occasional
//              Mountains->Water, ...)
//   - Stage 5: rivers (start on Hills, walk until they hit Water/River, restore
//              the saved map on failure; surrounding Forest -> Jungle on
//              success). The river-improvement-flag pass (the Flag80 bookkeeping
//              the C# does via F0_2aea_1653_SetTerrainImprovements) is STUBBED
//              here — see // TODO(port).
//
// The terrain grid lives in VCPU memory at MAP_SEG:MAP_OFF (one byte per cell,
// in the same encoding the C# uses — TerrainTypeToPixelValues[]). This mirrors
// the C# GBitmap-as-VCPU-memory layout: layer 1 = terrain data at (0,0). The
// generator reads/writes via cpu.ReadUInt8/WriteUInt8 so the storage IS the VCPU
// memory model, identical in shape to the reference.
//
// STUBS (each has a // TODO(port): marker in the .cpp):
//   - LoadEarthMap (we don't bundle map.pic — the user can swap it in later).
//   - GetPlayerLandOwnership / SetCityOwner / improvements / minimap / resources
//     / minor-tribe huts / build-location scoring / group IDs (Stage 6) / AI
//     starting positions / unit cell bookkeeping. Those are deep dependencies
//     of the playable game, not of the world-generator's terrain output.
//   - F7_0000_17cf_AdvanceAnimation — the per-cell progress animation; the
//     C# uses it to draw the in-progress map. Headless: no-op.
//   - The RandomMT19937 used here is a faithful re-implementation of OpenCiv1's
//     RandomMT19937 (1:1 with IRB/RNG/RandomMT19937.cs), so the deterministic
//     `seed -> map` pairing matches the C# reference bit-for-bit.
#pragma once
#include "OpenCiv1Game.h"
#include "TerrainTiles.h"
#include <cstdint>
#include <utility>
#include <vector>

namespace oc1 {

// Faithful port of OpenCiv1's IRB.RNG.RandomMT19937 (Mersenne Twister MT19937,
// 2002/01/26 initialization). Same Next(maxValue) semantics: floor(Sample() *
// maxValue) with Sample() == InternalSample() * 2^-32.
class RandomMT19937 {
public:
    explicit RandomMT19937(int32_t seed);
    uint32_t UNext();
    int32_t  Next();
    int32_t  Next(int32_t maxValue);
    int32_t  Next(int32_t minValue, int32_t maxValue);
    double   NextDouble();
private:
    static constexpr int kMatrixLength = 624;
    static constexpr int kMatrixMedian = 397;
    uint32_t matrix_[kMatrixLength] = {};
    int matrixIndex_ = kMatrixLength;
    uint32_t internalSample();
    double   sample();
};

class MapManagement {
public:
    // Map dimensions (1:1 with C#: GSize Size = (80, 50)).
    static constexpr int kWidth  = 80;
    static constexpr int kHeight = 50;
    // Improvement bitflag values — a faithful subset of OpenCiv1's
    // TerrainImprovementFlagsEnum (see
    // OpenCiv1/src/Game/State/Definitions/TerrainImprovementFlagsEnum.cs):
    // Irrigation=0x2, Mines=0x4, Road=0x8. We keep the SAME numeric bit
    // positions so a future widening (RailRoad=0x10, etc.) is a straight
    // extension. (The full enum also has City=0x1 / Pollution=0x40 / Flag80;
    // those are not yet modeled by this port.)
    static constexpr uint8_t kImprovementNone       = 0x0;
    static constexpr uint8_t kImprovementIrrigation = 0x2;
    static constexpr uint8_t kImprovementMine       = 0x4;
    static constexpr uint8_t kImprovementRoad       = 0x8;
    // VCPU memory location of the terrain grid (layer 1 in the C# GBitmap
    // layout). 80*50 = 4000 bytes. Chosen at a DS offset that doesn't overlap
    // the other ported CodeObjects' working areas (the LanguageTools/CAPI
    // string buffers live at 0xba06+).
    static constexpr uint16_t kMapSeg = 0x4000;
    static constexpr uint16_t kMapOff = 0x0000;

    explicit MapManagement(OpenCiv1Game& parent);

    // ---- C# API parity ----
    int  AdjustXPosition(int xPos) const;
    bool F0_2aea_1326_ValidateMapCoordinates(int x, int y) const;
    Terrain GetTerrainType(int x, int y) const;
    void    SetTerrainType(int x, int y, Terrain t);

    // ---- World generator (entry, faithful port of MapInitAndIntro.cs
    //      F7_0000_0012_GenerateMap, excluding the EarthMap branch) ----
    //
    // Inputs that the C# reads from Var_7ef6/7ef8/7efa/7efc (planet land-mass,
    // temperature, climate, age — 0..2 each, default 1) and Var_d76a_EarthMap
    // are passed here as a settings struct so the port is self-contained and
    // testable. The seed routes through RandomMT19937 1:1 with the reference.
    struct GenSettings {
        int32_t seed       = 12345;
        int planetLandMass  = 1; // 0..2
        int planetTemperature = 1;
        int planetClimate   = 1;
        int planetAge       = 1;
    };
    void generate(const GenSettings& s);
    void generate(int32_t seed) { generate(GenSettings{seed, 1, 1, 1, 1}); }

    // ---- C++ accessors mirroring (kWidth, kHeight, terrainAt) so MiniWorld
    //      can render the generator's output without depending on the C# pixel
    //      encoding ----
    int width()  const { return kWidth; }
    int height() const { return kHeight; }
    Terrain terrainAt(int x, int y) const { return GetTerrainType(x, y); }

    // Clear the entire terrain grid to Water (the C# CreateNewEmptyMap fills
    // it with pixel-value 1 == Water).
    void CreateNewEmptyMap();

    // ---- improvements grid -------------------------------------------------
    // A parallel uint8_t grid (sized kWidth*kHeight, default 0) holding the
    // per-tile TerrainImprovementFlagsEnum bitmask (see kImprovement* above).
    // generate() resets it to all zeros so a fresh world starts with no
    // improvements. setImprovementFlag OR-s in `flag` (use kImprovementRoad
    // / kImprovementIrrigation). getImprovements returns the raw byte.
    // Out-of-bounds reads return 0; out-of-bounds writes are no-ops.
    uint8_t getImprovements(int x, int y) const;
    void    setImprovementFlag(int x, int y, uint8_t flag);
    void    clearImprovements();
    // Direct-write accessor used by GameLoadAndSave to restore the grid
    // byte-for-byte from a hex dump (see kImprovement* bitflags above).
    void    setImprovementsRaw(int x, int y, uint8_t bits);

    // TODO(port): LoadEarthMap — needs the bundled map.pic asset, not part of
    // the generator path.

private:
    OpenCiv1Game& p_;
    VCPU& cpu_;
    // Improvements grid (parallel to the VCPU terrain bytes); separate
    // from the VCPU layer to keep the terrain memory layout 1:1 with the
    // C# pixel-encoded map. Sized kWidth*kHeight, row-major (y*kWidth+x).
    std::vector<uint8_t> improvements_;

    static const int kTerrainToPixel[12];
    static const int8_t kPixelToTerrain[16];

    static uint16_t cellAddr(int x, int y) {
        return uint16_t(kMapOff + y * kWidth + x);
    }
};

// Civ1 move directions — 49 entries, 1:1 with OpenCiv1.MoveDirections in
// OpenCiv1GameGlobals.cs (the GPoint table used by stages 4-5 of the world
// generator). Index 0 is "no move"; 1..8 = the 8 adjacent cells; 9..24 = the
// 2-cell ring; 25..48 = the 3-cell ring.
struct MoveDir { int x, y; };
extern const MoveDir kMoveDirections[49];

// Place N starting positions across `mm`'s generated map. Mirrors the C#
// StartGameMenu.F5_0000_* placement loop (line 540-588 of StartGameMenu.cs):
// uniformly random (x in [8, kWidth-8), y in [8, kHeight-8)) until a tile that
// is (a) not Water/Arctic and (b) at least `minDistance` from every prior
// chosen position. `minDistance` defaults to 10 (the C# `distance < 10` reject
// threshold before the tryCount relaxation kicks in). Up to 2000 tries per
// position (the same cap the C# uses); if the cap is hit the distance check is
// relaxed to 1 so SOMETHING is placed. Deterministic for a given `seed` (uses
// the faithful MT19937 RNG so the headless tests can reproduce the layout).
// Appends to `out` (does not clear). Returns true if N positions were placed.
bool placeStartingPositions(const MapManagement& mm, int n, uint32_t seed,
                            std::vector<std::pair<int, int>>& out,
                            int minDistance = 10);

} // namespace oc1
