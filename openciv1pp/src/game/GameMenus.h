// GameMenus.h — ported CodeObject (OpenCiv1 GameMenus.cs).
//
// The in-game top-menu system: the five menus reachable from the menu bar
// (Game / Orders / Advisors / World / Encyclopedia). Each method builds the
// menu's item list and shows it through the already-ported MenuBoxDialog, which
// renders every line through DrawTools/GDriver — so the menu is localized for
// free (the item strings are zh_TW.json keys). The F0_* method names match the
// C# 1:1 for mechanical cross-referencing.
//
// STUB (deviation from C#): blocking input is not modelled here either — the
// menu selection is driven by MenuBoxDialog's existing `forcedSelection` /
// `navStep` stub pattern (see MenuBoxDialog.h). ShowTopMenu picks the menu, the
// per-menu method shows the menu box and reads its (forced) selection, then sets
// Var_d4ca_MenuShortcutKey / dispatches.
//
// Unported-dependency stubs (kept minimal so this compiles standalone). The C#
// reaches into many subsystems that are not ported yet; those reads/dispatches
// are replaced with safe local mirror fields + recorded "last dispatch" hooks:
//   - GameData (TurnCount, SpaceshipFlags, GameSettingFlags, HumanPlayerID, the
//     Players[].Units[] / Terrains / TerrainModifications tables)        -> mirror fields
//   - MapManagement (visible terrain improvements / terrain type)         -> mirror fields
//   - Segment_1ade.PlayerHasTechnology / CheckPlayerTurn.CanIrrigateCell  -> stub predicates
//   - MainCode.Hide/ShowMouse, Graphics screen1<->screen0 composite       -> best-effort / elided
//   - Segment_1238 (encyclopedia re-entry), GameReplay, and every advisor/
//     world/encyclopedia overlay entry point                             -> recorded as lastDispatch
// All such points carry a // TODO(port): marker in the .cpp.
#pragma once
#include "OpenCiv1Game.h"
#include <cstdint>
#include <string>
#include <vector>

namespace oc1 {

class GameMenus {
public:
    explicit GameMenus(OpenCiv1Game& parent);

    // Identifies which unported subsystem a menu selection dispatched into, so a
    // headless test can verify the dispatch without the subsystem being ported.
    enum class Dispatch {
        None,
        // Game menu
        ShowReplay,
        // Advisors menu (Overlay_13/14)
        CityStatus, MilitaryStatus, IntelligenceReport, AttitudeSurvey,
        TradeReport, ScienceReport,
        // World menu (WorldMap / HallOfFame / Overlay_18/20)
        WondersOfTheWorld, TopFiveCities, CivilizationScore, WorldMap,
        Demographics, Spaceships,
        // Encyclopedia menu (Encyclopedia / Segment_1238)
        Encyclopedia, EncyclopediaReentry,
    };

    // ---- public state (mirrors the C# public/local vars) -------------------
    // The shortcut key the selected menu option maps to (the C# pushes this into
    // the keyboard queue). Special values: -1 none, -2 revolution, 0x1000 end.
    int Var_d4ca_MenuShortcutKey = 0;

    // Records the last subsystem dispatch made by an advisor/world/encyclopedia
    // selection (since those subsystems are not ported). Plus the topic argument
    // for Encyclopedia. Reset to None at the start of each ShowTopMenu.
    Dispatch lastDispatch = Dispatch::None;
    int lastEncyclopediaTopic = -1;

    // ---- ported menu entry points (F0_* names match the C#) ----------------

    // Shows and handles one of the five top menus. menuIndex 0..4 selects the
    // menu directly; -1 derives it from the mouse X (mouseXPos / 60, clamped).
    void F0_2c84_0000_ShowTopMenu(int playerID, int unitID, int menuIndex);

    // The individual menus. Public so the test can exercise each draw/build path
    // directly (the C# keeps these private; widened here for headless testing).
    void F0_2c84_00ad_GameMenu();
    void F0_2c84_01d8_OrdersMenu(int playerID, int unitID);
    void F0_2c84_0615_AdvisorsMenu();
    void F0_2c84_06e4_ShowWorldMenu();
    void F0_2c84_07af_EncyclopediaMenu();

    // ---- stubbed game-state mirrors (see header note) ----------------------
    // GameData mirrors used by the menu-building logic.
    int turnCount = 0;                 // GameData.TurnCount (gates Save Game)
    uint16_t spaceshipFlags = 0;       // GameData.SpaceshipFlags (gates Replay/Spaceships)
    int16_t gameSettingFlags = 0;      // GameData.GameSettingFlags (Options toggles)
    int humanPlayerID = 0;             // GameData.HumanPlayerID
    bool debugFlag = false;            // Var_d806_DebugFlag (military advisor branch)

    // Orders-menu unit context (a minimal mirror of the C# Unit + terrain reads).
    // Defaults describe a Settlers unit on plain terrain so OrdersMenu builds a
    // representative item list. Real values come from the (unported) GameData.
    struct UnitContext {
        bool valid = false;            // unitID in [0,128)
        bool isSettlers = false;       // TypeID == Settlers
        bool isLand = true;            // MovementType == Land (Fortify)
        bool hasCity = false;          // City improvement present at the cell
        bool hasRoad = false;          // Road present
        bool hasRailRoad = false;      // RailRoad present
        bool hasIrrigation = false;    // Irrigation present
        bool hasMines = false;         // Mines present
        bool hasPollution = false;     // Pollution present
        bool canPillage = false;       // PillageMask set + non-diplomat/fighter
        bool canUnload = false;        // sea transport / carrier with cargo
        bool hasRailTech = false;      // PlayerHasTechnology(Railroad)
        bool hasConstructionTech = false; // PlayerHasTechnology(Construction)
        bool canIrrigate = true;       // CanIrrigateCell
    };
    UnitContext orderContext;          // populated by the caller / test

private:
    OpenCiv1Game& p;

    // Var_654a in the C#: post-menu cleanup selector (0 restore screen, 1 redraw
    // via encyclopedia re-entry, -1 leave as the advisor/world overlay left it).
    int16_t Var_654a = 0;

    // Show a menu box through the ported MenuBoxDialog (the localized render +
    // forcedSelection/navStep stub). Returns the selected option index.
    int showMenuBox(const std::string& menuString, int x, int y);
};

} // namespace oc1
