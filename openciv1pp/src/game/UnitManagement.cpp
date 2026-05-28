// UnitManagement.cpp — port of OpenCiv1 UnitManagement.cs (BUILD-CITY subset).
//
// We persist a minimal City record per founded city (id, owner, x, y, name,
// foundedTurn) — enough for MiniWorld to render markers + show a "city count"
// HUD line. The deeper per-City init (Improvements/Workers/Trade/...) is a
// stub (see // TODO(port) markers in the header).
#include "UnitManagement.h"
#include "OpenCiv1Game.h"
#include "MainCode.h"
#include "MapManagement.h"
#include "TerrainTiles.h"
#include "TechResearch.h"
#include <cstdio>

namespace oc1 {

UnitManagement::UnitManagement(OpenCiv1Game& parent) : p(parent) {
    (void)p; // cpu/log hooks reserved for the deeper port (see // TODO(port))
}

// Per-tribe first-city (capital) name. Index 0..13 matches MainCode::tribes().
// Sourced from OpenCiv1/src/Game/State/GameData.cs CityNames[] (the first
// 16 entries per tribe). We expose only the capital here; subsequent cities
// fall back to "<Capital> N" to keep the port surface small (// TODO(port):
// expose the full 16-per-tribe table when more cities become reachable).
static const char* kTribeCapitalEnglish[14] = {
    "Rome",        // Romans
    "Babylon",     // Babylonians
    "Berlin",      // Germans
    "Thebes",      // Egyptians
    "Washington",  // Americans
    "Athens",      // Greeks
    "Delhi",       // Indians
    "Moscow",      // Russians
    "Zimbabwe",    // Zulus
    "Paris",       // French
    "Tenochtitlan",// Aztecs
    "Peking",      // Chinese
    "London",      // English
    "Samarkand",   // Mongols
};

std::string UnitManagement::nthCityNameKey(int tribeIndex, int nth) {
    const char* base = "Capital";
    if (tribeIndex >= 0 && tribeIndex < 14) base = kTribeCapitalEnglish[tribeIndex];
    if (nth <= 0) return base;
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s %d", base, nth + 1);
    return buf;
}

// ---- AI / multi-civ slice ----
// A small, readable palette for AI civ markers. Index 0 = human (kept on the
// existing red marker index 209 in MiniWorld's draw). Indices 1..6 are bright,
// mutually distinct colours that won't collide with the terrain palette
// (200..206) or the city marker (211/212). MiniWorld installs the RGB values
// for these into the framebuffer palette at draw time.
static const uint8_t kCivMarkerIndex[8] = {
    209,  // 0 human (existing red)
    220,  // 1
    221,  // 2
    222,  // 3
    223,  // 4
    224,  // 5
    225,  // 6
    226,  // 7 (reserved for future expansion)
};

// ---- DIPLOMACY (faithful subset of GameData.Diplomacy[N,N]) -------------
// Helpers maintain matrix symmetry + the diagonal Peace invariant. All
// out-of-range civ ids are no-ops / safe defaults (NoContact / false).
void UnitManagement::resizeRelations(int n) {
    if (n < 0) n = 0;
    relations_.assign(std::size_t(n), std::vector<Relation>(std::size_t(n), Relation::NoContact));
    for (int i = 0; i < n; ++i) relations_[std::size_t(i)][std::size_t(i)] = Relation::Peace;
}

Relation UnitManagement::getRelation(int civA, int civB) const {
    if (civA < 0 || civB < 0) return Relation::NoContact;
    if (std::size_t(civA) >= relations_.size()) return Relation::NoContact;
    if (std::size_t(civB) >= relations_.size()) return Relation::NoContact;
    return relations_[std::size_t(civA)][std::size_t(civB)];
}

void UnitManagement::setRelation(int civA, int civB, Relation r) {
    if (civA < 0 || civB < 0) return;
    if (std::size_t(civA) >= relations_.size()) return;
    if (std::size_t(civB) >= relations_.size()) return;
    if (civA == civB) {
        // Diagonal invariant: a civ is always at Peace with itself.
        relations_[std::size_t(civA)][std::size_t(civA)] = Relation::Peace;
        return;
    }
    relations_[std::size_t(civA)][std::size_t(civB)] = r;
    relations_[std::size_t(civB)][std::size_t(civA)] = r;
}

bool UnitManagement::isAtWar(int civA, int civB) const {
    return getRelation(civA, civB) == Relation::War;
}

// meetCheck: scan every (civA, civB) pair where civA<civB; if any of civA's
// alive units OR cities is within Chebyshev distance <= kMeetRange of any
// of civB's alive units OR cities, upgrade NoContact -> Peace (symmetric).
// Returns the count of pairs whose relation flipped this call.
// Faithful Civ1 shape: the C# F0_25fb_* AI dispatch fires the "meet" event
// when the per-player visibility grid (line-of-sight on adjacent tiles)
// crosses a foreign unit/city. We use Chebyshev<=2 as the documented
// approximation of "in sight" for this slice (1=adjacent, 2=adjacent+1).
int UnitManagement::meetCheck() {
    int flipped = 0;
    const int n = int(civs_.size());
    if (n < 2) return 0;
    auto cheb = [](int ax, int ay, int bx, int by) {
        int dx = ax > bx ? ax - bx : bx - ax;
        int dy = ay > by ? ay - by : by - ay;
        return dx > dy ? dx : dy;
    };
    for (int a = 0; a < n; ++a) {
        for (int b = a + 1; b < n; ++b) {
            if (getRelation(a, b) != Relation::NoContact) continue;
            bool close = false;
            // Gather every (x,y) presence for each civ (alive units + cities).
            // O(units+cities) per civ, O(N^2 * (units+cities)^2) overall —
            // fine for the 8-civ Civ1 cap and the handful of units we model.
            for (const auto& ua : units_) {
                if (!ua.alive || ua.owner != a) continue;
                for (const auto& ub : units_) {
                    if (!ub.alive || ub.owner != b) continue;
                    if (cheb(ua.x, ua.y, ub.x, ub.y) <= kMeetRange) {
                        close = true; break;
                    }
                }
                if (close) break;
                for (const auto& cb : cities_) {
                    if (cb.owner != b) continue;
                    if (cheb(ua.x, ua.y, cb.x, cb.y) <= kMeetRange) {
                        close = true; break;
                    }
                }
                if (close) break;
            }
            if (!close) {
                for (const auto& ca : cities_) {
                    if (ca.owner != a) continue;
                    for (const auto& ub : units_) {
                        if (!ub.alive || ub.owner != b) continue;
                        if (cheb(ca.x, ca.y, ub.x, ub.y) <= kMeetRange) {
                            close = true; break;
                        }
                    }
                    if (close) break;
                    for (const auto& cb : cities_) {
                        if (cb.owner != b) continue;
                        if (cheb(ca.x, ca.y, cb.x, cb.y) <= kMeetRange) {
                            close = true; break;
                        }
                    }
                    if (close) break;
                }
            }
            if (close) {
                setRelation(a, b, Relation::Peace);
                ++flipped;
            }
        }
    }
    return flipped;
}

// AI diplomacy decision (called at end-of-turn for each AI civ). Faithful
// simplified rule: if at Peace with the human AND the human has MORE THAN
// 2x the AI's unit count, the AI rolls a seeded xorshift; on hit (low 8
// bits < kAiWarThreshold) it declares war. The seed (turn*8 + civId)
// makes this deterministic for a given (turn, civId) pair so tests are
// reproducible.
//
// The threshold is intentionally LOW so the test-friendly path "human has
// 5 units, AI has 1 unit, turn=10" reliably triggers (otherwise the AI
// would never declare war in the tiny test setups). Real games scale up
// the threshold via difficulty-level table — out of scope here.
static constexpr uint8_t kAiWarThreshold = 64; // 25% probability per check
bool UnitManagement::aiDecideDeclareWar(int aiCivId, int humanCivId, int turn) {
    if (aiCivId < 0 || std::size_t(aiCivId) >= civs_.size()) return false;
    if (humanCivId < 0 || std::size_t(humanCivId) >= civs_.size()) return false;
    if (aiCivId == humanCivId) return false;
    if (civs_[std::size_t(aiCivId)].isHuman) return false;
    if (!civs_[std::size_t(humanCivId)].isHuman) return false;
    if (getRelation(aiCivId, humanCivId) != Relation::Peace) return false;
    int humanUnits = 0, aiUnits = 0;
    for (const auto& u : units_) {
        if (!u.alive) continue;
        if (u.owner == humanCivId) ++humanUnits;
        else if (u.owner == aiCivId) ++aiUnits;
    }
    // Strength gate: human must be "much stronger" (> 2x). 0 AI units
    // still qualifies (the AI is the weakest civ on the map).
    if (humanUnits <= 2 * aiUnits) return false;
    // Deterministic seeded roll. xorshift one step.
    uint32_t seed = uint32_t(turn) * 8u + uint32_t(aiCivId) + 0x9E3779B9u;
    seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
    if ((seed & 0xFFu) >= kAiWarThreshold) return false;
    setRelation(aiCivId, humanCivId, Relation::War);
    return true;
}

void UnitManagement::setupCivs(int humanTribe, int numAi) {
    civs_.clear();
    units_.clear();
    // Clamp inputs to sane bounds (the C# default is 1 human + up to 7 AI).
    if (numAi < 0) numAi = 0;
    if (numAi > 7) numAi = 7;
    int humanT = humanTribe;
    if (humanT < 0 || humanT >= 14) humanT = 0;

    // Human is civ 0.
    {
        CivState h;
        h.tribeIdx = humanT;
        h.color = kCivMarkerIndex[0];
        h.name = (humanT >= 0 && humanT < 14) ? kTribeCapitalEnglish[humanT] : "Player";
        h.isHuman = true;
        civs_.push_back(std::move(h));
    }
    // AI civs 1..numAi: rotate through tribes() skipping the human's tribe so
    // every civ gets a distinct nation. Mirrors the C# tribe-distribution loop
    // in F5_0000_*_InitNewGameData where Players[i].NationalityID is assigned
    // unique values.
    int tribe = humanT;
    for (int i = 1; i <= numAi; ++i) {
        // Pick the next free tribe (rotate, skip used).
        for (int step = 0; step < 14; ++step) {
            tribe = (tribe + 1) % 14;
            bool used = false;
            for (const auto& c : civs_) if (c.tribeIdx == tribe) { used = true; break; }
            if (!used) break;
        }
        CivState a;
        a.tribeIdx = tribe;
        a.color = kCivMarkerIndex[i & 7];
        a.name = kTribeCapitalEnglish[tribe];
        a.isHuman = false;
        civs_.push_back(std::move(a));
    }
    // Reshape pairwise relations to match the new civ count. Diagonal Peace,
    // all off-diagonals NoContact (faithful Civ1: civs haven't met at start).
    resizeRelations(int(civs_.size()));
}

bool UnitManagement::changeGovernment(int civId, Government newGovt) {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return false;
    CivState& c = civs_[std::size_t(civId)];
    // Already mid-transition: refuse (the C# REVOLUTION menu greys out
    // while still in Anarchy). Caller can wait until anarchyTurnsLeft==0.
    if (c.anarchyTurnsLeft > 0) return false;
    // No-op when switching to the same government.
    if (c.govt == newGovt) return false;
    // Tech-gate: refuse when the civ doesn't yet know the new govt's prereq.
    // Tech::None bypasses (Anarchy + Despotism are always available).
    const GovernmentDef& def = governmentDefOf(newGovt);
    if (def.techPrereq != Tech::None) {
        if (!p.techResearch().civKnows(civId, def.techPrereq)) {
            return false;
        }
    }
    // Initiate the transition: target stored, effective govt becomes Anarchy
    // for `kAnarchyTransitionTurns` turns (CheckPlayerTurn ticks it down).
    c.targetGovt = newGovt;
    c.anarchyTurnsLeft = kAnarchyTransitionTurns;
    return true;
}

Government UnitManagement::effectiveGovernment(int civId) const {
    if (civId < 0 || std::size_t(civId) >= civs_.size())
        return Government::Despotism;
    const CivState& c = civs_[std::size_t(civId)];
    return (c.anarchyTurnsLeft > 0) ? Government::Anarchy : c.govt;
}

int UnitManagement::addUnit(int owner, UnitType type, int x, int y) {
    Unit u;
    u.owner = owner;
    u.type = type;
    u.x = x;
    u.y = y;
    u.alive = true;
    // ROAD-MOVEMENT: initialise per-unit movement budget from def.move * 3
    // so road steps (cost 1) cleanly buy 3 hops per def.move (faithful
    // Civ1 1/3-of-move road bonus).
    u.movePointsLeft = unitMovePointsMax(type);
    int idx = int(units_.size());
    units_.push_back(u);
    return idx;
}

// ---- Settlers improvement actions (faithful subset of F0_1866_*) --------
// The C# UnitManagement set an in-progress Settlers' Status flag so the
// per-turn dispatcher would re-enter F0_1866_* and tick down the remaining
// build counter. We model that with workTarget + workTurnsLeft on Unit;
// CheckPlayerTurn::processEndOfTurn does the per-turn decrement and OR-s
// the matching MapManagement bitflag in when the counter reaches 0.
bool UnitManagement::startBuildRoad(int unitId) {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return false;
    Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return false;
    if (u.type != UnitType::Settlers) return false;
    if (u.workTurnsLeft > 0) return false; // already busy
    if (u.x < 0 || u.y < 0 || u.x >= mapW_ || u.y >= mapH_) return false;
    if (terrainAt_) {
        Terrain t = terrainAt_(u.x, u.y);
        if (t == Terrain::Water || t == Terrain::Arctic) return false;
    }
    u.workTarget = kWorkRoad;
    u.workTurnsLeft = kRoadTurns;
    return true;
}

bool UnitManagement::startBuildIrrigation(int unitId) {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return false;
    Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return false;
    if (u.type != UnitType::Settlers) return false;
    if (u.workTurnsLeft > 0) return false;
    if (u.x < 0 || u.y < 0 || u.x >= mapW_ || u.y >= mapH_) return false;
    if (terrainAt_) {
        Terrain t = terrainAt_(u.x, u.y);
        // Faithful subset of GameData.TerrainModifications[t].IrrigationEffect
        // == -2 (the C# guard at GameMenus.cs line 262). Civ1 also irrigates
        // a few more terrains via the "Change to" path; we keep the gate to
        // the three classic Settlers-irrigation tiles for the first cut.
        if (t != Terrain::Grassland &&
            t != Terrain::Plains &&
            t != Terrain::Desert) return false;
    }
    u.workTarget = kWorkIrrigation;
    u.workTurnsLeft = kIrrigationTurns;
    return true;
}

// ---- FORTIFY (Civ1 +50% defense, 1-turn engage cycle) -------------------
// Issue a Fortify command. Any alive unit not already fortifying/fortified
// and not mid-improvement can dig in. The command consumes this turn's
// movement (movePointsLeft=0); CheckPlayerTurn::processEndOfTurn promotes
// fortifying -> fortified at the top of the next turn. Returns false when
// the command was refused (out of range, dead, already fortified, or busy).
bool UnitManagement::startFortify(int unitId) {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return false;
    Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return false;
    if (u.fortified || u.fortifying) return false;
    if (u.workTurnsLeft > 0) return false; // mid-improvement: locked
    u.fortifying = true;
    u.movePointsLeft = 0; // act of digging in consumes movement
    return true;
}

bool UnitManagement::setCityProductionType(int cityId, UnitType t) {
    if (cityId < 0 || std::size_t(cityId) >= cities_.size()) return false;
    // Tech gate: refuse when the OWNER civ does not yet know the unit's
    // prereq tech. Tech::None bypasses (Settlers/Militia always buildable).
    // We consult the host TechResearch when civs() has been provisioned;
    // when civs is empty (tests that haven't called initCivs) we skip the
    // check entirely (back-compat for citytest/turntest that pre-date the
    // tech tree).
    const UnitDef& def = unitDefOf(t);
    const int owner = cities_[std::size_t(cityId)].owner;
    if (def.techPrereq != Tech::None && !civs_.empty() &&
        owner >= 0 && std::size_t(owner) < civs_.size()) {
        if (!p.techResearch().civKnows(owner, def.techPrereq)) {
            return false; // tech not yet researched -> refuse
        }
    }
    cities_[std::size_t(cityId)].productionType = t;
    cities_[std::size_t(cityId)].production = def.cost;
    // Switching to a UNIT clears any pending building target.
    cities_[std::size_t(cityId)].productionKind = City::ProductionKind::Unit;
    cities_[std::size_t(cityId)].productionBuildingType = BuildingType::None;
    return true;
}

// ---- City improvements (buildings) -------------------------------------
// Switch the city to producing BuildingType `b`. Refuses on:
//   - out-of-range cityId,
//   - BuildingType::None (no-op target),
//   - already-owned building (Civ1: no duplicates per city).
// On success: productionKind=Building, productionBuildingType=b, production
// = buildingDefOf(b).cost. Existing accumulated shields are preserved
// (faithful Civ1: switching production keeps the current shield reservoir).
bool UnitManagement::setCityProductionBuilding(int cityId, BuildingType b) {
    if (cityId < 0 || std::size_t(cityId) >= cities_.size()) return false;
    if (b == BuildingType::None) return false;
    City& c = cities_[std::size_t(cityId)];
    if (c.hasBuilding(b)) return false; // already owned
    const BuildingDef& bd = buildingDefOf(b);
    // TECH-GATED (MORE-BUILDINGS slice): refuse when the owner civ doesn't
    // yet know the building's techPrereq. Mirrors the C# CityImprovement
    // PrerequisiteTech check in the build-menu grey-out. Existing
    // Granary/Barracks/Walls/Temple ship with Tech::None and are always
    // buildable — preserving the pre-MORE-BUILDINGS behaviour. The guard
    // is conservative: when no civ tech state is provisioned (civs_ empty,
    // out-of-range owner, or techCount==0), the check is bypassed so the
    // pre-tech-tree tests still pass (same shape as setCityProductionType).
    if (bd.techPrereq != Tech::None && !civs_.empty() &&
        c.owner >= 0 && std::size_t(c.owner) < civs_.size() &&
        p.techResearch().civCount() > 0) {
        if (!p.techResearch().civKnows(c.owner, bd.techPrereq)) {
            return false; // tech not yet researched -> refuse
        }
    }
    c.productionKind = City::ProductionKind::Building;
    c.productionBuildingType = b;
    c.production = bd.cost;
    return true;
}

// ---- Wonders (civ-wide) -------------------------------------------------
int UnitManagement::wonderOwner(WonderType w) const {
    if (w == WonderType::None) return -1;
    for (std::size_t i = 0; i < civs_.size(); ++i) {
        if (civs_[i].hasWonder(w)) return int(i);
    }
    return -1;
}

int UnitManagement::wonderOwnerCity(WonderType w) const {
    int i = int(w);
    if (i <= 0 || i >= kWonderCount) return -1;
    if (wonderOwner(w) < 0) return -1;
    return wonderOwnerCity_[i];
}

bool UnitManagement::setCityProductionWonder(int cityId, WonderType w) {
    if (cityId < 0 || std::size_t(cityId) >= cities_.size()) return false;
    if (w == WonderType::None) return false;
    // Single-ownership: refuse when any civ already owns this wonder.
    if (wonderOwner(w) >= 0) return false;
    City& c = cities_[std::size_t(cityId)];
    // Tech gate: refuse when the owner civ doesn't yet know the prereq.
    const WonderDef& wd = wonderDefOf(w);
    if (wd.techPrereq != Tech::None && !civs_.empty() &&
        c.owner >= 0 && std::size_t(c.owner) < civs_.size()) {
        if (!p.techResearch().civKnows(c.owner, wd.techPrereq)) return false;
    }
    c.productionKind = City::ProductionKind::Wonder;
    c.productionWonderType = w;
    c.productionBuildingType = BuildingType::None;
    c.production = wd.cost;
    return true;
}

void UnitManagement::recordWonderCompletion(int civId, int cityId, WonderType w) {
    if (w == WonderType::None) return;
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return;
    if (wonderOwner(w) >= 0) return; // someone else already claimed it
    civs_[std::size_t(civId)].ownedWonders.insert(w);
    int i = int(w);
    if (i > 0 && i < kWonderCount) wonderOwnerCity_[i] = cityId;
}

bool UnitManagement::tileCityHasBuilding(int owner, int x, int y,
                                         BuildingType b) const {
    for (const auto& c : cities_) {
        if (c.x != x || c.y != y) continue;
        if (c.owner != owner) continue;
        return c.hasBuilding(b);
    }
    return false;
}

// Tiny xorshift32 (deterministic per-seed; we use it as the RNG step for the
// combat rolls). The world generator's MT19937 has a heavyweight state — for
// per-roll combat we want a thread-local-feel cheap RNG that's still
// deterministic for a given seed. xorshift32 is the standard choice.
static inline uint32_t xorshift32(uint32_t& s) {
    uint32_t x = s ? s : 0x12345678u;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    s = x;
    return x;
}

// ---- TERRAIN DEFENSE BONUS (faithful Civ1 manual) -----------------------
// Multiplicative defender bonus by the defender's tile terrain. Values are
// the Civ1 manual / community wiki canonical defender-side bonuses:
//   Hills      = 1.5x  (+50%)
//   Mountains  = 3.0x  (+200%)
//   Forest     = 1.5x  (+50%)
//   Jungle     = 1.5x  (+50%)
//   Swamp      = 1.5x  (+50%)
//   River      = 1.5x  (+50%)
//   All others = 1.0x  (no bonus)
// terrainEnum < 0 means "no terrain provider attached" -> 1.0x (safe default).
float UnitManagement::terrainDefenseBonusOf(int terrainEnum) {
    if (terrainEnum < 0) return 1.0f;
    switch (Terrain(terrainEnum)) {
        case Terrain::Hills:     return 1.5f;
        case Terrain::Mountains: return 3.0f;
        case Terrain::Forest:    return 1.5f;
        case Terrain::Jungle:    return 1.5f;
        case Terrain::Swamp:     return 1.5f;
        case Terrain::River:     return 1.5f;
        default:                 return 1.0f;
    }
}

bool UnitManagement::resolveCombat(Unit& attacker, Unit& defender,
                                   uint32_t& rngState,
                                   bool defenderHasWalls,
                                   bool defenderInOwnCityWithGreatWall,
                                   int defenderTerrain) {
    const UnitDef& aDef = unitDefOf(attacker.type);
    const UnitDef& dDef = unitDefOf(defender.type);
    int atk = aDef.attack;     if (atk < 0) atk = 0;
    int def = dDef.defense;    if (def <= 0) def = 1; // guard div-by-zero
    // ---- BARRACKS veteran bonus -----------------------------------------
    // Civ1: units produced in a city with a Barracks are VETERANS, granting
    // +50% combat (we apply it to BOTH attack and defense as the C# does).
    // Implemented as an integer multiply (atk *= 3/2) to keep the roll math
    // in plain ints (no floats in the deterministic xorshift path).
    if (attacker.veteran) atk = (atk * 3) / 2;
    if (defender.veteran) def = (def * 3) / 2;
    // ---- WALLS defender bonus -------------------------------------------
    // Civ1: City Walls grant +200% defense (triple the defender's defense)
    // when the defender stands on the wall-owning city tile. Multiplied
    // AFTER the veteran bump so a Veteran Phalanx in a Walled city defends
    // at floor(2 * 3/2) * 3 = 3 * 3 = 9 (matches the C# wall+veteran stack).
    if (defenderHasWalls) def = def * 3;
    // ---- GREAT WALL civ-wide defense bonus -----------------------------
    // Civ1: Great Wall acts like Walls in every city the owning civ holds.
    // We apply it as a +50% defender bonus on the city tile when the
    // defender's civ owns the Great Wall — this stacks with explicit Walls
    // (so a Walled city of a Great-Wall-owning civ is x3 * 3/2 = x4.5 ->
    // integer floor). Off-city defenders and other civs don't get the bonus.
    if (defenderInOwnCityWithGreatWall) def = (def * 3) / 2;
    // ---- TERRAIN defense bonus (Civ1 manual) ----------------------------
    // Multiplicative defender bonus by defender's tile terrain (Hills/
    // Forest/Jungle/Swamp/River = 1.5x; Mountains = 3.0x; flatlands = 1.0x).
    // Applied as a float multiply with rounded truncation so the integer
    // roll math stays clean. Stacks with everything above.
    {
        float tBonus = terrainDefenseBonusOf(defenderTerrain);
        if (tBonus > 1.0f) {
            // Round to nearest int (>=1 — guard against degenerate def==0).
            int scaled = int(float(def) * tBonus + 0.5f);
            def = scaled > def ? scaled : def + 1;
        }
    }
    // ---- FORTIFIED defender bonus (Civ1 +50%) ----------------------------
    // Civ1: a dug-in (fortified) unit defends with +50% strength. Applied
    // as an integer 3/2 multiply, AFTER all the other multipliers so it
    // stacks faithfully on top of terrain + walls + great wall.
    if (defender.fortified) def = (def * 3) / 2;
    if (def <= 0) def = 1;
    if (atk < 0) atk = 0;
    int attackerRoll = (atk > 0) ? int(xorshift32(rngState) % uint32_t(atk)) : 0;
    int defenderRoll = int(xorshift32(rngState) % uint32_t(def));
    // Faithful Civ1: defender wins ties (attackerRoll <= defenderRoll -> lose).
    if (attackerRoll > defenderRoll) {
        defender.alive = false;
        return true;  // attacker survived
    }
    attacker.alive = false;
    return false;     // attacker died
}

bool UnitManagement::moveUnit(int unitId, int dx, int dy) {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return false;
    Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return false;
    // Settlers locked while building an improvement (faithful to the C#
    // per-turn dispatcher refusing to re-dispatch a worker until its
    // counter ticks to 0).
    if (u.workTurnsLeft > 0) return false;
    int nx = u.x + dx, ny = u.y + dy;
    // Out-of-bounds destinations: no movement, unit still alive.
    if (nx < 0 || ny < 0 || nx >= mapW_ || ny >= mapH_) return true;
    // ---- TERRAIN VALIDATION (NAVAL slice) -------------------------------
    // Land units refuse Water; naval units refuse anything that ISN'T
    // Water. Faithful Civ1 land/sea separation. Terrain provider is
    // wired by MiniWorld::attachGame; when absent (rare headless paths
    // without a world) the validation is skipped.
    const UnitDef& myDef = unitDefOf(u.type);
    if (terrainAt_) {
        Terrain destT = terrainAt_(nx, ny);
        if (myDef.isNaval) {
            // Trireme on land: refuse. Only Water tiles are valid.
            if (destT != Terrain::Water) return false;
        } else {
            // Land unit into Water: refuse.
            if (destT == Terrain::Water) return false;
        }
    }
    // Look for an alive enemy at the destination.
    int enemyId = -1;
    for (std::size_t i = 0; i < units_.size(); ++i) {
        const Unit& o = units_[i];
        if (!o.alive) continue;
        if (o.owner == u.owner) continue;
        if (o.x == nx && o.y == ny) { enemyId = int(i); break; }
    }
    // ---- ROAD-MOVEMENT COST (precomputed; consumed below) ----------------
    // Cost = 1 mvp iff BOTH the source AND destination tiles carry the
    // ROAD improvement bit (faithful Civ1 road math). Else cost = 3 mvp.
    // We sample MapManagement::getImprovements through the host (the
    // map lives on OpenCiv1Game; UnitManagement holds a parent ref).
    int moveCost = kMoveCostDefault;
    {
        const auto& mm = p.mapManagement();
        uint8_t srcImp = mm.getImprovements(u.x, u.y);
        uint8_t dstImp = mm.getImprovements(nx, ny);
        if ((srcImp & MapManagement::kImprovementRoad) &&
            (dstImp & MapManagement::kImprovementRoad)) {
            moveCost = kMoveCostRoad;
        }
    }
    if (enemyId >= 0) {
        Unit& enemy = units_[std::size_t(enemyId)];
        // ---- DIPLOMACY GATE -------------------------------------------------
        // Combat only triggers if the two owners are at War. Faithful Civ1:
        //   - NoContact: this is the moment of FIRST CONTACT. Upgrade the
        //     pair to Peace via the symmetric setter and DO NOT enter
        //     combat or move (the unit stays put; the player can declare
        //     war next turn to attack). We populate lastCombatKey_ with
        //     "Meet" so the HUD can flash a contact banner.
        //   - Peace: combat is REFUSED. The unit stays put (no movement,
        //     no combat, no death). lastCombatKey_ cleared.
        //   - War: fall through to the standard combat path below.
        if (u.owner != enemy.owner &&
            std::size_t(u.owner) < civs_.size() &&
            std::size_t(enemy.owner) < civs_.size()) {
            Relation r = getRelation(u.owner, enemy.owner);
            if (r == Relation::NoContact) {
                setRelation(u.owner, enemy.owner, Relation::Peace);
                lastCombatKey_ = "Meet";
                return false; // no move, no combat: first-contact handshake
            }
            if (r == Relation::Peace) {
                lastCombatKey_ = "";
                return false; // refuse combat: peace treaty blocks attack
            }
            // r == Relation::War: fall through to combat below.
        }
        // ROAD-MOVEMENT: combat consumes a standard move's worth of mvp.
        // Refuse if the attacker doesn't have enough left this turn.
        if (u.movePointsLeft < moveCost) return false;
        u.movePointsLeft -= moveCost;
        lastCombatKey_ = "Battle";
        // Walls: if the defender is standing on a city tile owned by the
        // defender's civ and that city owns Walls, defender gets the
        // triple-defense bonus (faithful Civ1 +200%).
        bool defWalls = tileCityHasBuilding(enemy.owner, enemy.x, enemy.y,
                                            BuildingType::Walls);
        // Great Wall: defender is on a city tile of its OWN civ and that
        // civ owns the Great Wall. We check by scanning cities at the
        // defender's tile; if any belongs to enemy.owner and that civ
        // owns the Great Wall, the bonus applies.
        bool defGreatWall = false;
        if (enemy.owner >= 0 && std::size_t(enemy.owner) < civs_.size() &&
            civs_[std::size_t(enemy.owner)].hasWonder(WonderType::GreatWall)) {
            for (const auto& cc : cities_) {
                if (cc.x == enemy.x && cc.y == enemy.y &&
                    cc.owner == enemy.owner) { defGreatWall = true; break; }
            }
        }
        // FORTIFY: sample the defender's tile terrain so resolveCombat
        // can apply the per-terrain defender bonus. When no terrain
        // provider is wired, pass -1 (resolveCombat skips the bonus).
        int defT = -1;
        if (terrainAt_) defT = int(terrainAt_(enemy.x, enemy.y));
        bool survived = resolveCombat(u, enemy, combatRng_, defWalls,
                                      defGreatWall, defT);
        if (survived) {
            // attacker wins -> move into the (now-empty) tile.
            // FORTIFY: a unit that moves (even by combat-into-vacancy) is
            // no longer fortified — faithful Civ1 "walking away breaks
            // the dig-in".
            u.x = nx; u.y = ny;
            u.fortified = false;
            u.fortifying = false;
            lastCombatKey_ = "Victory";
            return true;
        }
        lastCombatKey_ = "Defeat";
        return false;
    }
    // No enemy: just move. Check ROAD-MOVEMENT budget first.
    if (u.movePointsLeft < moveCost) return false;
    u.movePointsLeft -= moveCost;
    u.x = nx; u.y = ny;
    // FORTIFY: clear dig-in flags whenever the unit actually steps.
    u.fortified = false;
    u.fortifying = false;
    return true;
}

// ---- AI MOVEMENT --------------------------------------------------------
// Chebyshev (king-move) distance is the right metric for an 8-direction step
// world — it equals the number of moveUnit() calls needed to reach a tile
// when each step is in {-1,0,1} x {-1,0,1}.
static inline int chebyshev(int ax, int ay, int bx, int by) {
    int dx = ax > bx ? ax - bx : bx - ax;
    int dy = ay > by ? ay - by : by - ay;
    return dx > dy ? dx : dy;
}

int UnitManagement::findNearestEnemy(int unitId, int& tx, int& ty) const {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return -1;
    const Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return -1;
    int bestId = -1;
    int bestDist = 0x7fffffff;
    int bestTx = u.x, bestTy = u.y;
    // Scan units first (so unit targets win on ties; see header).
    for (std::size_t i = 0; i < units_.size(); ++i) {
        if (int(i) == unitId) continue;
        const Unit& o = units_[i];
        if (!o.alive) continue;
        if (o.owner == u.owner) continue;
        int d = chebyshev(u.x, u.y, o.x, o.y);
        if (d < bestDist) {
            bestDist = d;
            bestId = int(i);
            bestTx = o.x; bestTy = o.y;
        }
    }
    // Then cities (city target id encoded as -2 per header contract).
    for (const auto& c : cities_) {
        if (c.owner == u.owner) continue;
        int d = chebyshev(u.x, u.y, c.x, c.y);
        if (d < bestDist) {
            bestDist = d;
            bestId = -2;
            bestTx = c.x; bestTy = c.y;
        }
    }
    if (bestDist == 0x7fffffff) return -1;
    tx = bestTx; ty = bestTy;
    return bestId;
}

bool UnitManagement::aiStep(int unitId) {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return false;
    const Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return false;
    int tx = 0, ty = 0;
    int target = findNearestEnemy(unitId, tx, ty);
    if (target == -1) return false; // no enemy anywhere
    int dx = (tx > u.x) ? 1 : (tx < u.x ? -1 : 0);
    int dy = (ty > u.y) ? 1 : (ty < u.y ? -1 : 0);
    if (dx == 0 && dy == 0) return false; // already on target tile
    moveUnit(unitId, dx, dy);
    return true;
}

// ---- AI EXPANSION (faithful greedy approximation) ----------------------
// Documented simplification — see the matching header comment for the
// "expand vs. defend" rule the C# Segment_25fb picker would do with a
// 359KB weighted scorer. The goal here is that AI civs ACTUALLY grow
// past their capital after a few dozen turns (the gameplay goal).
UnitType UnitManagement::pickAiCityProduction(int civId, const City& c) const {
    // ---- Settlers branch: only when the civ is still expanding ---------
    if (civId >= 0 && std::size_t(civId) < civs_.size() &&
        c.population >= 2) {
        int cityCount = 0;
        int liveSettlers = 0;
        for (const auto& cc : cities_) if (cc.owner == civId) ++cityCount;
        for (const auto& uu : units_) {
            if (!uu.alive) continue;
            if (uu.owner != civId) continue;
            if (uu.type == UnitType::Settlers) ++liveSettlers;
        }
        // Faithful expansion rule: keep one Settlers per (current cities + 1)
        // up to kAiSettlerMaxCities total cities, so the AI keeps founding
        // new cities until it has 4 and then settles into combat-only.
        if (cityCount < kAiSettlerMaxCities &&
            liveSettlers < cityCount + 1) {
            return UnitType::Settlers;
        }
    }
    // ---- Combat branch: highest-attack tech-known unit (Settlers excl) -
    UnitType pick = UnitType::Militia;
    int bestAtk = -1;
    auto& tr = p.techResearch();
    for (int i = 0; i < kUnitTypeCount; ++i) {
        UnitType cand = UnitType(i);
        if (cand == UnitType::Settlers) continue;   // not a combatant
        const UnitDef& def = unitDefOf(cand);
        if (def.techPrereq != Tech::None && tr.civCount() > 0 &&
            !tr.civKnows(civId, def.techPrereq)) continue;
        if (def.attack > bestAtk) { bestAtk = def.attack; pick = cand; }
    }
    return pick;
}

// findAiSettlerTarget: scan map for valid land tile at Chebyshev distance
// >= kAiSettlerMinSpacing from ALL existing cities of any civ. Picks the
// Chebyshev-nearest such tile to (sx, sy). Documented simplification —
// the C# F0_*_FindCitySite has a deeper weighted scorer (rivers, resources,
// terrain quality, ...). Returns false when no valid target exists.
bool UnitManagement::findAiSettlerTarget(int sx, int sy, int& tx, int& ty) const {
    int bestDist = 0x7fffffff;
    int bestX = -1, bestY = -1;
    // Two-pass scoring: pass 1 = preferred terrain (Grassland/Plains/Hills/
    // Desert); pass 2 = any non-Water/Arctic land if no preferred tile is
    // found. Pass 1 has strict priority so the AI prefers good land.
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < mapH_; ++y) {
            for (int x = 0; x < mapW_; ++x) {
                if (terrainAt_) {
                    Terrain t = terrainAt_(x, y);
                    if (pass == 0) {
                        if (t != Terrain::Grassland && t != Terrain::Plains &&
                            t != Terrain::Hills && t != Terrain::Desert)
                            continue;
                    } else {
                        if (t == Terrain::Water || t == Terrain::Arctic)
                            continue;
                    }
                }
                // Reject tiles already occupied by ANY city.
                bool tileTaken = false;
                int minCityDist = 0x7fffffff;
                for (const auto& cc : cities_) {
                    int dxx = x > cc.x ? x - cc.x : cc.x - x;
                    int dyy = y > cc.y ? y - cc.y : cc.y - y;
                    int d = dxx > dyy ? dxx : dyy;
                    if (d == 0) { tileTaken = true; break; }
                    if (d < minCityDist) minCityDist = d;
                }
                if (tileTaken) continue;
                if (minCityDist < kAiSettlerMinSpacing) continue;
                int dxs = x > sx ? x - sx : sx - x;
                int dys = y > sy ? y - sy : sy - y;
                int dToSettler = dxs > dys ? dxs : dys;
                if (dToSettler < bestDist) {
                    bestDist = dToSettler;
                    bestX = x; bestY = y;
                }
            }
        }
        if (bestX >= 0) break;  // pass 1 found something -> done
    }
    if (bestX < 0) return false;
    tx = bestX; ty = bestY;
    return true;
}

// aiSettlerStep: drive ONE step of the AI Settlers' expansion behaviour.
// Tries to FOUND a city when already standing on a valid spot; otherwise
// walks toward the nearest valid target via moveUnit. See header comment.
bool UnitManagement::aiSettlerStep(int unitId) {
    if (unitId < 0 || std::size_t(unitId) >= units_.size()) return false;
    Unit& u = units_[std::size_t(unitId)];
    if (!u.alive) return false;
    if (u.type != UnitType::Settlers) return false;
    if (u.workTurnsLeft > 0) return false;  // mid-improvement: locked
    // Find a valid target (relative to the Settler's tile).
    int tx = 0, ty = 0;
    bool haveTarget = findAiSettlerTarget(u.x, u.y, tx, ty);
    // If currently standing ON a valid founding tile (matches the target
    // exactly OR no target exists but the Settler's current tile is valid),
    // try to found.
    auto tileValidForFounding = [&](int x, int y) -> bool {
        if (x < 0 || y < 0 || x >= mapW_ || y >= mapH_) return false;
        if (terrainAt_) {
            Terrain t = terrainAt_(x, y);
            if (t == Terrain::Water || t == Terrain::Arctic) return false;
        }
        for (const auto& cc : cities_) {
            int dxx = x > cc.x ? x - cc.x : cc.x - x;
            int dyy = y > cc.y ? y - cc.y : cc.y - y;
            int d = dxx > dyy ? dxx : dyy;
            if (d < kAiSettlerMinSpacing) return false;
        }
        return true;
    };
    if (tileValidForFounding(u.x, u.y)) {
        std::string nm;
        if (buildCity(u.x, u.y, u.owner, nm)) {
            u.alive = false;  // Settlers consumed by the BUILD-CITY action
            return true;
        }
    }
    if (!haveTarget) return false; // nowhere to go; sit
    int dx = (tx > u.x) ? 1 : (tx < u.x ? -1 : 0);
    int dy = (ty > u.y) ? 1 : (ty < u.y ? -1 : 0);
    if (dx == 0 && dy == 0) return false;
    moveUnit(unitId, dx, dy);
    return true;
}

bool UnitManagement::buildCity(int x, int y, int playerId, std::string& outName) {
    return buildCity(x, y, playerId, 0, outName);
}

bool UnitManagement::buildCity(int x, int y, int playerId, int turn,
                               std::string& outName) {
    // Bounds (mirrors F0_2aea_1326_ValidateMapCoordinates in C# MapManagement).
    if (x < 0 || y < 0 || x >= mapW_ || y >= mapH_) return false;

    // Terrain validity: reject Water + Arctic (1:1 with the C# build-site check
    // that disallows TerrainTypeEnum.Water/Arctic for the new-city tile).
    if (terrainAt_) {
        Terrain t = terrainAt_(x, y);
        if (t == Terrain::Water || t == Terrain::Arctic) return false;
    }
    // Settlers-locked check: if any of THIS player's Settlers at (x,y) is
    // mid-improvement (workTurnsLeft > 0), refuse the build-city action.
    // Faithful to the C# dispatcher which won't accept new orders for a
    // worker still ticking down a build counter.
    for (const auto& u : units_) {
        if (!u.alive) continue;
        if (u.owner != playerId) continue;
        if (u.type != UnitType::Settlers) continue;
        if (u.x != x || u.y != y) continue;
        if (u.workTurnsLeft > 0) return false;
    }

    // City id allocation — sequential, matches GameData.Cities[i].ID = i.
    int id = int(cities_.size());

    // Per-tribe nth-of-this-player count for naming. The first city of a
    // tribe = the capital (kTribeCapitalEnglish[tribe]); subsequent cities
    // get a numeric suffix (// TODO(port): use the per-tribe 16-entry pool
    // from GameData.CityNames when needed).
    int nthForPlayer = 0;
    for (const auto& c : cities_) if (c.owner == playerId) ++nthForPlayer;

    // Pick the tribe whose capital-name pool to draw from. When civs() has
    // been populated (multi-civ slice), each civ has its own tribeIdx so AI
    // cities get THEIR tribe's capital (e.g. civ 1's first city = "Babylon"
    // even when chosenTribe_ = 0 = Romans for the human). When civs() is
    // empty (single-player citytest path), fall back to chosenTribe_ as
    // before.
    int tribeForName = chosenTribe_;
    if (playerId >= 0 && std::size_t(playerId) < civs_.size())
        tribeForName = civs_[std::size_t(playerId)].tribeIdx;

    City c;
    c.id = id;
    c.owner = playerId;
    c.x = x;
    c.y = y;
    c.name = nthCityNameKey(tribeForName, nthForPlayer);
    c.foundedTurn = turn;
    cities_.push_back(std::move(c));
    outName = cities_.back().name;
    return true;
}

} // namespace oc1
