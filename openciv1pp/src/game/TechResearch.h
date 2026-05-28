// TechResearch.h — ported CodeObject (subset of OpenCiv1 Encyclopedia.cs +
// GameData.technologyAdvanceTypes).
//
// Civ1's RESEARCH / TECH TREE: each civ accumulates "research points" per
// turn (proportional to city count in this slice; the C# variant scales by
// per-city science output via the F0_*_GetCityResourceCount fan-out). When
// points reach the current tech's cost, the tech is UNLOCKED into the civ's
// knownTechs set and the cheapest unknown tech whose prereqs are satisfied
// is auto-picked as the next research target.
//
// FAITHFUL SUBSET (early-era handful + their prereq edges, cross-checked
// against /home/anr2/civ1_cht/OpenCiv1/src/Game/State/GameData.cs lines
// 266-308):
//   Alphabet       (prereq: None)
//   Pottery        (prereq: None)
//   TheWheel       (prereq: None)
//   Masonry        (prereq: None)
//   BronzeWorking  (prereq: None)
//   IronWorking    (prereq: BronzeWorking)            (GameData line 284)
//   Writing        (prereq: Alphabet)                 (GameData line 288)
//   Currency       (prereq: BronzeWorking)            (GameData line 268)
//
// Numeric enum values are kept 1:1 with TechnologyAdvanceEnum (Definitions/
// TechnologyAdvanceEnum.cs) so save/load round-trips and a future deeper
// port read the same ids. The C# `TechnologyAdvanceDefinition` has TWO
// prereqs (RequiresTechnologyAdvance1/2); for our early-era handful only
// ONE prereq is meaningfully used (all the techs above have prereq2=None),
// so the C++ port carries a single `prereq` field — straight-forward to
// widen to two later.
//
// COST: Civ1 scales each tech's cost by difficulty and number of techs
// already known (the F0_*_GetTechCost helper). For this slice we use a
// faithful FLAT cost ladder: 10 / 20 / 30 / 40 / 50 across the 8 techs in
// roughly the same "early/mid" tier they sit on (Alphabet/Pottery/TheWheel
// = 10 each; BronzeWorking/Masonry = 20; Writing/Currency = 30;
// IronWorking = 40). Tests assert thresholds, not Civ1's exact formula.
//
// STUBS:
//   * Full 72-tech enum / two-prereq table / weighted-cost formula — see
//     `// TODO(port)` in .cpp. Only the early-era handful is exposed here.
#pragma once
#include <bitset>
#include <cstdint>
#include <vector>

namespace oc1 {

class OpenCiv1Game;

// Numeric values match C# TechnologyAdvanceEnum (cross-checked above).
// Monarchy / CodeOfLaws / Democracy added for the GOVERNMENT slice (the
// 3 governments past the always-available Despotism each gate behind one
// of these techs). Numeric ids match TechnologyAdvanceEnum.cs:
//   Monarchy=24, CodeOfLaws=25, Democracy=27. Prereq chains (faithful
//   GameData.cs / Civ1 manual): Monarchy <- CeremonialBurial (stubbed
//   here to Tech::None so the early-era slice can reach it), CodeOfLaws
//   <- Alphabet (one of the two Civ1 prereqs), Democracy <- {Philosophy
//   + Literacy} (both stubbed to CodeOfLaws so the chain is real but
//   reachable without porting the deeper tree).
enum class Tech : int8_t {
    None          = -1,
    Alphabet      = 0,
    Currency      = 2,
    // Construction = 5 (faithful to C# TechnologyAdvanceEnum.Construction);
    // Civ1 prereq is Masonry + CurrencyAlternate, we collapse to a single
    // Masonry prereq for the early-era subset (mirrors the same single-
    // prereq simplification used by Monarchy/CodeOfLaws above).
    // Added for the Wonders slice: Great Wall requires Construction.
    Construction  = 5,
    Masonry       = 16,
    BronzeWorking = 17,
    IronWorking   = 18,
    Writing       = 22,
    Monarchy      = 24,
    CodeOfLaws    = 25,
    Democracy     = 27,
    TheWheel      = 33,
    Pottery       = 42,
};

// One row of the (early-era subset of the) C# technologyAdvanceTypes table.
// `cost` is the FLAT cost ladder described in the header comment.
struct TechDef {
    Tech id;
    const char* name;   // English key (Translator turns it into Chinese)
    int  cost;          // research points to unlock
    Tech prereq;        // single prereq (Tech::None means "no prereq")
};

class TechResearch {
public:
    explicit TechResearch(OpenCiv1Game& parent);

    // Reset per-civ state for `civCount` civs. Every civ starts with an empty
    // knownTechs set, 0 research points, and currentResearch = Alphabet (the
    // canonical first Civ1 starting tech). Mirrors the C# loop that fills
    // Players[i].TechnologyAdvances[] with false at GameData init.
    void initCivs(int civCount);

    // Add `n` research points to civ `civId`. When the running total reaches
    // the cost of the current research target, that tech is unlocked into
    // knownTechs, points are zeroed, and the cheapest still-unknown tech
    // whose prereq is already known is auto-picked as the new target. When
    // no further tech qualifies, currentResearch is set to Tech::None.
    void addPoints(int civId, int n);

    // Query — bounds-safe; out-of-range civ ids return false / None / 0.
    bool civKnows(int civId, Tech t) const;
    Tech civResearching(int civId) const;
    int  civPoints(int civId) const;

    // The cost of a civ's currently-researched tech (0 if no target).
    int  civResearchCost(int civId) const;

    // Direct mutation hooks used by GameLoadAndSave (restore persisted state).
    void setCivKnows(int civId, Tech t, bool known);
    void setCivResearching(int civId, Tech t);
    void setCivPoints(int civId, int pts);

    // The shipped TechDef table (size + indexed read). Stable order; index
    // is NOT the Tech enum value — use techDefById() to look up by Tech id.
    static int             techCount();
    static const TechDef&  techByIndex(int i);
    static const TechDef*  techDefById(Tech t); // nullptr when t == None

    // English key for a Tech (delegates to techDefById(t)->name). Returns
    // "" for Tech::None.
    static const char*     techNameKey(Tech t);

    // The number of civs initCivs() has provisioned.
    int civCount() const { return int(civs_.size()); }

private:
    OpenCiv1Game& p;
    // Per-civ runtime state. knownTechs sized at compile time to cover the
    // 8-tech early-era handful (max Tech enum value = 42 -> 64-bit bitset
    // gives us headroom for the full 72-tech expansion without breaking the
    // save format).
    struct CivTechState {
        std::bitset<128> knownTechs;       // indexed by Tech enum value
        Tech currentResearch = Tech::Alphabet;
        int  points = 0;
    };
    std::vector<CivTechState> civs_;

    // Pick the cheapest still-unknown tech whose (single) prereq is known
    // by civId. Returns Tech::None when nothing qualifies.
    Tech pickNextTech(int civId) const;
};

} // namespace oc1
