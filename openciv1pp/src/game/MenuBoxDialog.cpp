#include "MenuBoxDialog.h"
#include "DrawTools.h"
#include <algorithm>

namespace oc1 {

namespace {
// TrimEnd + split on '\n' dropping empty entries (mirrors C#
// menuString.TrimEnd().Split("\n", RemoveEmptyEntries)).
std::vector<std::string> splitMenuItems(const std::string& s) {
    // TrimEnd
    std::size_t end = s.size();
    while (end > 0 && (s[end - 1] == ' ' || s[end - 1] == '\n' ||
                       s[end - 1] == '\r' || s[end - 1] == '\t'))
        --end;
    std::vector<std::string> out;
    std::string cur;
    for (std::size_t i = 0; i < end; ++i) {
        if (s[i] == '\n') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(s[i]);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}
} // namespace

MenuBoxDialog::MenuBoxDialog(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {}

int MenuBoxDialog::F0_2d05_0031_ShowMenuBox(const std::vector<std::string>& items, int x, int y,
                                            bool windowFrame, bool helpOption) {
    std::string joined;
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (i) joined += '\n';
        joined += items[i];
    }
    return F0_2d05_0031_ShowMenuBox(joined, x, y, windowFrame, helpOption, false);
}

int MenuBoxDialog::F0_2d05_0031_ShowMenuBox(const std::string& menuString, int x, int y,
                                            bool windowFrame, bool helpOption,
                                            bool emptyKeyboardAndMouse) {
    (void)emptyKeyboardAndMouse; // TODO(port): CheckPlayerTurn.EmptyKeyboardAndMouse — input subsystem, stubbed.

    // TODO(port): Var_2f9e_MessageBoxStyle report-prefix and dialog images
    // (DrawImage / DrawBitmapToScreen) are elided; this port draws the plain
    // windowFrame menu with no message-box style. menuString is used as-is.

    std::vector<std::string> menuItems = splitMenuItems(menuString);

    const int screenW = p.graphics.screen(p.var_aa.screenID).width();
    const int screenH = p.graphics.screen(p.var_aa.screenID).height();

    // Determine line height from the active font (C#: F0_VGA_11ae_GetTextHeight).
    int lineHeight = p.graphics.font(p.var_aa.fontID).pixelHeight;
    if (lineHeight == 9) lineHeight--; // C# special-cases the 9px font to 8.

    std::vector<int> optionIndexes; // line index of each selectable option
    std::vector<char> optionChars;  // keyboard shortcut chars (parity only)
    int selectedOptionIndex = 0;

    int maxContentWidth = screenW - (windowFrame ? 8 : 0) - x;
    int maxContentHeight = screenH - (windowFrame ? 8 : 0) - y;
    int maxLineWidth = 0;
    int maxLineCount = (maxContentHeight - (helpOption && windowFrame ? 8 : 0)) / lineHeight;

    // Limit number of lines to the available window height.
    if (int(menuItems.size()) > maxLineCount && maxLineCount >= 0)
        menuItems.resize(std::size_t(maxLineCount));

    // Detect the default "checked" leading char (space or underscore, not '^').
    char defaultCheckedChar = '\0';
    for (const std::string& mi : menuItems) {
        if (!mi.empty() && (mi[0] == ' ' || mi[0] == '_')) { defaultCheckedChar = mi[0]; break; }
    }

    for (std::size_t i = 0; i < menuItems.size(); ++i) {
        std::string menuItem = menuItems[i];
        int itemWidth = p.graphics.getDrawStringSize(p.var_aa.fontID, menuItem).w;

        // Trim items wider than the content area, appending "..." (C# loop).
        while (itemWidth > maxContentWidth && !menuItem.empty()) {
            menuItem = menuItem.substr(0, menuItem.size() - 1);
            itemWidth = p.graphics.getDrawStringSize(p.var_aa.fontID, menuItem + "...").w;
            if (itemWidth <= maxContentWidth) {
                menuItem += "...";
                menuItems[i] = menuItem;
            }
        }

        if (itemWidth > maxLineWidth) maxLineWidth = itemWidth;

        // Detect selectable options (lines starting with the checked char).
        if (defaultCheckedChar != '\0' && !menuItem.empty() && menuItem[0] == defaultCheckedChar) {
            optionIndexes.push_back(int(i));
            if ((checkedOptions & (uint32_t(1) << (optionIndexes.size() - 1))) != 0)
                menuItems[i] = '^' + menuItem.substr(1);
            if (menuItem.size() > 1) optionChars.push_back(menuItem[1]);
        }
    }

    // If no leading-char convention was used, treat every line as a selectable
    // option (this is the main-menu case where items are plain strings).
    bool plainItemsAreOptions = optionIndexes.empty();
    if (plainItemsAreOptions) {
        for (std::size_t i = 0; i < menuItems.size(); ++i) optionIndexes.push_back(int(i));
    }

    int contentLeft = x + (windowFrame ? 4 : 0);
    int contentTop = y + (windowFrame ? 4 : 0);
    int contentWidth = maxLineWidth;
    int contentHeight = std::max(int(menuItems.size()) * lineHeight +
                                     (helpOption && windowFrame ? 8 : 0), 0);

    int windowWidth = contentWidth + (windowFrame ? 8 : 0);
    int windowHeight = contentHeight + (windowFrame ? 8 : 0);

    // Adjust default selected option (if pre-selected).
    if (defaultOptionIndex != -1 && !optionIndexes.empty())
        selectedOptionIndex = std::min(int(optionIndexes.size()) - 1, defaultOptionIndex);

    // Determine colors (C# logic, minus the on-screen-background special case).
    int textColor;
    int backgroundColor = 7;
    int highlightColor = 22;
    if (y == 139) highlightColor = 8;
    if (!windowFrame && y != 139) {
        // background follows the existing screen color under (x,y).
        backgroundColor = p.graphics.getPixel(p.var_aa.screenID, x, y);
        highlightColor = -1;
    }
    textColor = (backgroundColor == 15) ? 3 : 15;

    if (windowFrame) {
        // TODO(port): pattern-fill (Segment_2dc4 / Var_2f98_PatternAvailable)
        // elided — uses a plain filled rectangle.
        FillRectangleWithDoubleShadow(x, y, windowWidth, windowHeight, 7);
        // TODO(port): Var_2f9e_MessageBoxStyle report image elided.
    }

    if (windowFrame && helpOption) {
        // TODO(port): C# switches to font 2 for "(HELP AVAILABLE)"; we keep the
        // active font (font 2 is not registered in this port).
        std::string helpText = "(HELP AVAILABLE)";
        Size helpSize = p.graphics.getDrawStringSize(p.var_aa.fontID, helpText);
        p.drawTools().F0_1182_005c_DrawStringToScreen0(
            helpText, contentLeft + contentWidth - helpSize.w,
            contentTop + contentHeight - helpSize.h, 10);
    }

    p.var_aa.frontColor = uint8_t(textColor);

    // Draw the menu items.
    int optionIndex = -1;
    for (std::size_t i = 0; i < menuItems.size(); ++i) {
        const std::string& menuItem = menuItems[i];

        if (!optionIndexes.empty()) {
            if (optionIndex == -1 && int(i) == optionIndexes[0]) {
                optionIndex = 0;
            } else if (optionIndex != -1) {
                if (optionIndex < int(optionIndexes.size())) optionIndex++;
            }
        }

        if (optionIndex == -1) {
            if (lineHeight > 9)
                p.drawTools().F0_1182_0086_DrawStringWithShadowToScreen0(
                    menuItem, contentLeft, contentTop + int(i) * lineHeight, uint8_t(textColor));
            else
                p.drawTools().F0_1182_005c_DrawStringToScreen0(
                    menuItem, contentLeft, contentTop + int(i) * lineHeight, uint8_t(textColor));
        } else {
            if ((disabledOptions & (uint32_t(1) << optionIndex)) != 0)
                p.drawTools().F0_1182_005c_DrawStringToScreen0(
                    menuItem, contentLeft, contentTop + int(i) * lineHeight, 4);
            else
                p.drawTools().F0_1182_005c_DrawStringToScreen0(
                    menuItem, contentLeft, contentTop + int(i) * lineHeight, 0);
        }
    }

    // Cache the box geometry + per-option rects for hit-test (itemAt / handleMouse).
    // Rect math mirrors the highlight bar drawn below: same x/y/w/h formula.
    lastBoxX_ = x;            lastBoxY_ = y;
    lastBoxW_ = windowWidth;  lastBoxH_ = windowHeight;
    lastItemRects_.clear();
    lastItemRects_.reserve(optionIndexes.size());
    for (std::size_t oi = 0; oi < optionIndexes.size(); ++oi) {
        int li = optionIndexes[oi];
        lastItemRects_.push_back(ItemRect{
            contentLeft, contentTop + li * lineHeight - 1,
            maxLineWidth, lineHeight, int(oi)});
    }

    // ---- INPUT LOOP STUB ----------------------------------------------------
    // The C# blocking poll loop (mouse + kbhit/getch navigation) is skipped.
    // We honour `defaultOptionIndex` then override with `forcedSelection` so a
    // test can drive the result, and draw the single highlight the loop would
    // have produced for that option (the ReplaceColor pair from the C#).
    if (forcedSelection != -1) selectedOptionIndex = forcedSelection;
    if (selectedOptionIndex < 0 || selectedOptionIndex >= int(optionIndexes.size()))
        selectedOptionIndex = -1;

    if (selectedOptionIndex != -1) {
        int lineIndex = optionIndexes[selectedOptionIndex];
        Rect hl{contentLeft, contentTop + lineIndex * lineHeight - 1, maxLineWidth, lineHeight};
        // C#: ReplaceColor(background->11) then (highlight->3) over the line.
        p.graphics.replaceColor(p.var_aa.screenID, hl, uint8_t(backgroundColor), 11);
        if (highlightColor != -1)
            p.graphics.replaceColor(p.var_aa.screenID, hl, uint8_t(highlightColor), 3);
    }

    // Reset the per-call option state (mirrors the C# tail).
    defaultOptionIndex = -1;
    disabledOptions = 0;
    checkedOptions = 0;

    return selectedOptionIndex;
}

void MenuBoxDialog::FillRectangleWithDoubleShadow(int x, int y, int width, int height, uint16_t mode) {
    // TODO(port): pattern-fill path (Segment_2dc4 / Var_2f98_PatternAvailable)
    // elided — always uses a plain filled rectangle (CommonTools.FillRectangle).
    p.graphics.fillRectangle(p.var_aa.screenID, Rect{x, y, width, height}, uint8_t(mode));

    p.var_aa.backColor = uint8_t(mode);

    F0_2d05_0a66_DrawShadowRectangle(x - 1, y - 1, width + 1, height + 1, 15, 8);
    F0_2d05_0a05_DrawRectangle(x - 2, y - 2, width + 3, height + 3, 0);
}

void MenuBoxDialog::F0_2d05_0a05_DrawRectangle(int x, int y, int width, int height, uint16_t mode) {
    int s = p.var_aa.screenID;
    p.graphics.drawLine(s, x, y, x + width, y, uint8_t(mode));
    p.graphics.drawLine(s, x, y + height, x + width, y + height, uint8_t(mode));
    p.graphics.drawLine(s, x + width, y, x + width, y + height, uint8_t(mode));
    p.graphics.drawLine(s, x, y, x, y + height, uint8_t(mode));
}

void MenuBoxDialog::F0_2d05_0a66_DrawShadowRectangle(int x, int y, int width, int height,
                                                     uint16_t mode, uint16_t mode1) {
    int s = p.var_aa.screenID;
    p.graphics.drawLine(s, x, y, x + width, y, uint8_t(mode1));                  // top
    p.graphics.drawLine(s, x, y + height, x + width, y + height, uint8_t(mode)); // bottom
    p.graphics.drawLine(s, x + width, y, x + width, y + height, uint8_t(mode1)); // right
    p.graphics.drawLine(s, x, y + 1, x, y + height, uint8_t(mode));              // left
}

int MenuBoxDialog::F0_2d05_0ac9_GetNavigationKey() {
    // STUB: no real SDL input polling in this port. Returns 0 ("no key").
    return 0; // TODO(port): CAPI.getch()
}

// ---- navigation core -------------------------------------------------------
// A pure, headless-testable extraction of the C# ShowMenuBox input loop
// (MenuBoxDialog.cs cases 0x4800/0x5000/0xd/0x1b). Clamps at the ends like the
// C#, additionally skipping disabled options (a deliberate refinement: the C#
// only blocks ENTER on disabled, but navigating onto a dead option is useless).

int MenuBoxDialog::navMove(int idx, int dir) const {
    if (navOptionCount_ <= 0) return -1;
    if (idx < 0) idx = 0; // first key on a fresh menu lands on option 0 (C#).
    int next = idx;
    for (int i = idx + dir; i >= 0 && i < navOptionCount_; i += dir) {
        if ((navDisabled_ & (uint32_t(1) << i)) == 0) { next = i; break; }
    }
    // If `next` is still pointing at a disabled start, scan forward for any
    // enabled option (keeps the initial highlight on something selectable).
    if ((navDisabled_ & (uint32_t(1) << next)) != 0) {
        for (int i = 0; i < navOptionCount_; ++i)
            if ((navDisabled_ & (uint32_t(1) << i)) == 0) { next = i; break; }
    }
    return next;
}

int MenuBoxDialog::setupNav(int optionCount, uint32_t disabledMask, int startIndex) {
    navOptionCount_ = optionCount;
    navDisabled_ = disabledMask;
    if (optionCount <= 0) { highlight = -1; return -1; }
    int idx = startIndex;
    if (idx < 0) idx = 0;
    if (idx >= optionCount) idx = optionCount - 1;
    // Advance off a disabled starting option onto the first enabled one.
    if ((navDisabled_ & (uint32_t(1) << idx)) != 0) idx = navMove(idx, +1);
    highlight = idx;
    return highlight;
}

// ---- mouse hit-test / dispatch ---------------------------------------------
// Uses the rects cached by F0_2d05_0031_ShowMenuBox. itemAt returns the option
// index whose rect contains (fbX,fbY), or -1 outside the box / on a non-option
// (header) line. handleMouse turns motion into a hover-highlight, left-click
// into a selection, and right-click / click-outside into a cancel (-1).

int MenuBoxDialog::itemAt(int fbX, int fbY) const {
    for (const ItemRect& r : lastItemRects_) {
        if (fbX >= r.x && fbX < r.x + r.w &&
            fbY >= r.y && fbY < r.y + r.h)
            return r.optionIndex;
    }
    return -1;
}

bool MenuBoxDialog::handleMouse(const MouseEvent& ev, int* outSelection) {
    // Inside-the-box test for cancel-on-outside-click (uses the cached box
    // geometry rather than the per-option rects so clicks on the box border
    // are not treated as cancel).
    auto insideBox = [&](int x, int y) {
        return x >= lastBoxX_ && x < lastBoxX_ + lastBoxW_ &&
               y >= lastBoxY_ && y < lastBoxY_ + lastBoxH_;
    };

    int idx = itemAt(ev.x, ev.y);

    // Hover (motion or any non-down event): just move the highlight.
    if (ev.motion || !ev.down) {
        if (idx >= 0) highlight = idx;
        return false;
    }

    // Button-down dispatch.
    if (ev.button == 1) {                       // left
        if (idx >= 0) {
            // Respect the disabled mask exactly like navStep(KeyEnter).
            if ((navDisabled_ & (uint32_t(1) << idx)) != 0) return false;
            highlight = idx;
            if (outSelection) *outSelection = idx;
            return true;
        }
        if (!insideBox(ev.x, ev.y)) {
            highlight = -1;
            if (outSelection) *outSelection = -1; // outside = cancel
            return true;
        }
        return false; // inside the box but not on an option: no-op
    }
    if (ev.button == 3) {                       // right = cancel
        highlight = -1;
        if (outSelection) *outSelection = -1;
        return true;
    }
    return false;
}

int MenuBoxDialog::navStep(int keycode) {
    switch (keycode) {
        case KeyUp:
            highlight = navMove(highlight, -1);
            return NavNone;
        case KeyDown:
            highlight = navMove(highlight, +1);
            return NavNone;
        case KeyEnter:
            if (highlight < 0 || highlight >= navOptionCount_) return NavNone;
            if ((navDisabled_ & (uint32_t(1) << highlight)) != 0) return NavNone; // disabled: ignore
            return highlight;
        case KeyEsc:
            highlight = -1;
            return NavCancel;
        default:
            return NavNone;
    }
}

} // namespace oc1
