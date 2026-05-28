// TechResearch.cpp — see TechResearch.h for the header comment.
//
// Faithful early-era subset: 8 techs + their single prereq edges. Numeric ids
// match TechnologyAdvanceEnum (C# Definitions/TechnologyAdvanceEnum.cs); the
// prereq edges match GameData.cs lines 266-308 (cross-checked in the header).
//
// // TODO(port): full 72-tech enum, two-prereq table, weighted-cost formula
// (Civ1's F0_*_GetTechCost scales by difficulty + count of known techs).
#include "TechResearch.h"
#include "OpenCiv1Game.h"
#include <algorithm>
#include <cstddef>

namespace oc1 {

namespace {
// Early-era handful (Civ1 era 1 + the cheapest era-2 starters). The cost
// ladder is the FLAT one documented in the header. Order is "logical" so a
// freshly-init'd civ researching Alphabet gets a deterministic next pick.
const TechDef kTechs[] = {
    {Tech::Alphabet,      "Alphabet",       10, Tech::None},
    {Tech::Pottery,       "Pottery",        10, Tech::None},
    {Tech::TheWheel,      "The Wheel",      10, Tech::None},
    {Tech::Masonry,       "Masonry",        20, Tech::None},
    {Tech::BronzeWorking, "Bronze Working", 20, Tech::None},
    {Tech::Writing,       "Writing",        30, Tech::Alphabet},
    {Tech::Currency,      "Currency",       30, Tech::BronzeWorking},
    {Tech::IronWorking,   "Iron Working",   40, Tech::BronzeWorking},
};
constexpr int kTechN = int(sizeof(kTechs) / sizeof(kTechs[0]));
} // namespace

int TechResearch::techCount() { return kTechN; }
const TechDef& TechResearch::techByIndex(int i) {
    if (i < 0 || i >= kTechN) i = 0;
    return kTechs[i];
}
const TechDef* TechResearch::techDefById(Tech t) {
    if (t == Tech::None) return nullptr;
    for (int i = 0; i < kTechN; ++i) if (kTechs[i].id == t) return &kTechs[i];
    return nullptr;
}
const char* TechResearch::techNameKey(Tech t) {
    const TechDef* d = techDefById(t);
    return d ? d->name : "";
}

TechResearch::TechResearch(OpenCiv1Game& parent) : p(parent) {
    (void)p; // reserved for future hooks
}

void TechResearch::initCivs(int civCount) {
    if (civCount < 0) civCount = 0;
    civs_.assign(std::size_t(civCount), CivTechState{});
    // Default: every civ researches Alphabet, 0 points, empty knownTechs.
    // (CivTechState defaults already encode this; loop kept for clarity.)
    for (auto& c : civs_) {
        c.knownTechs.reset();
        c.currentResearch = Tech::Alphabet;
        c.points = 0;
    }
}

bool TechResearch::civKnows(int civId, Tech t) const {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return false;
    if (t == Tech::None) return true; // "no prereq" is trivially known
    int bit = int(t);
    if (bit < 0 || bit >= int(civs_[std::size_t(civId)].knownTechs.size()))
        return false;
    return civs_[std::size_t(civId)].knownTechs.test(std::size_t(bit));
}

Tech TechResearch::civResearching(int civId) const {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return Tech::None;
    return civs_[std::size_t(civId)].currentResearch;
}

int TechResearch::civPoints(int civId) const {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return 0;
    return civs_[std::size_t(civId)].points;
}

int TechResearch::civResearchCost(int civId) const {
    Tech t = civResearching(civId);
    const TechDef* d = techDefById(t);
    return d ? d->cost : 0;
}

void TechResearch::setCivKnows(int civId, Tech t, bool known) {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return;
    if (t == Tech::None) return;
    int bit = int(t);
    if (bit < 0 || bit >= int(civs_[std::size_t(civId)].knownTechs.size())) return;
    civs_[std::size_t(civId)].knownTechs.set(std::size_t(bit), known);
}
void TechResearch::setCivResearching(int civId, Tech t) {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return;
    civs_[std::size_t(civId)].currentResearch = t;
}
void TechResearch::setCivPoints(int civId, int pts) {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return;
    if (pts < 0) pts = 0;
    civs_[std::size_t(civId)].points = pts;
}

Tech TechResearch::pickNextTech(int civId) const {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return Tech::None;
    Tech best = Tech::None;
    int bestCost = 0x7fffffff;
    for (int i = 0; i < kTechN; ++i) {
        const TechDef& d = kTechs[i];
        if (civKnows(civId, d.id)) continue;
        // Prereq satisfied: either None or already known by this civ.
        if (d.prereq != Tech::None && !civKnows(civId, d.prereq)) continue;
        if (d.cost < bestCost) {
            bestCost = d.cost;
            best = d.id;
        }
    }
    return best;
}

void TechResearch::addPoints(int civId, int n) {
    if (civId < 0 || std::size_t(civId) >= civs_.size()) return;
    if (n <= 0) return;
    auto& s = civs_[std::size_t(civId)];
    Tech target = s.currentResearch;
    if (target == Tech::None) return; // nothing to research (all unlocked)
    const TechDef* d = techDefById(target);
    if (!d) return;
    s.points += n;
    if (s.points >= d->cost) {
        // Unlock. C# also doubles overflow into the next target; we drop it
        // (simpler + faithful enough for the early-era handful).
        setCivKnows(civId, target, true);
        s.points = 0;
        s.currentResearch = pickNextTech(civId);
    }
}

} // namespace oc1
