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
#include "MiniWorld.h"
#include <cstdint>
#include <memory>
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
    enum class State { TITLE, MAIN_MENU, DIFFICULTY, TRIBE, NAME, STARTING, PLAYING, DONE, QUIT };

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
    // every frame. In PLAYING this delegates to the owned MiniWorld's draw().
    void draw();

    // ---- integrated --game flow (PLAYING state) ----
    // Returns true when the flow has entered the playable map (real Civ1 world
    // generated via MapManagement, MiniWorld attached, Settlers placed).
    bool inPlayingState() const { return state_ == State::PLAYING; }

    // The MiniWorld that backs the PLAYING state. Created lazily on the first
    // transition into PLAYING (so the headless flow tests can still exercise
    // the TITLE..STARTING transitions without paying the 80x50 generation cost).
    // nullptr until PLAYING has been entered at least once.
    MiniWorld* miniWorld() { return miniWorld_.get(); }
    const MiniWorld* miniWorld() const { return miniWorld_.get(); }

    // Optional DOS-asset directory for the playable map's real tileset. When
    // set BEFORE the first PLAYING transition, MiniWorld::loadTileset(dir) is
    // called so the map draws with the faithful TER257.PIC tiles.
    void setAssetDir(std::string dir) { assetDir_ = std::move(dir); }

    // Optional seed override for the world generator. When 0 (default) the
    // seed is derived deterministically from chosenDifficulty + chosenTribe +
    // a base (so the same picks always produce the same map).
    void setWorldSeed(uint32_t s) { worldSeedOverride_ = s; }

private:
    // (Re)arm the menu's navStep state for the screen we just entered.
    void enterMainMenu();
    void enterDifficulty();
    void enterTribe();
    void enterName();

    // Build the playable world: generate via MapManagement, install on
    // MiniWorld, attach the game, place the Settlers on a valid land tile
    // near the centre, record the chosen tribe for the capital name. Idempotent
    // — re-entering PLAYING (e.g. via ESC back to MAIN_MENU then re-start)
    // rebuilds it with the (potentially new) chosen tribe/difficulty.
    void enterPlaying();

    OpenCiv1Game& p;
    State state_ = State::MAIN_MENU;
    int chosenDifficulty_ = -1;
    int chosenTribe_      = -1;
    std::string chosenName_;
    std::string defaultName_;

    // Lazily-built playable map. Owned by FrontEndFlow so its lifetime ties to
    // the flow's PLAYING state. attachGame() is called on creation so the B-key
    // BuildCity action and endTurn() per-turn pass are wired through.
    std::unique_ptr<MiniWorld> miniWorld_;
    std::string assetDir_;
    uint32_t worldSeedOverride_ = 0;
};

} // namespace oc1
