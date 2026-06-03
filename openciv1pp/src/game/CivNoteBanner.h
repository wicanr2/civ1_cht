// CivNoteBanner.h — A4: yellow-on-green "--- CIVILIZATION NOTE ---" pop-up.
//
// Civ1 DOS displays a short banner on the bottom of the screen for the FIRST
// occurrence of certain notable events (first city founded, first tech, first
// wonder, first contact with another civ). Each banner lingers a few turns,
// then auto-pops. The "first" gate lives on the (human) civ — a single
// boolean per event-class — so subsequent events of the same kind no longer
// trigger the banner.
//
// API:
//   queueNote(key)        — push a banner onto the queue. The key is a
//                            translatable English string (e.g. "Your first
//                            tech research: Pottery").
//   draw(GBitmap& screen) — paint the front-of-queue banner at the bottom
//                            32px strip (dark green bg + yellow border +
//                            white text, "--- 文明備忘 ---" header line).
//   tick()                — call once per end-of-turn. Decrements
//                            turnsLeft on the head; when it hits 0 the head
//                            is popped (next note from the queue becomes
//                            active, or no banner).
//   empty()               — true when no note is queued.
//
// First-fired flags live on CivState (see UnitManagement.h) as `notesFired_`
// (a bitset of NoteKind values); GameLoadAndSave v17 persists them so a
// save/load round-trip on turn 3 doesn't re-fire turn 1's first-city note.
#pragma once
#include <cstdint>
#include <deque>
#include <string>

namespace oc1 {

class GBitmap;
class GDriver;

// The kinds of events that can fire a CIVILIZATION NOTE. The bitset on
// CivState uses bit positions matching these integer values.
enum class CivNoteKind : uint8_t {
    FirstCity     = 0,
    FirstTech     = 1,
    FirstWonder   = 2,
    FirstContact  = 3,
    kMax          = 4,
};

struct PendingNote {
    std::string textKey;   // translatable label (run through Translator at draw time)
    int turnsLeft = 3;     // default display duration: 3 turns
};

class CivNoteBanner {
public:
    // Push a note onto the queue. Default duration: kDefaultTurns (3).
    void queueNote(std::string textKey, int turns = kDefaultTurns);

    // Decrement the head note's turnsLeft. When it hits 0 the head is popped.
    void tick();

    // Render the front-of-queue note as a bottom-32px banner. No-op when the
    // queue is empty. fontId picks the GFont used for both header and body.
    void draw(GDriver& gd, int fontId) const;

    bool   empty()  const { return queue_.empty(); }
    size_t pending() const { return queue_.size(); }

    // Direct access for save/load + tests.
    const std::deque<PendingNote>& queue() const { return queue_; }
    std::deque<PendingNote>&       queueMut()   { return queue_; }

    static constexpr int kDefaultTurns = 3;
    static constexpr int kBannerHeight = 32;

private:
    std::deque<PendingNote> queue_;
};

} // namespace oc1
