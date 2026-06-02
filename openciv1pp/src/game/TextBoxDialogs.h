// TextBoxDialogs.h — ported CodeObject (OpenCiv1 TextBoxDialogs.cs).
//
// The Civ message / text pop-up boxes: the bordered box that shows a (possibly
// word-wrapped, multi-line) message with an optional title and option buttons.
// This is a key "display Chinese" UI surface — the title, the message body and
// every button label route through DrawTools/GDriver, so the box is localized
// for free.
//
// SOURCE NOTE: TextBoxDialogs.cs ships its real DOS drawing commented out (the
// live C# delegates name/find dialogs to WinForms EditBox/MessageBox). The
// commented-out original (F23_0000_0000_CityNameDialog / _00d6_PlayerNameDialog /
// _025b_FindCityDialog) reveals the box-draw recipe, reproduced faithfully here:
//   FillRectangle (background)  ->  MenuBoxDialog.F0_2d05_0a05_DrawRectangle
//   (border)  ->  DrawStringTools.F0_1182_005c_DrawStringToScreen0 (title).
// We keep the F23_* names so cross-referencing the disassembly stays mechanical,
// and add a generic word-wrapped message body (DrawTextBlock) + option buttons,
// which is the shape the engine's message pop-ups use.
//
// STUB (deviation from C#): the blocking input/edit loop (EditBox.ShowEditBox /
// MessageBox.Show / the original F23_0000_0414_EditBoxDialog kbhit/getch poll) is
// NOT ported. It is replaced by a settable `forcedSelection` field — exactly the
// MenuBoxDialog approach in this project — so the render is verifiable headlessly.
// Everything visual (box fill, border, title, word-wrapped message, buttons,
// the selected-button highlight) is the real drawing code.
//
// Unported-dependency stubs (kept minimal, each // TODO(port) in the .cpp):
//   - HideMouse/ShowMouse (MainCode.F0_11a8_*), the screen1 save/restore blit
//     (Graphics.F0_VGA_07d8_DrawImage), the cursor-blink/edit machinery
//     (Graphics.F0_VGA_009a_ReplaceColor, CommonTools.WaitTimer, CAPI.*) and the
//     city/player GameData lookups are elided. This port covers the render path.
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

class TextBoxDialogs {
public:
    explicit TextBoxDialogs(OpenCiv1Game& parent);

    // navStep() sentinels mirror MenuBoxDialog: a chosen button index (>=0),
    // cancel (ESC) = -1, or "no selection".
    enum { NavCancel = -1, NavNone = -2 };

    // Test/stub hook (mirrors MenuBoxDialog::forcedSelection): which button index
    // the (skipped) input loop would have landed on. -1 == no/ESC selection. The
    // matching button highlight is drawn and this value is returned.
    int forcedSelection = -1;

    // ---- the bordered-box draw recipe (faithful to the commented-out DOS C#) --

    // Draws the message box and returns the selected button index (driven by
    // forcedSelection). Lays out: a box sized to fit the (word-wrapped) message
    // and buttons, a filled background, a border, an optional title bar, the
    // wrapped message body, and the option buttons along the bottom. If `buttons`
    // is empty an "OK" button is implied. The text flows through DrawTools, so a
    // translatable title/message/button renders Chinese.
    int F23_0000_0000_ShowTextBox(const std::string& title, const std::string& message,
                                  const std::vector<std::string>& buttons,
                                  int x, int y, int width);

    // The original-shaped name-entry box: fill + border + title (no live edit).
    // Mirrors the commented-out F23_0000_0000_CityNameDialog box draw. `prompt`
    // is shown as the editable line content. Returns forcedSelection (1=accept,
    // 0=reject) as the C# editbox result.
    int F23_0000_0000_CityNameDialog(const std::string& title, const std::string& prompt,
                                     int x, int y, int maxTextLength);

    // Draws a 4-line rectangle border (delegates to MenuBoxDialog's, kept here
    // under the C# call-site name for parity with TextBoxDialogs.cs).
    void DrawBorder(int x, int y, int width, int height, uint16_t mode);

private:
    OpenCiv1Game& p;
    VCPU& cpu;
};

} // namespace oc1
