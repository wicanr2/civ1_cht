// UnitManagement.h — ported CodeObject (OpenCiv1 UnitManagement.cs, F0_1866_*).
//
// Ports the BUILD-CITY action faithfully: a Settlers unit on a valid land
// terrain founds a city at (x,y) with an owner playerID and a tribe-derived
// name. Mirrors the bookkeeping in OpenCiv1's UnitManagement.F0_1866_* family:
// the original creates a City record at the unit's tile (validating land vs.
// water/ice) and removes the Settlers. The deeper city init (production /
// worker tiles / improvements) is OUT OF SCOPE here — we only persist a "city
// placed at (x,y) by player N, named S, on turn T" record (the minimum the C#
// City class needs to be queryable: ID, PlayerID, Position, NameID + the
// rendered/looked-up name). cpu.* hooks (RNG, log) are wired the same way the
// other ported CodeObjects use the shared OpenCiv1Game.cpu.
//
// Faithful mapping:
//   * City record fields (id, owner, x, y, name, foundedTurn) match the subset
//     of OpenCiv1.City used by F0_1866_01dc and the GameData City[] arrays
//     (see OpenCiv1/src/Game/State/City.cs and GameData.cs Cities[128]).
//   * Invalid build sites = Water + Arctic (1:1 with the C# build-site checks
//     that reject TerrainTypeEnum.Water and Arctic).
//
// STUBS (// TODO(port) in .cpp):
//   * deeper city init (Improvements, WorkerFlags, ActualSize, FoodCount, ...)
//   * Settlers-unit consumption inside the Players[].Units[] table (we only
//     return success; the caller (MiniWorld) handles the visible-side effect).
//   * production/work tiles, trade route arrays, city-status flags.
#pragma once
#include "TerrainTiles.h"
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace oc1 {

class OpenCiv1Game;

// ---- AI / multi-civ slice ----
// Unit types — a faithful SUBSET of C# UnitTypeEnum (UnitTypeEnum.cs lines
// 9-41). Settlers/Militia/Phalanx cover the early-era handful the gameplay
// slice needs (founder + cheap attacker + cheap defender). Numeric values
// are kept 1:1 with the C# enum so save/load round-trip is straight-through
// when the deeper port lands.
enum class UnitType : uint8_t { Settlers = 0, Militia = 1, Phalanx = 2 };

// A unit's fixed stats — faithful subset of UnitDefinition.cs (only the
// fields actually consumed by the combat / cost / movement paths in this
// slice). `cost` is the SHIELD threshold (C# stores Cost as 10s, multiplied
// by local_4a in CityWorker.cs ~line 838; we store the EFFECTIVE shield cost
// directly so end-of-turn comparisons stay simple — e.g. Militia=10 shields).
struct UnitDef {
    const char* name;     // English key (Translator turns it into Chinese)
    int attack;           // AttackStrength (UnitDefinition.cs line 14)
    int defense;          // DefenseStrength (UnitDefinition.cs line 15)
    int move;             // MoveCount (line 12)
    int cost;             // shield cost (Cost * local_4a, line 838)
};

// Faithful UnitDef table for the early-era handful. Index by UnitType.
// Values mirror GameData.cs lines 209-211:
//   Settlers: attack=0, defense=1, move=1, cost=4*10 = 40
//   Militia:  attack=1, defense=1, move=1, cost=1*10 = 10
//   Phalanx:  attack=1, defense=2, move=1, cost=2*10 = 20
inline const UnitDef& unitDefOf(UnitType t) {
    static const UnitDef kDefs[3] = {
        {"Settlers", 0, 1, 1, 40},
        {"Militia",  1, 1, 1, 10},
        {"Phalanx",  1, 2, 1, 20},
    };
    int i = int(t);
    if (i < 0 || i > 2) i = 0;
    return kDefs[i];
}

struct Unit {
    int owner = 0;            // civ index into UnitManagement::civs()
    int x = 0, y = 0;         // Position (matches C# GPoint)
    UnitType type = UnitType::Settlers;
    bool alive = true;
};

// Per-civilization state (a SUBSET of GameData.Players[i] + Nations[i]).
// Enough for {tribeIdx -> name & first-city name, color -> map marker, isHuman
// -> input routing}. The full Players[] record (Diplomacy, Treasury, Tech,
// Government, ...) is OUT OF SCOPE here.
struct CivState {
    int tribeIdx = 0;         // index into MainCode::tribes() (0..13)
    uint8_t color = 0;        // palette index for the AI unit marker
    std::string name;         // resolved nation name (e.g. "Romans")
    bool isHuman = false;
};

struct City {
    int id = -1;
    int owner = 0;            // PlayerID
    int x = 0, y = 0;         // Position (matches C# GPoint)
    std::string name;         // resolved + (later) translated city name
    int foundedTurn = 0;      // founding turn (derived from the world's turn)

    // Per-turn production state (mirrors City.ShieldsCount + the
    // Units[CurrentProductionID].Cost threshold in CityWorker.cs). `productionType`
    // is the unit type currently being built (mirrors City.CurrentProductionID
    // in C# CityWorker.cs ~line 836); `production` caches its UnitDef.cost so
    // the end-of-turn pass stays a single comparison. `units` is the running
    // count of units produced by this city.
    UnitType productionType = UnitType::Militia;  // default = cheapest defender
    int shields = 0;
    int production = 10;  // unitDefOf(productionType).cost, kept in sync
    int units = 0;
};

class UnitManagement {
public:
    explicit UnitManagement(OpenCiv1Game& parent);

    // Faithful (subset of) F0_1866_01dc / city-creation path: validate the
    // tile, allocate a new City record and assign it the next tribe city name.
    // Returns false when the tile is invalid (Water/Arctic) or out of bounds.
    // `outName` receives the resolved city name (English key — caller can run
    // it through Translator for the Chinese label, e.g. "Capital"->"首都").
    bool buildCity(int x, int y, int playerId, std::string& outName);

    // Same as above, but takes an explicit founding turn (mirrors GameData.
    // TurnCount being captured into City record on creation). Defaults to 0.
    bool buildCity(int x, int y, int playerId, int turn, std::string& outName);

    // Map width/height bounds for the validator. Set from MiniWorld at wire-up
    // time (so the headless tests can validate without a full map).
    void setMapBounds(int w, int h) { mapW_ = w; mapH_ = h; }

    // Terrain provider — MiniWorld plugs in its terrainAt(x,y) so buildCity
    // can reject Water/Arctic (the C# build-site validity check). When unset,
    // the validator only checks bounds (used by tests that don't care about
    // terrain; the dedicated water-tile test installs a provider).
    void setTerrainProvider(std::function<Terrain(int, int)> fn) {
        terrainAt_ = std::move(fn);
    }

    // The chosen tribe index (0..13) into MainCode::tribes() that the first-
    // city name resolver uses. -1 means "use the generic 'Capital' fallback".
    void setChosenTribe(int tribeIndex) { chosenTribe_ = tribeIndex; }
    int chosenTribe() const { return chosenTribe_; }

    const std::vector<City>& cities() const { return cities_; }
    std::vector<City>& citiesMut() { return cities_; } // CheckPlayerTurn uses this
    std::size_t cityCount() const { return cities_.size(); }

    // Total units produced across all cities (sum of City.units). Used by HUD
    // and tests to verify the threshold-trigger unit production loop.
    int totalUnitsProduced() const {
        int n = 0;
        for (const auto& c : cities_) n += c.units;
        return n;
    }

    // The terrain provider plugged in by MiniWorld::attachGame. Exposed so
    // CheckPlayerTurn can sample adjacent tiles for shield yield.
    const std::function<Terrain(int, int)>& terrainProvider() const { return terrainAt_; }

    // GameData.Year — Civ1 starts at -4000 (4000 BC) per StartGameMenu.cs.
    // The end-of-turn pass mutates this via CheckPlayerTurn::advanceYear.
    int year() const { return year_; }
    void setYear(int y) { year_ = y; }

    // The english key for the Nth city of this tribe (0 = capital). Falls back
    // to "Capital" for the first city of an unknown tribe; subsequent unknown-
    // tribe cities get "Capital 2", "Capital 3", etc.
    static std::string nthCityNameKey(int tribeIndex, int nth);

    // ---- AI / multi-civ slice ----
    // setupCivs: human is civ 0 with tribe=humanTribe; AI civs are 1..numAi,
    // each assigned a distinct tribe (rotated through MainCode::tribes()) and a
    // distinct palette colour index. Clears any prior civs/units. A 1:1 with
    // the START path in C# StartGameMenu.F5_0000_*_InitNewGameData where the
    // Players[0..7] table is built (human at GameData.HumanPlayerID, AI at the
    // others). The deeper Players[] init (Diplomacy/Treasury/Tech/ ...) stays
    // a STUB — only the {tribeIdx, color, name, isHuman} subset is materialised.
    void setupCivs(int humanTribe, int numAi);
    const std::vector<CivState>& civs() const { return civs_; }
    // Direct-write access used by GameLoadAndSave to restore civs from disk.
    std::vector<CivState>& civsMut() { return civs_; }

    // ---- units (multi-civ) ----
    // Append a Unit owned by `owner` (civ index) at (x,y). Returns its index.
    int addUnit(int owner, UnitType type, int x, int y);
    const std::vector<Unit>& units() const { return units_; }
    std::vector<Unit>& unitsMut() { return units_; }

    // Set a city's current production type AND sync the cached `production`
    // shield cost from the UnitDef table (so the end-of-turn threshold uses
    // the freshly-picked unit's cost without the caller doing it manually).
    // Mirrors the C# CityWorker.cs path that updates City.CurrentProductionID
    // and re-reads Units[ID].Cost on the next end-of-turn pass.
    void setCityProductionType(int cityId, UnitType t);

    // ---- COMBAT (faithful Civ1 formula) -----------------------------------
    // The reference combat path in C# is buried in Segment_25fb (the AI unit
    // dispatcher) and the per-unit move handler — too obfuscated for a 1:1
    // port. We use the well-documented Civ1 roll formula (Sid Meier interview
    // / Civ1 manual / community traces of the original asm):
    //     attackerRoll = rng() % attackerDef.attack
    //     defenderRoll = rng() % defenderDef.defense
    //     if attackerRoll > defenderRoll: defender dies, attacker survives
    //     else                          : attacker dies (defender wins ties)
    // The roll inputs use the same MT19937 source the world generator uses
    // (RandomMT19937 / IRB.RNG.RandomMT19937) so results are deterministic
    // for a given seed. Returns true when the attacker survives (and should
    // be moved into the defender's tile by the caller); false when defender
    // wins (attacker.alive = false, attacker stays put). Both units' .alive
    // flags are updated in place.
    //
    // Edge cases (faithful): attack==0 always loses (attackerRoll forced to
    // 0, but defenderRoll >= 0 -> defender wins ties). defense==0 is treated
    // as 1 to avoid div-by-zero (Civ1 has no 0-defense units; Settlers' 0
    // attack is the only zero stat in this slice).
    static bool resolveCombat(Unit& attacker, Unit& defender,
                              uint32_t& rngState);

    // moveUnit: move a unit by (dx,dy) by ONE step. When the destination tile
    // has an enemy alive unit, runs combat instead of moving (see resolveCombat).
    // Returns the new alive state of the moving unit (true == survived). Out-of-
    // bounds destinations are a no-op (returns true, unit not moved). Combat
    // outcome side-effects:
    //   - attacker wins -> defender.alive = false; attacker moves into the tile
    //   - defender wins -> attacker.alive = false; attacker stays put
    bool moveUnit(int unitId, int dx, int dy);

    // ---- AI MOVEMENT (faithful greedy approximation) ---------------------
    // findNearestEnemy: scans units_ for alive units owned by a different civ,
    // and scans cities_ for cities owned by a different civ. Returns the
    // Chebyshev-nearest target's tile in (tx,ty). Returns the unit id of the
    // nearest enemy UNIT, or -2 if a city is the nearest target, or -1 when
    // no target exists. Ties: cities are scanned after units, so unit targets
    // win on equal distance (first-found wins within a category).
    //
    // The full Civ1 AI (Segment_25fb.F0_25fb_0c9d, ~359KB of x86) chooses
    // targets using a weighted score over (distance, strength, terrain,
    // diplomacy, ...). We use the well-documented "greedy Chebyshev nearest"
    // heuristic as a faithful first-cut — this is the same shape every
    // public Civ1 AI reverse-engineering write-up uses for the "advance on
    // nearest threat" behaviour.
    int findNearestEnemy(int unitId, int& tx, int& ty) const;

    // aiStep: find nearest enemy and take ONE step toward it. Computes
    // dx = sign(tx-ux), dy = sign(ty-uy) (each in {-1,0,1}) and calls
    // moveUnit; if the step lands on an enemy tile, the existing combat
    // resolution fires. Returns true when a step (or combat) occurred,
    // false when no target exists or the unit is dead.
    bool aiStep(int unitId);

    // The last combat outcome (English key for the HUD): "" / "Victory" /
    // "Defeat" / "Battle" (in-progress). MiniWorld reads this for the HUD line.
    const std::string& lastCombatKey() const { return lastCombatKey_; }
    void setLastCombatKey(std::string k) { lastCombatKey_ = std::move(k); }

    // Deterministic RNG state for combat rolls (seeded from the world seed in
    // FrontEndFlow::enterPlaying; tests inject a known seed via setCombatRngSeed
    // so the win-rate statistics are reproducible). Public so tests can read
    // the post-roll state for assertions.
    uint32_t combatRngState() const { return combatRng_; }
    void setCombatRngSeed(uint32_t s) { combatRng_ = s ? s : 1u; }

private:
    OpenCiv1Game& p;
    std::vector<City> cities_;
    std::vector<Unit> units_;     // multi-civ units (Settlers for now)
    std::vector<CivState> civs_;  // human is civs_[0], AI civs follow
    int mapW_ = 80, mapH_ = 50;
    int chosenTribe_ = -1;
    std::function<Terrain(int, int)> terrainAt_;
    int year_ = -4000; // StartGameMenu.cs line 124: GameData.Year = -4000
    uint32_t combatRng_ = 0xCAFEBABEu; // tests override via setCombatRngSeed
    std::string lastCombatKey_;        // "" / "Victory" / "Defeat"
};

} // namespace oc1
