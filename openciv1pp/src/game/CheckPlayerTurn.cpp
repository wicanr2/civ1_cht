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
#include "Government.h"
#include <algorithm>
#include <cmath>

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

// Civ1-standard per-tile food yield (manual + community wikis).
int CheckPlayerTurn::tileFoodYield(int terrainEnum) {
    switch (Terrain(terrainEnum)) {
        case Terrain::Grassland: return 2;
        case Terrain::Plains:    return 1;
        case Terrain::Forest:    return 1;
        case Terrain::Hills:     return 1;
        case Terrain::Mountains: return 0;
        case Terrain::Desert:    return 0;
        case Terrain::Tundra:    return 1;
        case Terrain::Arctic:    return 0;
        case Terrain::Swamp:     return 1;
        case Terrain::Jungle:    return 1;
        case Terrain::Water:     return 1;
        case Terrain::River:     return 2;
        default:                 return 0;
    }
}

bool CheckPlayerTurn::irrigationApplies(int terrainEnum) {
    Terrain t = Terrain(terrainEnum);
    return t == Terrain::Grassland || t == Terrain::Plains ||
           t == Terrain::Desert    || t == Terrain::River;
}

// Sum of food yield over the city tile + 4 cardinal neighbors (simplified
// stand-in for the full 21-tile "fat-cross" worker assignment, see
// // TODO(port) below). Adds +1 food per irrigated, irrigation-compatible
// tile (mirrors C# TerrainModifications.IrrigationEffect).
int CheckPlayerTurn::cityFoodGross(int cx, int cy) const {
    int gross = 0;
    const auto& tprov = p.unitManagement().terrainProvider();
    if (!tprov) {
        // No terrain provider wired (rare headless path) — assume the city
        // is on a Grassland tile (yield 2) with 4 Grassland neighbors. Same
        // 10-food/turn baseline used by the foodtest scaffolding.
        return 10;
    }
    const auto& mm = p.mapManagement();
    static const int kDx[5] = { 0,  0, 0, -1, 1 };
    static const int kDy[5] = { 0, -1, 1,  0, 0 };
    for (int i = 0; i < 5; ++i) {
        int tx = cx + kDx[i], ty = cy + kDy[i];
        Terrain t = tprov(tx, ty);
        int y = tileFoodYield(int(t));
        // Irrigation bonus: +1 food on compatible terrain (faithful Civ1).
        uint8_t imp = mm.getImprovements(tx, ty);
        if ((imp & MapManagement::kImprovementIrrigation) &&
            irrigationApplies(int(t))) {
            y += 1;
        }
        gross += y;
    }
    return gross;
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

    // ---- GOVERNMENT TRANSITION TICK --------------------------------------
    // Decrement anarchyTurnsLeft for each civ that is mid-transition. When
    // the counter reaches 0, the EFFECTIVE government becomes targetGovt
    // (we materialise the switch by writing govt = targetGovt). Mirrors the
    // Civ1 per-turn revolution counter (3-turn Anarchy then the chosen
    // stable government takes effect). Run BEFORE the per-city shield pass
    // so the production/science multipliers below see the freshly-updated
    // government.
    {
        auto& civs = um.civsMut();
        for (auto& c : civs) {
            if (c.anarchyTurnsLeft > 0) {
                c.anarchyTurnsLeft -= 1;
                if (c.anarchyTurnsLeft == 0) {
                    c.govt = c.targetGovt;
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
            // Per-civ government PRODUCTION multiplier (Civ1: Anarchy halves
            // shield output; other governments are baseline 1.0). Applied
            // multiplicatively to the per-turn shield yield with rounding
            // to keep results deterministic (floor for fractions <1).
            int baseShields = shieldYield(adjGood);
            float prodMul = 1.0f;
            if (std::size_t(c.owner) < um.civs().size()) {
                Government eg = um.effectiveGovernment(c.owner);
                prodMul = governmentDefOf(eg).productionMul;
            }
            int finalShields = int(std::floor(float(baseShields) * prodMul));
            if (finalShields < 0) finalShields = 0;
            c.shields += finalShields;

            // ---- FOOD + POPULATION GROWTH (Civ1 food box) ----------------
            // Faithful Civ1 growth math (simplified worker fan-out to the
            // city tile + 4 cardinal neighbors — full 21-tile "fat cross"
            // is // TODO(port)):
            //   foodPerTurn = cityFoodGross(c.x,c.y) - population*2
            //   food += foodPerTurn
            //   growthThreshold = (population+1)*10  (Granary halves -> *5)
            //   if food >= threshold: population++; if Granary food=thr/2
            //                                       else  food = 0
            //   if food < 0 and population > 1: population--, food = 0
            // (single-pop starvation just clamps food at 0; deeper city-loss
            // mechanics are out of scope.)
            int gross = cityFoodGross(c.x, c.y);
            c.foodPerTurn = gross - c.population * 2;
            c.food += c.foodPerTurn;
            bool hasGranary = c.hasBuilding(BuildingType::Granary);
            int threshold = (c.population + 1) * (hasGranary ? 5 : 10);
            if (c.food >= threshold) {
                c.population += 1;
                if (hasGranary) {
                    // New threshold for the NEW population (post-growth).
                    int newThr = (c.population + 1) * 5;
                    c.food = newThr / 2; // Granary keeps half-full storage
                } else {
                    c.food = 0;
                }
            } else if (c.food < 0) {
                if (c.population > 1) {
                    c.population -= 1;
                }
                c.food = 0;
            }

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
                if (cityCount <= 0) continue;
                // Per-civ government SCIENCE multiplier (Civ1: Democracy
                // +50% science; others 1.0). Multiply the baseline (one
                // research point per city per turn) by scienceMul and
                // round-up so a Democracy with 2 cities yields 2*1.5=3,
                // not 3.0->floor=3 (same answer here but ceil keeps a
                // 1-city Democracy gaining 2/turn instead of 1, which
                // matches the "Republic +1 trade per tile" character).
                float sciMul = 1.0f;
                if (civId >= 0 && std::size_t(civId) < um.civs().size()) {
                    Government eg = um.effectiveGovernment(civId);
                    sciMul = governmentDefOf(eg).scienceMul;
                }
                int pts = int(std::ceil(float(cityCount) * sciMul));
                if (pts > 0) tr.addPoints(civId, pts);
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
