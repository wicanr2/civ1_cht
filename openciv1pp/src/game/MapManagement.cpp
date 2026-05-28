#include "MapManagement.h"
#include <algorithm>
#include <cstdlib>
#include <utility>
#include <vector>

namespace oc1 {

// ---------------------------------------------------------------------------
// RandomMT19937 — faithful port of IRB/RNG/RandomMT19937.cs
// ---------------------------------------------------------------------------
RandomMT19937::RandomMT19937(int32_t seed) {
    matrix_[0] = uint32_t(seed);
    for (int i = 1; i < kMatrixLength; ++i) {
        matrix_[i] = 1812433253u * (matrix_[i - 1] ^ (matrix_[i - 1] >> 30)) + uint32_t(i);
    }
    matrixIndex_ = kMatrixLength;
}

uint32_t RandomMT19937::internalSample() {
    constexpr uint32_t upperMask = 0x80000000u;
    constexpr uint32_t lowerMask = 0x7fffffffu;
    static const uint32_t matrixA[2] = { 0x0u, 0x9908b0dfu };
    uint32_t value;
    if (matrixIndex_ >= kMatrixLength) {
        int i;
        for (i = 0; i < kMatrixLength - kMatrixMedian; ++i) {
            value = (matrix_[i] & upperMask) | (matrix_[i + 1] & lowerMask);
            matrix_[i] = matrix_[i + kMatrixMedian] ^ (value >> 1) ^ matrixA[value & 1];
        }
        for (; i < kMatrixLength - 1; ++i) {
            value = (matrix_[i] & upperMask) | (matrix_[i + 1] & lowerMask);
            matrix_[i] = matrix_[i + (kMatrixMedian - kMatrixLength)] ^ (value >> 1) ^ matrixA[value & 1];
        }
        value = (matrix_[kMatrixLength - 1] & upperMask) | (matrix_[0] & lowerMask);
        matrix_[kMatrixLength - 1] = matrix_[kMatrixMedian - 1] ^ (value >> 1) ^ matrixA[value & 1];
        matrixIndex_ = 0;
    }
    value = matrix_[matrixIndex_++];
    value ^= (value >> 11);
    value ^= (value << 7) & 0x9d2c5680u;
    value ^= (value << 15) & 0xefc60000u;
    value ^= (value >> 18);
    return value;
}

uint32_t RandomMT19937::UNext()   { return internalSample(); }
int32_t  RandomMT19937::Next()    { return int32_t(internalSample() >> 1); }
double   RandomMT19937::sample()  { return double(internalSample()) * 2.3283064365386962890625e-10; }
double   RandomMT19937::NextDouble() { return sample(); }
int32_t  RandomMT19937::Next(int32_t maxValue) {
    return int32_t(sample() * double(maxValue));
}
int32_t  RandomMT19937::Next(int32_t minValue, int32_t maxValue) {
    long range = long(maxValue) - long(minValue);
    return int32_t(sample() * double(range)) + minValue;
}

// ---------------------------------------------------------------------------
// MoveDirections — faithful port of OpenCiv1GameGlobals.MoveDirections.
// ---------------------------------------------------------------------------
const MoveDir kMoveDirections[49] = {
    {0,0}, {0,-1}, {1,-1}, {1,0}, {1,1}, {0,1}, {-1,1}, {-1,0}, {-1,-1},
    {0,-2}, {1,-2}, {2,-1}, {2,0}, {2,1}, {1,2}, {0,2}, {-1,2}, {-2,1}, {-2,0}, {-2,-1}, {-1,-2},
    {2,2}, {2,-2}, {-2,-2}, {-2,2},
    {0,-3}, {1,-3}, {2,-3}, {3,-2}, {3,-1}, {3,0}, {3,1}, {3,2}, {2,3}, {1,3}, {0,3}, {-1,3}, {-2,3},
    {-3,2}, {-3,1}, {-3,0}, {-3,-1}, {-3,-2}, {-2,-3}, {-1,-3}, {3,3}, {3,-3}, {-3,3}, {-3,-3}
};

// ---------------------------------------------------------------------------
// Terrain <-> pixel-value tables (1:1 with OpenCiv1GameGlobals).
// ---------------------------------------------------------------------------
// Terrain enum value (0..11) -> pixel value stored in VCPU memory.
const int MapManagement::kTerrainToPixel[12] = {
    14, // Desert
    6,  // Plains
    10, // Grassland
    2,  // Forest
    12, // Hills
    13, // Mountains
    7,  // Tundra
    15, // Arctic
    3,  // Swamp
    11, // Jungle
    1,  // Water
    9   // River
};
// Pixel value (0..15) -> Terrain enum value (-1 = Invalid).
const int8_t MapManagement::kPixelToTerrain[16] = {
    int8_t(Terrain::Invalid),   // 0
    int8_t(Terrain::Water),     // 1
    int8_t(Terrain::Forest),    // 2
    int8_t(Terrain::Swamp),     // 3
    int8_t(Terrain::Invalid),   // 4
    int8_t(Terrain::Invalid),   // 5
    int8_t(Terrain::Plains),    // 6
    int8_t(Terrain::Tundra),    // 7
    int8_t(Terrain::Invalid),   // 8
    int8_t(Terrain::River),     // 9
    int8_t(Terrain::Grassland), // 10
    int8_t(Terrain::Jungle),    // 11
    int8_t(Terrain::Hills),     // 12
    int8_t(Terrain::Mountains), // 13
    int8_t(Terrain::Desert),    // 14
    int8_t(Terrain::Arctic)     // 15
};

// ---------------------------------------------------------------------------
// ctor + core get/set (VCPU memory model)
// ---------------------------------------------------------------------------
MapManagement::MapManagement(OpenCiv1Game& parent) : p_(parent), cpu_(parent.cpu) {
    improvements_.assign(std::size_t(kWidth) * kHeight, 0);
    CreateNewEmptyMap();
}

// ---- improvements grid ----------------------------------------------------
// Parallel grid storing the per-tile TerrainImprovementFlagsEnum bitmask.
// We keep this as a std::vector<uint8_t> (NOT in VCPU memory) — the C#
// equivalent (F0_2aea_1585_GetVisibleTerrainImprovements + the Flag80 pass
// on the river tail) reads from the SAME GBitmap as the terrain, packed
// into the upper nibble; we keep the two grids physically separate so the
// faithful terrain bytes stay 1:1 with the C# pixel encoding (which the
// generator passes already depend on) while the improvements layer can
// evolve independently.
uint8_t MapManagement::getImprovements(int x, int y) const {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return 0;
    return improvements_[std::size_t(y) * kWidth + x];
}

void MapManagement::setImprovementFlag(int x, int y, uint8_t flag) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    improvements_[std::size_t(y) * kWidth + x] |= flag;
}

void MapManagement::setImprovementsRaw(int x, int y, uint8_t bits) {
    if (x < 0 || y < 0 || x >= kWidth || y >= kHeight) return;
    improvements_[std::size_t(y) * kWidth + x] = bits;
}

void MapManagement::clearImprovements() {
    std::fill(improvements_.begin(), improvements_.end(), uint8_t(0));
}

int MapManagement::AdjustXPosition(int xPos) const {
    while (xPos < 0)        xPos += kWidth;
    while (xPos >= kWidth)  xPos -= kWidth;
    return xPos;
}

bool MapManagement::F0_2aea_1326_ValidateMapCoordinates(int x, int y) const {
    x = AdjustXPosition(x);
    return x >= 0 && x < kWidth && y >= 0 && y < kHeight;
}

Terrain MapManagement::GetTerrainType(int x, int y) const {
    // Clamp y like the C# (it indexes the bitmap directly with no bounds check
    // beyond the X wraparound; we return Water for y out of range to be safe).
    if (y < 0 || y >= kHeight) return Terrain::Water;
    uint8_t px = cpu_.ReadUInt8(kMapSeg, cellAddr(AdjustXPosition(x), y)) & 0x0F;
    int8_t t = kPixelToTerrain[px];
    return t < 0 ? Terrain::Water : Terrain(t);
}

void MapManagement::SetTerrainType(int x, int y, Terrain t) {
    if (y < 0 || y >= kHeight) return;
    if (t == Terrain::Invalid) return;
    int idx = int(t);
    if (idx < 0 || idx >= 12) return;
    cpu_.WriteUInt8(kMapSeg, cellAddr(AdjustXPosition(x), y),
                    uint8_t(kTerrainToPixel[idx]));
}

void MapManagement::CreateNewEmptyMap() {
    // 1:1 with C# CreateNewEmptyMap: every cell = pixel value 1 (Water).
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            cpu_.WriteUInt8(kMapSeg, cellAddr(x, y), 1);
        }
    }
}

// ---------------------------------------------------------------------------
// generate() — faithful port of MapInitAndIntro.cs F7_0000_0012_GenerateMap
//              (the procedural branch; EarthMap branch is out of scope).
// ---------------------------------------------------------------------------
void MapManagement::generate(const GenSettings& s) {
    RandomMT19937 rng(s.seed);

    CreateNewEmptyMap();
    clearImprovements();

    // ---- Stage 1: Generate continents ----
    {
        const int totalCellCount = s.planetLandMass * 320 + 640;
        for (int i = 0; i < totalCellCount;) {
            int cloudCellCount = 0;
            bool cloud[80][50] = {};

            int x = rng.Next(72) + 4;
            int y = rng.Next(34) + 8;
            int cloudSize = rng.Next(64) + 1;

            do {
                int x1  = AdjustXPosition(x + 1);
                int xm1 = AdjustXPosition(x - 1);
                cloud[x][y]     = true;
                cloud[x][y - 1] = true;
                cloud[x1][y]    = true;
                cloud[x][y + 1] = true;
                cloud[xm1][y]   = true;

                switch (rng.Next(4)) {
                    case 0: y -= 1; break;
                    case 1: x += 1; break;
                    case 2: y += 1; break;
                    case 3: x -= 1; break;
                }
                x = AdjustXPosition(x);
                --cloudSize;
            } while (cloudSize > 0 && y > 4 && y < 45);

            // Copy cloud to map.
            for (int j = 0; j < kWidth; ++j) {
                for (int k = 0; k < kHeight; ++k) {
                    if (cloud[j][k]) {
                        switch (GetTerrainType(j, k)) {
                            case Terrain::Water:     SetTerrainType(j, k, Terrain::Plains);    break;
                            case Terrain::Plains:    SetTerrainType(j, k, Terrain::Hills);     break;
                            case Terrain::Hills:     SetTerrainType(j, k, Terrain::Mountains); break;
                            default: break;
                        }
                        ++cloudCellCount;
                    }
                }
            }
            i += cloudCellCount;
            // TODO(port): F7_0000_17cf_AdvanceAnimation — UI progress hook;
            // no-op here (headless).
        }
    }

    // ---- Stage 2: Add map details according to temperature ----
    for (int i = 0; i < kWidth; ++i) {
        for (int j = 0; j < kHeight; ++j) {
            if (GetTerrainType(i, j) == Terrain::Plains) {
                int band = (std::abs(rng.Next(8) + j - 29) + (1 - s.planetTemperature)) / 6 + 1;
                switch (band) {
                    case 0:
                    case 1: SetTerrainType(i, j, Terrain::Desert); break;
                    case 4:
                    case 5: SetTerrainType(i, j, Terrain::Tundra); break;
                    case 6:
                    case 7: SetTerrainType(i, j, Terrain::Arctic); break;
                    default: break;
                }
            }
        }
    }

    // ---- Stage 3: Adjust climate regions (two horizontal sweeps per row) ----
    for (int i = 0; i < kHeight; ++i) {
        int medianY = std::abs(25 - i);
        int climateValue = 0;
        for (int j = 0; j < kWidth; ++j) {
            Terrain cell = GetTerrainType(j, i);
            if (cell != Terrain::Water) {
                if (climateValue > 0) {
                    climateValue -= rng.Next(-((s.planetClimate * 2) - 7));
                    switch (cell) {
                        case Terrain::Plains:    SetTerrainType(j, i, Terrain::Grassland); break;
                        case Terrain::Tundra:    SetTerrainType(j, i, Terrain::Arctic);    break;
                        case Terrain::Hills:     SetTerrainType(j, i, Terrain::Forest);    break;
                        case Terrain::Mountains: climateValue -= 3;                        break;
                        case Terrain::Desert:    SetTerrainType(j, i, Terrain::Plains);    break;
                        default: break;
                    }
                }
            } else {
                if (std::abs(12 - medianY) + s.planetClimate * 4 > climateValue) {
                    ++climateValue;
                }
            }
        }
        climateValue = 0;
        for (int j = kWidth - 1; j >= 0; --j) {
            Terrain cell = GetTerrainType(j, i);
            if (cell != Terrain::Water) {
                if (climateValue > 0) {
                    climateValue -= rng.Next(-(s.planetClimate * 2 - 7));
                    switch (cell) {
                        case Terrain::Swamp:
                        case Terrain::Hills:     SetTerrainType(j, i, Terrain::Forest);    break;
                        case Terrain::Plains:    SetTerrainType(j, i, Terrain::Grassland); break;
                        case Terrain::Grassland:
                            if (medianY < 10) SetTerrainType(j, i, Terrain::Jungle);
                            else              SetTerrainType(j, i, Terrain::Swamp);
                            climateValue = -2;
                            break;
                        case Terrain::Mountains:
                            climateValue -= 3;
                            SetTerrainType(j, i, Terrain::Forest);
                            break;
                        case Terrain::Desert:    SetTerrainType(j, i, Terrain::Plains); break;
                        default: break;
                    }
                }
            } else {
                if (medianY / 2 + s.planetClimate > climateValue) {
                    ++climateValue;
                }
            }
        }
    }

    // ---- Stage 4: Adjust planet age ----
    {
        int cellX = 0, cellY = 0;
        int ageValue = 800 + 800 * s.planetAge;
        for (int i = 0; i < ageValue; ++i) {
            if ((i & 0x1) != 0) {
                MoveDir dir = kMoveDirections[rng.Next(8) + 1];
                cellX += dir.x;
                cellY += dir.y;
            } else {
                cellX = rng.Next(kWidth);
                cellY = rng.Next(kHeight);
            }
            // Bounds guard: the C# indexes the GBitmap directly; out-of-range
            // y would be invalid. We skip OOB cells (X wraps via AdjustX).
            if (cellY < 0 || cellY >= kHeight) continue;
            switch (GetTerrainType(cellX, cellY)) {
                case Terrain::Forest:    SetTerrainType(cellX, cellY, Terrain::Jungle);    break;
                case Terrain::Swamp:     SetTerrainType(cellX, cellY, Terrain::Grassland); break;
                case Terrain::Plains:
                case Terrain::Tundra:    SetTerrainType(cellX, cellY, Terrain::Hills);     break;
                case Terrain::Grassland: SetTerrainType(cellX, cellY, Terrain::Forest);    break;
                case Terrain::Jungle:    SetTerrainType(cellX, cellY, Terrain::Swamp);     break;
                case Terrain::Hills:
                case Terrain::Arctic:    SetTerrainType(cellX, cellY, Terrain::Mountains); break;
                case Terrain::Mountains:
                    if (cellY - 1 >= 0 && cellY + 1 < kHeight &&
                        GetTerrainType(cellX - 1, cellY - 1) != Terrain::Water &&
                        GetTerrainType(cellX - 1, cellY + 1) != Terrain::Water &&
                        GetTerrainType(cellX + 1, cellY - 1) != Terrain::Water &&
                        GetTerrainType(cellX + 1, cellY + 1) != Terrain::Water) {
                        SetTerrainType(cellX, cellY, Terrain::Water);
                    }
                    break;
                case Terrain::Desert:    SetTerrainType(cellX, cellY, Terrain::Plains); break;
                default: break;
            }
        }
    }

    // ---- Stage 5: Generate rivers ----
    {
        int riverValue = (s.planetLandMass + s.planetClimate) * 2 + 6;
        int riverCellCount = 0;
        Terrain mapCopy[80][50] = {};

        for (int i = 0; i < 256 && riverCellCount <= riverValue; ++i) {
            int local16 = 0;
            // Save map.
            for (int j = 0; j < kWidth; ++j)
                for (int k = 0; k < kHeight; ++k)
                    mapCopy[j][k] = GetTerrainType(j, k);
            // Need at least one Hills cell to start a river from.
            bool hasHills = false;
            for (int j = 0; j < kWidth && !hasHills; ++j)
                for (int k = 0; k < kHeight; ++k)
                    if (GetTerrainType(j, k) == Terrain::Hills) { hasHills = true; break; }
            if (!hasHills) break;

            int riverX = 0, riverY = 0;
            do {
                riverX = rng.Next(kWidth);
                riverY = rng.Next(kHeight);
            } while (GetTerrainType(riverX, riverY) != Terrain::Hills);

            int newRiverX = riverX, newRiverY = riverY;
            bool riverEnds = false;
            Terrain cellValue = Terrain::Hills;
            int newDirection = rng.Next(4) * 2;
            int oldDirection = newDirection;
            do {
                SetTerrainType(newRiverX, newRiverY, Terrain::River);
                for (int j = 1; j < 9; j += 2) {
                    MoveDir d = kMoveDirections[j];
                    if (GetTerrainType(newRiverX + d.x, newRiverY + d.y) == Terrain::Water)
                        riverEnds = true;
                }
                oldDirection = newDirection;
                newDirection = ((rng.Next(2) - (local16 & 1)) * 2 + newDirection) & 0x7;
                // TODO(port): the C# also calls F0_2aea_1653_SetTerrainImprovements
                // (newRiverX, newRiverY, Flag80) when (oldDirection ^ 4) > newDirection
                // — the improvement bitmap layer is not ported, so the river-
                // improvement flag is omitted here. The terrain base type is still
                // set to River, which is what the renderer needs.
                (void)oldDirection;
                MoveDir d1 = kMoveDirections[newDirection + 1];
                newRiverX += d1.x;
                newRiverY += d1.y;
                cellValue = GetTerrainType(newRiverX, newRiverY);
                ++local16;
            } while (!riverEnds &&
                     cellValue != Terrain::Water &&
                     cellValue != Terrain::River &&
                     cellValue != Terrain::Mountains &&
                     local16 < 1000); // safety guard against runaway

            if ((!riverEnds && cellValue != Terrain::River) || local16 < 5) {
                // Restore the saved map (the river attempt failed).
                for (int j = 0; j < kWidth; ++j)
                    for (int k = 0; k < kHeight; ++k)
                        SetTerrainType(j, k, mapCopy[j][k]);
            } else {
                ++riverCellCount;
                // Surrounding Forest -> Jungle within the 2-cell ring.
                for (int j = 1; j < 22; ++j) {
                    MoveDir d = kMoveDirections[j];
                    int nx = riverX + d.x;
                    int ny = riverY + d.y;
                    if (ny < 0 || ny >= kHeight) continue;
                    if (GetTerrainType(nx, ny) == Terrain::Forest)
                        SetTerrainType(nx, ny, Terrain::Jungle);
                }
            }
        }
    }

    // TODO(port): Stage 6 — generate map groups (continent / ocean group IDs)
    // is not ported. The generator layer above (terrain types) is what
    // MiniWorld renders; the group bookkeeping is a deep dependency of the AI
    // city-placement / pathfinding code, none of which is ported yet.
}

// ---------------------------------------------------------------------------
// placeStartingPositions — port of the C# starting-position picker in
// StartGameMenu.F5_0000_*_InitNewGameData (L0a5e..L0b54). Faithful subset:
//   * uniformly random (x in [8, W-8), y in [8, H-8)) via MT19937
//   * reject Water/Arctic (matches the C# Water reject + the player can't
//     start on the polar Arctic ring)
//   * reject when within `minDistance` (default 10) of any prior chosen
//     position (matches `distance < 10 - (tryCount / 64)` with tryCount=0)
//   * up to 2000 tries per position (the C# `tryCount <= 2000` cap); on
//     exhaustion the distance gate is relaxed to 1 so SOMETHING is placed.
// The deeper rejects (GetPlayerLandOwnership, GetGroupID/BuildSiteCount,
// MinorTribeHut) are STUBs — they depend on Stage 6 (continent groups) which
// is not yet ported. The simpler land+distance reject still produces a
// recognisably Civ1-ish layout (one civ per region).
bool placeStartingPositions(const MapManagement& mm, int n, uint32_t seed,
                            std::vector<std::pair<int, int>>& out,
                            int minDistance) {
    if (n <= 0) return true;
    RandomMT19937 rng{int32_t(seed)};
    const int W = mm.width(), H = mm.height();
    // Inset margins match the C# RNG ranges: Next(W-16)+8 -> [8, W-8).
    const int xLo = 8, xHi = W - 8;          // exclusive upper
    const int yLo = 8, yHi = H - 8;          // exclusive upper
    const int xRange = (xHi > xLo) ? (xHi - xLo) : 1;
    const int yRange = (yHi > yLo) ? (yHi - yLo) : 1;
    for (int i = 0; i < n; ++i) {
        int picked = -1, py = -1, tries = 0;
        int effectiveDist = minDistance;
        while (tries < 4000) {
            ++tries;
            int x = xLo + rng.Next(xRange);
            int y = yLo + rng.Next(yRange);
            Terrain t = mm.terrainAt(x, y);
            if (t == Terrain::Water || t == Terrain::Arctic) continue;
            // Distance check: Chebyshev distance (Civ1's "king move" metric;
            // same as max(|dx|,|dy|) used by F0_2dc4_0289_GetShortestDistance
            // for the unobstructed case).
            bool tooClose = false;
            for (const auto& p : out) {
                int dx = std::abs(p.first  - x);
                int dy = std::abs(p.second - y);
                int d = dx > dy ? dx : dy;
                if (d < effectiveDist) { tooClose = true; break; }
            }
            if (tooClose) {
                // Relax the gate gradually after the C# 2000-try cap.
                if (tries == 2000) effectiveDist = (minDistance > 4) ? minDistance / 2 : 1;
                if (tries == 3000) effectiveDist = 1;
                continue;
            }
            picked = x; py = y;
            break;
        }
        if (picked < 0) {
            // Last-resort scan: linear walk over the inset rect, accept the
            // first land tile. Guarantees we always return SOMETHING for n
            // positions so the caller's units().size() invariant holds.
            for (int yy = yLo; yy < yHi && picked < 0; ++yy)
                for (int xx = xLo; xx < xHi && picked < 0; ++xx) {
                    Terrain t = mm.terrainAt(xx, yy);
                    if (t == Terrain::Water || t == Terrain::Arctic) continue;
                    bool dup = false;
                    for (const auto& p : out)
                        if (p.first == xx && p.second == yy) { dup = true; break; }
                    if (!dup) { picked = xx; py = yy; }
                }
        }
        if (picked < 0) return false;
        out.emplace_back(picked, py);
    }
    return true;
}

} // namespace oc1
