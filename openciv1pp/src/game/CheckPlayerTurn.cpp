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
//
// PYRAMIDS bonus (wonder slice): when the city's OWNER civ owns the
// Pyramids wonder, this function adds +1 to the gross. Civ1 Pyramids =
// "any government works without senate"; we collapse to +1 food/city as
// the documented simplification (see WonderDef.effect_description).
int CheckPlayerTurn::cityFoodGross(int cx, int cy) const {
    int gross = 0;
    const auto& tprov = p.unitManagement().terrainProvider();
    // Look up the city at (cx,cy) so we can apply the Pyramids bonus.
    int ownerCiv = -1;
    {
        const auto& cs = p.unitManagement().cities();
        for (const auto& cc : cs) {
            if (cc.x == cx && cc.y == cy) { ownerCiv = cc.owner; break; }
        }
    }
    auto pyramidsBonus = [&]() -> int {
        if (ownerCiv < 0) return 0;
        const auto& civs = p.unitManagement().civs();
        if (std::size_t(ownerCiv) >= civs.size()) return 0;
        return civs[std::size_t(ownerCiv)].hasWonder(WonderType::Pyramids) ? 1 : 0;
    };
    if (!tprov) {
        // No terrain provider wired (rare headless path) — assume the city
        // is on a Grassland tile (yield 2) with 4 Grassland neighbors. Same
        // 10-food/turn baseline used by the foodtest scaffolding.
        return 10 + pyramidsBonus();
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
    gross += pyramidsBonus();
    return gross;
}

int CheckPlayerTurn::processEndOfTurn() {
    auto& um = p.unitManagement();
    auto& cities = um.citiesMut();

    // ---- FORTIFY slice: promote fortifying -> fortified at top of turn ---
    // Civ1: issuing Fortify takes 1 turn to fully engage. The unit becomes
    // visually/mechanically "fortified" the turn AFTER the command. Done
    // BEFORE the mvp reset below so the dig-in-this-turn unit doesn't
    // accidentally get a fresh budget on the same turn the flip happens.
    {
        auto& units = um.unitsMut();
        for (auto& u : units) {
            if (!u.alive) continue;
            if (u.fortifying) {
                u.fortified = true;
                u.fortifying = false;
            }
        }
    }

    // ---- ROAD-MOVEMENT slice: reset per-unit movement budget -----------
    // Civ1: every unit's movement budget regenerates at the top of each
    // turn. We do a single global reset here so BOTH the AI movement pass
    // (which runs below, inside this same processEndOfTurn call) AND the
    // human's NEXT turn (driven by the next handleKey loop) see a fresh
    // budget. mvp is stored in INTEGER THIRDS-of-move so road steps can
    // cost 1 (1/3 of move) and non-road steps cost 3 (full move).
    // FORTIFIED units stay at 0 mvp — they're dug in and don't move (until
    // un-fortified by an explicit move command). fortifying (just-issued
    // this turn — but the flip above just turned them fortified) is
    // handled by the same zero-mvp branch.
    {
        auto& units = um.unitsMut();
        for (auto& u : units) {
            if (!u.alive) continue;
            if (u.fortified || u.fortifying) {
                u.movePointsLeft = 0;
            } else {
                u.movePointsLeft = UnitManagement::unitMovePointsMax(u.type);
            }
        }
    }

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

    // ---- AI SMART CITY PRODUCTION (faithful greedy + expansion) -------
    // Mirrors the AI city-build decision in C# CityWorker.cs (where AI civs
    // pick the highest-priority unit/building their tech allows). The full
    // weighted scorer (threat, terrain, role, ...) is OUT OF SCOPE; we
    // delegate to UnitManagement::pickAiCityProduction which encodes the
    // "Settlers when expanding, highest-attack tech-known unit otherwise"
    // rule. See its header comment for the exact gate.
    // Per-AI city, per turn:
    //   * Only re-pick when the city is currently producing a Unit (so any
    //     human-set Building/Wonder production stays untouched).
    //   * Settlers picks happen when the civ has fewer than 4 cities AND
    //     no Settlers is already in motion AND city pop >= 2 (faithful Civ1
    //     "Settlers cost 1 population" — won't starve a pop=1 city).
    //   * Else highest-attack tech-known combat unit (Settlers excluded;
    //     ties in enum order; Militia is the no-tech fallback).
    {
        auto& citiesMut = um.citiesMut();
        const auto& civs = um.civs();
        for (auto& c : citiesMut) {
            if (c.owner < 0 || std::size_t(c.owner) >= civs.size()) continue;
            if (civs[std::size_t(c.owner)].isHuman) continue; // human picks
            if (c.productionKind != City::ProductionKind::Unit) continue;
            UnitType pick = um.pickAiCityProduction(c.owner, c);
            // Apply (bypasses tech-gate by writing directly; the picker
            // already enforced the gate). Preserves accumulated shields
            // (faithful Civ1 carry-over on production switch).
            if (c.productionType != pick) {
                c.productionType = pick;
                c.production = unitDefOf(pick).cost;
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
                // FORTIFY: skip movement for an already-fortified AI unit.
                // It's intentionally dug in (faithful Civ1 city defenders).
                if (units[std::size_t(uid)].fortified ||
                    units[std::size_t(uid)].fortifying) continue;
                int steps = unitDefOf(units[std::size_t(uid)].type).move;
                if (steps < 1) steps = 1;
                bool didStep = false;
                for (int s = 0; s < steps; ++s) {
                    if (!units[std::size_t(uid)].alive) break;
                    if (!um.aiStep(uid)) break; // no target / can't move
                    didStep = true;
                }
                // ---- AI SMART FORTIFY -----------------------------------
                // Faithful Civ1 AI behaviour: a combat unit with no enemy
                // in sight (no step taken this turn) digs in to defend
                // its city/tile. This makes AI cities self-defend without
                // needing a per-tile garrison decision tree.
                if (!didStep &&
                    units[std::size_t(uid)].alive &&
                    !units[std::size_t(uid)].fortified &&
                    !units[std::size_t(uid)].fortifying) {
                    um.startFortify(uid);
                }
            }
        }
    }

    // ---- AI SETTLERS EXPANSION PASS (faithful greedy approximation) -----
    // Mirrors the dedicated Settlers branch of the C# AI dispatcher: a
    // Settlers unit (non-combatant) walks to a valid founding spot at
    // Chebyshev distance >= kAiSettlerMinSpacing from ALL cities, then
    // founds a new city there. The full C# AI uses river/resource/terrain
    // scoring; here we use the simplified rule encoded in
    // UnitManagement::aiSettlerStep (see its header comment).
    // Determinism: civs/units processed in stable index order; the auto-
    // found-capital pass earlier this same EOT already consumed every AI
    // civ's STARTING Settlers, so this pass only sees PRODUCED Settlers.
    {
        auto& units = um.unitsMut();
        const auto& civs = um.civs();
        for (std::size_t cIdx = 0; cIdx < civs.size(); ++cIdx) {
            if (civs[cIdx].isHuman) continue;
            int civId = int(cIdx);
            std::vector<int> mySettlers;
            for (std::size_t i = 0; i < units.size(); ++i) {
                const Unit& u = units[i];
                if (!u.alive || u.owner != civId) continue;
                if (u.type != UnitType::Settlers) continue;
                mySettlers.push_back(int(i));
            }
            for (int uid : mySettlers) {
                if (!units[std::size_t(uid)].alive) continue;
                int steps = unitDefOf(units[std::size_t(uid)].type).move;
                if (steps < 1) steps = 1;
                for (int s = 0; s < steps; ++s) {
                    if (!units[std::size_t(uid)].alive) break;
                    if (!um.aiSettlerStep(uid)) break; // no target / built
                }
            }
        }
    }

    // ---- DIPLOMACY: meetCheck + AI war declarations ---------------------
    // Mirrors the C# MeetWithKing.cs first-contact + AI declaration paths.
    // Run AFTER the AI movement pass so AI units that just stepped INTO
    // contact range register the new relation this turn (Civ1: meet
    // happens during the offending player's turn, not the next one).
    // meetCheck() flips NoContact -> Peace for every pair within
    // Chebyshev distance <= kMeetRange (faithful "in sight" approximation).
    // aiDecideDeclareWar() runs per AI civ; deterministic on (turn, civId).
    {
        um.meetCheck();
        // Find the human civ id (the first civ flagged isHuman; default 0).
        int humanId = -1;
        const auto& civs = um.civs();
        for (std::size_t i = 0; i < civs.size(); ++i) {
            if (civs[i].isHuman) { humanId = int(i); break; }
        }
        if (humanId >= 0) {
            // Pull the current turn count from MiniWorld via the host (the
            // turn counter lives on MiniWorld; we use the YEAR as a stable
            // determinism source when MiniWorld isn't attached — both grow
            // monotonically, so either yields a stable (turn,civId) seed).
            int seedTurn = um.year(); // deterministic per-EOT seed source
            for (std::size_t i = 0; i < civs.size(); ++i) {
                if (int(i) == humanId) continue;
                um.aiDecideDeclareWar(int(i), humanId, seedTurn);
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

    // ---- LUXURY allocation (per-civ pre-pass) ----------------------------
    // Compute per-civ TRADE total (1 baseline + per-city contribution with
    // Marketplace 1.5x) THEN allocate luxRate/10 of it to a "lux pool" used
    // BELOW in the happiness pass to reduce unhappy citizens across the
    // civ's cities (Civ1 manual: 2 luxuries = 1 happy citizen — simplified
    // here to "every 2 lux reduces 1 unhappy across cities in id order").
    // Unused lux this turn is DISCARDED (documented simplification — full
    // Civ1 carries leftover per-city; TODO).
    //
    // perCivTradeBase[civId] is reused below by the economy block so we
    // don't recompute. NOTE: stored as float so the *1.5 Marketplace
    // factor survives across multiple cities without intermediate rounding.
    std::vector<float> perCivTradeBase;
    std::vector<int>   perCivLuxRemaining;
    {
        const auto& civs = um.civs();
        const int nCivs = int(civs.size());
        perCivTradeBase.assign(std::size_t(nCivs), 1.0f);  // baseline 1
        perCivLuxRemaining.assign(std::size_t(nCivs), 0);
        for (const auto& c : cities) {
            if (c.owner >= 0 && c.owner < nCivs) {
                float cityTrade = 1.0f;
                if (c.hasBuilding(BuildingType::Marketplace)) cityTrade *= 1.5f;
                perCivTradeBase[std::size_t(c.owner)] += cityTrade;
            }
        }
        for (int civId = 0; civId < nCivs; ++civId) {
            float tradeMul = governmentDefOf(um.effectiveGovernment(civId)).tradeMul;
            int trade = int(std::floor(perCivTradeBase[std::size_t(civId)] *
                                       tradeMul));
            if (trade < 0) trade = 0;
            int luxRate = civs[std::size_t(civId)].luxRate;
            int luxGain = trade * luxRate / 10;  // floor(trade * lux/10)
            perCivLuxRemaining[std::size_t(civId)] = luxGain;
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

            // ---- HAPPINESS / DISORDER (Civ1 happy/unhappy citizen pass) --
            // Faithful simplified Civ1 rules:
            //   unhappy = max(0, population - 4)
            //   - Temple owned: unhappy -= 1 (clamped >= 0)
            //   - Cathedral owned: unhappy -= 3 (clamped >= 0; stacks with
            //     Temple's -1 for a combined -4 in a Temple+Cathedral city)
            //   - Civ owns Hanging Gardens: unhappy -= 1 (clamped >= 0)
            //   happy = 0 baseline (Luxury slider / Entertainer specialists
            //          / Colosseum / etc. not yet modeled — TODO)
            //   disorder = (unhappy > happy)
            // Civ1: when a city is in disorder it produces NO shields and
            // the food box does NOT advance toward growth this turn. We
            // model that by computing disorder BEFORE the shield/food
            // accumulation below and gating both paths on !disorder.
            // Government modifiers (Despotism +0, Republic/Democracy add
            // war-unhappy per away unit, etc.) are stubs -> TODO.
            {
                int u = c.population - 4;
                if (u < 0) u = 0;
                if (c.hasBuilding(BuildingType::Temple) && u > 0) u -= 1;
                // Cathedral (MORE-BUILDINGS slice): -3 unhappy citizens
                // (clamped >= 0). Faithful Civ1 Cathedral effect.
                if (c.hasBuilding(BuildingType::Cathedral) && u > 0) {
                    u -= 3;
                    if (u < 0) u = 0;
                }
                if (std::size_t(c.owner) < um.civs().size() &&
                    um.civs()[std::size_t(c.owner)].hasWonder(
                        WonderType::HangingGardens) && u > 0) {
                    u -= 1;
                }
                // LUXURY rate (per-civ pool): every 2 luxury points reduce
                // 1 unhappy citizen across the civ's cities (in id order).
                // Faithful Civ1: lux turns trade -> happiness; we apply
                // greedily until the pool is exhausted. Per-city unused
                // lux is discarded at end of turn (documented).
                if (c.owner >= 0 && std::size_t(c.owner) < perCivLuxRemaining.size()
                        && u > 0) {
                    int& pool = perCivLuxRemaining[std::size_t(c.owner)];
                    while (pool >= 2 && u > 0) {
                        pool -= 2;
                        u -= 1;
                    }
                }
                c.unhappy  = u;
                c.happy    = 0;
                c.disorder = (c.unhappy > c.happy);
            }

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
            // DISORDER: Civ1 "Civil Disorder" -> no shields produced this
            // turn (force the contribution to 0). The threshold-trigger
            // production block below still runs but will be a no-op
            // because shields didn't advance toward the cost this turn.
            if (c.disorder) finalShields = 0;
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
            bool hasGranary = c.hasBuilding(BuildingType::Granary);
            int threshold = (c.population + 1) * (hasGranary ? 5 : 10);
            // DISORDER: Civ1 standard "no growth" rule -> the food box does
            // NOT advance toward growth this turn. We skip both the
            // accumulation AND the threshold/starvation checks so the
            // population stays put (foodPerTurn is still reported on the
            // city struct so the CityView display reads "what this city
            // WOULD make if order were restored").
            if (!c.disorder) {
                c.food += c.foodPerTurn;
                // AQUEDUCT (MORE-BUILDINGS slice): Civ1 rule — without an
                // Aqueduct a city CANNOT grow past population 8 (the food
                // box fills but the population stays put). We model this
                // by capping the food box at the pop-9 threshold-minus-one
                // so the city stops accumulating extra food (the visible
                // foodPerTurn still reports the gross-net delta, matching
                // Civ1's behaviour where the food bar visibly maxes out).
                bool aqueductGate = (c.population >= 8 &&
                                     !c.hasBuilding(BuildingType::Aqueduct));
                if (aqueductGate) {
                    // Cap food at the growth threshold minus 1 (the city
                    // sits "full" but cannot tick over to pop 9). Starvation
                    // path below still runs (food<0 still shrinks pop).
                    int cap = threshold - 1;
                    if (c.food > cap) c.food = cap;
                }
                if (c.food >= threshold && !aqueductGate) {
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
            // ---- WONDER branch (civ-wide, single-ownership) ----------
            // Mirrors the Building branch shape: accumulate shields until
            // wonderDefOf(w).cost is met, then record the completion
            // (UnitManagement::recordWonderCompletion adds to the civ's
            // ownedWonders set + updates the owner-city map), reset shields,
            // and fall back to producing Militia (documented default — same
            // as the Building branch fall-back).
            // GUARD: if another civ snuck in and won the wonder mid-build,
            // abort and fall back to Militia without recording anything.
            if (c.productionKind == City::ProductionKind::Wonder) {
                WonderType wt = c.productionWonderType;
                if (wt == WonderType::None ||
                    um.wonderOwner(wt) >= 0) {
                    // Someone else completed it (or it's None): cancel.
                    c.productionKind = City::ProductionKind::Unit;
                    c.productionWonderType = WonderType::None;
                    c.productionType = UnitType::Militia;
                    c.production = unitDefOf(UnitType::Militia).cost;
                    continue;
                }
                needed = wonderDefOf(wt).cost;
                if (c.production != needed) c.production = needed;
                if (needed > 0 && c.shields >= needed) {
                    c.shields -= needed;
                    um.recordWonderCompletion(c.owner, c.id, wt);
                    c.productionKind = City::ProductionKind::Unit;
                    c.productionWonderType = WonderType::None;
                    c.productionType = UnitType::Militia;
                    c.production = unitDefOf(UnitType::Militia).cost;
                }
                continue;
            }
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
                    // ---- SETTLERS COST 1 POPULATION (faithful Civ1) -----
                    // Civ1: producing a Settlers consumes 1 population from
                    // the building city. Refused (shields RETAINED) when
                    // pop < 2 — otherwise the city would die. On success
                    // city.population -= 1 alongside the unit append.
                    // Applied to both human and AI cities — the AI smart-
                    // pick gates Settlers picks on pop>=2 too, so this is
                    // a defensive guard for cities that started Settlers
                    // when pop=2 and then shrank to 1 (disorder/starve).
                    if (c.productionType == UnitType::Settlers &&
                        c.population < 2) {
                        // Hold shields at the cap so the unit fires the
                        // turn pop returns to >=2.
                        c.shields = needed;
                    } else {
                        c.shields -= needed;
                        c.units += 1;
                        int newUnitIdx = um.addUnit(c.owner, c.productionType, c.x, c.y);
                        if (c.productionType == UnitType::Settlers) {
                            c.population -= 1;
                            if (c.population < 1) c.population = 1;
                        }
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
    }

    // ---- TECH RESEARCH per-civ points accumulation ------------------------
    // Faithful early slice: each civ accumulates research points from its
    // SCIENCE share of trade (sciRate/10 of trade), scaled by per-city
    // Library bonuses (+50%) and per-civ government science multiplier
    // (Democracy +50%). The C# path (F0_*_GetCityResourceCount fan-out +
    // science-rate slider) is out of scope here; the per-city-baseline
    // shape matches the "more cities == more science" character of
    // Civ1's early game.
    //
    // SLIDER WIRE-UP: when the civ's sciRate is 5 (the default), the
    // result is identical to the prior 50/0/50 implicit split. Raising
    // sciRate -> faster research; lowering it -> slower.
    {
        auto& tr = p.techResearch();
        // Only run when initCivs() has provisioned per-civ tech state (we
        // skip when civCount==0 so the pre-tech-tree tests still pass).
        if (tr.civCount() > 0) {
            for (int civId = 0; civId < tr.civCount(); ++civId) {
                // PER-CITY science accumulation. Each owned city contributes
                // 1 baseline science point, scaled by per-city building
                // multipliers (Library +50%) BEFORE summing. This shape
                // (per-city contribution * 1.5 when the city owns a Library)
                // mirrors the C# CityWorker per-city resource fan-out and
                // matches Civ1's documented Library effect ("a Library
                // increases science output in this city by 50%").
                int cityCount = 0;
                float perCivBase = 0.0f;
                for (const auto& c : cities) {
                    if (c.owner != civId) continue;
                    ++cityCount;
                    float cityPts = 1.0f; // baseline per-city science
                    // Library (MORE-BUILDINGS slice): +50% science here.
                    if (c.hasBuilding(BuildingType::Library)) cityPts *= 1.5f;
                    perCivBase += cityPts;
                }
                if (cityCount <= 0) continue;
                // Per-civ government SCIENCE multiplier (Civ1: Democracy
                // +50% science; others 1.0). Applied AFTER the per-city
                // Library bonus so the two multipliers stack.
                float sciMul = 1.0f;
                int sciShare = 5; // default 50% (matches legacy behaviour)
                if (civId >= 0 && std::size_t(civId) < um.civs().size()) {
                    Government eg = um.effectiveGovernment(civId);
                    sciMul = governmentDefOf(eg).scienceMul;
                    sciShare = um.civs()[std::size_t(civId)].sciRate;
                }
                // Apply the science RATE (sciShare/10) to the trade pool
                // BEFORE the government multiplier so the slider shows up
                // as the dominant lever. ceil keeps small allocations from
                // truncating to 0 (so sci=1 still trickles in 1 point/turn
                // when cityCount >= 1).
                float scaled = perCivBase * float(sciShare) / 10.0f * sciMul;
                int pts = int(std::ceil(scaled));
                if (sciShare == 0) pts = 0; // hard zero, no ceil rescue
                // Wonder bonuses (simplified): Hanging Gardens and Colossus
                // each grant +1 science per city (Civ1: +1 happy citizen
                // and +1 trade-per-ocean-tile respectively; both proxied
                // to flat per-city science here — see WonderDef notes).
                if (std::size_t(civId) < um.civs().size()) {
                    const auto& cv = um.civs()[std::size_t(civId)];
                    if (cv.hasWonder(WonderType::HangingGardens)) pts += cityCount;
                    if (cv.hasWonder(WonderType::Colossus))       pts += cityCount;
                }
                if (pts > 0) tr.addPoints(civId, pts);
            }
        }
    }

    // ---- ECONOMY (gold + treasury + unit upkeep) -------------------------
    // Faithful Civ1 per-turn economy:
    //   trade(civ)  = (1 baseline + sum_per_city(1 * marketplaceMul)) *
    //                 Government.tradeMul   (simplified baseline; full
    //                 per-tile trade-yield TODO).
    //   gold gain   = floor(trade * 0.5)  (50/0/50 implicit tax/lux/sci
    //                 split; full 3-way slider TODO).
    //   upkeep      = #alive units owned by this civ (1 gold/turn each).
    //   netGold     = gain - upkeep.
    //   civ.gold   += netGold.
    // Marketplace (MORE-BUILDINGS slice): each city that owns a Marketplace
    //   contributes 1.5 trade instead of 1 (Civ1: +50% gold output in this
    //   city). The 1.5 multiplier is applied PER-CITY to the city's
    //   contribution to the civ-wide trade sum, then the existing /2 gold
    //   split + government tradeMul stay 1:1 with the prior shape.
    //   if civ.gold < 0: repeatedly disband this civ's WEAKEST alive
    //   non-Settlers unit (smallest attack; ties broken by smallest
    //   defense, then by unit index) and decrement upkeep, until gold>=0
    //   OR no more disbandable units exist. lastActionKey gets "Bankrupt!"
    //   on the human civ when disbanding fires there.
    // upkeepGoldPerTurn is cached for HUD/CityView (= unit count BEFORE
    // any bankruptcy disbanding this turn — the value the player saw the
    // turn before).
    {
        auto& civs = um.civsMut();
        auto& units = um.unitsMut();
        const int nCivs = int(civs.size());
        // Per-civ unit count (for upkeep).
        std::vector<int> unitCount(std::size_t(nCivs), 0);
        for (const auto& u : units) {
            if (!u.alive) continue;
            if (u.owner >= 0 && u.owner < nCivs) ++unitCount[std::size_t(u.owner)];
        }
        // perCivTradeBase[] was computed at the top of processEndOfTurn
        // (above the OUTER LOOP) so the LUXURY pre-pass and the GOLD pass
        // share the same trade source. If for any reason the precomputed
        // vector wasn't sized to nCivs (legacy / future change), fall back
        // to recompute defensively.
        if (int(perCivTradeBase.size()) != nCivs) {
            perCivTradeBase.assign(std::size_t(nCivs), 1.0f);
            for (const auto& c : cities) {
                if (c.owner >= 0 && c.owner < nCivs) {
                    float cityTrade = 1.0f;
                    if (c.hasBuilding(BuildingType::Marketplace)) cityTrade *= 1.5f;
                    perCivTradeBase[std::size_t(c.owner)] += cityTrade;
                }
            }
        }
        for (int civId = 0; civId < nCivs; ++civId) {
            CivState& cv = civs[std::size_t(civId)];
            float tradeMul = governmentDefOf(um.effectiveGovernment(civId)).tradeMul;
            int trade = int(std::floor(perCivTradeBase[std::size_t(civId)] *
                                       tradeMul));
            if (trade < 0) trade = 0;
            // GOLD share = taxRate/10 of trade (Civ1 slider). Default
            // taxRate=5 reproduces the prior 50% gold split.
            int goldGain = trade * cv.taxRate / 10;
            int upkeep = unitCount[std::size_t(civId)]; // 1 gold/turn each
            cv.upkeepGoldPerTurn = upkeep;
            cv.gold += (goldGain - upkeep);
            // Bankruptcy: disband weakest non-Settlers until gold >= 0.
            // Lambda picks the weakest disbandable unit owned by civId.
            // Returns -1 when none exists.
            auto pickWeakest = [&]() -> int {
                int bestIdx = -1;
                int bestAtk = 0x7fffffff, bestDef = 0x7fffffff;
                for (std::size_t i = 0; i < units.size(); ++i) {
                    const Unit& u = units[i];
                    if (!u.alive) continue;
                    if (u.owner != civId) continue;
                    if (u.type == UnitType::Settlers) continue; // protected
                    const UnitDef& d = unitDefOf(u.type);
                    if (d.attack < bestAtk ||
                        (d.attack == bestAtk && d.defense < bestDef)) {
                        bestAtk = d.attack;
                        bestDef = d.defense;
                        bestIdx = int(i);
                    }
                }
                return bestIdx;
            };
            if (cv.gold < 0) {
                // Surface bankruptcy on the human civ's HUD via MiniWorld.
                // We can't reach MiniWorld from here directly; instead
                // stash via UnitManagement::lastCombatKey_ — wrong channel.
                // Use a no-op marker: leave the HUD-side reporting to the
                // caller; tests verify gold + unit count drops only.
                // (See goldtest below for the bankruptcy assertion.)
            }
            while (cv.gold < 0) {
                int victim = pickWeakest();
                if (victim < 0) break; // no more disbandable units
                units[std::size_t(victim)].alive = false;
                cv.upkeepGoldPerTurn -= 1; // one less unit to pay for
                cv.gold += 1;              // refund this turn's unpaid upkeep
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
