// HallOfFame.h — endgame ranking table (Civ1's "Hall of Fame" screen).
//
// Simplified port of OpenCiv1's HallOfFame.cs (which implements the DOS
// FAME.DTA on-disk record format via a Borland CAPI open/read/write/movedata
// sequence). This C++ port abandons the binary FAME.DTA wire format in favour
// of a plain JSON file at ~/.openciv1pp/halloffame.json (override with
// setStorageDir for headless tests). The on-screen UI mirrors the C# layout:
//
//   Hall of Fame
//   Rank  Name        Civ        Year     Score  Result
//   1     Caesar      Romans     1850 AD  742    Conquest Victory
//   ...
//
// Persistence:
//   - load()  reads the JSON. Missing/corrupt -> empty table (silent).
//   - save()  writes the JSON.
//   - addRecord() appends + re-sorts by score DESC.
//   - sortByScore() / records() expose the current table.
//
// Score formula (faithful simplified Civ1):
//   score = sum(cityPop) + 2 * wonderCount + techCount + percentLandControlled
//   On loss (Defeat) the score is halved (Civ1 "lost the game" penalty: the
//   DOS code zeros it; we keep half so loss entries stay visible). On
//   Conquest victory the score is unchanged. On Space Race victory the score
//   is DOUBLED (Civ1's space-race bonus). Game year is stored as the AD/BC
//   year integer (negative == BC, positive == AD).
//
// This object is pure / headless-testable: addRecord/sortByScore/save/load
// operate on an in-memory vector; rendering writes to screen 0 via GDriver.
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

enum class HallOfFameResult : int {
    Conquest = 0,  // military victory
    Space    = 1,  // space-race victory
    Defeat   = 2,  // game over (lost or quit)
};

struct HallOfFameRecord {
    std::string name;    // player name
    std::string tribe;   // tribe English key (e.g. "Romans"); rendered translated
    int         score;   // final score (post-multiplier)
    int         year;    // negative = BC, positive = AD
    HallOfFameResult result;
    // ISO 8601-ish UTC date string for the record (e.g. "2026-06-03"). Used
    // for debugging / display below; empty when unset.
    std::string date;
};

class HallOfFame {
public:
    explicit HallOfFame(OpenCiv1Game& parent);

    // Compute the faithful simplified Civ1 score.
    //   citiesPop:       sum of every owned city's population
    //   wonders:         number of wonders the civ has built
    //   techs:           number of techs known
    //   landPercent:     0..100 percent of land tiles controlled
    //   result:          Conquest=baseline, Space=doubled, Defeat=halved
    static int computeScore(int citiesPop, int wonders, int techs,
                            int landPercent, HallOfFameResult result);

    // Append a record, then re-sort by score DESC (stable).
    void addRecord(const HallOfFameRecord& r);

    // Sort the current table by score DESC (stable on equal scores).
    void sortByScore();

    // Path management — defaults to ~/.openciv1pp/halloffame.json. Tests use
    // setStorageDir("/tmp/...") to redirect.
    void setStorageDir(std::string dir) { storageDir_ = std::move(dir); }
    std::string storagePath() const;

    // Round-trip JSON storage. Returns true on success. load() merges over
    // the in-memory table only when the file parses (corrupt -> in-memory
    // unchanged, returns false). save() writes the current table.
    bool load();
    bool save() const;

    // Clear the in-memory table (does not touch disk).
    void clear() { records_.clear(); }

    const std::vector<HallOfFameRecord>& records() const { return records_; }
    std::vector<HallOfFameRecord>& recordsMut() { return records_; }

    // English key for HallOfFameResult; goes through Translator -> Chinese.
    static const char* resultKey(HallOfFameResult r);

    // Render the table to screen 0 (banner + column headers + up to 8 rows).
    // Safe to call any time.
    void draw();

private:
    OpenCiv1Game& p;
    std::vector<HallOfFameRecord> records_;
    std::string storageDir_; // empty -> default ~/.openciv1pp/
};

} // namespace oc1
