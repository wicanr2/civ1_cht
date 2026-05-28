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
// A unit on the world map. Mirrors a SUBSET of GameData.Players[i].Units[j]
// (Position + TypeID + alive flag). Only Settlers are modelled here — the
// other unit types are part of the deeper UnitGoTo / combat port. AI behaviour
// is stubbed (units don't move yet); the win is a populated world.
enum class UnitType : uint8_t { Settlers = 0 };
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
    // Units[CurrentProductionID].Cost threshold in CityWorker.cs). `production`
    // is the cost to complete the currently-built unit (default 10 — a
    // Settlers/Militia-class cost). `units` is the running count of units
    // produced by this city (the visible-side Player.Units[] table wire-up is
    // a TODO; see CheckPlayerTurn.cpp).
    int shields = 0;
    int production = 10;
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

    // ---- units (multi-civ) ----
    // Append a Unit owned by `owner` (civ index) at (x,y). Returns its index.
    int addUnit(int owner, UnitType type, int x, int y);
    const std::vector<Unit>& units() const { return units_; }
    std::vector<Unit>& unitsMut() { return units_; }

private:
    OpenCiv1Game& p;
    std::vector<City> cities_;
    std::vector<Unit> units_;     // multi-civ units (Settlers for now)
    std::vector<CivState> civs_;  // human is civs_[0], AI civs follow
    int mapW_ = 80, mapH_ = 50;
    int chosenTribe_ = -1;
    std::function<Terrain(int, int)> terrainAt_;
    int year_ = -4000; // StartGameMenu.cs line 124: GameData.Year = -4000
};

} // namespace oc1
