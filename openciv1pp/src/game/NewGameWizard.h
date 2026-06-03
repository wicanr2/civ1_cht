// NewGameWizard.h — A2: the DOS-style new-game cascade frame.
//
// DOS Civ1's new-game flow shows a recognisable two-column layout across the
// difficulty / civilizations / tribe / name screens: a narrow LEFT column of
// stacked leader portraits and a wide RIGHT column with a yellow-bordered
// list of options. The C# port draws the per-screen ShowMenuBox at fixed
// coords (e.g. F5_0000_0000_InitNewGameData at (160, 35)) without the framing
// chrome; openciv1pp's MenuBoxDialog gives the box but not the cascade-wide
// visual. This module adds the chrome (frame + portrait column + stage title
// bar) and tracks the 4-stage state machine.
//
// Stages (1:1 with the DOS sequence):
//   1. DIFFICULTY     - "Chieftain (easiest)" .. "Emperor (toughest)"
//   2. CIVILIZATIONS  - opponent count 3..6 (DOS default 6; min 1 ai + 1
//                       human; we expose the 3..6 spread)
//   3. TRIBE          - the 14-tribe nationality list (MainCode::tribes())
//   4. NAME           - leader name (defaults to the tribe's leader name)
//
// State is persistent across stages: choices made earlier are carried through
// so an interactive caller can re-enter (e.g. ESC back to DIFFICULTY) without
// losing later selections. drawWizardFrame() is the headless-callable render
// entrypoint; nav() drives the state machine via the same NavKey codes
// MenuBoxDialog uses.
//
// Pixel-perfect parity with the 1991 DOS boot path is impossible without the
// original .PIC backgrounds for the wizard frame, so the geometry below is a
// reasonable DOS-feel approximation:
//   - Left column:  width 96 px, full height; stacks up to 7 portraits.
//   - Right column: yellow (palette index 14) 2-px outer border, 1-px inner;
//                   a 12-px title bar at top with the stage label.
//   - Highlight:    inverted background (palette index 14 -> 0) for the
//                   selected row (the MenuBoxDialog convention).
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

class NewGameWizard {
public:
    explicit NewGameWizard(OpenCiv1Game& parent);

    enum class Stage { DIFFICULTY = 0, CIVILIZATIONS = 1, TRIBE = 2, NAME = 3, DONE = 4 };

    // Reset to stage DIFFICULTY with the highlight at row 0.
    void reset();

    Stage stage() const { return stage_; }
    int highlight() const { return highlight_; }

    int chosenDifficulty() const { return chosenDifficulty_; }
    int chosenCivCount()   const { return chosenCivCount_; }
    int chosenTribe()      const { return chosenTribe_; }
    const std::string& chosenName() const { return chosenName_; }

    // Right-column options for the current stage. The civilizations stage
    // lists the four spread values 3..6; the tribe stage forwards
    // MainCode::tribes() nationalities; the name stage returns a single-entry
    // list (the default leader name) for symmetry — the wizard frame draws
    // an edit-box prompt instead of a list there.
    std::vector<std::string> currentOptions() const;

    // The stage title-bar label (English; translated at draw time).
    std::string currentStageLabel() const;

    // Step the state machine with a MenuBoxDialog NavKey. Returns the new
    // stage. Transitions:
    //   DIFFICULTY  : ENTER -> remember chosenDifficulty, -> CIVILIZATIONS
    //                 ESC   -> stay (front-end's ESC handler decides)
    //   CIVS        : ENTER -> remember chosenCivCount,   -> TRIBE
    //                 ESC   -> -> DIFFICULTY
    //   TRIBE       : ENTER -> remember chosenTribe,      -> NAME
    //                 ESC   -> -> CIVILIZATIONS
    //   NAME        : ENTER -> remember chosenName (default = tribe leader),
    //                          -> DONE
    //                 ESC   -> -> TRIBE
    Stage nav(int navKey);

    // Index of the portrait to highlight in the left column. By default this
    // follows chosenTribe_ (so once the user picks Romans the Caesar portrait
    // stays highlighted across the wizard); during the tribe stage it follows
    // the current highlight_ row so the user sees the leader they're hovering.
    int leftPortraitIndex() const;

    // Draw the wizard frame (left portrait stack + right list with the stage
    // title bar) into screen 0. Idempotent / side-effect-only. The right-
    // column options use the SAME MenuBoxDialog convention for highlight as
    // the bare DIFFICULTY/TRIBE screens (inverted palette 14 -> 0 row).
    //
    // `stage` / `leftIdx` / `rightOptions` are parameterised so a caller can
    // render an arbitrary state for snapshots; the no-arg overload uses the
    // wizard's current internal state.
    void drawWizardFrame();
    void drawWizardFrame(Stage stage, int leftPortraitIndex,
                         const std::vector<std::string>& rightOptions);

    // The civilizations-stage option list (3..6).
    static const std::vector<int>& civilizationCounts();

    // Default leader name for the NAME stage. Same logic as FrontEndFlow:
    // when a tribe has been chosen, return its leader name; else "Player".
    std::string defaultLeaderName() const;

private:
    OpenCiv1Game& p;

    Stage stage_ = Stage::DIFFICULTY;
    int   highlight_ = 0;
    int   chosenDifficulty_ = -1;
    int   chosenCivCount_   = -1;
    int   chosenTribe_      = -1;
    std::string chosenName_;
};

} // namespace oc1
