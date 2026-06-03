#include "NewGameWizard.h"
#include "DrawTools.h"
#include "MainCode.h"
#include "MenuBoxDialog.h"
#include "../graphics/GBitmap.h"
#include <algorithm>
#include <cstdio>

namespace oc1 {

NewGameWizard::NewGameWizard(OpenCiv1Game& parent) : p(parent) { reset(); }

const std::vector<int>& NewGameWizard::civilizationCounts() {
    // DOS Civ1 lets the player pick how many civs play (default 7 = 1 human +
    // 6 AI). The C# F5_0000_0000_InitNewGameData loops 3..7 for the spread;
    // we expose the same range minus the human (3..6 AI counts).
    static const std::vector<int> v = { 3, 4, 5, 6 };
    return v;
}

void NewGameWizard::reset() {
    stage_ = Stage::DIFFICULTY;
    highlight_ = 0;
    chosenDifficulty_ = -1;
    chosenCivCount_   = -1;
    chosenTribe_      = -1;
    chosenName_.clear();
}

std::vector<std::string> NewGameWizard::currentOptions() const {
    switch (stage_) {
        case Stage::DIFFICULTY: return MainCode::difficultyItems();
        case Stage::CIVILIZATIONS: {
            std::vector<std::string> v;
            for (int n : civilizationCounts()) {
                // The C# menu string uses the count directly (no fancy label);
                // we localize via the count -> "N civilizations" key shape so
                // zh_TW.json can render "三/四/五/六 個文明" with the {n}
                // placeholder substituted by the Translator (or fall back to
                // the English count when not translated).
                char buf[24];
                std::snprintf(buf, sizeof(buf), "%d civilizations", n);
                v.emplace_back(buf);
            }
            return v;
        }
        case Stage::TRIBE: {
            std::vector<std::string> v;
            v.reserve(MainCode::tribes().size());
            for (const auto& t : MainCode::tribes()) v.push_back(t.nationality);
            return v;
        }
        case Stage::NAME: {
            return { defaultLeaderName() };
        }
        case Stage::DONE: return {};
    }
    return {};
}

std::string NewGameWizard::currentStageLabel() const {
    switch (stage_) {
        case Stage::DIFFICULTY:    return "Choose Difficulty";
        case Stage::CIVILIZATIONS: return "Choose Civilizations";
        case Stage::TRIBE:         return "Pick Your Tribe";
        case Stage::NAME:          return "Name Your Leader";
        case Stage::DONE:          return "";
    }
    return "";
}

std::string NewGameWizard::defaultLeaderName() const {
    if (chosenTribe_ >= 0 && chosenTribe_ < int(MainCode::tribes().size())) {
        return MainCode::tribes()[std::size_t(chosenTribe_)].leader;
    }
    return "Player";
}

int NewGameWizard::leftPortraitIndex() const {
    // During the tribe stage the left column previews the row the user is
    // hovering; once a tribe has been picked it sticks to that selection.
    if (stage_ == Stage::TRIBE) {
        int n = int(MainCode::tribes().size());
        return std::max(0, std::min(highlight_, n - 1));
    }
    if (chosenTribe_ >= 0) return chosenTribe_;
    return 0;
}

NewGameWizard::Stage NewGameWizard::nav(int navKey) {
    // The number of selectable rows for the current stage.
    int rowCount = int(currentOptions().size());
    if (rowCount <= 0) rowCount = 1;

    switch (navKey) {
        case MenuBoxDialog::KeyUp:
            if (highlight_ > 0) --highlight_;
            else highlight_ = rowCount - 1;          // wrap
            break;
        case MenuBoxDialog::KeyDown:
            if (highlight_ < rowCount - 1) ++highlight_;
            else highlight_ = 0;                     // wrap
            break;
        case MenuBoxDialog::KeyEnter: {
            switch (stage_) {
                case Stage::DIFFICULTY:
                    chosenDifficulty_ = highlight_;
                    stage_ = Stage::CIVILIZATIONS;
                    highlight_ = 0;
                    break;
                case Stage::CIVILIZATIONS:
                    if (highlight_ >= 0 && highlight_ < int(civilizationCounts().size()))
                        chosenCivCount_ = civilizationCounts()[std::size_t(highlight_)];
                    stage_ = Stage::TRIBE;
                    highlight_ = 0;
                    break;
                case Stage::TRIBE:
                    chosenTribe_ = highlight_;
                    stage_ = Stage::NAME;
                    highlight_ = 0;
                    break;
                case Stage::NAME:
                    chosenName_ = defaultLeaderName();
                    stage_ = Stage::DONE;
                    break;
                case Stage::DONE: break;
            }
            break;
        }
        case MenuBoxDialog::KeyEsc: {
            switch (stage_) {
                case Stage::CIVILIZATIONS: stage_ = Stage::DIFFICULTY;    highlight_ = std::max(0, chosenDifficulty_); break;
                case Stage::TRIBE:         stage_ = Stage::CIVILIZATIONS; highlight_ = 0; break;
                case Stage::NAME:          stage_ = Stage::TRIBE;         highlight_ = std::max(0, chosenTribe_); break;
                case Stage::DIFFICULTY:
                case Stage::DONE: break; // caller decides what to do.
            }
            break;
        }
        default: break;
    }
    return stage_;
}

// ---- rendering ----

namespace {

// DOS palette indexes used by the C# port: 14 = bright yellow, 0 = black,
// 15 = bright white, 8 = mid grey. The numbers mirror MenuBoxDialog's
// hardcoded F0_2d05_0031 palette choices for the box outline + highlight.
constexpr uint8_t kBorderYellow = 14;
constexpr uint8_t kBgBlack      = 0;
constexpr uint8_t kTextWhite    = 15;
constexpr uint8_t kPanelGrey    = 8;

// Pseudo-portrait: a colour block sized 80x44 with a 1-px black border. Real
// SP257 sprites would be drawn here when present (TODO: wire the SP257
// loader); the colour seed is derived from the tribe index so each portrait
// is visually distinct on the left column even without the asset.
void drawPseudoPortrait(GBitmap& fb, int x, int y, int w, int h, int seed) {
    if (w <= 0 || h <= 0) return;
    // Border first.
    Rect r{ x, y, w, h };
    fb.fillRect(r, kPanelGrey);
    // Inset coloured block — palette indexes 1..7 give DOS bright primaries.
    uint8_t color = uint8_t(1 + (seed % 7));
    Rect inset{ x + 2, y + 2, w - 4, h - 4 };
    fb.fillRect(inset, color);
    // A small "face" — two eye dots + a mouth line — to read as a portrait
    // at a glance without an actual sprite.
    int cx = x + w / 2, cy = y + h / 2;
    fb.setPixel(cx - 8, cy - 4, kBgBlack);
    fb.setPixel(cx + 8, cy - 4, kBgBlack);
    fb.setPixel(cx - 7, cy - 4, kBgBlack);
    fb.setPixel(cx + 9, cy - 4, kBgBlack);
    for (int dx = -6; dx <= 6; ++dx)
        fb.setPixel(cx + dx, cy + 6, kBgBlack);
}

} // namespace

void NewGameWizard::drawWizardFrame() {
    drawWizardFrame(stage_, leftPortraitIndex(), currentOptions());
}

void NewGameWizard::drawWizardFrame(Stage stage, int leftIdx,
                                    const std::vector<std::string>& rightOptions) {
    if (!p.graphics.hasScreen(GDriver::MainScreen)) return;
    GBitmap& fb = p.graphics.screen(GDriver::MainScreen);
    const int W = fb.width(), H = fb.height();

    // Backdrop wash. C# leaves the previous frame visible; we paint black so
    // the wizard chrome reads cleanly regardless of caller-state.
    fb.fillRect(Rect{0, 0, W, H}, kBgBlack);

    // --- LEFT column (96 px wide, full height) ---
    const int kLeftW = 96;
    fb.fillRect(Rect{0, 0, kLeftW, H}, kBgBlack);
    fb.drawRect(Rect{0, 0, kLeftW, H}, kBorderYellow);

    // Portrait stack: 7 slots (matches the DOS column's typical 7 visible
    // leaders) at 80 wide × 44 tall, with an 8 px outer margin and a 4 px
    // gap between slots. The highlighted slot gets a yellow outline.
    const int slotW = 80, slotH = 44, gap = 4, slotsTopY = 24;
    const int numSlots = 7;
    int n = int(MainCode::tribes().size());
    if (n <= 0) n = 1;
    // Choose a window of `numSlots` portraits centered on leftIdx (so the
    // highlighted leader is visible without scrolling state).
    int half = numSlots / 2;
    int firstShown = leftIdx - half;
    if (firstShown < 0) firstShown = 0;
    if (firstShown + numSlots > n) firstShown = std::max(0, n - numSlots);
    for (int s = 0; s < numSlots; ++s) {
        int tribeIdx = firstShown + s;
        if (tribeIdx >= n) break;
        int sx = 8;
        int sy = slotsTopY + s * (slotH + gap);
        if (sy + slotH > H - 8) break;
        drawPseudoPortrait(fb, sx, sy, slotW, slotH, tribeIdx);
        if (tribeIdx == leftIdx) {
            // Yellow outline (2 px) around the highlighted slot.
            fb.drawRect(Rect{sx - 1, sy - 1, slotW + 2, slotH + 2}, kBorderYellow);
            fb.drawRect(Rect{sx - 2, sy - 2, slotW + 4, slotH + 4}, kBorderYellow);
        }
    }

    // --- RIGHT column (wide, yellow-bordered) ---
    const int rightX = kLeftW + 12;
    const int rightW = W - rightX - 12;
    const int rightY = 24;
    const int rightH = H - rightY - 24;
    // 2-px outer border + 1-px inner gap to read as the DOS double-border.
    fb.drawRect(Rect{rightX,     rightY,     rightW,     rightH    }, kBorderYellow);
    fb.drawRect(Rect{rightX + 1, rightY + 1, rightW - 2, rightH - 2}, kBorderYellow);

    // Title bar — top strip of the right column, kBorderYellow background.
    const int titleH = 24;
    fb.fillRect(Rect{rightX + 3, rightY + 3, rightW - 6, titleH - 3}, kBorderYellow);
    // Stage label (drawn black on yellow so it pops against the title bar).
    std::string label;
    switch (stage) {
        case Stage::DIFFICULTY:    label = "Choose Difficulty";    break;
        case Stage::CIVILIZATIONS: label = "Choose Civilizations"; break;
        case Stage::TRIBE:         label = "Pick Your Tribe";      break;
        case Stage::NAME:          label = "Name Your Leader";     break;
        case Stage::DONE:          label = "";                     break;
    }
    if (!label.empty() && p.graphics.hasFont(1)) {
        try {
            p.drawTools().F0_1182_005c_DrawStringToScreen0(label,
                rightX + 12, rightY + 18, kBgBlack);
        } catch (...) {}
    }

    // Option rows — under the title bar, one row per option.
    const int rowH = 22;
    int rowsTop = rightY + titleH + 6;
    int rowsBottom = rightY + rightH - 6;
    int maxRows = (rowsBottom - rowsTop) / rowH;
    int nOpts = int(rightOptions.size());
    int shown = std::min(nOpts, maxRows);
    for (int i = 0; i < shown; ++i) {
        int ry = rowsTop + i * rowH;
        bool isHilite = (i == highlight_);
        if (isHilite) {
            // Inverted highlight bar — yellow fill, black text.
            fb.fillRect(Rect{rightX + 6, ry, rightW - 12, rowH - 2}, kBorderYellow);
        }
        if (p.graphics.hasFont(1)) {
            try {
                uint8_t color = isHilite ? kBgBlack : kTextWhite;
                p.drawTools().F0_1182_005c_DrawStringToScreen0(
                    rightOptions[std::size_t(i)],
                    rightX + 16, ry + 16, color);
            } catch (...) {}
        }
    }

    // For the NAME stage, draw an edit-box hint under the (single) option row
    // so the cascade visually transitions into the name entry without an
    // entirely separate screen.
    if (stage == Stage::NAME && p.graphics.hasFont(1)) {
        try {
            p.drawTools().F0_1182_005c_DrawStringToScreen0("Press Enter to accept:",
                rightX + 16, rowsTop + rowH + 24, kTextWhite);
        } catch (...) {}
    }
}

} // namespace oc1
