#include "FrontEndFlow.h"
#include "MainCode.h"
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
    return MainCode::difficultyItems();
}

std::vector<std::string> FrontEndFlow::tribeItems() {
    std::vector<std::string> items;
    items.reserve(MainCode::tribes().size());
    for (const auto& t : MainCode::tribes()) items.push_back(t.nationality);
    return items;
}

void FrontEndFlow::enterTitle() {
    state_ = State::TITLE;
    // The TITLE screen reuses the MAIN_MENU nav state (the title is just
    // LOGO.PIC + the main menu drawn over it). A press of any key drops the
    // logo overlay and continues with MAIN_MENU.
    p.menuBoxDialog().setupNav(int(mainMenuItems().size()), /*disabled*/ 0, /*startIndex*/ 0);
}

void FrontEndFlow::enterMainMenu() {
    state_ = State::MAIN_MENU;
    p.menuBoxDialog().setupNav(int(mainMenuItems().size()), /*disabled*/ 0, /*startIndex*/ 0);
}

void FrontEndFlow::enterDifficulty() {
    state_ = State::DIFFICULTY;
    p.menuBoxDialog().setupNav(int(difficultyItems().size()), /*disabled*/ 0, /*startIndex*/ 0);
}

void FrontEndFlow::enterTribe() {
    state_ = State::TRIBE;
    p.menuBoxDialog().setupNav(int(MainCode::tribes().size()), /*disabled*/ 0, /*startIndex*/ 0);
}

void FrontEndFlow::enterName() {
    state_ = State::NAME;
    // No menu navigation — the NAME screen is a single-line edit box (stubbed).
    // Set the default name to the chosen tribe's leader name (per the C# else
    // branch in F5_0000_0000_InitNewGameData: Players[].Name = Nations[].Leader)
    // unless the caller already supplied one via setDefaultName().
    if (defaultName_.empty() && chosenTribe_ >= 0 &&
        chosenTribe_ < int(MainCode::tribes().size())) {
        defaultName_ = MainCode::tribes()[std::size_t(chosenTribe_)].leader;
    }
}

FrontEndFlow::State FrontEndFlow::handleKey(int navKey) {
    MenuBoxDialog& mb = p.menuBoxDialog();
    switch (state_) {
        case State::TITLE: {
            // TITLE is just a logo+menu splash; the first key drops it and we
            // continue with the bare MAIN_MENU (the menu nav state is already
            // armed by enterTitle()/enterMainMenu()).
            if (navKey != MenuBoxDialog::KeyNone) enterMainMenu();
            break;
        }
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
                enterTribe();
            }
            break;
        }
        case State::TRIBE: {
            int r = mb.navStep(navKey);
            if (r == MenuBoxDialog::NavCancel) {
                enterDifficulty();           // ESC -> back to the difficulty menu.
            } else if (r >= 0) {
                chosenTribe_ = r;            // remember the chosen tribe.
                enterName();
            }
            break;
        }
        case State::NAME: {
            // STUB: there is no live keyboard edit loop (TextBoxDialogs' edit
            // path is itself stubbed in this port). ENTER accepts the default
            // name (tribe's leader, unless caller set one); ESC backs up.
            if (navKey == MenuBoxDialog::KeyEsc) {
                enterTribe();
            } else if (navKey == MenuBoxDialog::KeyEnter) {
                chosenName_ = defaultName_;
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
        case State::TITLE: {
            // Logo + main menu in one go. MainCode handles the LOGO.PIC load
            // (or skips it gracefully if there's no asset) and draws the
            // (Chinese) menu via MenuBoxDialog.
            fb.clear(1);
            p.mainCode().F0_11a8_0486_LogoAndMainGameMenu(mb.highlight, nullptr);
            break;
        }
        case State::MAIN_MENU:
        case State::DIFFICULTY:
        case State::TRIBE: {
            fb.clear(1);
            // Pre-select + highlight the currently navigated item.
            mb.defaultOptionIndex = mb.highlight;
            mb.forcedSelection = mb.highlight;
            if (state_ == State::MAIN_MENU) {
                mb.F0_2d05_0031_ShowMenuBox(mainMenuItems(), 30, 20,
                                            /*windowFrame*/ true, /*helpOption*/ false);
            } else if (state_ == State::DIFFICULTY) {
                mb.F0_2d05_0031_ShowMenuBox(difficultyItems(), 30, 20,
                                            /*windowFrame*/ true, /*helpOption*/ false);
            } else {
                mb.F0_2d05_0031_ShowMenuBox(tribeItems(), 30, 20,
                                            /*windowFrame*/ true, /*helpOption*/ false);
            }
            break;
        }
        case State::NAME: {
            fb.clear(1);
            // The C# F23_0000_00d6_PlayerNameDialog draws a small text-edit
            // box. The port reuses the (faithful) city-name-style box from
            // TextBoxDialogs as the render — the edit loop is itself stubbed.
            TextBoxDialogs& tb = p.textBoxDialogs();
            tb.forcedSelection = 1; // 1 = ENTER/accept in the C# editbox.
            // Show the default name (leader, unless explicitly overridden) as
            // the prompt content.
            std::string dn = defaultName_;
            if (dn.empty() && chosenTribe_ >= 0 &&
                chosenTribe_ < int(MainCode::tribes().size())) {
                dn = MainCode::tribes()[std::size_t(chosenTribe_)].leader;
            }
            tb.F23_0000_0000_CityNameDialog("Pick your tribe...", dn, 60, 80, 14);
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
