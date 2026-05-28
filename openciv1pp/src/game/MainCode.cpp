#include "MainCode.h"
#include "ImageTools.h"
#include "MainIntro.h"
#include "MenuBoxDialog.h"
#include "TextBoxDialogs.h"
#include "CommonTools.h"
#include "../resource/PicLoader.h"
#include <filesystem>

namespace oc1 {

MainCode::MainCode(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {}

// The authentic Civ1 main game-type menu. Mirrors the C# menu string passed to
// F0_2d05_0031_ShowMenuBox in F0_11a8_0486_LogoAndMainGameMenu:
//   " Start a New Game\n Load a Saved Game\n EARTH\n Customize World\n
//     View Hall of Fame\n"
// (the trailing leading-space layout is recreated by MenuBoxDialog). These are
// English keys; DrawTools/Translator localize them to Chinese at draw time.
const std::vector<std::string>& MainCode::mainMenuItems() {
    static const std::vector<std::string> items = {
        "Start a New Game",
        "Load a Saved Game",
        "EARTH",
        "Customize World",
        "View Hall of Fame",
    };
    return items;
}

std::string MainCode::logoPath() const {
    // The C# MainIntro loads "logo.pic" (DS:0x328a) prefixed with ResourcePath
    // and uppercased. Mirror that: <resourcePath>/LOGO.PIC.
    std::filesystem::path dir(p.resourcePath());
    return (dir / "LOGO.PIC").string();
}

void MainCode::F0_11a8_0250_ShowMouse() {
    // C#: increments the hide-count; on the 1->1 transition calls the cursor-show
    // service. TODO(port): mouse cursor show is a no-op (CommonTools mouse
    // hardware not wired).
    ++mouseHideCount_;
}

void MainCode::F0_11a8_0268_HideMouse() {
    // C#: on count==1 calls the cursor-hide service, then decrements.
    // TODO(port): mouse cursor hide is a no-op.
    --mouseHideCount_;
}

int MainCode::F0_11a8_0486_LogoAndMainGameMenu(int forcedSelection, bool* logoLoaded) {
    bool loaded = false;

    // C#: Var_d76a_EarthMap = false; then the game-type selection loop. The
    // selection loop's per-case bodies (GenerateMap, Customize sub-menus, Hall
    // of Fame, Load dialog) are NOT ported — TODO(port): new-game/map internals.

    // Title background: load the real LOGO.PIC onto screen 0. In the original the
    // logo is brought up by MainIntro (DS:0x328a "logo.pic") before this menu;
    // we render it here so the boot screen shows the authentic title even with
    // MainIntro stubbed. Graceful fallback: if the asset is absent, leave the
    // existing (colored/blank) background and just draw the menu.
    std::error_code ec;
    const std::string path = logoPath();
    if (!p.resourcePath().empty() && std::filesystem::exists(path, ec)) {
        try {
            // screen 0, full-frame, palettePtr==1 -> also apply the logo palette.
            p.imageTools().F0_2fa1_01a2_LoadBitmapOrPalette(GDriver::MainScreen, 0, 0, path, 1);
            loaded = true;
        } catch (const std::exception&) {
            // Corrupt/undecodable asset: fall back to whatever background exists.
            loaded = false;
        }
    }
    if (logoLoaded) *logoLoaded = loaded;

    // Show the mouse cursor over the title (bookkeeping only).
    F0_11a8_0250_ShowMouse();

    // The boxed main menu on top of the logo. MenuBoxDialog routes every item
    // through DrawTools -> Translator, so the menu is Chinese for free. Position
    // (100, 140) matches the C# ShowMenuBox call; windowFrame=true draws the
    // double-shadow box; helpOption=false.
    MenuBoxDialog& mb = p.menuBoxDialog();
    mb.forcedSelection = forcedSelection;
    int selected = mb.F0_2d05_0031_ShowMenuBox(mainMenuItems(), 100, 140,
                                               /*windowFrame*/ true, /*helpOption*/ false);

    // TODO(port): the C# PlayTune(1,0) menu chime and the per-case game-type
    // bodies run here. STUB: sound + game-type dispatch elided.
    return selected;
}

void MainCode::F0_11a8_0008_Main() {
    // TODO(port): the full entry sequence — InitMouse, NewGameMenu, the GameTurn
    // loop, CloseSound/StopTimer — is not driven by the port. We do call the
    // (now-ported) MainIntro slideshow before the logo+menu screen-build so the
    // authentic intro plays first when DOS assets are available; without assets
    // mainIntro().play() is a safe no-op.
    p.mainIntro().play();
    F0_11a8_0486_LogoAndMainGameMenu();
}

// Faithful to the C# in F5_0000_0000_InitNewGameData (the menu string passed to
// ShowMenuBox):
//   "Difficulty Level...\n Chieftain (easiest)\n Warlord\n Prince\n King\n Emperor (toughest)\n"
// MenuBoxDialog wraps and localizes each entry; the first entry is the title
// row but for the abstract item list we expose only the 5 selectable levels.
const std::vector<std::string>& MainCode::difficultyItems() {
    static const std::vector<std::string> items = {
        "Chieftain (easiest)",
        "Warlord",
        "Prince",
        "King",
        "Emperor (toughest)",
    };
    return items;
}

// Faithful to GameData.nationTypes IDs 1..7 then 9..15 — the 14 playable tribes
// the C# tribe-list loop emits (ID 0 = Barbarians, ID 8 = empty placeholder).
const std::vector<MainCode::Nation>& MainCode::tribes() {
    static const std::vector<Nation> n = {
        { 1,  "Caesar",       "Roman",      "Romans"     },
        { 2,  "Hammurabi",    "Babylonian", "Babylonians"},
        { 3,  "Frederick",    "German",     "Germans"    },
        { 4,  "Ramesses",     "Egyptian",   "Egyptians"  },
        { 5,  "Abe Lincoln",  "American",   "Americans"  },
        { 6,  "Alexander",    "Greek",      "Greeks"     },
        { 7,  "M.Gandhi",     "Indian",     "Indians"    },
        { 9,  "Stalin",       "Russian",    "Russians"   },
        { 10, "Shaka",        "Zulu",       "Zulus"      },
        { 11, "Napoleon",     "French",     "French"     },
        { 12, "Montezuma",    "Aztec",      "Aztecs"     },
        { 13, "Mao Tse Tung", "Chinese",    "Chinese"    },
        { 14, "Elizabeth I",  "English",    "English"    },
        { 15, "Genghis Khan", "Mongol",     "Mongols"    },
    };
    return n;
}

bool MainCode::F0_11a8_087c_NewGameMenu(int forcedDifficulty, int forcedTribeIndex,
                                        const std::string& chosenName,
                                        int* outDifficulty, int* outTribeIndex,
                                        std::string* outName) {
    // C#: CommonTools.F0_1000_1697(0, 0, Array_d4ce[7]) sets the active palette
    // entry on entry. TODO(port): palette/cursor entry call elided.
    // C#: when GameData.TurnCount == 0, ImageTools.LoadBitmapOrPalette("diffs.pic",1)
    // paints the difficulty-selection backdrop. Try to honor it when the asset
    // is present; otherwise leave the existing background.
    std::error_code ec;
    std::filesystem::path diffs = std::filesystem::path(p.resourcePath()) / "DIFFS.PIC";
    if (!p.resourcePath().empty() && std::filesystem::exists(diffs, ec)) {
        try {
            p.imageTools().F0_2fa1_01a2_LoadBitmapOrPalette(GDriver::MainScreen, 0, 0, diffs.string(), 1);
        } catch (const std::exception&) { /* graceful */ }
    }
    // TODO(port): StartGameMenu.F5_0000_1af6_LoadGovernmentImage and the
    // GameData.TurnCount path (F5_0000_0000_InitNewGameData) are elided —
    // only the menu-building halves are ported below.

    // ---- DIFFICULTY: F5_0000_0000_InitNewGameData ShowMenuBox at (160, 35).
    // C# menu string:
    //   "Difficulty Level...\n Chieftain (easiest)\n Warlord\n Prince\n King\n Emperor (toughest)\n"
    // MenuBoxDialog draws the box, the title (first item) and the option rows.
    MenuBoxDialog& mb = p.menuBoxDialog();
    {
        std::vector<std::string> menu;
        menu.push_back("Difficulty Level..."); // title row (first item)
        for (const auto& it : difficultyItems()) menu.push_back(it);
        // INPUT-LOOP STUB: forcedDifficulty (0..4) drives the highlight and the
        // returned selection. Add 1 because the title row is item 0.
        mb.defaultOptionIndex = forcedDifficulty + 1;
        mb.forcedSelection = forcedDifficulty + 1;
        mb.F0_2d05_0031_ShowMenuBox(menu, 160, 35,
                                    /*windowFrame*/ true, /*helpOption*/ false);
    }
    int chosenDiff = forcedDifficulty;
    if (chosenDiff < 0) chosenDiff = 0;
    if (chosenDiff > 4) chosenDiff = 4;

    // TODO(port): the C# "Level of Competition" menu (number of AI opponents)
    // is elided — the port goes straight from difficulty to tribe with the
    // full 14-tribe list. The mouse hit-rect logic and FillRectangle column
    // cleanup between menus are likewise STUBs.

    // ---- TRIBE: F5_0000_0000_InitNewGameData ShowMenuBox at (160, 35).
    // C# tribe-list build loop emits Nations[i].Nationality for i in
    // [1..AIOpponentCount+1] then [9..9+AIOpponentCount+1]. The port shows all
    // 14 playable tribes regardless of opponent count.
    {
        std::vector<std::string> menu;
        menu.push_back("Pick your tribe..."); // title row (first item)
        for (const auto& t : tribes()) menu.push_back(t.nationality);
        mb.defaultOptionIndex = forcedTribeIndex + 1;
        mb.forcedSelection = forcedTribeIndex + 1;
        mb.F0_2d05_0031_ShowMenuBox(menu, 160, 35,
                                    /*windowFrame*/ true, /*helpOption*/ false);
    }
    int chosenTribe = forcedTribeIndex;
    if (chosenTribe < 0) chosenTribe = 0;
    if (chosenTribe >= int(tribes().size())) chosenTribe = int(tribes().size()) - 1;

    // ---- NAME: TextBoxDialogs.F23_0000_00d6_PlayerNameDialog (the C#
    // PlayerNameDialog). INPUT-LOOP STUB: defaults to the chosen tribe's
    // leader name (Augustus per the task brief is only used when there is no
    // tribe context); the live edit loop is the existing TextBoxDialogs stub.
    const std::string defName = chosenName.empty()
                                    ? tribes()[std::size_t(chosenTribe)].leader
                                    : chosenName;
    // Render the (single-line) name entry box faithfully via the city-name
    // dialog draw recipe (commented-out DOS path that PlayerNameDialog reused).
    p.textBoxDialogs().forcedSelection = 1; // 1 = ENTER/accept in the C# editbox
    p.textBoxDialogs().F23_0000_0000_CityNameDialog("Pick your tribe...",
                                                    defName, 80, 80, 14);

    if (outDifficulty) *outDifficulty = chosenDiff;
    if (outTribeIndex) *outTribeIndex = chosenTribe;
    if (outName)       *outName       = defName;
    return true;
}

} // namespace oc1
