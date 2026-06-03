// CivNoteBanner.cpp — see CivNoteBanner.h. A4: yellow-on-green pop-up.
#include "CivNoteBanner.h"
#include "../graphics/GBitmap.h"
#include "../graphics/GDriver.h"
#include "../graphics/GFont.h"

namespace oc1 {

namespace {
// Reserve palette slots that don't collide with the rest of the renderer
// (MiniWorld uses 200..220; the tutorial uses 252..254). 245..247 fit neatly.
constexpr uint8_t kBannerBg     = 245; // dark green
constexpr uint8_t kBannerBorder = 246; // bright yellow
constexpr uint8_t kBannerText   = 247; // white (header + body)
} // namespace

void CivNoteBanner::queueNote(std::string textKey, int turns) {
    if (turns <= 0) turns = kDefaultTurns;
    queue_.push_back(PendingNote{ std::move(textKey), turns });
}

void CivNoteBanner::tick() {
    if (queue_.empty()) return;
    PendingNote& head = queue_.front();
    --head.turnsLeft;
    if (head.turnsLeft <= 0) queue_.pop_front();
}

void CivNoteBanner::draw(GDriver& gd, int fontId) const {
    if (queue_.empty()) return;
    GBitmap& fb = gd.screen(GDriver::MainScreen);
    const PendingNote& head = queue_.front();

    fb.palette.set(kBannerBg,        0,  90,  30);
    fb.palette.set(kBannerBorder,  255, 230,  60);
    fb.palette.set(kBannerText,    255, 255, 255);

    const int W = fb.width();
    const int H = fb.height();
    const int bannerH = kBannerHeight;
    const Rect box{ 0, H - bannerH, W, bannerH };

    // Dark green fill + bright yellow 1-px border
    fb.fillRect(box, kBannerBg);
    fb.drawRect(box, kBannerBorder);

    const GFont& font = gd.font(fontId);
    int lineH = font.pixelHeight + font.lineSpacing;

    // Header line: centred "--- 文明備忘 ---" (translated key:
    // "CIVILIZATION NOTE"; we surround it with "--- " ... " ---" at draw time).
    const char* headerKey = "CIVILIZATION NOTE";
    Size hsz = gd.getDrawStringSize(fontId, headerKey);
    // Render the dashes separately (they stay literal) then the translated key.
    // To keep the layout centred we measure the full "--- KEY ---" composite.
    const std::string dashesL = "--- ";
    const std::string dashesR = " ---";
    int dashLW = font.pixelHeight; // rough fixed-width for the prefix; ASCII
    // Easier: build the full composite via two drawString calls and a measured
    // approximate dash width — measure dashesL/R as ASCII (no translation).
    Size dlSize = measureString(font, dashesL);
    Size drSize = measureString(font, dashesR);
    int total = dlSize.w + hsz.w + drSize.w;
    int hx = (W - total) / 2;
    int hy = H - bannerH + 2;
    int penX = fb.drawString(font, hx, hy, dashesL, kBannerBorder);
    penX = gd.drawString(GDriver::MainScreen, font, penX, hy, headerKey, kBannerBorder);
    fb.drawString(font, penX, hy, dashesR, kBannerBorder);
    (void)dashLW; // suppress warning if unused

    // Body line: centred, translated.
    Size bsz = gd.getDrawStringSize(fontId, head.textKey);
    int bx = (W - bsz.w) / 2;
    int by = hy + lineH + 2;
    if (by + font.pixelHeight > H - 2) by = H - font.pixelHeight - 2;
    gd.drawString(GDriver::MainScreen, font, bx, by, head.textKey, kBannerText);
}

} // namespace oc1
