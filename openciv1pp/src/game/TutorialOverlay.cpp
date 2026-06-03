// TutorialOverlay.cpp — see TutorialOverlay.h. A3: first-turn schematic.
#include "TutorialOverlay.h"
#include "MenuBoxDialog.h"
#include "../graphics/GFont.h"

namespace oc1 {

namespace {
// Palette indices we install/borrow for the overlay. Index 254 is reserved as
// the "tutorial-white" colour (R=G=B=255) so we don't clobber any existing
// HUD index (207/208/209/210 used by MiniWorld::draw + 218..220 used for the
// road/irrigation/hut overlays). 253 holds a darker bright-yellow used for
// label text so it pops on top of the (semi-transparent) white box. 252 is a
// translucent-style dim white used for the box FILL (a 1-in-4 stipple pattern
// instead of real alpha — palette indexed frame buffers don't have alpha, so
// the "semitransparent" effect is faked by skipping 3 of 4 pixels).
constexpr uint8_t kBoxOutline = 254;
constexpr uint8_t kLabelInk   = 253;
constexpr uint8_t kBoxStipple = 252;
} // namespace

void TutorialOverlay::show(GDriver& gd, int fontId) const {
    GBitmap& fb = gd.screen(GDriver::MainScreen);

    // Install our three palette slots (idempotent). 254=pure white, 253=bright
    // yellow (labels), 252=mid-grey (stipple). The MiniWorld::draw path uses
    // 207..220 so 252..254 don't collide with anything.
    fb.palette.set(kBoxOutline, 255, 255, 255);
    fb.palette.set(kLabelInk,   255, 240,  90);
    fb.palette.set(kBoxStipple, 200, 200, 210);

    const int W = fb.width();
    const int H = fb.height();
    const int hudH = 64; // matches MiniWorld::draw's HUD strip

    // The five callout regions. (x,y,w,h, label_key, label_x, label_y).
    // Coordinates are tuned for 640x480 -- the same canvas MiniWorld::draw
    // targets. Labels sit OUTSIDE the box so they don't sit on top of any
    // game UI text.
    struct Callout {
        Rect box;
        const char* labelKey;
        int labelX;
        int labelY;
    };
    // Map viewport (most of the screen above the HUD), minimap (top-right
    // ~80x50 + border, see MiniWorld::renderMinimap), HUD line 1 (year), HUD
    // line 2 (help — gold/science live here in the slider row), HUD line 3
    // (cities). Labels are placed where they can't overlap the boxed region.
    const int mmW = 80 + 4;       // minimap + 2px border on each side
    const int mmH = 50 + 4;
    const int mmX = W - mmW - 2;
    const int mmY = 2;
    Callout calls[] = {
        // Map viewport (slightly inset from full FB so the outline is visible).
        { Rect{ 8, 8, W - mmW - 24, H - hudH - 16 },
          "your unit", 16, 24 },
        // Minimap
        { Rect{ mmX - 2, mmY - 2, mmW + 4, mmH + 4 },
          "minimap", mmX - 80, mmY + mmH + 8 },
        // HUD line 1 (year): top of the HUD strip
        { Rect{ 0, H - hudH, W, 18 },
          "year", 8, H - hudH - 16 },
        // HUD line 2 (gold/science slider row)
        { Rect{ 0, H - hudH + 18, W, 18 },
          "gold", W - 220, H - hudH + 24 },
        // HUD line 3 (cities + civs + government)
        { Rect{ 0, H - hudH + 36, W, hudH - 36 },
          "science", W - 220, H - hudH + 44 },
    };

    // Draw each box (semitransparent fill stipple + solid outline) and label.
    const GFont& font = gd.font(fontId);
    for (const Callout& c : calls) {
        // Semitransparent fill: stipple kBoxStipple every 4th pixel.
        for (int y = c.box.y; y < c.box.y + c.box.h; ++y) {
            for (int x = c.box.x; x < c.box.x + c.box.w; ++x) {
                if (((x + y) & 3) == 0) fb.setPixel(x, y, kBoxStipple);
            }
        }
        // Solid white outline
        fb.drawRect(c.box, kBoxOutline);

        // Label: routed through Translator (zh_TW.json). The label sits at
        // (labelX, labelY); when those would land off-screen we clamp.
        int lx = c.labelX;
        int ly = c.labelY;
        if (lx < 4)               lx = 4;
        if (ly < 4)               ly = 4;
        if (lx > W - 12)          lx = W - 12;
        if (ly > H - font.pixelHeight - 4) ly = H - font.pixelHeight - 4;
        gd.drawString(GDriver::MainScreen, font, lx, ly, c.labelKey, kLabelInk);
    }

    // Bottom-centre footer: "按任意鍵繼續" (translated). Sits BELOW the HUD
    // strip if there's room; otherwise it overlays the bottom 2px of the HUD.
    const char* footerKey = "press any key to continue";
    Size sz = gd.getDrawStringSize(fontId, footerKey);
    int fx = (W - sz.w) / 2;
    int fy = H - sz.h - 2;
    // Black backdrop so the footer always reads against whatever is below
    fb.fillRect(Rect{fx - 4, fy - 2, sz.w + 8, sz.h + 4}, 210);
    gd.drawString(GDriver::MainScreen, font, fx, fy, footerKey, kBoxOutline);
}

bool TutorialOverlay::handleKey(int navKey) {
    if (shown) return false;
    // ESC or Enter dismisses (the explicit spec keys). Any OTHER non-None key
    // also dismisses ("any key" per spec). KeyNone never dismisses (the loop
    // can poll without consuming).
    if (navKey == MenuBoxDialog::KeyNone) return false;
    shown = true;
    return true;
}

} // namespace oc1
