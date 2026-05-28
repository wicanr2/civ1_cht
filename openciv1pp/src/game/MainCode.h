// MainCode.h — ported CodeObject (OpenCiv1 MainCode.cs).
//
// The boot-screen path of the original game's entry point. In the C# this is
// the segment-0x11a8 code that runs the intro, then shows the title logo with
// the main game-type menu ("Start a New Game / Load / EARTH / Customize / Hall
// of Fame"), then drops into the new-game setup and the turn loop.
//
// This port covers ONLY the screen-building of the boot path: render the
// authentic Civ1 title — the real LOGO.PIC background on screen 0 — then draw
// the (already-ported, Chinese) main menu via MenuBoxDialog on top. The
// F0_* method names mirror the C# 1:1 for mechanical cross-referencing.
//
// Faithfully ported:
//   - F0_11a8_0486_LogoAndMainGameMenu : load the title logo (logo.pic, the same
//     asset the C# MainIntro loads at DS:0x328a) onto screen 0 via ImageTools,
//     then show the boxed main menu via MenuBoxDialog (localized for free).
//     When the asset is absent it skips the logo (colored background) and still
//     draws the menu — graceful fallback for headless/no-asset runs.
//   - F0_11a8_0250_ShowMouse / F0_11a8_0268_HideMouse : the mouse hide-count
//     bookkeeping (the actual show/hide hardware call is stubbed — see below).
//
// STUBS (minimal safe no-ops; see // TODO(port) markers in the .cpp):
//   - F0_11a8_0008_Main          : the full entry sequence (intro + new-game +
//                                  turn loop) is NOT driven here; only the logo+
//                                  menu screen-build is exercised by the port.
//   - MainIntro (F2_0000_0000)   : the intro animation (logo zoom / planet) is a
//                                  no-op.
//   - music / sound / timer      : InitSound/PlayTune/StopTimer are no-ops.
//   - mouse show/hide hardware   : the CommonTools mouse cursor calls are no-ops
//                                  (only the hide-count is tracked).
//   - new-game / map internals   : GenerateMap, NewGameMenu, the game-type
//                                  switch bodies (cases 0..4) are NOT ported.
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

class MainCode {
public:
    explicit MainCode(OpenCiv1Game& parent);

    // The C# entry point. STUB: the port does not drive the whole sequence; it
    // forwards to the logo+menu screen-build so a caller can show the title.
    void F0_11a8_0008_Main();

    // Mouse show/hide hide-count bookkeeping (cursor hardware call stubbed).
    void F0_11a8_0250_ShowMouse();
    void F0_11a8_0268_HideMouse();

    // The boot-screen build: render the real LOGO.PIC background onto screen 0,
    // then draw the Chinese main menu via MenuBoxDialog over it.
    //
    // `forcedSelection` drives the (stubbed) MenuBoxDialog input loop so the
    // render is verifiable headlessly and so an interactive caller can show a
    // particular highlight. Returns the selected game-type index (or -1).
    // `logoLoaded` (out, optional) reports whether the logo asset contributed.
    int F0_11a8_0486_LogoAndMainGameMenu(int forcedSelection = 0, bool* logoLoaded = nullptr);

    // The authentic main-menu items (English keys; localized at draw time).
    static const std::vector<std::string>& mainMenuItems();

    // Resolve the title logo path against the game's resourcePath(). Returns the
    // full path whether or not the file exists (caller checks existence).
    std::string logoPath() const;

private:
    OpenCiv1Game& p;
    VCPU& cpu;

    // Mirrors Var_deea_MouseHideCount (the C# mouse show/hide counter).
    int mouseHideCount_ = 0;
};

} // namespace oc1
