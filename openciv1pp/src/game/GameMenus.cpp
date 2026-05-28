#include "GameMenus.h"
#include "MenuBoxDialog.h"
#include "DrawTools.h"
#include <algorithm>

namespace oc1 {

GameMenus::GameMenus(OpenCiv1Game& parent) : p(parent) {}

int GameMenus::showMenuBox(const std::string& menuString, int x, int y) {
    // Delegate to the ported MenuBoxDialog: localized render + the
    // forcedSelection/navStep input stub. windowFrame=true, no help glyph
    // (matches every GameMenus call site, which pass windowFrame:true,
    // helpOption:false, emptyKeyboardAndMouse:false).
    return p.menuBoxDialog().F0_2d05_0031_ShowMenuBox(menuString, x, y,
                                                      /*windowFrame*/ true,
                                                      /*helpOption*/ false,
                                                      /*emptyKeyboardAndMouse*/ false);
}

void GameMenus::F0_2c84_0000_ShowTopMenu(int playerID, int unitID, int menuIndex) {
    Var_d4ca_MenuShortcutKey = -1;
    Var_654a = 0;
    lastDispatch = Dispatch::None;
    lastEncyclopediaTopic = -1;

    if (menuIndex == -1) {
        // C#: CheckValueRange(MouseXPos / 60, 0, 4). MouseXPos is not mirrored
        // here; default the derived index to 0 (the Game menu).
        // TODO(port): Var_db3c_MouseXPos / Segment_2dc4.CheckValueRange — mouse
        // subsystem not ported; clamp a (stubbed) 0 into [0,4].
        menuIndex = std::clamp(0, 0, 4);
    }

    // TODO(port): MainCode.HideMouse / the screen1->screen0 backdrop composite
    // (F0_VGA_07d8_DrawImage Var_19d4_Screen1 -> Var_aa_Screen0) + ShowMouse are
    // elided — screen 1 is not allocated in this slice; the menu draws onto the
    // current screen 0 directly.

    switch (menuIndex) {
        case 0: F0_2c84_00ad_GameMenu(); break;
        case 1: F0_2c84_01d8_OrdersMenu(playerID, unitID); break;
        case 2: F0_2c84_0615_AdvisorsMenu(); break;
        case 3: F0_2c84_06e4_ShowWorldMenu(); break;
        case 4: F0_2c84_07af_EncyclopediaMenu(); break;
        default: break;
    }

    // TODO(port): MainCode.HideMouse elided.

    if (Var_654a == 1) {
        // C#: Segment_1238.F0_1238_1b44() — encyclopedia re-entry / screen redraw.
        // TODO(port): Segment_1238 not ported; record the re-entry intent.
        lastDispatch = Dispatch::EncyclopediaReentry;
    }

    if (Var_654a == 0) {
        // TODO(port): restore the backdrop (screen1 -> screen0). screen 1 is not
        // allocated here, so this restore is elided.
    }

    // TODO(port): MainCode.ShowMouse elided.
}

void GameMenus::F0_2c84_00ad_GameMenu() {
    // C# gates the 'Save Game' option when TurnCount == 0 (sets a disabled mask
    // that is commented out in the C# too) — preserved as a no-op here.
    if (turnCount == 0) {
        // Disable 'Save Game' option (commented out in the C# as well).
    }

    std::string menu =
        " Tax Rate\n Luxuries Rate\n FindCity\n Options\n Save Game\n REVOLUTION!\n \n Retire\n QUIT to DOS\n";
    if ((spaceshipFlags & 0x100) != 0)
        menu += " View Replay\n";

    int selectedOption = showMenuBox(menu, 16, 8);

    // TODO(port): CheckPlayerTurn.F0_1403_4545_EmptyKeyboardAndMouse — input
    // subsystem not ported; elided.

    switch (selectedOption) {
        case 0: Var_d4ca_MenuShortcutKey = '='; break;   // Tax Rate
        case 1: Var_d4ca_MenuShortcutKey = '-'; break;   // Luxuries
        case 2: Var_d4ca_MenuShortcutKey = '?'; break;   // Find City
        case 3: {                                        // Options submenu
            int index;
            do {
                // Show current flags as checkmarks (Var_d7f2_MenuBoxCheckedOptions).
                p.menuBoxDialog().checkedOptions = uint32_t(uint16_t(gameSettingFlags));
                index = showMenuBox(
                    "Options:\n Instant Advice\n AutoSave\n End of Turn\n Animations\n Sound\n"
                    " Enemy Moves\n Encyclopedia Text\n Palace\n Debug saves\n",
                    24, 16);

                if (index == -1) break; // C#: continue then loop exits (no help stub)

                // TODO(port): Var_2f9c_MenuBoxHelpRequested not modelled (always
                // false here), so the flag toggle always applies.
                gameSettingFlags ^= int16_t(1 << index);
                p.menuBoxDialog().defaultOptionIndex = index; // Var_2f9a default option
            } while (index != -1);
            break;
        }
        case 4: Var_d4ca_MenuShortcutKey = 'S'; break;   // Save Game
        case 5: Var_d4ca_MenuShortcutKey = -2;  break;   // Revolution
        case 6: break;                                   // empty option line
        case 7:                                          // Retire
            // TODO(port): Var_dc48_GameEndType not modelled; only the shortcut key.
            Var_d4ca_MenuShortcutKey = 0x1000;
            break;
        case 8:                                          // Quit
            Var_d4ca_MenuShortcutKey = 0x1000;
            break;
        case 9:                                          // View replay
            // TODO(port): GameReplay.F9_0000_0000 not ported; record the dispatch.
            lastDispatch = Dispatch::ShowReplay;
            break;
        default: break;
    }
}

void GameMenus::F0_2c84_01d8_OrdersMenu(int playerID, int unitID) {
    // C# guards unitID in [0,128); the UnitContext mirror carries the same gate.
    if (!(unitID >= 0 && unitID < 128) || !orderContext.valid) {
        Var_d4ca_MenuShortcutKey = -1;
        return;
    }
    (void)playerID; // PlayerHasTechnology(playerID, ...) reads come via orderContext.

    const UnitContext& u = orderContext;
    std::string menu;
    std::vector<char> orders;
    orders.reserve(15);

    // All orders enabled by default (Var_b276_MenuBoxDisabledOptions = 0).
    uint32_t disabled = 0;

    // 0x8f is the hotkey-glyph marker the C# embeds before the shortcut letter.
    menu += " No Orders \x8f""space\n";
    orders.push_back(' ');

    if (u.isSettlers) {
        if (u.hasCity) menu += " Add to City \x8f""b\n";
        else           menu += " Found New City \x8f""b\n";
        orders.push_back('b');

        if (!u.hasRoad) {
            menu += " Build Road \x8f""r\n";
            orders.push_back('r');
        } else if (!u.hasRailRoad && u.hasRailTech) {
            menu += " Build RailRoad \x8f""r\n";
            orders.push_back('r');
        }

        if (!u.hasIrrigation) {
            // C# inspects TerrainModifications[type].IrrigationEffect. Not
            // mirrored in detail; model the common "Build Irrigation" case.
            // TODO(port): GameData.TerrainModifications / Terrains tables not
            // ported — the "Change to <terrain>" variant is not produced.
            menu += " Build Irrigation";
            if (!u.canIrrigate)
                disabled |= uint32_t(1) << orders.size(); // disable this option
            menu += " \x8f""i\n";
            orders.push_back('i');
        }

        if (!u.hasMines) {
            // TODO(port): MiningEffect / Terrains not ported — model "Build Mines".
            menu += " Build Mines \x8f""m\n";
            orders.push_back('m');
        }

        if (u.hasPollution) {
            menu += " Clean up Pollution \x8f""p\n";
            orders.push_back('p');
        }
    }

    if (u.isSettlers) {
        menu += " Build Fortress \x8f""f\n";
        if (!u.hasConstructionTech)
            disabled |= uint32_t(1) << orders.size(); // disable 'Build Fortress'
    } else if (u.isLand) {
        menu += " Fortify \x8f""f\n";
    }
    if (u.isLand) orders.push_back('f');

    menu += " Wait \x8f""w\n Sentry \x8f""s\n GoTo\n";
    orders.push_back('w');
    orders.push_back('s');
    orders.push_back('g');

    if (u.canPillage) {
        menu += " Pillage \x8f""P\n";
        orders.push_back('P');
    }
    if (u.hasCity) {
        menu += " Home City \x8f""h\n";
        orders.push_back('h');
    }
    if (u.canUnload) {
        menu += " Unload \x8f""u\n";
        orders.push_back('u');
    }

    menu += " \n Disband Unit \x8f""D\n";
    orders.push_back('\0'); // empty option line: no hotkey
    orders.push_back('D');

    p.menuBoxDialog().disabledOptions = disabled;
    int selectedOrder = showMenuBox(menu, 72, 8);

    if (selectedOrder < 0 || selectedOrder >= int(orders.size()))
        Var_d4ca_MenuShortcutKey = -1;
    else
        Var_d4ca_MenuShortcutKey = (unsigned char)orders[std::size_t(selectedOrder)];
}

void GameMenus::F0_2c84_0615_AdvisorsMenu() {
    int selectedOption = showMenuBox(
        " City Status (F1)\n Military Advisor (F2)\n Intelligence Advisor (F3)\n"
        " Attitude Advisor (F4)\n Trade Advisor (F5)\n Science Advisor (F6)\n",
        112, 8);

    // TODO(port): HideMouse / screen1->screen0 restore / ShowMouse elided.

    Var_654a = -1;

    switch (selectedOption) {
        case 0: lastDispatch = Dispatch::CityStatus; break;          // Overlay_14.CityStatus
        case 1: lastDispatch = debugFlag ? Dispatch::MilitaryStatus  // Overlay_13 (debug) / Overlay_14
                                         : Dispatch::MilitaryStatus; break;
        case 2: lastDispatch = Dispatch::IntelligenceReport; break;  // Overlay_14.IntelligenceReport
        case 3: lastDispatch = Dispatch::AttitudeSurvey; break;      // Overlay_14.AttitudeSurvey
        case 4: lastDispatch = Dispatch::TradeReport; break;         // Overlay_14.TradeReport
        case 5: lastDispatch = Dispatch::ScienceReport; break;       // Overlay_14.ScienceReport
        default: break;
        // TODO(port): the advisor overlays (Overlay_13/14) are not ported; each
        // selection is recorded as a Dispatch instead of entering the overlay.
    }
}

void GameMenus::F0_2c84_06e4_ShowWorldMenu() {
    uint32_t disabled = 0;
    if ((spaceshipFlags & 0xfe00) == 0)
        disabled = 0x20; // disable 'SpaceShips' option (Var_b276_MenuBoxDisabledOptions)
    p.menuBoxDialog().disabledOptions = disabled;

    int selectedOption = showMenuBox(
        " Wonders of the World (F7)\n Top 5 Cities (F8)\n Civilization Score (F9)\n"
        " World Map (F10)\n Demographics\n SpaceShips\n",
        144, 8);

    // TODO(port): HideMouse / screen1->screen0 restore / ShowMouse elided.

    Var_654a = -1;

    switch (selectedOption) {
        case 0: lastDispatch = Dispatch::WondersOfTheWorld; break;  // WorldMap popup
        case 1: lastDispatch = Dispatch::TopFiveCities; break;      // HallOfFame popup
        case 2: lastDispatch = Dispatch::CivilizationScore; break;  // Overlay_20 popup
        case 3: lastDispatch = Dispatch::WorldMap; break;           // WorldMap popup
        case 4: lastDispatch = Dispatch::Demographics; break;       // WorldMap popup
        case 5: lastDispatch = Dispatch::Spaceships; break;         // Overlay_18 dialog
        default: break;
        // TODO(port): WorldMap / HallOfFame / Overlay_18 / Overlay_20 not ported;
        // each selection is recorded as a Dispatch.
    }
}

void GameMenus::F0_2c84_07af_EncyclopediaMenu() {
    int selectedOption = showMenuBox(
        " Complete\n Technology Advances\n City Improvements\n"
        " Unit Types\n Terrain Types\n Miscellaneous\n",
        182, 8);

    if (selectedOption < 0) {
        Var_654a = 1;
    } else {
        // TODO(port): Encyclopedia.F8_0000_0000_ShowEncyclopediaByTopic not
        // ported; record the topic + dispatch.
        lastDispatch = Dispatch::Encyclopedia;
        lastEncyclopediaTopic = selectedOption;
        Var_654a = -1;
    }
}

} // namespace oc1
