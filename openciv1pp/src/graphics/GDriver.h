// GDriver.h — screen/bitmap manager, ported from OpenCiv1 GDriver.cs.
//
// CodeObjects address graphics by screen ID (0 = main screen) and by bitmap ID
// (loaded images, allocated from 0xb000). The driver owns those GBitmaps and the
// screen<->screen / bitmap<->screen blits the game logic relies on. Display is
// orthogonal: SdlPresenter just presents whichever screen is the main one.
#pragma once
#include "GBitmap.h"
#include "GFont.h"
#include "../localization/Translator.h"
#include <map>
#include <stdexcept>
#include <string>

namespace oc1 {

// Drawing context, mirroring OpenCiv1's CRectangle (the "0xaa" rectangle): which
// screen + font to use and the current colours/flags. CodeObjects pass one of
// these to the text routines.
struct CRectangle {
    int screenID = 0;
    int fontID = 1;
    uint8_t frontColor = 15;
    uint8_t backColor = 0;
    int flags = 1;
    // left/top are the rectangle origin the C# CRectangle carries; the
    // F0_VGA_0c3e/0d47 bitmap-to-screen blits offset their (x,y) by these.
    int left = 0;
    int top = 0;
    int width = 320;
    int height = 200;
};

class GDriver {
public:
    static constexpr int MainScreen = 0;

    GBitmap& addScreen(int id, int w, int h) {
        auto res = screens_.emplace(id, GBitmap(w, h));
        return res.first->second;
    }
    bool hasScreen(int id) const { return screens_.count(id) != 0; }
    GBitmap& screen(int id) {
        auto it = screens_.find(id);
        if (it == screens_.end()) throw std::runtime_error("screen not allocated");
        return it->second;
    }

    bool hasBitmap(int id) const { return bitmaps_.count(id) != 0; }
    GBitmap& bitmap(int id) {
        auto it = bitmaps_.find(id);
        if (it == bitmaps_.end()) throw std::runtime_error("bitmap not allocated");
        return it->second;
    }
    // Register an externally-built bitmap (e.g. a loaded .pic). Returns its id.
    int addBitmap(GBitmap bmp) {
        int id = nextBitmapId_++;
        bitmaps_.emplace(id, std::move(bmp));
        return id;
    }

    // F0_VGA_07d8_DrawImage: blit a rect from src screen to (dstX,dstY) of dst screen.
    void drawImage(int srcId, const Rect& srcRect, int dstId, int dstX, int dstY) {
        screen(dstId).drawBitmap(dstX, dstY, screen(srcId), srcRect, false);
    }

    // F0_VGA_0b85_ScreenToBitmap: capture a screen region into a new bitmap.
    int screenToBitmap(int srcId, const Rect& r) {
        GBitmap& src = screen(srcId);
        GBitmap cap(r.w, r.h);
        cap.copyPaletteFrom(src);
        cap.drawBitmap(0, 0, src, r, false);
        return addBitmap(std::move(cap));
    }

    // drawBitmapToScreen: blit a stored bitmap onto a screen (the simple,
    // screen-id keyed convenience the rest of the project already uses).
    void drawBitmapToScreen(int dstScreenId, int x, int y, int bitmapId, bool transparent = false) {
        screen(dstScreenId).drawBitmap(x, y, bitmap(bitmapId), transparent);
    }

    // ---- screen-composite / bitmap-to-screen ports (GDriver.cs) -------------

    // F0_VGA_06b7_DrawScreenToMainScreen: composite an offscreen screen onto the
    // main screen 0. The C# does screen0.DrawBitmap(newScreen) — a full-frame,
    // non-transparent blit at (0,0). screenID 0 is a no-op. Throws if unallocated.
    void F0_VGA_06b7_DrawScreenToMainScreen(int screenID) {
        if (screenID == 0) return;
        if (!hasScreen(screenID)) throw std::runtime_error("screen not allocated");
        screen(MainScreen).drawBitmap(0, 0, screen(screenID), false);
    }

    // F0_VGA_06b7_DrawScreenToMainScreenWithEffect: same end result as the plain
    // composite, but the C# reveals the new screen via a random per-pixel fade-in
    // (RandomMT19937, Thread.Sleep batched timing). The animation timing is a
    // presentation/timer concern and is NOT modelled here (stubbed, see note);
    // the final composited frame is produced faithfully by delegating to the
    // plain composite, so screen 0 ends pixel-identical to the C# result.
    void F0_VGA_06b7_DrawScreenToMainScreenWithEffect(int screenID) {
        // NOTE(port): random-fade animation + Thread.Sleep timing not modelled;
        // we produce the final composited frame only (visually identical result).
        F0_VGA_06b7_DrawScreenToMainScreen(screenID);
    }

    // F0_VGA_0c3e_DrawBitmapToScreen: blit a stored bitmap onto rect.screenID at
    // (rect.left + x, rect.top + y), transparently. C# logs (does not throw) when
    // the bitmap is missing; we mirror that by silently skipping. Throws if the
    // destination screen is unallocated.
    void F0_VGA_0c3e_DrawBitmapToScreen(const CRectangle& rect, int x, int y, int bitmapID) {
        if (!hasScreen(rect.screenID)) throw std::runtime_error("screen not allocated");
        if (!hasBitmap(bitmapID)) return; // C#: log "bitmap not allocated", no draw
        screen(rect.screenID).drawBitmap(rect.left + x, rect.top + y, bitmap(bitmapID), true);
    }

    // F0_VGA_0d47_DrawBitmapToScreen: identical behaviour to 0c3e in the C#
    // (separate entry point, same body). Kept distinct to preserve the API names.
    void F0_VGA_0d47_DrawBitmapToScreen(const CRectangle& rect, int x, int y, int bitmapPtr) {
        if (!hasScreen(rect.screenID)) throw std::runtime_error("screen not allocated");
        if (!hasBitmap(bitmapPtr)) return; // C#: log "bitmap not allocated", no draw
        screen(rect.screenID).drawBitmap(rect.left + x, rect.top + y, bitmap(bitmapPtr), true);
    }

    // ---- high-level draw entry points keyed by screen id (the F0_VGA_* API
    //      CodeObjects call; they delegate to the screen's GBitmap) ----
    void setPixel(int screenId, int x, int y, uint8_t color, WriteMode mode = WriteMode::Normal) {
        screen(screenId).setPixel(x, y, color, mode);
    }
    uint8_t getPixel(int screenId, int x, int y) { return screen(screenId).getPixel(x, y); }
    void drawLine(int screenId, int x1, int y1, int x2, int y2, uint8_t color, WriteMode mode = WriteMode::Normal) {
        screen(screenId).drawLine(x1, y1, x2, y2, color, mode);
    }
    void fillRectangle(int screenId, const Rect& r, uint8_t color, WriteMode mode = WriteMode::Normal) {
        screen(screenId).fillRect(r, color, mode);
    }
    void replaceColor(int screenId, const Rect& r, uint8_t oldColor, uint8_t newColor) {
        screen(screenId).replaceColor(r, oldColor, newColor);
    }
    // F0_VGA_11d7_DrawString: localized text. Translation happens here (the
    // single chokepoint), then the screen composites ASCII + CJK glyphs.
    int drawString(int screenId, const GFont& font, int x, int y, const std::string& text, uint8_t color) {
        return screen(screenId).drawString(font, x, y, Translator::instance().translate(text), color);
    }

    // ---- font registry (CodeObjects address fonts by id) ----
    void addFont(int id, GFont f) { fonts_[id] = std::move(f); }
    bool hasFont(int id) const { return fonts_.count(id) != 0; }
    GFont& font(int id) {
        auto it = fonts_.find(id);
        if (it == fonts_.end()) throw std::runtime_error("font not registered");
        return it->second;
    }

    // GetDrawStringSize: measure the *translated* text so centering/wrapping
    // accounts for Chinese glyph widths.
    Size getDrawStringSize(int fontId, const std::string& text) {
        return measureString(font(fontId), Translator::instance().translate(text));
    }

    // F0_VGA_11d7_DrawString via a CRectangle context (translation chokepoint).
    int drawString(const CRectangle& r, int x, int y, const std::string& text) {
        return screen(r.screenID).drawString(font(r.fontID), x, y,
                                              Translator::instance().translate(text), r.frontColor);
    }

    int screenCount() const { return int(screens_.size()); }
    int bitmapCount() const { return int(bitmaps_.size()); }

private:
    std::map<int, GBitmap> screens_;
    std::map<int, GBitmap> bitmaps_;
    std::map<int, GFont> fonts_;
    int nextBitmapId_ = 0xb000;
};

} // namespace oc1
