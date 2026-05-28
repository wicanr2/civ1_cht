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

struct City {
    int id = -1;
    int owner = 0;            // PlayerID
    int x = 0, y = 0;         // Position (matches C# GPoint)
    std::string name;         // resolved + (later) translated city name
    int foundedTurn = 0;      // founding turn (derived from the world's turn)
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
    std::size_t cityCount() const { return cities_.size(); }

    // The english key for the Nth city of this tribe (0 = capital). Falls back
    // to "Capital" for the first city of an unknown tribe; subsequent unknown-
    // tribe cities get "Capital 2", "Capital 3", etc.
    static std::string nthCityNameKey(int tribeIndex, int nth);

private:
    OpenCiv1Game& p;
    std::vector<City> cities_;
    int mapW_ = 80, mapH_ = 50;
    int chosenTribe_ = -1;
    std::function<Terrain(int, int)> terrainAt_;
};

} // namespace oc1
