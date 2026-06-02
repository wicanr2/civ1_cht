// MenuBoxDialog.h — ported CodeObject (OpenCiv1 MenuBoxDialog.cs).
//
// The pop-up menu surface: the boxed list used by the main menu ("Start a New
// Game / ... / Quit") and the in-game menus. This is the key "display Chinese
// menu" surface — every item routes through DrawTools/GDriver, so the menu is
// localized for free.
//
// What is ported faithfully (the F0_* names match the C# 1:1 for mechanical
// cross-referencing):
//   - F0_2d05_0031_ShowMenuBox        : layout + box/border/background/items +
//                                       highlighted selection.
//   - FillRectangleWithDoubleShadow   : filled box + double drop-shadow border.
//   - F0_2d05_0a05_DrawRectangle      : 4-line rectangle outline.
//   - F0_2d05_0a66_DrawShadowRectangle: light/dark shadow rectangle.
//   - F0_2d05_0ac9_GetNavigationKey   : key fetch (STUBBED — see below).
//
// STUB (deviation from C#): the blocking input loop in ShowMenuBox (mouse poll,
// kbhit/getch navigation, ReplaceColor highlight-tracking against live input) is
// NOT ported. Real SDL polling is skipped. Instead a settable `forcedSelection`
// drives which option ends up highlighted/returned, so the render is verifiable
// headlessly. The highlight that the C# input loop draws incrementally is drawn
// once here for `forcedSelection`. Everything else (the box, border, shadows,
// text) is the real drawing code.
//
// Unported-dependency stubs (kept minimal so this compiles standalone):
//   - the C# pattern-fill (Segment_2dc4 / Var_2f98_PatternAvailable), report
//     dialog images (Var_2f9e_MessageBoxStyle / DrawImage / DrawBitmapToScreen),
//     CheckPlayerTurn.EmptyKeyboardAndMouse, MeetWithKing, CommonTools.WaitTimer/
//     PlayTune, MainCode.UpdateMouseState, CAPI.kbhit/getch are all elided: this
//     port covers the windowFrame menu render path with no message-box style and
//     no help glyph image. See // TODO(port) markers in the .cpp.
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

class MenuBoxDialog {
public:
    explicit MenuBoxDialog(OpenCiv1Game& parent);

    // Navigation key constants for navStep() / SdlPresenter::pollKey(). These
    // are abstract codes (NOT the raw C# scancodes 0x4800/0x5000) so the SDL
    // backend and the headless test feed the same values.
    enum NavKey { KeyNone = 0, KeyUp = 1, KeyDown = 2, KeyEnter = 3, KeyEsc = 4 };

    // navStep() sentinels: ENTER returns the selected option index (>=0);
    // ESC/cancel returns NavCancel; any other key (or a move) returns NavNone.
    enum { NavCancel = -1, NavNone = -2 };

    // Pure, headless-testable navigation core. Updates `highlight` (the index
    // into the option list) per the C# main-menu input loop (clamp, skipping
    // disabled options) and returns:
    //   >=0       : the selected option index (ENTER on an enabled option)
    //   NavCancel : ESC / cancel
    //   NavNone   : still navigating (move or no-op)
    // Call setupNav() first to populate the option/disabled state for the menu
    // currently being shown.
    int navStep(int keycode);

    // Prepare navStep state: number of selectable options, the disabled mask,
    // and the initially highlighted option (clamped, advanced off a disabled
    // start). Returns the resolved starting highlight.
    int setupNav(int optionCount, uint32_t disabledMask, int startIndex);

    // The option index navStep currently highlights (also what the draw path
    // should highlight). -1 = none.
    int highlight = -1;

    // Test/stub hook: which option index the (skipped) input loop would have
    // landed on. -1 means "no selection" (mirrors ESC / out-of-range). The
    // matching highlight is drawn and this value is returned.
    int forcedSelection = -1;

    // Mirrors Var_2f9a_MenuBoxDefaultOptionIndex: pre-selected option, -1 = none.
    int defaultOptionIndex = -1;
    // Mirrors Var_b276_MenuBoxDisabledOptions / Var_d7f2_MenuBoxCheckedOptions.
    uint32_t disabledOptions = 0;
    uint32_t checkedOptions = 0;

    // Convenience: build a "\n"-joined menu string from items, then show it.
    int F0_2d05_0031_ShowMenuBox(const std::vector<std::string>& items, int x, int y,
                                 bool windowFrame, bool helpOption);

    // Shows the customized MenuBox. `menuString` is "\n"-separated lines.
    // Returns the selected option index (driven by forcedSelection here).
    int F0_2d05_0031_ShowMenuBox(const std::string& menuString, int x, int y,
                                 bool windowFrame, bool helpOption, bool emptyKeyboardAndMouse);

    // Fills the rectangle and draws a double shadow around it (the box border).
    void FillRectangleWithDoubleShadow(int x, int y, int width, int height, uint16_t mode);

    // Draws a rectangle (4 lines).
    void F0_2d05_0a05_DrawRectangle(int x, int y, int width, int height, uint16_t mode);

    // Draws a shadow rectangle (light on bottom/left, dark on top/right).
    void F0_2d05_0a66_DrawShadowRectangle(int x, int y, int width, int height,
                                          uint16_t mode, uint16_t mode1);

    // Get navigation key. STUB: no real SDL input; returns 0 ("no key").
    int F0_2d05_0ac9_GetNavigationKey();

    // ---- mouse hit-test / dispatch (headless-testable) -----------------------
    // The last drawn box's geometry is remembered after F0_2d05_0031_ShowMenuBox
    // so itemAt()/handleMouse() can be called without re-running the renderer.
    // Each "item rect" covers a single option line (matches the C# highlight bar
    // the input loop draws and the screen area you'd click to pick it).
    struct ItemRect { int x, y, w, h; int optionIndex; };

    // Returns the option index whose rect contains (fbX,fbY), or -1 if outside
    // the box or on a non-option (header) line.
    int itemAt(int fbX, int fbY) const;

    // Mouse event for dispatch. Mirrors SdlPresenter::MouseEvent but kept
    // independent so the game header has no SDL dep.
    struct MouseEvent { int x, y, button; bool down; bool motion; };

    // Hovering moves the highlight to the option under the cursor (motion or
    // any-button event). Left-click (button==1, down) on an option both sets
    // the highlight AND writes that index to *outSelection (caller treats it
    // as ENTER). Right-click (button==3, down) or any click OUTSIDE the box
    // writes -1 to *outSelection (caller treats it as ESC/cancel). Returns
    // true if outSelection was written (a selection or cancel happened); false
    // for plain hover/motion or button-up events.
    bool handleMouse(const MouseEvent& ev, int* outSelection);

    // The cached item rects from the last draw (one entry per selectable
    // option; rect is the highlight-bar area). Exposed for tests.
    const std::vector<ItemRect>& lastItemRects() const { return lastItemRects_; }

private:
    // navStep state (populated by setupNav).
    int navOptionCount_ = 0;
    uint32_t navDisabled_ = 0;

    // Move `idx` by `dir` (+1/-1) skipping disabled options, clamping at the
    // ends (matches the C# clamp behaviour). Returns the new index.
    int navMove(int idx, int dir) const;

    OpenCiv1Game& p;
    VCPU& cpu;

    // Cached layout from the last F0_2d05_0031_ShowMenuBox call (for hit-test).
    std::vector<ItemRect> lastItemRects_;
    int lastBoxX_ = 0, lastBoxY_ = 0, lastBoxW_ = 0, lastBoxH_ = 0;
};

} // namespace oc1
