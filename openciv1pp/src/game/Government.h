// Government.h — Civ1 government types + per-government effect table.
//
// Civ1 has 5 governments (Anarchy is the transitional state between two
// stable governments). Cross-checked against OpenCiv1GameGlobals.cs line 68:
//   public string[] Array_1966 =
//     { "Anarchy", "Despotism", "Monarchy", "Communist", "Republic", "Democratic" };
// The C# enum stretches across 6 entries (Communist=3 sits between Monarchy
// and Republic); this early-era C++ port ships the FIVE iconic ones the task
// scopes (Anarchy, Despotism, Monarchy, Republic, Democracy) and uses
// numeric values {0,1,2,3,4} so the table indexing stays trivial. Communist
// can be slotted in later (give it id 5 or relabel; on-disk format would
// stay back-compat since the loader treats the value as a plain int).
//
// Per-government effects (faithful Civ1 values from the manual + community
// wikis; the C# port doesn't materialise them as a single table — they fan
// out across CityWorker.cs / Encyclopedia.cs / Segment_1238):
//   Anarchy    : transition only. No bonuses. Production halved
//                (productionMul=0.5). No science bonus. Set automatically
//                when a civ switches government (3 turns of transition).
//   Despotism  : the default starting government. No tech prereq. Baseline
//                multipliers (all 1.0). Civ1 also applies a -1 food penalty
//                on tiles yielding >= 3 — modelled as a small foodPenalty
//                flag but not exercised in this slice (food math is
//                simplified). [/* TODO(port): tile-yield food penalty */]
//   Monarchy   : tech prereq = Monarchy. +25% trade (tradeMul=1.25).
//                Production / science unchanged.
//   Republic   : tech prereq = Code of Laws. +1 trade per tile (modelled
//                as tradeMul=1.5 in this slice) + production unchanged.
//                Civ1 ALSO removes the Despotism food penalty (n/a here
//                since we don't apply it) and introduces war-unhappiness
//                (also out of scope — diplomacy/happiness aren't ported).
//   Democracy  : tech prereq = Democracy. +50% science (scienceMul=1.5),
//                +1 trade per tile (tradeMul=1.75 in this slice). Civ1's
//                "one civil-disorder city = REVOLUTION" rule is gated
//                behind happiness (out of scope; TODO).
//
// This slice exposes science / production / trade multipliers as floats.
// CheckPlayerTurn applies scienceMul to TechResearch::addPoints and
// productionMul to shield accumulation. tradeMul is plumbed for future use
// (the trade pipeline isn't materialised yet — money/luxury are stubbed).
#pragma once
#include "TechResearch.h"
#include <cstdint>

namespace oc1 {

// Numeric ids — kept dense {0..4} so the GovernmentDef table is index-based.
// On-disk format (GameLoadAndSave v6) writes/reads these as plain ints, so
// renumbering is a breaking change; do not reorder.
enum class Government : uint8_t {
    Anarchy   = 0,
    Despotism = 1,
    Monarchy  = 2,
    Republic  = 3,
    Democracy = 4,
};

// One row of the per-government effect table. `techPrereq` is Tech::None for
// the two governments any civ can run from turn 1 (Anarchy = transitional,
// Despotism = default start). Monarchy/Republic/Democracy gate behind their
// matching prereq techs (see TechResearch.h additions).
struct GovernmentDef {
    Government id;
    const char* name;       // English key (Translator turns it into Chinese)
    float scienceMul;       // multiplier applied to research-point accumulation
    float productionMul;    // multiplier applied to per-city shield yield
    float tradeMul;         // multiplier for the (future) trade pipeline
    Tech  techPrereq;       // required tech (Tech::None when always available)
};

// Get the GovernmentDef for a Government id. Falls back to the Despotism row
// for out-of-range ids so callers can dereference safely.
inline const GovernmentDef& governmentDefOf(Government g) {
    // Anarchy production is halved (faithful Civ1 transition penalty); the
    // other governments keep production at 1.0 (Despotism/Monarchy baseline)
    // and only diverge on science / trade. Trade multipliers approximate the
    // "+1 trade per tile" effect by stretching the trade multiplier (the
    // exact per-tile delta lands when the trade pipeline is materialised).
    static const GovernmentDef kDefs[5] = {
        {Government::Anarchy,   "Anarchy",   1.0f, 0.5f, 1.0f,  Tech::None},
        {Government::Despotism, "Despotism", 1.0f, 1.0f, 1.0f,  Tech::None},
        // "Govt Monarchy" (not "Monarchy") so the Government name key and
        // the Monarchy-TECH name key don't collide in the Translator (the
        // tech name "Monarchy" is reserved for the research advance —
        // Chinese "君主制" — while the government label resolves to "君主").
        {Government::Monarchy,  "Govt Monarchy", 1.0f, 1.0f, 1.25f, Tech::Monarchy},
        {Government::Republic,  "Republic",  1.0f, 1.0f, 1.5f,  Tech::CodeOfLaws},
        // Democracy gates behind the Democracy tech; the tech is shipped in
        // the early-era table so the prereq chain is real (Democracy needs
        // its prereqs known too — see TechResearch.cpp for the chain).
        // Govt-prefix to avoid colliding with the Tech::Democracy name in the
        // Translator (the tech name reuses "Democracy" -> "民主政治").
        {Government::Democracy, "Govt Democracy", 1.5f, 1.0f, 1.75f, Tech::Democracy},
    };
    int i = int(g);
    if (i < 0 || i > 4) i = 1; // out-of-range -> Despotism
    return kDefs[i];
}

// English key for a Government id (delegates to governmentDefOf(g).name).
inline const char* governmentNameKey(Government g) {
    return governmentDefOf(g).name;
}

} // namespace oc1
