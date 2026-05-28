// FrontEndFlow.h — the Civ1 start-sequence front-end state machine.
//
// A small reusable module (not a 1:1 ported CodeObject) that composes the
// already-ported MenuBoxDialog (Chinese-rendering menu + navStep/setupNav) and
// TextBoxDialogs (message box) into Civ1's real start flow:
//
//   TITLE -> MAIN_MENU -> DIFFICULTY -> TRIBE -> NAME -> STARTING -> DONE
//
// The transition core (handleKey) is pure and headless-testable: it feeds the
// abstract NavKey codes (MenuBoxDialog::NavKey: 1=Up,2=Down,3=Enter,4=Esc) into
// the current menu's navStep, advances the state, and remembers chosen
// difficulty / tribe / name. The interactive --menuflow / --newgame mode wires
// SdlPresenter::pollKey() -> handleKey() -> draw(); the headless --flowtest /
// --newgametest drives handleKey() directly.
//
// Rendering goes through the parent game's MenuBoxDialog / TextBoxDialogs /
// MainCode, so every label is localized for free (the menu items are
// zh_TW.json keys).
#pragma once
#include "OpenCiv1Game.h"
#include "MenuBoxDialog.h"
#include <string>
#include <vector>

namespace oc1 {

class FrontEndFlow {
public:
    explicit FrontEndFlow(OpenCiv1Game& parent);

    // The front-end states. QUIT and DONE both terminate the interactive loop;
    // QUIT means the player picked "Quit" from the main menu, DONE means the
    // start sequence reached the "starting game…" message box and ended.
    //
    // TITLE is the optional logo+menu entry point: a screen showing LOGO.PIC
    // and the main menu in one go (driven by MainCode). When the caller starts
    // the flow at MAIN_MENU directly (the legacy behaviour exercised by
    // flowtest), TITLE is simply skipped. Choosing "Start a New Game" advances
    // through DIFFICULTY -> TRIBE -> NAME -> STARTING -> DONE.
    enum class State { TITLE, MAIN_MENU, DIFFICULTY, TRIBE, NAME, STARTING, DONE, QUIT };

    // Main-menu item labels (English; rendered translated). Index 0 = "Start a
    // New Game" -> DIFFICULTY; last index = "Quit" -> QUIT.
    static const std::vector<std::string>& mainMenuItems();
    // Difficulty item labels (English; rendered translated). All five are
    // present in zh_TW.json.
    static const std::vector<std::string>& difficultyItems();
    // Tribe (nationality) labels in NationDefinition order ids 1..7, 9..15.
    // Forwards to MainCode::tribes() so the leader/nation pairs stay together.
    static std::vector<std::string> tribeItems();

    // Pure transition core. Feeds `navKey` (a MenuBoxDialog::NavKey) into the
    // current menu's navStep and advances the state machine:
    //   TITLE      : any key -> MAIN_MENU.
    //   MAIN_MENU  : ENTER on item 0 -> DIFFICULTY; ENTER on "Quit" -> QUIT;
    //                ESC -> QUIT.
    //   DIFFICULTY : ENTER -> remember chosenDifficulty, -> TRIBE; ESC ->
    //                back to MAIN_MENU.
    //   TRIBE      : ENTER -> remember chosenTribe, -> NAME; ESC -> back to
    //                DIFFICULTY.
    //   NAME       : ENTER -> STARTING (chosenName defaults to the tribe's
    //                leader); ESC -> back to TRIBE.
    //   STARTING   : any key -> DONE (the message box has been dismissed).
    // Returns the (possibly new) current state.
    State handleKey(int navKey);

    // Current state / current highlighted item / chosen values. -1 / "" means
    // "not yet selected".
    State state() const { return state_; }
    int highlight() const { return p.menuBoxDialog().highlight; }
    int chosenDifficulty() const { return chosenDifficulty_; }
    int chosenTribe() const { return chosenTribe_; }
    const std::string& chosenName() const { return chosenName_; }

    // Settable default name for the NAME stub (we have no real keyboard edit
    // loop). When empty, the chosen tribe's leader name is used at ENTER.
    void setDefaultName(std::string n) { defaultName_ = std::move(n); }

    // Optional: start the flow at TITLE (logo+main menu) instead of the bare
    // MAIN_MENU. Used by the interactive --newgame entry. Safe to call any
    // time; resets the menu nav state for the new screen.
    void enterTitle();

    // Draw the current screen into the parent game's screen 0. Safe to call
    // every frame.
    void draw();

private:
    // (Re)arm the menu's navStep state for the screen we just entered.
    void enterMainMenu();
    void enterDifficulty();
    void enterTribe();
    void enterName();

    OpenCiv1Game& p;
    State state_ = State::MAIN_MENU;
    int chosenDifficulty_ = -1;
    int chosenTribe_      = -1;
    std::string chosenName_;
    std::string defaultName_;
};

} // namespace oc1
