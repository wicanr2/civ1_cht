// TerrainTiles.h — canonical Civ1 terrain enum + faithful TER257.PIC tile
// (col,row) lookup table.
//
// Ported 1:1 from OpenCiv1 (C#):
//   TerrainTypeEnum:       OpenCiv1/src/Game/State/Definitions/TerrainTypeEnum.cs
//   TER257 tile loader:    OpenCiv1/src/Game/CodeObjects/StartGameMenu.cs
//                          F5_0000_1455_LoadBitmaps() lines 1023-1039:
//                            for (i 0..10) for (j 0..16)
//                              Array_b886[i,j] = ScreenToBitmap(1, j*16, i*16, 16, 16);
//   Tile draw:             OpenCiv1/src/Game/CodeObjects/MapManagement.cs
//                          F0_2aea_03ba_DrawCell() line 471:
//                            DrawBitmap(... Array_b886[(int)terrainType, mask])
//
// The Civ1 tilesheet TER257.PIC is a 320x200 image laid out as a 20-column x
// 12-row grid of 16x16 tiles. For terrain rows 0..5 (Desert..Mountains) each
// of the 16 columns is a distinct blend variant (border mask). For rows 6..9
// (Tundra..Jungle) only column 0 is unique — the rest are replicas of col 0.
//
// row 0 = Desert     row 5 = Mountains
// row 1 = Plains     row 6 = Tundra
// row 2 = Grassland  row 7 = Arctic
// row 3 = Forest     row 8 = Swamp
// row 4 = Hills      row 9 = Jungle
//
// Water and River use other (non-row-indexed) regions of the same sheet
// (coastal corner blocks at y=176, river deltas at y=176..192, ocean overlay
// at (256,120) — see MapManagement.cs F0_2aea_03ba). The "base" tile we
// expose for them mirrors the renderer's grassland-background convention:
//   Water -> (16, 7.5) i.e. pixel (256,120) -> the renderer's water-fill tile,
//            but as a (col,row) on the 16x16 grid that's (16,7). Adjusted to
//            col=15,row=7 (the closest aligned 16x16 cell) for a sensible
//            base, with a row=11 col=0 fallback used by the original renderer.
//   River -> approximated by water for the base tile (true rivers use the
//            16-mask deltas in Array_6e00 — not part of the base mapping).
//
// Variants we did NOT port (see // TODO(port): in the .cpp): coast border
// masks (16 per terrain), river deltas (32), special-resource overlays.
#pragma once
#include <cstdint>

namespace oc1 {

// 1:1 with OpenCiv1's TerrainTypeEnum.
enum class Terrain : int8_t {
    Invalid   = -1,
    Desert    = 0,
    Plains    = 1,
    Grassland = 2,
    Forest    = 3,
    Hills     = 4,
    Mountains = 5,
    Tundra    = 6,
    Arctic    = 7,
    Swamp     = 8,
    Jungle    = 9,
    Water     = 10,
    River     = 11,
    Count
};

// Faithful TER257.PIC (col,row) lookup for the BASE 16x16 tile of each terrain.
// Tile pixel position = (col*16, row*16). Source: Array_b886[terrain,0] in
// StartGameMenu.F5_0000_1455_LoadBitmaps + the renderer's water/river overlays.
struct TileXY { int col; int row; };
TileXY terrainToTileXY(Terrain t);

const char* terrainNameKey(Terrain t); // English key for the Translator

} // namespace oc1
