#include "TextBoxDialogs.h"
#include "DrawTools.h"
#include "MenuBoxDialog.h"
#include "CommonTools.h"
#include <algorithm>

namespace oc1 {

TextBoxDialogs::TextBoxDialogs(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {}

void TextBoxDialogs::DrawBorder(int x, int y, int width, int height, uint16_t mode) {
    // Faithful to the commented-out C#: the box border is drawn by
    // MenuBoxDialog.F0_2d05_0a05_DrawRectangle (a 4-line rectangle outline).
    p.menuBoxDialog().F0_2d05_0a05_DrawRectangle(x, y, width, height, mode);
}

int TextBoxDialogs::F23_0000_0000_CityNameDialog(const std::string& title,
                                                 const std::string& prompt,
                                                 int x, int y, int maxTextLength) {
    // TODO(port): MainCode.F0_11a8_0268_HideMouse / _0250_ShowMouse — mouse
    // subsystem not ported; elided.
    // TODO(port): Graphics.F0_VGA_07d8_DrawImage screen0<->screen1 save/restore
    // blit — elided (no backing-store screen here).

    // Box geometry mirrors the C# city-name dialog (FillRectangle(80,80,160,32)
    // + DrawRectangle border + title at +8,+2), generalised to (x,y).
    const int boxW = std::max(maxTextLength * 8 + 8, 160);
    const int boxH = 32;

    // Background fill (C#: CommonTools.F0_1000_0bfa_FillRectangle ... colour 15).
    p.commonTools().F0_1000_0bfa_FillRectangle(p.var_aa, x, y, boxW, boxH, 15);
    // Border (C#: MenuBoxDialog.F0_2d05_0a05_DrawRectangle ... colour 11).
    DrawBorder(x, y, boxW, boxH, 11);
    // Title (C#: DrawStringTools.F0_1182_005c_DrawStringToScreen0(title, x+8, y+2, 0)).
    p.drawTools().F0_1182_005c_DrawStringToScreen0(title, x + 8, y + 2, 0);

    // The editable line content — drawn as static text (the live edit loop, the
    // F23_0000_0414_EditBoxDialog kbhit/getch poll, is the STUB).
    // TODO(port): F23_0000_0414_EditBoxDialog blink/edit machinery
    // (Graphics.F0_VGA_009a_ReplaceColor, CommonTools.WaitTimer, CAPI.kbhit/getch).
    p.commonTools().F0_1000_0bfa_FillRectangle(p.var_aa, x + 8, y + 16, 7 * maxTextLength, 11, 15);
    DrawBorder(x + 8, y + 15, maxTextLength * 8 + 8, 13, 0);
    p.drawTools().F0_1182_005c_DrawStringToScreen0(prompt, x + 10, y + 16, 0);

    // INPUT-LOOP STUB: the blocking edit loop is skipped; forcedSelection drives
    // the result (1 = ENTER/accept, 0 = ESC/reject), mirroring the C# return.
    return (forcedSelection != 0) ? 1 : 0;
}

int TextBoxDialogs::F23_0000_0000_ShowTextBox(const std::string& title,
                                              const std::string& message,
                                              const std::vector<std::string>& buttons,
                                              int x, int y, int width) {
    // TODO(port): MainCode HideMouse/ShowMouse + screen1 save/restore blit elided.

    const int screenW = p.graphics.screen(p.var_aa.screenID).width();
    const int screenH = p.graphics.screen(p.var_aa.screenID).height();

    // Implied "OK" button when none are supplied (the engine's plain message box).
    std::vector<std::string> btns = buttons;
    if (btns.empty()) btns.push_back("OK");

    const int pad = 4;          // inner padding inside the border
    const int titleH = title.empty() ? 0
                                      : p.graphics.font(p.var_aa.fontID).pixelHeight + 2;
    const int buttonH = p.graphics.font(p.var_aa.fontID).pixelHeight + 2;

    // Content area width available to the word-wrapped message (inside padding).
    int contentW = std::max(width - 2 * pad, 16);

    // Measure the wrapped message height without drawing (DrawTools::GetTextBlockHeight).
    int msgH = message.empty()
                   ? 0
                   : p.drawTools().GetTextBlockHeight(message, 0, contentW);

    // Total box height: padding + title + message + gap + a button row.
    int boxH = pad + titleH + msgH + (msgH ? pad : 0) + buttonH + pad;
    int boxW = width;

    // Clamp the box to the screen so the border/fill stay on-screen.
    if (x + boxW > screenW) x = std::max(0, screenW - boxW);
    if (y + boxH > screenH) y = std::max(0, screenH - boxH);

    // --- the box: fill + border (faithful recipe from the commented-out C#) ---
    p.commonTools().F0_1000_0bfa_FillRectangle(p.var_aa, x, y, boxW, boxH, 15);
    DrawBorder(x, y, boxW, boxH, 11);

    int cursorY = y + pad;

    // Title (drawn in colour 0, like the C# dialog titles).
    if (!title.empty()) {
        p.drawTools().F0_1182_005c_DrawStringToScreen0(title, x + pad, cursorY, 0);
        cursorY += titleH;
    }

    // Word-wrapped, translatable message body (DrawTextBlock both wraps AND
    // translates — the recommended path for localized multi-line text).
    if (!message.empty()) {
        p.drawTools().DrawTextBlock(x + pad, cursorY, message, contentW, 0);
        cursorY += msgH + pad;
    }

    // --- option buttons laid out left-to-right along the bottom row ----------
    const int buttonY = y + boxH - buttonH - pad + 1;
    int buttonX = x + pad;
    std::vector<Rect> buttonRects;
    buttonRects.reserve(btns.size());
    const int gap = 8;

    for (const std::string& label : btns) {
        int lw = p.graphics.getDrawStringSize(p.var_aa.fontID, label).w;
        Rect r{buttonX, buttonY - 1, lw + 6, buttonH};
        // Button frame + label.
        DrawBorder(r.x, r.y, r.w, r.h, 0);
        p.drawTools().F0_1182_005c_DrawStringToScreen0(label, buttonX + 3, buttonY, 0);
        buttonRects.push_back(r);
        buttonX += r.w + gap;
    }

    // --- INPUT-LOOP STUB -----------------------------------------------------
    // The blocking input poll (EditBox/MessageBox / kbhit/getch) is skipped.
    // forcedSelection drives which button is chosen; the matching button gets
    // the highlight the loop would have drawn (background 15 -> 11).
    int selected = forcedSelection;
    if (selected < 0 || selected >= int(btns.size()))
        selected = (forcedSelection == NavCancel) ? NavCancel : 0; // default: first button

    if (selected >= 0 && selected < int(buttonRects.size())) {
        const Rect& r = buttonRects[std::size_t(selected)];
        // Highlight the chosen button's interior (swap the fill colour 15 -> 11),
        // mirroring the C# ReplaceColor highlight technique.
        p.graphics.replaceColor(p.var_aa.screenID,
                                Rect{r.x + 1, r.y + 1, r.w - 1, r.h - 1}, 15, 11);
    }

    // TODO(port): MainCode.F0_11a8_0250_ShowMouse — elided.
    return selected;
}

} // namespace oc1
