// CheckPlayerTurn.cpp — port of OpenCiv1 CheckPlayerTurn.cs + the end-of-turn
// tail of Segment_1238 (see header). The interactive per-unit command loop is
// stubbed; this file ports the per-turn HOUSEKEEPING shape (year++, per-civ
// per-city shield accumulation + threshold-triggered unit production).
#include "CheckPlayerTurn.h"
#include "OpenCiv1Game.h"
#include "UnitManagement.h"
#include "TerrainTiles.h"
#include <algorithm>

namespace oc1 {

CheckPlayerTurn::CheckPlayerTurn(OpenCiv1Game& parent) : p(parent) {
    (void)p; // cpu/log hooks reserved for the deeper port
}

// Ported 1:1 from Segment_1238.cs lines 268-305 (the C# year-step ladder).
int CheckPlayerTurn::advanceYear(int year) {
    int y = year;
    if (y < 1000) {
        y += 20;
        if (y == 21) y = 20; // BC->AD fix-up: keep 20-year cadence past 1 AD
    } else if (y < 1500) {
        y += 10;
    } else if (y < 1750) {
        y += 5;
    } else if (y < 1850) {
        // TODO(port): C# guards this branch with "(SpaceshipFlags & 0xfe) == 0"
        // — once Spaceship is ported, restore the guard. Until then always +2.
        y += 2;
    } else {
        y += 1;
    }
    if (y == 0) {
        // BC->AD crossing: C# also doubles every player's research progress
        // here (Segment_1238.cs lines 299-302). The Player[].ResearchProgress
        // table is not ported yet -> TODO(port): apply the 2x research boost
        // when the tech system lands.
        y = 1;
    }
    return y;
}

// Simplified shield yield: 1 baseline + 1 per adjacent Grassland/Plains tile,
// capped at 5. CityWorker.cs adds tile-by-tile via GetCityResourceCount over
// the 20-tile worker-flag fan-out — see header for the stubbing rationale.
int CheckPlayerTurn::shieldYield(int adjacentGoodTiles) {
    int s = 1 + std::max(0, adjacentGoodTiles);
    if (s > 5) s = 5;
    return s;
}

int CheckPlayerTurn::processEndOfTurn() {
    auto& um = p.unitManagement();
    auto& cities = um.citiesMut();

    // OUTER LOOP: iterate civs (player + AI placeholders). The current port
    // only has player 0 in cities[]; the loop SHAPE is faithful so AI civs
    // can be added incrementally without changing the dispatcher.
    // C# equivalent: Segment_1238 per-turn housekeeping iterates all 8
    // player slots (GameData.Players[0..7]) before incrementing TurnCount.
    const int numCivs = 8; // matches GameData.Players[8]
    for (int playerID = 0; playerID < numCivs; ++playerID) {
        // INNER LOOP: for each city owned by this civ, accumulate shields and
        // trigger unit production at threshold. Mirrors the CityWorker.cs
        // ShieldsCount += yield ... if (Cost <= ShieldsCount) CreateUnit
        // shape (lines 834-855) without the full F0_1d12_0045 state pass.
        for (auto& c : cities) {
            if (c.owner != playerID) continue;

            // Count adjacent "good" (Grassland/Plains) tiles via the terrain
            // provider installed by MiniWorld::attachGame. When no provider
            // is set (headless tests without a world) we treat the baseline
            // (1 shield/turn) as the floor.
            int adjGood = 0;
            const auto& tprov = um.terrainProvider();
            if (tprov) {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        Terrain t = tprov(c.x + dx, c.y + dy);
                        if (t == Terrain::Grassland || t == Terrain::Plains)
                            ++adjGood;
                    }
            }
            c.shields += shieldYield(adjGood);

            // Threshold-triggered unit production. C# does the same with
            // Units[ProductionID].Cost * local_4a; we use the per-city
            // `production` cost (default 10 == a Settlers/Militia-class unit).
            // The produced unit is tracked as a per-city counter (units++);
            // wiring it into a visible Player.Units[] table is out of scope
            // (stubbed — see UnitManagement.h // TODO(port) for the deeper port).
            if (c.production > 0 && c.shields >= c.production) {
                c.shields -= c.production;
                c.units += 1;
            }
        }
    }

    // TURN COUNTER + YEAR advance — mirrors Segment_1238.cs line 266-305.
    // The C++ port keeps the live turn counter in MiniWorld (turn_), so the
    // caller (MiniWorld::endTurn) is responsible for ++turn_; here we ONLY
    // advance the year (the per-civ pass above already ran).
    um.setYear(advanceYear(um.year()));
    return um.year();
}

} // namespace oc1
