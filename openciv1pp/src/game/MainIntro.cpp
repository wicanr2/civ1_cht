#include "MainIntro.h"
#include "DrawTools.h"
#include "ImageTools.h"
#include "../resource/PicLoader.h"
#include <filesystem>

namespace oc1 {

MainIntro::MainIntro(OpenCiv1Game& parent) : p(parent) {}

// The slide list mirrors the load order in MainIntro.cs:
//   F2_0000_0000 loads LOGO.PIC (0x328a) then PLANET1.PIC (0x3293) +
//   PLANET2.PIC (0x329e). F2_0000_0bd7 loads BIRTH0..BIRTH8 (0x32d8 / 0x32e4
//   etc.). The credits-card final frame is stubbed as a caption over the logo.
const std::vector<MainIntro::Slide>& MainIntro::slides() {
    static const std::vector<Slide> s = {
        { "LOGO.PIC",    "Sid Meier's CIVILIZATION" },           // F2_0000_0000 title
        { "PLANET1.PIC", "宇宙誕生於約一百五十億年前..." },        // planet rotation A
        { "PLANET2.PIC", "地球誕生於約四十六億年前..." },          // planet rotation B
        { "BIRTH0.PIC",  "生命的曙光..." },                       // F2_0000_0bd7 birth0
        { "BIRTH1.PIC",  "從海洋走向陸地..." },                   // birth1
        { "BIRTH2.PIC",  "人類祖先的出現..." },                   // birth2
        { "BIRTH3.PIC",  "學會用火..." },                         // birth3
        { "BIRTH4.PIC",  "群居與部落..." },                       // birth4
        { "BIRTH5.PIC",  "石器與工具..." },                       // birth5
        { "BIRTH6.PIC",  "農業的開始..." },                       // birth6
        { "BIRTH7.PIC",  "文明的曙光..." },                       // birth7
        { "BIRTH8.PIC",  "建立你的文明！" },                      // birth8
        { "LOGO.PIC",    "按任意鍵開始..." },                     // F2_0000_152a credits-card stub
    };
    return s;
}

bool MainIntro::hasAssets() const {
    if (p.resourcePath().empty()) return false;
    std::error_code ec;
    return std::filesystem::exists(std::filesystem::path(p.resourcePath()) / "LOGO.PIC", ec);
}

void MainIntro::play() {
    // C# F2_0000_0000 entry is normally driven by the F0_11a8_0008_Main loop.
    // Without DOS assets we cannot render the real screens — fall back to a
    // no-op (the caller decides what to show next, e.g. LogoAndMainGameMenu).
    if (!hasAssets()) return;
    reset();
    // The port leaves the actual display loop (SDL pollKey + present) to the
    // caller via nextFrame(); play() just primes the cursor so a non-interactive
    // caller can step through every slide and dump it. TODO(port): the real
    // palette-cycle / sound-timer pacing is not modelled.
}

bool MainIntro::nextFrame(GBitmap& fb) {
    const auto& list = slides();
    if (cursor_ < 0 || cursor_ >= int(list.size())) return false;

    // Without assets there is nothing to draw — same short-circuit as play().
    if (!hasAssets()) { cursor_ = int(list.size()); return false; }

    const Slide& sl = list[std::size_t(cursor_)];
    std::filesystem::path picPath = std::filesystem::path(p.resourcePath()) / sl.picBase;

    std::error_code ec;
    if (sl.picBase.empty() || !std::filesystem::exists(picPath, ec)) {
        // Caption-only / missing asset: clear to palette index 0 (DOS black).
        fb.clear(0);
    } else {
        // Load the .PIC and blit it at (0,0). Copy palette so colors match.
        std::unique_ptr<GBitmap> img = loadPicFile(picPath.string(), true);
        if (!img) { fb.clear(0); }
        else {
            fb.copyPaletteFrom(*img);
            fb.clear(0);
            fb.drawBitmap(0, 0, *img, false);
        }
    }

    // Draw the caption (uses the GDriver's font 1 if present; safe no-op
    // otherwise). Position: bottom-of-screen, centered horizontally. Uses
    // DrawTools (Translator + word-shadow) when the screen 0 is bound.
    if (!sl.caption.empty() && p.graphics.hasFont(1) && p.graphics.hasScreen(GDriver::MainScreen)
        && &p.graphics.screen(GDriver::MainScreen) == &fb) {
        try {
            // Drop-shadow + foreground centered string near the bottom.
            p.drawTools().F0_1182_00b3_DrawCenteredStringWithShadowToScreen0(
                sl.caption, fb.width() / 2, fb.height() - 16, /*color*/ 15);
        } catch (...) { /* font unavailable: skip caption */ }
    }

    ++cursor_;
    return true;
}

} // namespace oc1
