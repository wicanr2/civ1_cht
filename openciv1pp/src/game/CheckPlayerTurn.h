// CheckPlayerTurn.h — ported CodeObject (OpenCiv1 CheckPlayerTurn.cs + the
// end-of-turn tail of Segment_1238.F0_1238_*).
//
// The C# CheckPlayerTurn.F0_1403_000e_CheckPlayerTurn is the "process one
// player's turn" entry — for the HUMAN player it drives the interactive
// per-unit command loop (mouse/keyboard, menus). The end-of-turn housekeeping
// (TurnCount++, Year +=, ShieldsCount accumulation + unit production) sits at
// the tail of Segment_1238 (the per-turn housekeeping pass that runs AFTER
// every player has acted).
//
// What is ported faithfully here:
//   - The OUTER turn-loop SHAPE: advance the turn counter, iterate civs
//     (human + AI placeholders), iterate each civ's cities for per-turn
//     housekeeping. Matches the C# pattern of "for each player, then for each
//     city, advance state."
//   - YEAR math from Segment_1238.cs lines 268-305 (1:1 ported):
//        year < 1000  -> +20  (with the 21 AD -> 20 AD fix-up + 0->1 AD jump)
//        year < 1500  -> +10
//        year < 1750  -> +5
//        year < 1850  -> +2  (the spaceship-flag guard is stubbed -> always +2)
//        else         -> +1
//     Starts at -4000 (4000 BC), set by StartGameMenu.cs line 124. Earlier
//     turns therefore advance ~20 years/turn (faithful), NOT the simpler
//     50/25/20 schedule the task notes as a fallback.
//   - SHIELD accumulation per city per turn: simplified vs CityWorker.cs (the
//     full F0_1d12 city-state pass needs the 20-tile worker-flag fan-out +
//     improvement set + corruption + happiness — out of scope here). We
//     accumulate `shields += baselineShields(cityTile) + adjacentBonus()` and
//     when shields >= city.production, we PRODUCE one unit (cities[].units++,
//     shields -= production), exactly the threshold-trigger shape in
//     CityWorker.cs lines 836-994.
//
// What is stubbed (// TODO(port) in .cpp):
//   - The HUMAN-player interactive command loop (CheckPlayerTurn.cs lines
//     230-870): keyboard/mouse dispatch, unit menus, city-screen dialog. The
//     C++ port already has its own per-key handlers in main.cpp/MiniWorld; the
//     end-of-turn dispatcher here only does the per-turn housekeeping pass.
//   - AI player decision logic (Segment_25fb.F0_25fb_0c9d etc.).
//   - Per-civ tech research, diplomacy, combat resolution.
//   - The full CityWorker.F0_1d12_0045_ProcessCityState: tile-yield math via
//     F0_1d12_6abc_GetCityResourceCount, improvement upkeep, happiness/disorder
//     checks, the Cost*local_4a (difficulty-scaled cost) production threshold.
#pragma once
#include <cstdint>

namespace oc1 {

class OpenCiv1Game;

class CheckPlayerTurn {
public:
    explicit CheckPlayerTurn(OpenCiv1Game& parent);

    // The end-of-turn housekeeping pass. Called by MiniWorld::endTurn() once
    // the human player has hit "end turn". Mirrors the tail of Segment_1238
    // (per-civ city pass, then TurnCount++, then Year math) but stays inside
    // the C++ City struct (no GameData.Cities[] yet).
    //
    // Returns the new turn number (== oldTurn + 1).
    int processEndOfTurn();

    // ---- year math (ported 1:1 from Segment_1238.cs lines 268-305) ----
    // Civ1 stores Year as a signed int: negative = BC (Year = -4000 at game
    // start), 0 is replaced with 1 AD after the BC->AD crossing, positive = AD.
    static int initialYear() { return -4000; }     // StartGameMenu.cs line 124
    static int advanceYear(int year);              // returns NEW year

    // Per-turn shield production for a city tile. Simplified vs CityWorker.cs
    // (1 baseline + 1 per adjacent Grassland/Plains, capped at 5) — see header
    // comment above for the stubbing rationale.
    static int shieldYield(int adjacentGoodTiles);

    // ---- Food yield helpers (Civ1 standard tile food values) -------------
    // Per-tile baseline food yield (Civ1 standard, sourced from the manual
    // + community wikis):
    //   Grassland=2 Plains=1 Forest=1 Hills=1 Mountains=0 Desert=0
    //   Tundra=1   Arctic=0 Swamp=1   Jungle=1 Water=1  River=2
    // The C# CityWorker.GetCityResourceCount has more fan-out (special
    // resources, government modifiers, ...) — out of scope here.
    static int tileFoodYield(int terrainEnum);
    // Irrigation +1 food bonus on Grassland/Plains/Desert/River tiles.
    // Mirrors the C# TerrainModifications[t].IrrigationEffect == -2 guard
    // that gates the BUILD-IRRIGATION menu entry to those terrains.
    static bool irrigationApplies(int terrainEnum);
    // Per-city per-turn food production summed over the city tile + the 4
    // cardinal neighbors (a simplified stand-in for the full 21-tile
    // "fat-cross" worker assignment — see the // TODO(port) below).
    // Returns sum-of-food (BEFORE subtracting population*2).
    int cityFoodGross(int cx, int cy) const;

private:
    OpenCiv1Game& p;
};

} // namespace oc1
