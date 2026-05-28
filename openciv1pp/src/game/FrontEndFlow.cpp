#include "FrontEndFlow.h"
#include "MainCode.h"
#include "MapManagement.h"
#include "MiniWorld.h"
#include "TerrainTiles.h"
#include "TextBoxDialogs.h"
#include "UnitManagement.h"
#include "TechResearch.h"
#include <algorithm>
#include <cstdint>
#include <utility>
#include <vector>

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

void FrontEndFlow::enterPlaying() {
    state_ = State::PLAYING;
    // Deterministic seed derived from chosen difficulty + chosen tribe + base.
    // Same picks -> same map; the user can also override via setWorldSeed().
    uint32_t seed = worldSeedOverride_;
    if (seed == 0) {
        uint32_t d = uint32_t(chosenDifficulty_ < 0 ? 0 : chosenDifficulty_);
        uint32_t t = uint32_t(chosenTribe_      < 0 ? 0 : chosenTribe_);
        seed = 0xC1110001u ^ (d * 0x9E3779B1u) ^ (t * 0x85EBCA77u);
    }
    // (Re)build the MiniWorld. We always rebuild on (re-)entry so a player who
    // ESCs out to MAIN_MENU and re-picks gets a fresh world for the new picks.
    miniWorld_ = std::make_unique<MiniWorld>(MapManagement::kWidth,
                                             MapManagement::kHeight, seed);
    miniWorld_->useRealGenerator(p.mapManagement(), seed);
    if (!assetDir_.empty()) miniWorld_->loadTileset(assetDir_);
    miniWorld_->attachGame(p);

    // Record the chosen tribe so the first city's name resolves to that tribe's
    // capital (e.g. tribe 0 -> "Rome", tribe 3 -> "Thebes"). When the tribe is
    // unset (-1) UnitManagement falls back to the generic "Capital" name.
    p.unitManagement().setChosenTribe(chosenTribe_);

    // ---- Multi-civ slice: spawn the human + 6 AI civs on distributed tiles.
    // Mirrors Civ1's default of 7 civilizations (1 human + 6 AI). Each AI civ
    // gets its own Settlers at a valid land tile with a min spacing of 10
    // (the C# `distance < 10` reject before tryCount relaxation). The placement
    // RNG is seeded from the same world seed (offset by a constant) so the
    // layout is reproducible for a given (difficulty, tribe) pick.
    constexpr int kNumAi = 6;
    p.unitManagement().setupCivs(chosenTribe_, kNumAi);
    // Wire the per-civ tech tree (Alphabet as the canonical first research
    // target for every civ). Mirrors the C# GameData init where every
    // Players[i].TechnologyAdvances[] starts empty.
    p.techResearch().initCivs(1 + kNumAi);
    std::vector<std::pair<int,int>> starts;
    placeStartingPositions(p.mapManagement(), 1 + kNumAi,
                           seed ^ 0xA17C1051u, starts, /*minDistance*/ 10);
    // Populate units(): index 0 = human Settlers, 1..6 = AI Settlers.
    p.unitManagement().unitsMut().clear();
    for (std::size_t i = 0; i < starts.size(); ++i) {
        p.unitManagement().addUnit(int(i), UnitType::Settlers,
                                   starts[i].first, starts[i].second);
    }

    // Find a valid starting tile near the centre: scan an outward ring for the
    // first Grassland/Plains hit; fall back to any non-Water/Arctic tile if no
    // grass/plains is found in the search radius. (Used when the multi-civ
    // placer above didn't produce a starts[0] — e.g. degenerate maps.)
    const int W = miniWorld_->width(), H = miniWorld_->height();
    int sx = W / 2, sy = H / 2;
    bool found = !starts.empty();
    if (found) { sx = starts[0].first; sy = starts[0].second; }
    int maxR = std::max(W, H);
    auto isPreferred = [&](int x, int y) {
        Terrain t = miniWorld_->terrainAt(x, y);
        return t == Terrain::Grassland || t == Terrain::Plains;
    };
    auto isAnyLand = [&](int x, int y) {
        Terrain t = miniWorld_->terrainAt(x, y);
        return t != Terrain::Water && t != Terrain::Arctic;
    };
    // Pass 1: prefer Grassland/Plains.
    for (int r = 0; r <= maxR && !found; ++r) {
        for (int dy = -r; dy <= r && !found; ++dy)
            for (int dx = -r; dx <= r && !found; ++dx) {
                int nx = W / 2 + dx, ny = H / 2 + dy;
                if (nx >= 0 && ny >= 0 && nx < W && ny < H && isPreferred(nx, ny)) {
                    sx = nx; sy = ny; found = true;
                }
            }
    }
    // Pass 2: any land if no grass/plains exists (unlikely on a Civ1 map).
    if (!found) {
        for (int r = 0; r <= maxR && !found; ++r) {
            for (int dy = -r; dy <= r && !found; ++dy)
                for (int dx = -r; dx <= r && !found; ++dx) {
                    int nx = W / 2 + dx, ny = H / 2 + dy;
                    if (nx >= 0 && ny >= 0 && nx < W && ny < H && isAnyLand(nx, ny)) {
                        sx = nx; sy = ny; found = true;
                    }
                }
        }
    }
    miniWorld_->setUnitPosition(sx, sy);
}

void FrontEndFlow::rebuildPlayingShell() {
    // The subset of enterPlaying() that creates the MiniWorld and links it
    // to the game host — WITHOUT spawning civs/units/cities (those are
    // restored verbatim by GameLoadAndSave). The world is regenerated with
    // the recorded seed so the underlying terrain matches; the loader then
    // overwrites individual tiles to restore the exact saved grid (covers
    // the case where seed -> map determinism ever drifts, and makes the
    // savefile self-contained).
    state_ = State::PLAYING;
    uint32_t seed = worldSeedOverride_;
    if (seed == 0) {
        uint32_t d = uint32_t(chosenDifficulty_ < 0 ? 0 : chosenDifficulty_);
        uint32_t t = uint32_t(chosenTribe_      < 0 ? 0 : chosenTribe_);
        seed = 0xC1110001u ^ (d * 0x9E3779B1u) ^ (t * 0x85EBCA77u);
    }
    miniWorld_ = std::make_unique<MiniWorld>(MapManagement::kWidth,
                                             MapManagement::kHeight, seed);
    miniWorld_->useRealGenerator(p.mapManagement(), seed);
    if (!assetDir_.empty()) miniWorld_->loadTileset(assetDir_);
    miniWorld_->attachGame(p);
    p.unitManagement().setChosenTribe(chosenTribe_);
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
            // The "starting game…" message box is up; any key transitions into
            // the live PLAYING state (the unified --game flow). Headless tests
            // that only need the legacy DONE terminal can use the
            // STARTING->DONE path from before via the inPlayingState() guard
            // (they simply don't read state_ past STARTING).
            if (navKey != MenuBoxDialog::KeyNone) enterPlaying();
            break;
        case State::PLAYING: {
            // Arrows move the unit (HOST applies bounds via MiniWorld::moveUnit).
            // Enter ends the turn; ESC backs out to MAIN_MENU (a new flow can
            // be started); other navKeys are no-ops here. The B / mouse paths
            // are handled by the integrated --game loop directly on MiniWorld;
            // this entry point keeps the headless test surface symmetric with
            // the earlier states (a single handleKey driver).
            if (!miniWorld_) break;
            switch (navKey) {
                case MenuBoxDialog::KeyUp:    miniWorld_->moveUnit(0, -1); break;
                case MenuBoxDialog::KeyDown:  miniWorld_->moveUnit(0,  1); break;
                case MenuBoxDialog::KeyEnter: miniWorld_->endTurn();       break;
                case MenuBoxDialog::KeyEsc:   enterMainMenu();             break;
                default: break;
            }
            break;
        }
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
            // 640x480 native: shift menus down + right so they sit comfortably
            // in the upper-left quadrant rather than hugging the corner. The
            // old (30, 20) on 320x200 maps to roughly (60, 40) on 640x480.
            if (state_ == State::MAIN_MENU) {
                mb.F0_2d05_0031_ShowMenuBox(mainMenuItems(), 60, 40,
                                            /*windowFrame*/ true, /*helpOption*/ false);
            } else if (state_ == State::DIFFICULTY) {
                mb.F0_2d05_0031_ShowMenuBox(difficultyItems(), 60, 40,
                                            /*windowFrame*/ true, /*helpOption*/ false);
            } else {
                mb.F0_2d05_0031_ShowMenuBox(tribeItems(), 60, 40,
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
            tb.F23_0000_0000_CityNameDialog("Pick your tribe...", dn, 200, 200, 14);
            break;
        }
        case State::STARTING: {
            fb.clear(1);
            // Placeholder "starting game…" box. Title reuses the "Quit" -> 離開
            // placeholder key as the task suggests; the body explains the slice.
            // Centered for the 640x480 fb.
            TextBoxDialogs& tb = p.textBoxDialogs();
            tb.forcedSelection = 0;
            tb.F23_0000_0000_ShowTextBox(
                /*title*/ "Quit",
                /*message*/ "Start a New Game",
                /*buttons*/ {}, 180, 200, 280);
            break;
        }
        case State::PLAYING: {
            // The live playable map. Delegates straight to MiniWorld's draw
            // (terrain + cities + unit + Chinese HUD). MiniWorld picks the tile
            // size from whether a real tileset is loaded (16 vs 12 px).
            if (miniWorld_) {
                int tileSize = miniWorld_->hasTileset() ? 16 : 12;
                miniWorld_->draw(p.graphics, p.var_aa.fontID, tileSize);
            }
            break;
        }
        case State::DONE:
        case State::QUIT:
            break; // nothing to draw for terminal states.
    }
}

} // namespace oc1
