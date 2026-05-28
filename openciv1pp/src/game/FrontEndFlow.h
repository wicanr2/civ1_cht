// FrontEndFlow.h — the Civ1 start-sequence front-end state machine.
//
// A small reusable module (not a 1:1 ported CodeObject) that composes the
// already-ported MenuBoxDialog (Chinese-rendering menu + navStep/setupNav) and
// TextBoxDialogs (message box) into Civ1's real start flow:
//
//     MAIN_MENU  ->  DIFFICULTY  ->  STARTING (message box)  ->  DONE
//
// The transition core (handleKey) is pure and headless-testable: it feeds the
// abstract NavKey codes (MenuBoxDialog::NavKey: 1=Up,2=Down,3=Enter,4=Esc) into
// the current menu's navStep, advances the state, and remembers the chosen
// difficulty. The interactive --menuflow mode wires SdlPresenter::pollKey() ->
// handleKey() -> draw(); the headless --flowtest drives handleKey() directly.
//
// Rendering goes through the parent game's MenuBoxDialog / TextBoxDialogs, so
// every label is localized for free (the menu items are zh_TW.json keys).
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
    enum class State { MAIN_MENU, DIFFICULTY, STARTING, DONE, QUIT };

    // Main-menu item labels (English; rendered translated). Index 0 = "Start a
    // New Game" -> DIFFICULTY; last index = "Quit" -> QUIT.
    static const std::vector<std::string>& mainMenuItems();
    // Difficulty item labels (English; rendered translated). All five are
    // present in zh_TW.json.
    static const std::vector<std::string>& difficultyItems();

    // Pure transition core. Feeds `navKey` (a MenuBoxDialog::NavKey) into the
    // current menu's navStep and advances the state machine:
    //   MAIN_MENU  : ENTER on item 0 -> DIFFICULTY; ENTER on "Quit" -> QUIT.
    //   DIFFICULTY : ENTER -> remember chosenDifficulty, -> STARTING; ESC ->
    //                back to MAIN_MENU.
    //   STARTING   : any key -> DONE (the message box has been dismissed).
    // Returns the (possibly new) current state.
    State handleKey(int navKey);

    // Current state / current highlighted item / chosen difficulty (-1 until a
    // difficulty is selected). Getters for the interactive loop and the tests.
    State state() const { return state_; }
    int highlight() const { return p.menuBoxDialog().highlight; }
    int chosenDifficulty() const { return chosenDifficulty_; }

    // Draw the current screen into the parent game's screen 0 (menu via
    // MenuBoxDialog, the STARTING screen via TextBoxDialogs). Highlights the
    // currently navigated item. Safe to call every frame.
    void draw();

private:
    // (Re)arm the menu's navStep state for the screen we just entered.
    void enterMainMenu();
    void enterDifficulty();

    OpenCiv1Game& p;
    State state_ = State::MAIN_MENU;
    int chosenDifficulty_ = -1;
};

} // namespace oc1
