// GameLoadAndSave.h — minimal save/load for the in-progress game.
//
// Counterpart to the C# OpenCiv1/src/Game/CodeObjects/GameLoadAndSave.cs
// (F11_0000_* family that reads/writes the original CIVIL*.SVE binary). The
// C# class implements the FAITHFUL .SVE binary format (header, per-player
// records, map bytes, units/cities arrays, etc.) — that is a significant
// reverse-engineering effort and OUT OF SCOPE for THIS PASS.
//
// What this port DOES is a small, modern, human-inspectable text format that
// snapshots the in-memory state the C++ port actually has. It is enough to
// demonstrate "save the game, exit, load it again, play continues from the
// same place" without depending on a binary format we don't yet handle.
//
// FORMAT (single text file, ASCII; one record per line):
//   line 1   : "OpenCiv1pp savegame v3"   (v1 = pre-tech-tree; v2 = pre-
//              improvements; both still load for back-compat — the missing
//              tail fields default to 0/empty)
//   line 2+  : "<KEY> <value...>" records, in this order:
//                turn          <int>
//                year          <int>
//                difficulty    <int>     (FrontEndFlow chosenDifficulty)
//                tribe         <int>     (FrontEndFlow chosenTribe / UnitManagement)
//                seed          <uint32>  (FrontEndFlow worldSeedOverride or derived)
//                name          <string-rest-of-line>  (FrontEndFlow chosenName)
//                state         <int>     (FrontEndFlow::State as integer)
//                unitpos       <x> <y>   (the human Unit position from MiniWorld)
//                civs          <count>
//                civ           <tribeIdx> <color> <isHuman 0|1> <name...>
//                units         <count>
//                unit          <owner> <x> <y> <typeInt> <alive 0|1>
//                              [<workTurnsLeft> <workTargetInt>]   (v3+)
//                cities        <count>
//                city          <id> <owner> <x> <y> <foundedTurn> <shields>
//                              <production> <units> <productionTypeInt> <name...>
//                terrain       <80*50 hex bytes; one byte per cell in row-major
//                              (y * 80 + x) order; 4000 bytes -> 8000 hex chars>
//                improvements  <80*50 hex bytes; per-tile TerrainImprovementFlags
//                              bitmask (Road=8, Irrigation=2, ...)>   (v3+)
//                techcivs      <count>   (per-civ tech state count)
//                techcsv       <civId> <researchingInt> <points> <techCsv>
//                              techCsv = comma-separated integer Tech ids the
//                              civ knows; "" (or "-") when none yet.
//
// TODO(port): the faithful CIVIL*.SVE binary read/write (the C# F11_0000_*
// methods + ReadInt16/WriteInt16 layout) — that is the entire C# class.
#pragma once
#include <string>

namespace oc1 {

class OpenCiv1Game;
class FrontEndFlow;

class GameLoadAndSave {
public:
    explicit GameLoadAndSave(OpenCiv1Game& parent);

    // Snapshot the current game state to `path`. When `flow` is non-null its
    // chosen{Difficulty,Tribe,Name,Seed,State} are recorded; otherwise zeros /
    // defaults are written. Returns false on I/O failure.
    bool saveToFile(const std::string& path, const FrontEndFlow* flow = nullptr) const;

    // Restore the game state from `path`. When `flow` is non-null its
    // chosen{Difficulty,Tribe,Name,Seed,State} are restored AND its MiniWorld
    // is rebuilt with the restored state. Returns false on parse/I/O failure.
    bool loadFromFile(const std::string& path, FrontEndFlow* flow = nullptr);

private:
    OpenCiv1Game& p;
};

} // namespace oc1
