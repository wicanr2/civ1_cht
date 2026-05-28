// UnitManagement.cpp — port of OpenCiv1 UnitManagement.cs (BUILD-CITY subset).
//
// We persist a minimal City record per founded city (id, owner, x, y, name,
// foundedTurn) — enough for MiniWorld to render markers + show a "city count"
// HUD line. The deeper per-City init (Improvements/Workers/Trade/...) is a
// stub (see // TODO(port) markers in the header).
#include "UnitManagement.h"
#include "OpenCiv1Game.h"
#include "MainCode.h"
#include "TerrainTiles.h"
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

    // City id allocation — sequential, matches GameData.Cities[i].ID = i.
    int id = int(cities_.size());

    // Per-tribe nth-of-this-player count for naming. The first city of a
    // tribe = the capital (kTribeCapitalEnglish[tribe]); subsequent cities
    // get a numeric suffix (// TODO(port): use the per-tribe 16-entry pool
    // from GameData.CityNames when needed).
    int nthForPlayer = 0;
    for (const auto& c : cities_) if (c.owner == playerId) ++nthForPlayer;

    City c;
    c.id = id;
    c.owner = playerId;
    c.x = x;
    c.y = y;
    c.name = nthCityNameKey(chosenTribe_, nthForPlayer);
    c.foundedTurn = turn;
    cities_.push_back(std::move(c));
    outName = cities_.back().name;
    return true;
}

} // namespace oc1
