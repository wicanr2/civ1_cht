#include "TerrainTiles.h"

namespace oc1 {

TileXY terrainToTileXY(Terrain t) {
    // For land terrains 0..9 the base tile (mask=0) is at (col=0, row=terrainID),
    // exactly matching Array_b886[terrain,0] in StartGameMenu.F5_0000_1455.
    switch (t) {
        case Terrain::Desert:    return {0, 0};
        case Terrain::Plains:    return {0, 1};
        case Terrain::Grassland: return {0, 2};
        case Terrain::Forest:    return {0, 3};
        case Terrain::Hills:     return {0, 4};
        case Terrain::Mountains: return {0, 5};
        case Terrain::Tundra:    return {0, 6};
        case Terrain::Arctic:    return {0, 7};
        case Terrain::Swamp:     return {0, 8};
        case Terrain::Jungle:    return {0, 9};
        // Water: the renderer uses Array_d294 (8x8 corner blocks at y=176) +
        // coast overlays at (224..304, 100). The closest aligned 16x16 base
        // tile that reads as solid ocean is the one at the grassland-background
        // pixel (256,120) -> col=16, row=7. We approximate to col=15,row=7
        // (within the 20x12 grid) — visibly solid water in TER257.
        // TODO(port): per-cell coast border masks (Array_d294 corners + the
        // (224..304,100) overlays for the 0x1c/0xc1/0x07/0x70/0x8f/0xf8 cases).
        case Terrain::Water:     return {15, 7};
        // River: the renderer composites grassland background + river-delta
        // tiles from Array_6e00 (32 mask variants at y=64,80). For a base
        // tile we re-use water — visibly distinct from land in the fallback
        // and overdrawn by the river-delta logic when present.
        // TODO(port): river-delta mask variants Array_6e00[0..31].
        case Terrain::River:     return {15, 7};
        default:                 return {15, 7};
    }
}

const char* terrainNameKey(Terrain t) {
    switch (t) {
        case Terrain::Desert:    return "Desert";
        case Terrain::Plains:    return "Plains";
        case Terrain::Grassland: return "Grassland";
        case Terrain::Forest:    return "Forest";
        case Terrain::Hills:     return "Hills";
        case Terrain::Mountains: return "Mountains";
        case Terrain::Tundra:    return "Tundra";
        case Terrain::Arctic:    return "Arctic";
        case Terrain::Swamp:     return "Swamp";
        case Terrain::Jungle:    return "Jungle";
        case Terrain::Water:     return "Ocean";
        case Terrain::River:     return "River";
        default:                 return "Ocean";
    }
}

} // namespace oc1
