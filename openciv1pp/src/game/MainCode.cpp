#include "MainCode.h"
#include "ImageTools.h"
#include "MenuBoxDialog.h"
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
    // TODO(port): the full entry sequence — MainIntro animation, InitMouse,
    // NewGameMenu, the GameTurn loop, CloseSound/StopTimer — is not driven by
    // the port. We expose only the boot-screen build below so a caller can show
    // the authentic title interactively.
    F0_11a8_0486_LogoAndMainGameMenu();
}

} // namespace oc1
