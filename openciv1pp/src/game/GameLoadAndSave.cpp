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

    // Header. v2 adds the per-civ tech-tree state (techcivs/techcsv records).
    os << "OpenCiv1pp savegame v2\n";

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

    // Units.
    const auto& units = p.unitManagement().units();
    os << "units " << units.size() << "\n";
    for (const auto& u : units) {
        os << "unit " << u.owner << " " << u.x << " " << u.y << " "
           << int(u.type) << " " << (u.alive ? 1 : 0) << "\n";
    }

    // Cities.
    const auto& cities = p.unitManagement().cities();
    os << "cities " << cities.size() << "\n";
    for (const auto& c : cities) {
        os << "city " << c.id << " " << c.owner << " " << c.x << " " << c.y
           << " " << c.foundedTurn << " " << c.shields << " " << c.production
           << " " << c.units << " " << int(c.productionType) << " "
           << c.name << "\n";
    }

    // Terrain bytes (80*50 = 4000 bytes -> 8000 hex chars on one line).
    os << "terrain ";
    writeHex(os, dumpTerrainBytes(const_cast<OpenCiv1Game&>(p)));
    os << "\n";

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
    // Accept v1 (pre-tech-tree) and v2 (current). v1 files just skip the
    // tech state restore and TechResearch::initCivs runs (Alphabet default).
    if (header != "OpenCiv1pp savegame v1" &&
        header != "OpenCiv1pp savegame v2") return false;

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
    // Per-civ tech state we'll restore after the main vectors are applied.
    struct TechRow { int civId; int researching; int points; std::vector<int> known; };
    std::vector<TechRow> techRows;
    int techCivsCount = 0;

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
        else if (key == "terrain") {
            std::string hex;
            iss >> hex;
            if (!parseHex(hex, terrain)) return false;
            if (terrain.size() != std::size_t(MapManagement::kWidth) *
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

    // Restore the terrain grid bytes into VCPU memory (and into MiniWorld's
    // cache when one is attached). Run AFTER rebuildPlayingShell so MiniWorld
    // exists.
    MiniWorld* mw = flow ? flow->miniWorld() : nullptr;
    if (!terrain.empty()) {
        restoreTerrainBytes(p, mw, terrain);
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
