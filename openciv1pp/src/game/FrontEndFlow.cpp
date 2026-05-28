#include "FrontEndFlow.h"
#include "TextBoxDialogs.h"

namespace oc1 {

FrontEndFlow::FrontEndFlow(OpenCiv1Game& parent) : p(parent) {
    enterMainMenu();
}

const std::vector<std::string>& FrontEndFlow::mainMenuItems() {
    static const std::vector<std::string> items = {
        "Start a New Game", "Load a Saved Game", "Play on EARTH",
        "Customize World", "View Hall of Fame", "Quit",
    };
    return items;
}

const std::vector<std::string>& FrontEndFlow::difficultyItems() {
    static const std::vector<std::string> items = {
        "Chieftain (easiest)", "Warlord", "Prince", "King", "Emperor (toughest)",
    };
    return items;
}

void FrontEndFlow::enterMainMenu() {
    state_ = State::MAIN_MENU;
    p.menuBoxDialog().setupNav(int(mainMenuItems().size()), /*disabled*/ 0, /*startIndex*/ 0);
}

void FrontEndFlow::enterDifficulty() {
    state_ = State::DIFFICULTY;
    p.menuBoxDialog().setupNav(int(difficultyItems().size()), /*disabled*/ 0, /*startIndex*/ 0);
}

FrontEndFlow::State FrontEndFlow::handleKey(int navKey) {
    MenuBoxDialog& mb = p.menuBoxDialog();
    switch (state_) {
        case State::MAIN_MENU: {
            int r = mb.navStep(navKey);
            if (r == MenuBoxDialog::NavCancel) {
                // ESC at the top level acts like "Quit".
                state_ = State::QUIT;
            } else if (r >= 0) {
                // ENTER on an item: index 0 starts a new game; the last item quits.
                if (r == 0) enterDifficulty();
                else if (r == int(mainMenuItems().size()) - 1) state_ = State::QUIT;
                // Other items are not wired in this slice: stay on the menu.
            }
            break;
        }
        case State::DIFFICULTY: {
            int r = mb.navStep(navKey);
            if (r == MenuBoxDialog::NavCancel) {
                enterMainMenu();             // ESC -> back to the main menu.
            } else if (r >= 0) {
                chosenDifficulty_ = r;       // remember the chosen difficulty.
                state_ = State::STARTING;
            }
            break;
        }
        case State::STARTING:
            // The "starting game…" message box is up; any key dismisses it.
            if (navKey != MenuBoxDialog::KeyNone) state_ = State::DONE;
            break;
        case State::DONE:
        case State::QUIT:
            break; // terminal states.
    }
    return state_;
}

void FrontEndFlow::draw() {
    MenuBoxDialog& mb = p.menuBoxDialog();
    GBitmap& fb = p.graphics.screen(p.var_aa.screenID);

    switch (state_) {
        case State::MAIN_MENU:
        case State::DIFFICULTY: {
            const std::vector<std::string>& items =
                (state_ == State::MAIN_MENU) ? mainMenuItems() : difficultyItems();
            fb.clear(1);
            // Pre-select + highlight the currently navigated item.
            mb.defaultOptionIndex = mb.highlight;
            mb.forcedSelection = mb.highlight;
            mb.F0_2d05_0031_ShowMenuBox(items, 30, 20, /*windowFrame*/ true, /*helpOption*/ false);
            break;
        }
        case State::STARTING: {
            fb.clear(1);
            // Placeholder "starting game…" box. Title reuses the "Quit" -> 離開
            // placeholder key as the task suggests; the body explains the slice.
            TextBoxDialogs& tb = p.textBoxDialogs();
            tb.forcedSelection = 0;
            tb.F23_0000_0000_ShowTextBox(
                /*title*/ "Quit",
                /*message*/ "Start a New Game",
                /*buttons*/ {}, 60, 80, 200);
            break;
        }
        case State::DONE:
        case State::QUIT:
            break; // nothing to draw for terminal states.
    }
}

} // namespace oc1
