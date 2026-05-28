// CheckPlayerTurn.cpp — port of OpenCiv1 CheckPlayerTurn.cs + the end-of-turn
// tail of Segment_1238 (see header). The interactive per-unit command loop is
// stubbed; this file ports the per-turn HOUSEKEEPING shape (year++, per-civ
// per-city shield accumulation + threshold-triggered unit production).
#include "CheckPlayerTurn.h"
#include "OpenCiv1Game.h"
#include "UnitManagement.h"
#include "MapManagement.h"
#include "TerrainTiles.h"
#include "TechResearch.h"
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

    // ---- SETTLERS IMPROVEMENT TICK ---------------------------------------
    // Mirrors the per-turn dispatcher entry where a Settlers with an
    // outstanding work counter has it decremented and the matching
    // improvement bit is OR-d into the map at completion. We iterate
    // every civ's units (human + AI), so AI-started improvements would
    // also complete here when the AI ever issues startBuildRoad/Irrigation
    // (no AI work code yet — TODO). On completion the unit unlocks
    // (workTurnsLeft=0, workTarget=kWorkNone).
    {
        auto& units = um.unitsMut();
        auto& mm = p.mapManagement();
        for (auto& u : units) {
            if (!u.alive) continue;
            if (u.workTurnsLeft <= 0) continue;
            --u.workTurnsLeft;
            if (u.workTurnsLeft == 0) {
                uint8_t flag = 0;
                if (u.workTarget == UnitManagement::kWorkRoad) {
                    flag = MapManagement::kImprovementRoad;
                } else if (u.workTarget == UnitManagement::kWorkIrrigation) {
                    flag = MapManagement::kImprovementIrrigation;
                }
                if (flag) mm.setImprovementFlag(u.x, u.y, flag);
                u.workTarget = UnitManagement::kWorkNone;
                // TODO: food/movement bonuses (Road = move/3, Irrigation
                // = +1 food) not yet modeled; only the bitflag is tracked.
            }
        }
    }

    // ---- AI BEHAVIOUR PASS (faithful approximation) -----------------------
    // Mirrors the "AI civ first acts" decision in C# CheckPlayerTurn (which
    // dispatches to Segment_25fb.F0_25fb_0c9d for each AI unit). The full AI
    // decision-tree is 359KB of x86 logic and out of scope here; the milestone
    // is "AI exists and acts" — the simplest faithful action being: if an AI
    // civ still has a Settlers unit AND has founded no city, found its capital
    // on that Settlers' tile (1:1 with the player's B-key BUILD-CITY action,
    // which is how every Civ1 game's first AI capital appears in turn 1).
    {
        auto& units = um.unitsMut();
        const auto& civs = um.civs();
        for (std::size_t cIdx = 0; cIdx < civs.size(); ++cIdx) {
            if (civs[cIdx].isHuman) continue; // human acts via the input loop
            int civId = int(cIdx);
            // Skip if this civ already founded a city this game.
            bool hasCity = false;
            for (const auto& cc : cities)
                if (cc.owner == civId) { hasCity = true; break; }
            if (hasCity) continue;
            // Find the first alive Settlers owned by this civ.
            for (auto& u : units) {
                if (!u.alive || u.owner != civId ||
                    u.type != UnitType::Settlers) continue;
                std::string nm;
                if (um.buildCity(u.x, u.y, civId, nm)) {
                    // Settlers is consumed by the BUILD-CITY action
                    // (mirrors F0_1866_01dc removing the unit on success).
                    u.alive = false;
                }
                break; // only one action per AI civ per turn (1:1 with C#)
            }
        }
    }

    // ---- AI UNIT MOVEMENT PASS (faithful greedy approximation) -----------
    // Mirrors the per-AI-unit dispatch in C# Segment_25fb.F0_25fb_0c9d (each
    // AI unit gets a "what should I do" call once per turn). The full version
    // is a multi-page decision tree; here we use the well-documented "advance
    // on nearest enemy" heuristic: each AI combat unit takes ONE step toward
    // the Chebyshev-nearest enemy unit or city per turn (or `move` steps if
    // the unit's MoveCount > 1, e.g. Cavalry later). Settlers don't fight,
    // so they're skipped here (the AI auto-found pass above handles them).
    // Determinism: civs/units processed in stable index order.
    {
        auto& units = um.unitsMut();
        const auto& civs = um.civs();
        for (std::size_t cIdx = 0; cIdx < civs.size(); ++cIdx) {
            if (civs[cIdx].isHuman) continue;
            int civId = int(cIdx);
            // Snapshot unit indices for this civ BEFORE stepping (the units_
            // vector itself never grows here, but a snapshot keeps the loop
            // bounds independent of any later production-pass appends).
            std::vector<int> myCombatUnits;
            for (std::size_t i = 0; i < units.size(); ++i) {
                const Unit& u = units[i];
                if (!u.alive || u.owner != civId) continue;
                if (u.type == UnitType::Settlers) continue;
                myCombatUnits.push_back(int(i));
            }
            for (int uid : myCombatUnits) {
                // alive check survives between iterations (a unit may have
                // died if an earlier combat went wrong way — defensive).
                if (!units[std::size_t(uid)].alive) continue;
                int steps = unitDefOf(units[std::size_t(uid)].type).move;
                if (steps < 1) steps = 1;
                for (int s = 0; s < steps; ++s) {
                    if (!units[std::size_t(uid)].alive) break;
                    if (!um.aiStep(uid)) break; // no target / can't move
                }
            }
        }
    }

    // OUTER LOOP: iterate civs (player + AI placeholders). After the AI pass
    // above the cities[] table also contains AI capitals, so the per-civ city
    // pass is now genuinely multi-civ.
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

            // Threshold-triggered production. Civ1: a city builds EITHER a
            // unit OR a building per cycle. We branch on c.productionKind:
            //
            //  - Unit: add a new Unit of c.productionType at the city's tile
            //    (mirrors CityWorker.cs lines 836-855 + F0_1866_0cf5_CreateUnit).
            //    If this city OWNS a Barracks, the new unit is a VETERAN
            //    (+50% combat applied in UnitManagement::resolveCombat).
            //
            //  - Building: add the BuildingType to c.ownedBuildings, reset
            //    shields, and (Civ1 default) switch productionKind back to
            //    Unit (productionType=Militia) so the city keeps building
            //    SOMETHING. Documented choice: defaulting to Militia is a
            //    safe fallback identical to the C# CityWorker behaviour when
            //    a queued building completes with no next item picked.
            int needed = 0;
            if (c.productionKind == City::ProductionKind::Building) {
                needed = buildingDefOf(c.productionBuildingType).cost;
                if (c.production != needed) c.production = needed;
                if (needed > 0 && c.shields >= needed) {
                    BuildingType built = c.productionBuildingType;
                    c.shields -= needed;
                    c.ownedBuildings.insert(built);
                    // Fall back to producing Militia next cycle (documented
                    // default; the player can override via setCityProduction*).
                    c.productionKind = City::ProductionKind::Unit;
                    c.productionBuildingType = BuildingType::None;
                    c.productionType = UnitType::Militia;
                    c.production = unitDefOf(UnitType::Militia).cost;
                    // TODO: Granary food bonus (food not yet modeled) — for
                    // now ownership is tracked but no growth math runs.
                }
            } else {
                needed = unitDefOf(c.productionType).cost;
                if (c.production != needed) c.production = needed;
                if (needed > 0 && c.shields >= needed) {
                    c.shields -= needed;
                    c.units += 1;
                    int newUnitIdx = um.addUnit(c.owner, c.productionType, c.x, c.y);
                    // Barracks: produced units are veterans (+50% combat).
                    if (c.hasBuilding(BuildingType::Barracks) &&
                        newUnitIdx >= 0 &&
                        std::size_t(newUnitIdx) < um.unitsMut().size()) {
                        um.unitsMut()[std::size_t(newUnitIdx)].veteran = true;
                    }
                }
            }
        }
    }

    // ---- TECH RESEARCH per-civ points accumulation ------------------------
    // Faithful early slice: each civ accumulates research points proportional
    // to its city count (one baseline point per city per turn). The C# path
    // (F0_*_GetCityResourceCount fan-out + science-rate slider) is out of
    // scope here; the per-city-baseline shape matches the "more cities ==
    // more science" character of Civ1's early game. When the accumulated
    // points cross the current tech's cost, TechResearch::addPoints unlocks
    // it and auto-picks the cheapest still-reachable next target.
    {
        auto& tr = p.techResearch();
        // Only run when initCivs() has provisioned per-civ tech state (we
        // skip when civCount==0 so the pre-tech-tree tests still pass).
        if (tr.civCount() > 0) {
            for (int civId = 0; civId < tr.civCount(); ++civId) {
                int cityCount = 0;
                for (const auto& c : cities)
                    if (c.owner == civId) ++cityCount;
                if (cityCount > 0) tr.addPoints(civId, cityCount);
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
