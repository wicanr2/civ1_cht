// GameLoadAndSave.cpp — see GameLoadAndSave.h for the format header comment.
//
// NOTE: this is NOT a port of the C# F11_0000_* binary .SVE reader/writer
// (the original CIVIL*.SVE format requires a faithful header + per-player
// record layout that is its own port effort). It is a small, modern, text
// snapshot of the C++ port's in-memory state — enough to demonstrate
// "save the game, exit, load it again, play continues from the same place".
#include "GameLoadAndSave.h"
#include "OpenCiv1Game.h"
#include "FrontEndFlow.h"
#include "MapManagement.h"
#include "MiniWorld.h"
#include "UnitManagement.h"
#include "CheckPlayerTurn.h"
#include "TechResearch.h"
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace oc1 {

GameLoadAndSave::GameLoadAndSave(OpenCiv1Game& parent) : p(parent) {}

// ---- helpers ---------------------------------------------------------------
namespace {
// Read the 80*50 terrain grid bytes straight out of VCPU memory at
// MapManagement::kMapSeg:kMapOff (one byte per cell, row-major).
std::vector<uint8_t> dumpTerrainBytes(OpenCiv1Game& p) {
    std::vector<uint8_t> out;
    out.resize(std::size_t(MapManagement::kWidth) * MapManagement::kHeight);
    for (int y = 0; y < MapManagement::kHeight; ++y) {
        for (int x = 0; x < MapManagement::kWidth; ++x) {
            uint16_t off = uint16_t(MapManagement::kMapOff +
                                    y * MapManagement::kWidth + x);
            out[std::size_t(y) * MapManagement::kWidth + x] =
                p.cpu.ReadUInt8(MapManagement::kMapSeg, off);
        }
    }
    return out;
}

// Inverse: restore the VCPU memory bytes for the terrain grid AND mirror them
// into MiniWorld's tiles_ via setTerrainAt (so renderers reading either source
// see the restored map).
void restoreTerrainBytes(OpenCiv1Game& p, MiniWorld* mw,
                         const std::vector<uint8_t>& bytes) {
    for (int y = 0; y < MapManagement::kHeight; ++y) {
        for (int x = 0; x < MapManagement::kWidth; ++x) {
            uint8_t b = bytes[std::size_t(y) * MapManagement::kWidth + x];
            uint16_t off = uint16_t(MapManagement::kMapOff +
                                    y * MapManagement::kWidth + x);
            p.cpu.WriteUInt8(MapManagement::kMapSeg, off, b);
            if (mw) mw->setTerrainAt(x, y, p.mapManagement().GetTerrainType(x, y));
        }
    }
}

void writeHex(std::ostream& os, const std::vector<uint8_t>& bytes) {
    static const char kHex[] = "0123456789abcdef";
    std::string s;
    s.reserve(bytes.size() * 2);
    for (uint8_t b : bytes) {
        s.push_back(kHex[(b >> 4) & 0xF]);
        s.push_back(kHex[b & 0xF]);
    }
    os << s;
}

static int hexNibble(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}
bool parseHex(const std::string& hex, std::vector<uint8_t>& out) {
    if (hex.size() % 2 != 0) return false;
    out.clear();
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        int hi = hexNibble(hex[i]);
        int lo = hexNibble(hex[i + 1]);
        if (hi < 0 || lo < 0) return false;
        out.push_back(uint8_t((hi << 4) | lo));
    }
    return true;
}
} // namespace

// ---- save ------------------------------------------------------------------
bool GameLoadAndSave::saveToFile(const std::string& path,
                                 const FrontEndFlow* flow) const {
    std::ofstream os(path, std::ios::binary);
    if (!os) return false;

    // Header. v10 adds per-city {happy, unhappy, disorder} for the
    // HAPPINESS slice. v1..v9 readers skip the 'cityhappy' key entirely
    // (default: happy=0, unhappy=0, disorder=false from City's struct).
    // v9 adds per-civ gold for the ECONOMY (gold/treasury/upkeep)
    // slice. v1..v8 readers ignore the v9 'civgold' key (default: gold=50,
    // matching CivState's initial value).
    // v8 added the pairwise relations matrix + selected rival civ.
    // v7 added per-civ ownedWonders csv + per-city
    // productionWonderType + the global owner-city map for the Wonders
    // slice. v1..v6 readers ignore the v7 keys (default: no wonders owned).
    // v6 added per-civ government state for the Government slice.
    // v5 added per-city {population, food, foodPerTurn} for the
    // food + population growth slice (closes the Granary loop).
    // v4 added per-city {productionKind, productionBuildingType,
    // ownedBuildings list} + per-unit veteran flag for the buildings slice.
    // v3 added the improvements grid + per-unit work state.
    // v2 added per-civ tech-tree state; v1 was the pre-tech baseline.
    os << "OpenCiv1pp savegame v10\n";

    // Turn / year. Turn lives on MiniWorld; year on UnitManagement (mutated
    // by CheckPlayerTurn::advanceYear each end-of-turn).
    int turn = 0;
    if (flow && flow->miniWorld()) turn = flow->miniWorld()->turn();
    os << "turn " << turn << "\n";
    os << "year " << p.unitManagement().year() << "\n";

    // Front-end picks (from FrontEndFlow, when available).
    int difficulty = -1, tribe = p.unitManagement().chosenTribe();
    uint32_t seed = 0;
    int stateI = int(FrontEndFlow::State::PLAYING);
    std::string name;
    if (flow) {
        difficulty = flow->chosenDifficulty();
        tribe      = flow->chosenTribe();
        seed       = flow->worldSeed();
        stateI     = int(flow->state());
        name       = flow->chosenName();
    }
    os << "difficulty " << difficulty << "\n";
    os << "tribe " << tribe << "\n";
    os << "seed " << seed << "\n";
    os << "name " << name << "\n";
    os << "state " << stateI << "\n";

    // Human unit position (from MiniWorld, when available).
    int ux = 0, uy = 0;
    if (flow && flow->miniWorld()) {
        ux = flow->miniWorld()->unitX();
        uy = flow->miniWorld()->unitY();
    }
    os << "unitpos " << ux << " " << uy << "\n";

    // Civs.
    const auto& civs = p.unitManagement().civs();
    os << "civs " << civs.size() << "\n";
    for (const auto& c : civs) {
        os << "civ " << c.tribeIdx << " " << int(c.color) << " "
           << (c.isHuman ? 1 : 0) << " " << c.name << "\n";
    }
    // v6: per-civ government state, one line per civ. v1..v5 readers skip the
    // 'civgovt' key entirely (the istream lookup just falls through); v6
    // readers restore govt/targetGovt/anarchyTurnsLeft from these lines.
    for (std::size_t i = 0; i < civs.size(); ++i) {
        const auto& c = civs[i];
        os << "civgovt " << i << " " << int(c.govt) << " "
           << int(c.targetGovt) << " " << c.anarchyTurnsLeft << "\n";
    }
    // v7: per-civ ownedWonders, one line per civ. CSV of WonderType integer
    // ids (1..N); "-" sentinel for "no wonders owned". v1..v6 readers skip
    // the 'civwonders' key.
    for (std::size_t i = 0; i < civs.size(); ++i) {
        const auto& c = civs[i];
        os << "civwonders " << i << " ";
        bool first = true;
        for (WonderType w : c.ownedWonders) {
            if (!first) os << ",";
            os << int(w);
            first = false;
        }
        if (first) os << "-";
        os << "\n";
    }

    // Units. v3: per-unit work state (workTurnsLeft, workTarget) appended.
    // v4: per-unit veteran flag appended after workTarget (0/1). Older
    // readers stop at the last field they recognize; the v4 loader picks up
    // the trailing veteran column when present (default false).
    const auto& units = p.unitManagement().units();
    os << "units " << units.size() << "\n";
    for (const auto& u : units) {
        os << "unit " << u.owner << " " << u.x << " " << u.y << " "
           << int(u.type) << " " << (u.alive ? 1 : 0)
           << " " << u.workTurnsLeft << " " << int(u.workTarget)
           << " " << (u.veteran ? 1 : 0) << "\n";
    }

    // Cities. v4: per-city building state is written as a SEPARATE 'citybld'
    // line per city (cityId + productionKind + productionBuildingType +
    // comma-separated owned-buildings list). The main 'city' line layout
    // stays unchanged so v3 readers continue to parse it correctly.
    const auto& cities = p.unitManagement().cities();
    os << "cities " << cities.size() << "\n";
    for (const auto& c : cities) {
        os << "city " << c.id << " " << c.owner << " " << c.x << " " << c.y
           << " " << c.foundedTurn << " " << c.shields << " " << c.production
           << " " << c.units << " " << int(c.productionType) << " "
           << c.name << "\n";
    }
    // v4: per-city building state, one line per city.
    for (const auto& c : cities) {
        os << "citybld " << c.id << " " << int(c.productionKind) << " "
           << int(c.productionBuildingType) << " ";
        bool first = true;
        for (BuildingType b : c.ownedBuildings) {
            if (!first) os << ",";
            os << int(b);
            first = false;
        }
        if (first) os << "-"; // sentinel for "no owned buildings"
        os << "\n";
    }
    // v5: per-city food/population state, one line per city. v1..v4 readers
    // skip the cityfood key (the istream lookup just falls through); v5
    // readers restore population/food/foodPerTurn from these lines.
    for (const auto& c : cities) {
        os << "cityfood " << c.id << " " << c.population << " "
           << c.food << " " << c.foodPerTurn << "\n";
    }
    // v7: per-city productionWonderType, one line per city. v1..v6 readers
    // skip the 'citywonder' key entirely.
    for (const auto& c : cities) {
        os << "citywonder " << c.id << " " << int(c.productionWonderType) << "\n";
    }
    // v7: global per-wonder owner-city map. One line per wonder index
    // (1..kWonderCount-1). -1 means "wonder not yet built". v1..v6 readers
    // skip the 'wonderowner' key; v7 readers restore the array.
    {
        const int* arr = p.unitManagement().wonderOwnerCityArray();
        for (int w = 1; w < kWonderCount; ++w) {
            os << "wonderowner " << w << " " << arr[w] << "\n";
        }
    }

    // v8: pairwise relations matrix (NxN, symmetric, diagonal=Peace). One
    // line per row: 'relations <civId> <r0>,<r1>,...,<r_{N-1}>'. v1..v7
    // readers skip the 'relations' key entirely (default: NoContact
    // off-diagonal, Peace on diagonal — what resizeRelations() seeds).
    {
        const auto& rel = p.unitManagement().relationsRaw();
        for (std::size_t i = 0; i < rel.size(); ++i) {
            os << "relations " << i << " ";
            for (std::size_t j = 0; j < rel[i].size(); ++j) {
                if (j) os << ",";
                os << int(rel[i][j]);
            }
            os << "\n";
        }
    }
    // v8: selected rival civ (single integer; -1 means "no selection").
    os << "rival " << p.unitManagement().selectedRivalCiv() << "\n";

    // v9: per-civ gold (treasury). One line per civ. v1..v8 readers skip the
    // 'civgold' key entirely (default: gold=50 from CivState's struct init).
    for (std::size_t i = 0; i < civs.size(); ++i) {
        os << "civgold " << i << " " << civs[i].gold << "\n";
    }

    // v10: per-city {happy, unhappy, disorder}. One line per city. v1..v9
    // readers skip the 'cityhappy' key entirely (default: happy=0,
    // unhappy=0, disorder=false from City's struct init).
    for (const auto& c : cities) {
        os << "cityhappy " << c.id << " " << c.happy << " " << c.unhappy
           << " " << (c.disorder ? 1 : 0) << "\n";
    }

    // Terrain bytes (80*50 = 4000 bytes -> 8000 hex chars on one line).
    os << "terrain ";
    writeHex(os, dumpTerrainBytes(const_cast<OpenCiv1Game&>(p)));
    os << "\n";

    // v3: improvements grid (same row-major byte-per-tile layout as terrain,
    // 80*50 = 4000 bytes -> 8000 hex chars). Each byte holds the
    // TerrainImprovementFlagsEnum bitmask (Road=8, Irrigation=2, ...).
    {
        std::vector<uint8_t> improvBytes;
        improvBytes.resize(std::size_t(MapManagement::kWidth) *
                           MapManagement::kHeight);
        const auto& mm = p.mapManagement();
        for (int y = 0; y < MapManagement::kHeight; ++y) {
            for (int x = 0; x < MapManagement::kWidth; ++x) {
                improvBytes[std::size_t(y) * MapManagement::kWidth + x] =
                    mm.getImprovements(x, y);
            }
        }
        os << "improvements ";
        writeHex(os, improvBytes);
        os << "\n";
    }

    // Per-civ tech state. One techcsv line per civ; the techCsv tail is the
    // comma-separated list of Tech enum ids the civ knows ("" when none).
    const auto& tr = p.techResearch();
    int techN = tr.civCount();
    os << "techcivs " << techN << "\n";
    for (int civId = 0; civId < techN; ++civId) {
        os << "techcsv " << civId << " " << int(tr.civResearching(civId))
           << " " << tr.civPoints(civId) << " ";
        // Comma-separated list of known Tech ids (from the shared TechDef
        // table — we don't iterate the bitset directly so the on-disk form
        // stays portable across enum widenings).
        bool first = true;
        for (int i = 0; i < TechResearch::techCount(); ++i) {
            Tech t = TechResearch::techByIndex(i).id;
            if (!tr.civKnows(civId, t)) continue;
            if (!first) os << ",";
            os << int(t);
            first = false;
        }
        if (first) os << "-"; // sentinel for "no known techs"
        os << "\n";
    }

    return bool(os);
}

// ---- load ------------------------------------------------------------------
bool GameLoadAndSave::loadFromFile(const std::string& path, FrontEndFlow* flow) {
    std::ifstream is(path, std::ios::binary);
    if (!is) return false;

    std::string header;
    if (!std::getline(is, header)) return false;
    // Accept v1 (pre-tech-tree), v2 (pre-improvements), v3 (pre-buildings),
    // v4 (current). v1 files skip the tech-state restore; v2 files skip the
    // improvements grid + per-unit work state; v3 files skip the per-city
    // building state + per-unit veteran flag (those fields default to 0).
    if (header != "OpenCiv1pp savegame v1" &&
        header != "OpenCiv1pp savegame v2" &&
        header != "OpenCiv1pp savegame v3" &&
        header != "OpenCiv1pp savegame v4" &&
        header != "OpenCiv1pp savegame v5" &&
        header != "OpenCiv1pp savegame v6" &&
        header != "OpenCiv1pp savegame v7" &&
        header != "OpenCiv1pp savegame v8" &&
        header != "OpenCiv1pp savegame v9" &&
        header != "OpenCiv1pp savegame v10") return false;

    int turn = 0, year = -4000;
    int difficulty = -1, tribe = -1;
    uint32_t seed = 0;
    int stateI = int(FrontEndFlow::State::PLAYING);
    std::string name;
    int ux = 0, uy = 0;
    std::vector<CivState> civs;
    std::vector<Unit> units;
    std::vector<City> cities;
    std::vector<uint8_t> terrain;
    std::vector<uint8_t> improvements;
    // Per-civ tech state we'll restore after the main vectors are applied.
    struct TechRow { int civId; int researching; int points; std::vector<int> known; };
    std::vector<TechRow> techRows;
    int techCivsCount = 0;
    // v4: per-city building state, applied after the cities vector is built.
    struct CityBldRow { int cityId; int prodKind; int prodBuildingType;
                        std::vector<int> owned; };
    std::vector<CityBldRow> cityBldRows;
    // v5: per-city food/population state, applied after the cities vector
    // is built. v1..v4 saves don't emit these lines -> defaults remain
    // (population=1, food=0, foodPerTurn=0 from City's struct defaults).
    struct CityFoodRow { int cityId; int population; int food; int foodPerTurn; };
    std::vector<CityFoodRow> cityFoodRows;
    // v6: per-civ government state, applied after the civs vector is built.
    // v1..v5 saves don't emit these lines -> defaults remain (Despotism +
    // anarchyTurnsLeft=0 from CivState's struct defaults).
    struct CivGovtRow { int civId; int govt; int targetGovt; int anarchyTurnsLeft; };
    std::vector<CivGovtRow> civGovtRows;
    // v7: per-civ ownedWonders + per-city productionWonderType + global
    // per-wonder owner-city map. Applied after the main vectors are built.
    struct CivWondersRow { int civId; std::vector<int> wonders; };
    std::vector<CivWondersRow> civWondersRows;
    struct CityWonderRow  { int cityId; int prodWonderType; };
    std::vector<CityWonderRow> cityWonderRows;
    struct WonderOwnerRow { int wonderId; int cityId; };
    std::vector<WonderOwnerRow> wonderOwnerRows;
    // v8: per-civ relations row (csv of Relation enum ints). Applied after
    // the civs vector is restored. v1..v7 saves skip this key -> defaults
    // (NoContact off-diagonal, Peace on diagonal) hold.
    struct RelationsRow { int civId; std::vector<int> entries; };
    std::vector<RelationsRow> relationsRows;
    int rivalCiv = 1; // v8: selected rival civ (default 1)
    // v9: per-civ gold. One row per civ; applied after civs are restored.
    // v1..v8 saves don't emit this key -> default gold=50 from CivState.
    struct CivGoldRow { int civId; int gold; };
    std::vector<CivGoldRow> civGoldRows;
    // v10: per-city {happy, unhappy, disorder}. Applied after cities are
    // restored. v1..v9 saves don't emit this key -> defaults (0/0/false)
    // hold (matching City's struct init).
    struct CityHappyRow { int cityId; int happy; int unhappy; int disorder; };
    std::vector<CityHappyRow> cityHappyRows;

    std::string line;
    while (std::getline(is, line)) {
        if (line.empty()) continue;
        std::istringstream iss(line);
        std::string key;
        iss >> key;
        if (key == "turn")            { iss >> turn; }
        else if (key == "year")       { iss >> year; }
        else if (key == "difficulty") { iss >> difficulty; }
        else if (key == "tribe")      { iss >> tribe; }
        else if (key == "seed")       { iss >> seed; }
        else if (key == "name")       {
            // Rest of line after "name "
            auto pos = line.find(' ');
            name = (pos == std::string::npos) ? "" : line.substr(pos + 1);
        }
        else if (key == "state")      { iss >> stateI; }
        else if (key == "unitpos")    { iss >> ux >> uy; }
        else if (key == "civs")       { /* count -- next lines are 'civ' */ }
        else if (key == "civ") {
            CivState c;
            int color = 0, isHuman = 0;
            iss >> c.tribeIdx >> color >> isHuman;
            c.color = uint8_t(color);
            c.isHuman = (isHuman != 0);
            // rest of line (after the 3rd whitespace) is the civ name; allow
            // names with spaces by taking the tail after the 4th token start.
            std::string rest;
            std::getline(iss, rest);
            // strip the single leading space getline leaves behind
            if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
            c.name = rest;
            civs.push_back(std::move(c));
        }
        else if (key == "units")      { /* count */ }
        else if (key == "unit") {
            Unit u;
            int t = 0, alive = 1;
            iss >> u.owner >> u.x >> u.y >> t >> alive;
            u.type = UnitType(t);
            u.alive = (alive != 0);
            // v3: trailing workTurnsLeft + workTarget. v4: trailing veteran
            // flag after workTarget. Absent in older files (the istream
            // extraction silently fails and the defaults persist).
            int wtLeft = 0, wtTarget = 0, vet = 0;
            if (iss >> wtLeft) {
                u.workTurnsLeft = wtLeft;
                if (iss >> wtTarget) {
                    u.workTarget = uint8_t(wtTarget);
                    if (iss >> vet) u.veteran = (vet != 0);
                }
            }
            units.push_back(u);
        }
        else if (key == "cities")     { /* count */ }
        else if (key == "city") {
            City c;
            int pt = 0;
            iss >> c.id >> c.owner >> c.x >> c.y >> c.foundedTurn
                >> c.shields >> c.production >> c.units >> pt;
            c.productionType = UnitType(pt);
            std::string rest;
            std::getline(iss, rest);
            if (!rest.empty() && rest.front() == ' ') rest.erase(0, 1);
            c.name = rest;
            cities.push_back(std::move(c));
        }
        else if (key == "cityfood") {
            CityFoodRow r{};
            iss >> r.cityId >> r.population >> r.food >> r.foodPerTurn;
            cityFoodRows.push_back(r);
        }
        else if (key == "civgovt") {
            CivGovtRow r{};
            iss >> r.civId >> r.govt >> r.targetGovt >> r.anarchyTurnsLeft;
            civGovtRows.push_back(r);
        }
        else if (key == "civwonders") {
            CivWondersRow r;
            std::string csv;
            iss >> r.civId >> csv;
            if (csv != "-" && !csv.empty()) {
                std::size_t start = 0;
                while (start <= csv.size()) {
                    std::size_t comma = csv.find(',', start);
                    std::string tok = csv.substr(start,
                        (comma == std::string::npos ? csv.size() : comma) - start);
                    if (!tok.empty()) {
                        try { r.wonders.push_back(std::stoi(tok)); }
                        catch (...) { /* skip malformed */ }
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
            civWondersRows.push_back(std::move(r));
        }
        else if (key == "citywonder") {
            CityWonderRow r{};
            iss >> r.cityId >> r.prodWonderType;
            cityWonderRows.push_back(r);
        }
        else if (key == "wonderowner") {
            WonderOwnerRow r{};
            iss >> r.wonderId >> r.cityId;
            wonderOwnerRows.push_back(r);
        }
        else if (key == "relations") {
            RelationsRow r;
            std::string csv;
            iss >> r.civId >> csv;
            if (!csv.empty()) {
                std::size_t start = 0;
                while (start <= csv.size()) {
                    std::size_t comma = csv.find(',', start);
                    std::string tok = csv.substr(start,
                        (comma == std::string::npos ? csv.size() : comma) - start);
                    if (!tok.empty()) {
                        try { r.entries.push_back(std::stoi(tok)); }
                        catch (...) { /* skip */ }
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
            relationsRows.push_back(std::move(r));
        }
        else if (key == "rival") { iss >> rivalCiv; }
        else if (key == "civgold") {
            CivGoldRow r{};
            iss >> r.civId >> r.gold;
            civGoldRows.push_back(r);
        }
        else if (key == "cityhappy") {
            CityHappyRow r{};
            iss >> r.cityId >> r.happy >> r.unhappy >> r.disorder;
            cityHappyRows.push_back(r);
        }
        else if (key == "citybld") {
            CityBldRow r;
            std::string csv;
            iss >> r.cityId >> r.prodKind >> r.prodBuildingType >> csv;
            if (csv != "-" && !csv.empty()) {
                std::size_t start = 0;
                while (start <= csv.size()) {
                    std::size_t comma = csv.find(',', start);
                    std::string tok = csv.substr(start,
                        (comma == std::string::npos ? csv.size() : comma) - start);
                    if (!tok.empty()) {
                        try { r.owned.push_back(std::stoi(tok)); }
                        catch (...) { /* skip malformed token */ }
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
            cityBldRows.push_back(std::move(r));
        }
        else if (key == "terrain") {
            std::string hex;
            iss >> hex;
            if (!parseHex(hex, terrain)) return false;
            if (terrain.size() != std::size_t(MapManagement::kWidth) *
                                  MapManagement::kHeight) {
                return false;
            }
        }
        else if (key == "improvements") {
            std::string hex;
            iss >> hex;
            if (!parseHex(hex, improvements)) return false;
            if (improvements.size() != std::size_t(MapManagement::kWidth) *
                                       MapManagement::kHeight) {
                return false;
            }
        }
        else if (key == "techcivs") { iss >> techCivsCount; }
        else if (key == "techcsv") {
            TechRow r;
            std::string csv;
            iss >> r.civId >> r.researching >> r.points >> csv;
            if (csv != "-" && !csv.empty()) {
                // split on commas
                std::size_t start = 0;
                while (start <= csv.size()) {
                    std::size_t comma = csv.find(',', start);
                    std::string tok = csv.substr(start,
                        (comma == std::string::npos ? csv.size() : comma) - start);
                    if (!tok.empty()) {
                        try { r.known.push_back(std::stoi(tok)); }
                        catch (...) { /* skip malformed token */ }
                    }
                    if (comma == std::string::npos) break;
                    start = comma + 1;
                }
            }
            techRows.push_back(std::move(r));
        }
    }

    // Apply: front-end picks first (they drive the seed used by the MiniWorld
    // rebuild), then the actual game-state vectors.
    if (flow) {
        flow->setChosenDifficulty(difficulty);
        flow->setChosenTribe(tribe);
        flow->setChosenName(name);
        flow->setWorldSeed(seed);
        flow->rebuildPlayingShell(); // creates MiniWorld + attaches game host
        flow->setState(FrontEndFlow::State(stateI));
    }

    p.unitManagement().setChosenTribe(tribe);
    p.unitManagement().setYear(year);
    p.unitManagement().civsMut() = std::move(civs);
    p.unitManagement().unitsMut() = std::move(units);
    p.unitManagement().citiesMut() = std::move(cities);

    // v6: apply per-civ government state on top of the restored civs.
    // Absent in v1..v5 saves -> defaults (Despotism / Despotism / 0) hold.
    {
        auto& cv = p.unitManagement().civsMut();
        for (const auto& r : civGovtRows) {
            if (r.civId < 0 || std::size_t(r.civId) >= cv.size()) continue;
            CivState& c = cv[std::size_t(r.civId)];
            c.govt = Government(r.govt);
            c.targetGovt = Government(r.targetGovt);
            c.anarchyTurnsLeft = (r.anarchyTurnsLeft >= 0) ? r.anarchyTurnsLeft : 0;
        }
    }

    // v4: apply per-city building state on top of the freshly-restored cities.
    // Each citybld row targets a city by id and restores productionKind,
    // productionBuildingType, and the ownedBuildings set. Absent in v1/v2/v3
    // saves: the fields keep their City struct defaults (Unit, None, {}).
    {
        auto& cv = p.unitManagement().citiesMut();
        for (const auto& r : cityBldRows) {
            if (r.cityId < 0 || std::size_t(r.cityId) >= cv.size()) continue;
            City& c = cv[std::size_t(r.cityId)];
            c.productionKind = City::ProductionKind(r.prodKind);
            c.productionBuildingType = BuildingType(r.prodBuildingType);
            c.ownedBuildings.clear();
            for (int b : r.owned) c.ownedBuildings.insert(BuildingType(b));
        }
        // v5: apply per-city food/population state on top of the restored
        // cities. Absent in v1/v2/v3/v4 saves -> defaults (1/0/0) hold.
        for (const auto& r : cityFoodRows) {
            if (r.cityId < 0 || std::size_t(r.cityId) >= cv.size()) continue;
            City& c = cv[std::size_t(r.cityId)];
            c.population  = (r.population >= 1) ? r.population : 1;
            c.food        = r.food;
            c.foodPerTurn = r.foodPerTurn;
        }
        // v7: apply per-city productionWonderType. Absent in v1..v6 saves ->
        // default (WonderType::None) holds.
        for (const auto& r : cityWonderRows) {
            if (r.cityId < 0 || std::size_t(r.cityId) >= cv.size()) continue;
            City& c = cv[std::size_t(r.cityId)];
            int wi = r.prodWonderType;
            if (wi < 0 || wi >= kWonderCount) wi = 0;
            c.productionWonderType = WonderType(wi);
        }
    }
    // v7: apply per-civ ownedWonders + global owner-city map. Done OUTSIDE
    // the cities block (the wonder rows live on the civs, not the cities).
    {
        auto& cvs = p.unitManagement().civsMut();
        for (const auto& r : civWondersRows) {
            if (r.civId < 0 || std::size_t(r.civId) >= cvs.size()) continue;
            CivState& c = cvs[std::size_t(r.civId)];
            c.ownedWonders.clear();
            for (int wi : r.wonders) {
                if (wi > 0 && wi < kWonderCount)
                    c.ownedWonders.insert(WonderType(wi));
            }
        }
        // Owner-city map: restore the per-wonder city id.
        for (const auto& r : wonderOwnerRows) {
            if (r.wonderId <= 0 || r.wonderId >= kWonderCount) continue;
            p.unitManagement().setWonderOwnerCity(WonderType(r.wonderId), r.cityId);
        }
    }
    // v8: pairwise relations matrix. Resize to civ count (seeds the
    // default NoContact off-diagonal + Peace diagonal), then overlay
    // any explicit 'relations' rows from disk. Absent in v1..v7 saves
    // -> the defaults from resizeRelations(numCivs) hold. The matrix is
    // re-symmetrised via setRelation() during overlay (so a corrupt
    // half-row in the save doesn't produce an asymmetric matrix).
    {
        const int n = int(p.unitManagement().civs().size());
        p.unitManagement().resizeRelations(n);
        for (const auto& r : relationsRows) {
            if (r.civId < 0 || r.civId >= n) continue;
            for (int j = 0; j < int(r.entries.size()) && j < n; ++j) {
                int v = r.entries[std::size_t(j)];
                if (v < 0) v = 0;
                if (v > 2) v = 2;
                p.unitManagement().setRelation(r.civId, j, Relation(v));
            }
        }
        p.unitManagement().setSelectedRivalCiv(rivalCiv);
    }
    // v9: apply per-civ gold. Absent in v1..v8 saves -> default gold=50
    // (CivState struct init) holds.
    {
        auto& cv = p.unitManagement().civsMut();
        for (const auto& r : civGoldRows) {
            if (r.civId < 0 || std::size_t(r.civId) >= cv.size()) continue;
            cv[std::size_t(r.civId)].gold = r.gold;
        }
    }
    // v10: apply per-city {happy, unhappy, disorder}. Absent in v1..v9
    // saves -> defaults (0/0/false) hold (matching City struct init).
    {
        auto& cv = p.unitManagement().citiesMut();
        for (const auto& r : cityHappyRows) {
            if (r.cityId < 0 || std::size_t(r.cityId) >= cv.size()) continue;
            City& c = cv[std::size_t(r.cityId)];
            c.happy    = (r.happy    >= 0) ? r.happy    : 0;
            c.unhappy  = (r.unhappy  >= 0) ? r.unhappy  : 0;
            c.disorder = (r.disorder != 0);
        }
    }

    // Restore the terrain grid bytes into VCPU memory (and into MiniWorld's
    // cache when one is attached). Run AFTER rebuildPlayingShell so MiniWorld
    // exists.
    MiniWorld* mw = flow ? flow->miniWorld() : nullptr;
    if (!terrain.empty()) {
        restoreTerrainBytes(p, mw, terrain);
    }
    // v3: improvements grid. When absent (v1/v2 save), clear and skip — the
    // generator's clearImprovements() in rebuildPlayingShell already left
    // the grid empty for those legacy files.
    {
        auto& mm = p.mapManagement();
        if (!improvements.empty()) {
            for (int y = 0; y < MapManagement::kHeight; ++y) {
                for (int x = 0; x < MapManagement::kWidth; ++x) {
                    mm.setImprovementsRaw(x, y,
                        improvements[std::size_t(y) * MapManagement::kWidth + x]);
                }
            }
        } else {
            mm.clearImprovements();
        }
    }

    // Restore human unit position and turn counter on MiniWorld.
    if (mw) {
        mw->setUnitPosition(ux, uy);
        // The turn counter lives on MiniWorld; bump it from 1 (the constructor
        // default) up to the saved value. endTurn() also runs end-of-turn
        // housekeeping which we explicitly do NOT want here, so we use a
        // direct path: simulate (turn-1) raw increments via a helper. None
        // exists, but we don't want the side-effects of endTurn(). Instead we
        // rebuild MiniWorld with a fresh state and accept that turn() will
        // start at 1 unless the caller advances it themselves. To keep the
        // round-trip honest we therefore expose the persisted turn via a
        // direct write: see MiniWorld::setTurnForRestore below.
        // (no-op here; setTurn happens below)
    }

    // Independent path: even when flow == nullptr, save the turn into the
    // MiniWorld of `flow` if there is one. There isn't, so just record.
    if (mw) mw->setUnitPosition(ux, uy);

    // Turn restoration uses a direct setter (added to MiniWorld below).
    if (mw) mw->setTurnForRestore(turn);

    // Restore per-civ TechResearch state. When no techcivs record was present
    // (v1 savefile), fall back to a fresh init sized to the loaded civ count.
    auto& tr = p.techResearch();
    if (techCivsCount > 0 || !techRows.empty()) {
        tr.initCivs(techCivsCount > 0 ? techCivsCount
                                      : int(techRows.size()));
        for (const auto& r : techRows) {
            tr.setCivResearching(r.civId, Tech(r.researching));
            tr.setCivPoints(r.civId, r.points);
            for (int k : r.known) tr.setCivKnows(r.civId, Tech(k), true);
        }
    } else if (!p.unitManagement().civs().empty()) {
        tr.initCivs(int(p.unitManagement().civs().size()));
    }

    return true;
}

} // namespace oc1
