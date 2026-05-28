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
#include "TechResearch.h"
#include "Government.h"
#include <cstdint>
#include <functional>
#include <set>
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
// Cavalry (3) is the first unit with attack > 1 — added so combat-with-Walls
// statistics can be exercised against a non-degenerate baseline (atk=1 vs
// def=2 always rolls 0/0..1 which gives a 100% defender win rate even
// without Walls). Cavalry stats: a=2 d=1 m=2 cost=20 (faithful Civ1).
//
// MORE-UNITS slice (Legion/Knight/Catapult/Musketeers/Cannon): faithful
// Civ1 manual stats (atk/def/move/cost*10, see UnitDef table). Numeric
// values 4..8 do NOT mirror the C# UnitTypeEnum ordering 1:1 (C# has
// Legion=3, Musketeers=4, Cavalry=6, Knights=7, Catapult=8, Cannon=9) —
// because our existing Cavalry=3 was chosen ahead of the larger roster.
// The save format stores int(type), so the numeric values are STABLE
// across this port's saves; a future deeper port that wants 1:1 with C#
// will need a save migration step. Documented divergence.
enum class UnitType : uint8_t {
    Settlers   = 0,
    Militia    = 1,
    Phalanx    = 2,
    Cavalry    = 3,
    Legion     = 4,  // a=4 d=2 m=1 cost=40 (Civ1 manual) — tech IronWorking
    Knight     = 5,  // a=4 d=2 m=2 cost=40 (Civ1 manual) — tech Feudalism
    Catapult   = 6,  // a=6 d=1 m=1 cost=40 (Civ1 manual) — tech Mathematics
    Musketeers = 7,  // a=3 d=3 m=1 cost=30 (Civ1 manual) — tech Gunpowder
    Cannon     = 8,  // a=8 d=1 m=1 cost=40 (Civ1 manual) — tech Metallurgy
};
// Number of UnitType values shipped. Keep in sync with the kDefs table below.
static constexpr int kUnitTypeCount = 9;

// ---- DIPLOMACY (faithful Civ1 NoContact/Peace/War subset) ---------------
// Faithful SUBSET of the C# OpenCiv1 Diplomacy state machine
// (OpenCiv1/src/Game/CodeObjects/MeetWithKing.cs ~143KB; way too big to port
// 1:1) + the NxN GameData.Diplomacy[8,8] relation matrix (GameData.cs). The
// full C# state machine has many sub-states (Ceasefire, Treaty, Alliance,
// Vendetta, Embassy, ...); we ship the three classical ones the Civ1 manual
// + community wikis identify as the load-bearing diplomatic axis:
//   - NoContact: civs haven't met yet; combat is impossible, diplomacy
//                screen unreachable. Default for every civ pair on game start.
//   - Peace:     civs have met (got close enough at some point); combat
//                is REFUSED until war is declared. Default after the
//                meetCheck() upgrades NoContact -> Peace.
//   - War:       active combat allowed. Either side may have declared.
// Diagonal self-relation is Peace (a civ is never at war with itself).
// Matrix is symmetric: setRelation(A,B,r) sets both directions.
enum class Relation : uint8_t {
    NoContact = 0,  // default; never met (combat impossible)
    Peace     = 1,  // met; combat refused
    War       = 2,  // combat allowed
};

// English key for HUD/CityView display. Translator turns these into Chinese
// ("No Contact"->"未接觸", "Peace"->"和平", "War"->"戰爭").
inline const char* relationNameKey(Relation r) {
    switch (r) {
        case Relation::NoContact: return "No Contact";
        case Relation::Peace:     return "Peace";
        case Relation::War:       return "War";
    }
    return "No Contact";
}

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
    Tech techPrereq;      // required tech (Tech::None means buildable from
                          // game start). Mirrors UnitDefinition.PrerequisiteTech
                          // (GameData.cs line 209-236, last enum arg).
};

// Faithful UnitDef table for the early-era handful. Index by UnitType.
// Values mirror GameData.cs lines 209-211:
//   Settlers: attack=0, defense=1, move=1, cost=4*10 = 40
//   Militia:  attack=1, defense=1, move=1, cost=1*10 = 10
//   Phalanx:  attack=1, defense=2, move=1, cost=2*10 = 20
inline const UnitDef& unitDefOf(UnitType t) {
    static const UnitDef kDefs[kUnitTypeCount] = {
        // techPrereq matches the C# UnitDefinition.PrerequisiteTech args in
        // GameData.cs lines 209-211: Settlers/Militia = None; Phalanx =
        // BronzeWorking.
        // Cavalry stats are the standard Civ1 horseback (a=2 d=1 m=2 cost=20,
        // PrerequisiteTech HorsebackRiding). We leave the prereq as None for
        // now so existing tech-gated paths continue to behave; Cavalry is
        // primarily exercised by --buildingtest's combat statistics.
        {"Settlers", 0, 1, 1, 40, Tech::None},
        {"Militia",  1, 1, 1, 10, Tech::None},
        {"Phalanx",  1, 2, 1, 20, Tech::BronzeWorking},
        {"Cavalry",  2, 1, 2, 20, Tech::None},
        // MORE-UNITS slice — faithful Civ1 MANUAL stats (which differ from
        // the C# port's table in a few cases; e.g. C# Legion is 3/1/1 cost
        // 2, the manual lists 4/2/1 cost 4 — we ship the manual numbers as
        // the spec called for them and they give a sharper Knight>Phalanx
        // statistical signal in tests). Tech prereqs match the C# table.
        // cost field stores SHIELD cost = manual cost * 10 (same scheme as
        // Settlers/Militia/Phalanx/Cavalry).
        {"Legion",     4, 2, 1, 40, Tech::IronWorking},
        {"Knight",     4, 2, 2, 40, Tech::Feudalism},
        {"Catapult",   6, 1, 1, 40, Tech::Mathematics},
        {"Musketeers", 3, 3, 1, 30, Tech::Gunpowder},
        {"Cannon",     8, 1, 1, 40, Tech::Metallurgy},
    };
    int i = int(t);
    if (i < 0 || i >= kUnitTypeCount) i = 0;
    return kDefs[i];
}

// ---- City improvements (buildings) -------------------------------------
// A faithful SUBSET of the C# CityImprovementEnum (28 buildings in Civ1).
// We ship the iconic early-era 3: Granary, Barracks, Walls. Numeric ids match
// the Civ1 order (Granary=1, Barracks=2, Walls=3) so future expansion can
// re-use the same integer encoding on disk (see GameLoadAndSave v4).
enum class BuildingType : uint8_t {
    None     = 0,
    Granary  = 1,
    Barracks = 2,
    Walls    = 3,
};

// A building's fixed stats — faithful subset of the C# CityImprovement
// definition table. `cost` is the SHIELD threshold (Civ1 standard values).
struct BuildingDef {
    const char* name; // English key (Translator -> Chinese)
    int cost;         // shield cost (Granary=60, Barracks=40, Walls=80)
};

// Faithful BuildingDef table (Civ1 standard costs).
// Sources: Civ1 manual + community wikis (CivFanatics CityImprovements).
inline const BuildingDef& buildingDefOf(BuildingType t) {
    static const BuildingDef kDefs[4] = {
        {"None",     0},
        {"Granary",  60}, // food-bonus building (food not modeled — TODO)
        {"Barracks", 40}, // produces VETERAN units (+50% combat bonus)
        {"Walls",    80}, // defender on city tile: defense x3 (Civ1 +200%)
    };
    int i = int(t);
    if (i < 0 || i > 3) i = 0;
    return kDefs[i];
}

// ---- Wonders of the World -----------------------------------------------
// A faithful subset of Civ1's 28 wonders. We ship 4 iconic early-era wonders;
// the full 28-wonder roster + senate/spaceship-tier wonders are // TODO(port).
// Numeric ids are stable so v7 saves stay readable when more wonders ship.
//
// Wonder rules (faithful Civ1):
//   - Each wonder can be owned by AT MOST ONE civ for the whole game.
//   - Tech-gated: civ must know the WonderDef's techPrereq to start it.
//   - Built like a building: a city accumulates shields to the cost.
//   - When two civs simultaneously target the same wonder, the one that
//     completes first wins (the second's production switches off and
//     shields stay reserved — we don't model the "convert to gold" payoff).
//
// Effects (simplified — documented in WonderDef.effect_description):
//   - Pyramids        : civ-wide +1 food per city (foodGross bump).
//                       Civ1: any government works without Senate. We collapse
//                       to +1 food/city (food is modeled; senate isn't).
//   - HangingGardens  : civ-wide +1 science per city.
//                       Civ1: +1 happy citizen everywhere. Happiness isn't
//                       modeled, so we apply +1 science as a proxy effect.
//   - GreatWall       : civ-wide defender on city tile +50% defense.
//                       Civ1: Walls everywhere. We don't auto-grant Walls;
//                       instead resolveCombat multiplies defender's defense
//                       by 3/2 in cities owned by the Great Wall owner.
//                       Stacks with explicit city Walls (a Walled city of
//                       the Great Wall owner gets x3 * 3/2 = x4.5 → floor).
//   - Colossus        : civ-wide +1 science per city.
//                       Civ1: +1 trade per ocean tile. Trade isn't modeled
//                       per-tile, so we apply +1 science as a proxy effect.
enum class WonderType : uint8_t {
    None           = 0,
    Pyramids       = 1,
    HangingGardens = 2,
    GreatWall      = 3,
    Colossus       = 4,
};
static constexpr int kWonderCount = 5; // includes None at index 0

struct WonderDef {
    const char* name;            // English key (Translator -> Chinese)
    int         cost;            // shield cost (Civ1 wonders are 200; Great
                                 // Wall is 300 in some Civ1 revisions — we
                                 // use 200 across the board for parity).
    Tech        techPrereq;      // required tech
    const char* effect_description;
};

// Faithful WonderDef table. effect_description carries the simplification
// note so HUD/translation can show a faithful summary.
inline const WonderDef& wonderDefOf(WonderType w) {
    static const WonderDef kDefs[kWonderCount] = {
        {"None",            0,   Tech::None,          ""},
        {"Pyramids",        200, Tech::Masonry,
         "+1 food per city (Civ1: any govt w/o senate; simplified)"},
        {"Hanging Gardens", 200, Tech::Pottery,
         "+1 science per city (Civ1: +1 happy citizen; simplified)"},
        {"Great Wall",      200, Tech::Construction,
         "Defender +50% in own cities (stacks with Walls)"},
        {"Colossus",        200, Tech::BronzeWorking,
         "+1 science per city (Civ1: +1 trade per ocean tile; simplified)"},
    };
    int i = int(w);
    if (i < 0 || i >= kWonderCount) i = 0;
    return kDefs[i];
}

struct Unit {
    int owner = 0;            // civ index into UnitManagement::civs()
    int x = 0, y = 0;         // Position (matches C# GPoint)
    UnitType type = UnitType::Settlers;
    bool alive = true;
    // Settlers work state (faithful subset of C# UnitManagement
    // F0_1866_* build-road / build-irrigation flow + the per-unit
    // RemainingMoves / Status state used to lock a Settlers on its tile
    // while improvements are under construction). workTarget encodes
    // WHICH improvement is being built (0=None, 1=Road, 2=Irrigation),
    // workTurnsLeft is the per-improvement countdown decremented in
    // CheckPlayerTurn::processEndOfTurn; on reaching 0 the matching
    // improvement bit is set on MapManagement's improvements grid
    // (see MapManagement::setImprovementFlag) and workTarget clears.
    int workTurnsLeft = 0;
    uint8_t workTarget = 0;   // 0=None, 1=Road, 2=Irrigation
    // Veteran flag — set when this unit was produced in a city that owns
    // a Barracks (Civ1: Barracks-built units are Veterans -> +50% combat).
    // Mirrors C# Unit.IsVeteran. Resolved at production time in
    // CheckPlayerTurn::processEndOfTurn.
    bool veteran = false;
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

    // ---- Government (GOVERNMENT slice) ----
    // Civ1 civs START in Despotism (faithful: the C# StartGameMenu init also
    // sets Players[].Government = Despotism). When a civ initiates a govt
    // change via UnitManagement::changeGovernment, the EFFECTIVE government
    // becomes Anarchy for `anarchyTurnsLeft` turns (Civ1 fixed at 3 turns
    // for the basic case — a Statue-of-Liberty-style instant change would
    // override, but wonders aren't ported); when the counter hits 0 the
    // effective govt switches to `targetGovt` and the transition completes.
    // Effective government for gameplay purposes is therefore:
    //   anarchyTurnsLeft > 0 -> Anarchy
    //   else                 -> govt
    Government govt = Government::Despotism;
    Government targetGovt = Government::Despotism;
    int anarchyTurnsLeft = 0;

    // ---- Wonders (civ-wide) ----------------------------------------------
    // The set of Wonders this civ owns. Wonders are CIV-WIDE (not per-city)
    // because their effects (Pyramids food, Great Wall combat, etc.) apply
    // to every city of the owning civ. Single-ownership across the game is
    // enforced by UnitManagement::wonderOwner(w) returning -1 when unowned.
    std::set<WonderType> ownedWonders;
    bool hasWonder(WonderType w) const {
        return ownedWonders.find(w) != ownedWonders.end();
    }
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

    // ---- Building (city improvement) production -----------------------
    // Civ1 cities can build EITHER a unit OR a building per production cycle.
    // `productionKind` selects which; when Building, `productionBuildingType`
    // names the target and `production` caches buildingDefOf().cost so the
    // end-of-turn threshold pass stays a single comparison.
    // `ownedBuildings` is the set of buildings this city has already built;
    // it can't build the same improvement twice (faithful Civ1 rule).
    // ProductionKind expanded for the Wonders slice (v7): Wonder is the
    // third producible category. Numeric values are stable so v7 saves stay
    // readable. When productionKind == Wonder, productionWonderType names
    // the target and `production` caches wonderDefOf().cost. Building/Unit
    // branches stay 1:1 with the v4 path.
    enum class ProductionKind : uint8_t { Unit = 0, Building = 1, Wonder = 2 };
    ProductionKind productionKind = ProductionKind::Unit;
    BuildingType   productionBuildingType = BuildingType::None;
    WonderType     productionWonderType   = WonderType::None;
    std::set<BuildingType> ownedBuildings;

    // Convenience: does this city own `b`?
    bool hasBuilding(BuildingType b) const {
        return ownedBuildings.find(b) != ownedBuildings.end();
    }

    // ---- Food + population growth (Civ1 food box / ActualSize) ----------
    // Faithful subset of CityWorker.cs FoodCount + ActualSize + the per-tile
    // GetCityResourceCount food fan-out. Civ1 growth math:
    //   - Each turn the city accumulates (foodPerTurn) into `food`,
    //     where foodPerTurn = sum(yield over worked tiles) - population*2.
    //   - When food >= growthThreshold = (population+1)*10 the city grows
    //     (population++). Granary halves the threshold (*5) and retains
    //     HALF the threshold's worth on growth (rest of the box keeps food).
    //   - When food < 0 and population > 1, population shrinks by 1 and
    //     food resets to 0 (faithful Civ1 starvation behaviour).
    // foodPerTurn is recomputed each turn (not authoritative across saves)
    // but kept on the struct so CityView can show the current per-turn
    // delta without re-running the full tile-yield pass for the renderer.
    int population = 1;
    int food = 0;
    int foodPerTurn = 0;
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

    // ---- Government (GOVERNMENT slice) ----
    // Initiate a government change for civ `civId`. Faithful Civ1 rules:
    //   - civId must be in range,
    //   - the civ must KNOW the new government's tech prereq (consults the
    //     host TechResearch; Anarchy/Despotism have Tech::None -> always),
    //   - the civ must not already be mid-transition (anarchyTurnsLeft > 0
    //     refuses; switch-during-Anarchy is OUT — matches the C# game-menu
    //     greyout of the REVOLUTION button while still in Anarchy),
    //   - switching to the SAME government you already run is a no-op
    //     (returns false to signal nothing happened).
    // On success: govt is left untouched (still the OLD government — but
    // see effectiveGovernment() below), targetGovt is set to `newGovt`, and
    // anarchyTurnsLeft is set to 3 (the canonical Civ1 transition length).
    // CheckPlayerTurn::processEndOfTurn decrements the counter each turn
    // and, on reaching 0, sets govt = targetGovt. While anarchyTurnsLeft
    // > 0 the EFFECTIVE government is Anarchy (use effectiveGovernment()
    // to read it).
    static constexpr int kAnarchyTransitionTurns = 3;
    bool changeGovernment(int civId, Government newGovt);

    // Read the EFFECTIVE government for civ `civId` (Anarchy while
    // anarchyTurnsLeft > 0, else the stored govt). Bounds-safe: returns
    // Government::Despotism for an out-of-range civId.
    Government effectiveGovernment(int civId) const;

    // ---- Settlers improvement actions (faithful subset of F0_1866_*) ----
    // Work-target ids match the (Road/Irrigation) subset of the C#
    // TerrainImprovementFlagsEnum order; the actual map-grid bitflag
    // values live in MapManagement (kImprovementRoad / kImprovementIrrigation).
    static constexpr uint8_t kWorkNone       = 0;
    static constexpr uint8_t kWorkRoad       = 1;
    static constexpr uint8_t kWorkIrrigation = 2;
    // Per-improvement turn cost. Civ1 has terrain-dependent durations
    // (e.g. road on Plains is 2 turns, Forest is 4); we use a flat value
    // for now and note that as a TODO. Mirrors GameData.TerrainModifications
    // RoadEffect / IrrigationEffect threshold logic at a simplified level.
    static constexpr int kRoadTurns       = 2;
    static constexpr int kIrrigationTurns = 4;

    // Start a build-road action on `unitId` (Settlers only). Refuses
    // (returns false) when the unit isn't a Settlers, is dead, is already
    // working, is out of bounds, or stands on Water/Arctic. On success the
    // unit's workTarget becomes kWorkRoad and workTurnsLeft = kRoadTurns;
    // moveUnit/buildCity will refuse until the work completes (see the
    // end-of-turn pass in CheckPlayerTurn::processEndOfTurn).
    bool startBuildRoad(int unitId);
    // Start a build-irrigation action on `unitId` (Settlers only). Refuses
    // unless the terrain is Grassland/Plains/Desert (a simplified faithful
    // subset of GameData.TerrainModifications[t].IrrigationEffect == -2 —
    // the C# guard that gates the Build Irrigation menu entry).
    bool startBuildIrrigation(int unitId);

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
    //
    // TECH-GATED: refuses (returns false, no state change) when the OWNER civ
    // does not yet know the unit's techPrereq (mirrors the C# build menu's
    // unbuildable-unit greyout). Settlers/Militia (Tech::None) always build;
    // Phalanx needs BronzeWorking — see UnitDef table above. Bounds-invalid
    // city ids also return false.
    bool setCityProductionType(int cityId, UnitType t);

    // Switch a city's production to a BUILDING. Refuses (returns false) when:
    //   - cityId is out of range,
    //   - the city already OWNS this building (Civ1 rule: no duplicates),
    //   - the building is BuildingType::None.
    // On success: sets productionKind=Building, productionBuildingType=b,
    // and syncs `production` to buildingDefOf(b).cost.
    bool setCityProductionBuilding(int cityId, BuildingType b);

    // ---- Wonders (civ-wide) ---------------------------------------------
    // Switch a city's production to a WONDER. Refuses (returns false) when:
    //   - cityId is out of range,
    //   - the wonder is WonderType::None,
    //   - the owner civ does not yet know the wonder's techPrereq,
    //   - the wonder is ALREADY owned by ANY civ in this game (Civ1 rule:
    //     at most one civ owns each wonder).
    // On success: productionKind=Wonder, productionWonderType=w, and
    // `production` syncs to wonderDefOf(w).cost. Existing accumulated
    // shields are preserved (faithful Civ1 carry-over on production switch).
    bool setCityProductionWonder(int cityId, WonderType w);

    // Return the civ id that owns wonder `w`, or -1 when unowned (any civ
    // can still build it). Iterates civs() — O(numCivs * numWonders); cheap.
    int wonderOwner(WonderType w) const;

    // Return the cityId that BUILT wonder `w` for that civ (the "wonder home"
    // city), or -1 when the wonder is unowned. Useful for CityView to mark
    // the home city specially and for tests/UI to report provenance.
    int wonderOwnerCity(WonderType w) const;

    // Record a wonder completion: set civ.ownedWonders + the owner-city map.
    // No-op when w == None or civId out of range. When the wonder is already
    // owned by some civ, this is also a no-op (the completion path should
    // have checked first; defensive).
    void recordWonderCompletion(int civId, int cityId, WonderType w);

    // Direct access for GameLoadAndSave to restore the owner-city map.
    const int* wonderOwnerCityArray() const { return wonderOwnerCity_; }
    void setWonderOwnerCity(WonderType w, int cityId) {
        int i = int(w);
        if (i > 0 && i < kWonderCount) wonderOwnerCity_[i] = cityId;
    }

    // Combat-modifier helper: returns true when the city at tile (x,y) owned
    // by `owner` has BuildingType `b`. Used by resolveCombat to apply the
    // Barracks (+50% attack) and Walls (defender x3) bonuses without leaking
    // city/tile lookup details into the combat formula.
    bool tileCityHasBuilding(int owner, int x, int y, BuildingType b) const;

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
    // `defenderInOwnCityWithGreatWall` adds the civ-wide Great Wall city
    // defense bonus (defender.defense * 3/2) ON TOP of explicit Walls.
    // It is computed by moveUnit (which knows whether the defender stands
    // on a city tile owned by its own civ, and whether that civ owns the
    // Great Wall). Pure-headless callers pass true when testing the bonus.
    static bool resolveCombat(Unit& attacker, Unit& defender,
                              uint32_t& rngState,
                              bool defenderHasWalls = false,
                              bool defenderInOwnCityWithGreatWall = false);

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

    // ---- DIPLOMACY (faithful subset of GameData.Diplomacy[N,N]) ---------
    // Pairwise relations matrix, sized NxN where N = civs_.size(). Initialised
    // to NoContact off-diagonal, Peace on the diagonal (a civ is never at war
    // with itself). setupCivs() reshapes this; helpers below maintain symmetry.
    Relation getRelation(int civA, int civB) const;
    // Symmetric write: sets both (A,B) and (B,A). Self-set (A==B) coerces to
    // Peace (the diagonal invariant). Out-of-range civ ids are a no-op.
    void setRelation(int civA, int civB, Relation r);
    // Convenience: is the (civA, civB) pair at War? Bounds-safe (returns
    // false for out-of-range ids — non-existent civ can't be a combatant).
    bool isAtWar(int civA, int civB) const;
    // meetCheck: scan every (civA, civB) pair where civA<civB; if any of
    // civA's units OR cities is within Chebyshev distance <= kMeetRange of
    // ANY of civB's units OR cities, upgrade NoContact -> Peace (symmetric).
    // Returns the count of pairs whose relation was flipped this call (0
    // means "no new contacts this turn"). Existing Peace/War are untouched.
    // Idempotent across calls (re-running it after no movement returns 0).
    // O((units+cities)^2) — fine for the 8-civ Civ1 cap.
    static constexpr int kMeetRange = 2; // Chebyshev distance for "in sight"
    int meetCheck();
    // The "rival civ" the player most recently interacted with (used by the
    // SDL W/P keys to know which civ to declare war on / make peace with).
    // Defaults to 1 (civ 1) — the simplest UX choice for tests + first cut;
    // setSelectedRivalCiv() lets a fuller HUD cycle through rivals.
    int selectedRivalCiv() const { return selectedRivalCiv_; }
    void setSelectedRivalCiv(int civId) { selectedRivalCiv_ = civId; }
    // AI diplomacy decision (called at end-of-turn for each AI civ). If the
    // AI is at Peace with the human AND humanUnits > 2*aiUnits, AI rolls a
    // deterministic seeded RNG; on hit it sets the relation to War. Returns
    // true when AI declared war this call. RNG is seeded from (turn, civId)
    // so tests are reproducible.
    bool aiDecideDeclareWar(int aiCivId, int humanCivId, int turn);

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
    // Per-wonder owner cityId. -1 means "unowned (any civ may build it)".
    // Index 0 (WonderType::None) is unused but kept for indexing parity with
    // the enum's numeric values.
    int wonderOwnerCity_[kWonderCount] = { -1, -1, -1, -1, -1 };

    // ---- DIPLOMACY ------------------------------------------------------
    // Pairwise relations matrix (NxN where N = civs_.size()). Reshaped to
    // match civs_ in setupCivs(); GameLoadAndSave restores it from disk
    // (v8). All off-diagonal entries start at NoContact, diagonals at Peace.
    // Maintained symmetric by setRelation().
    std::vector<std::vector<Relation>> relations_;
    // The civ id the player's UI currently targets for W/P (Declare War /
    // Make Peace) keys. Default = 1 (first AI civ) so single-rival tests
    // and small games "just work" without a rival-selection UI.
    int selectedRivalCiv_ = 1;

public:
    // Direct write access used by GameLoadAndSave to restore the relations
    // matrix from disk after civs_ has been restored. Resizes to NxN.
    void resizeRelations(int n);
    // Public read of the raw matrix for GameLoadAndSave.
    const std::vector<std::vector<Relation>>& relationsRaw() const {
        return relations_;
    }
};

} // namespace oc1
