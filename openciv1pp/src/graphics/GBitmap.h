// GBitmap.h — palette-indexed software framebuffer (the heart of the renderer).
//
// All drawing writes 8-bit palette indices into px_. The presenter (SDL2 or the
// offscreen PPM dumper) converts to RGBA via the palette. Because text is
// composited here at the palette level, Chinese rendering is identical under any
// presenter — display backend choice and localization are fully decoupled.
#pragma once
#include "GFont.h"
#include "Palette.h"
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

namespace oc1 {

enum class WriteMode { Normal, And, Or, Xor };

// Minimal integer rectangle, mirroring OpenCiv1 GRectangle semantics used by the
// drawing primitives (Contains / Intersect).
struct Rect {
    int x = 0, y = 0, w = 0, h = 0;
    bool contains(int px, int py) const { return px >= x && py >= y && px < x + w && py < y + h; }
    bool empty() const { return w <= 0 || h <= 0; }
    static Rect intersect(const Rect& a, const Rect& b) {
        int x0 = std::max(a.x, b.x), y0 = std::max(a.y, b.y);
        int x1 = std::min(a.x + a.w, b.x + b.w), y1 = std::min(a.y + a.h, b.y + b.h);
        return Rect{x0, y0, std::max(0, x1 - x0), std::max(0, y1 - y0)};
    }
};

struct Size { int w = 0, h = 0; };

class GBitmap {
public:
    GBitmap(int w, int h) : w_(w), h_(h), px_(static_cast<std::size_t>(w) * h, 0) {}

    int width()  const { return w_; }
    int height() const { return h_; }
    Palette palette;

    void clear(uint8_t index) { std::fill(px_.begin(), px_.end(), index); }
    void copyPaletteFrom(const GBitmap& other) { palette.colors = other.palette.colors; }

    void setPixel(int x, int y, uint8_t index) {
        if (x >= 0 && y >= 0 && x < w_ && y < h_)
            px_[static_cast<std::size_t>(y) * w_ + x] = index;
    }
    void setPixel(int x, int y, uint8_t index, WriteMode mode);
    uint8_t getPixel(int x, int y) const {
        if (x < 0 || y < 0 || x >= w_ || y >= h_) return 0;
        return px_[static_cast<std::size_t>(y) * w_ + x];
    }

    Rect rect() const { return Rect{0, 0, w_, h_}; }

    // Drawing primitives (ported 1:1 from OpenCiv1 GBitmap.cs).
    void drawLine(int x1, int y1, int x2, int y2, uint8_t color, WriteMode mode = WriteMode::Normal);
    void fillRect(const Rect& r, uint8_t color, WriteMode mode = WriteMode::Normal);
    void drawRect(const Rect& r, uint8_t color, WriteMode mode = WriteMode::Normal); // outline (convenience)
    void replaceColor(const Rect& r, uint8_t oldColor, uint8_t newColor);

    // 18-bit (6 bits/channel) palette "color struct" — the byte layout the game
    // passes around (0x304d header + from/to indices + RGB triples). Ported from
    // GBitmap.SetPaletteFromColorStruct.
    void setPaletteFromColorStruct(const std::vector<uint8_t>& cs);
    std::vector<uint8_t> exportPaletteColorStruct(int from, int to) const;
    void drawBitmap(int x, int y, const GBitmap& src, bool transparent);
    void drawBitmap(int x, int y, const GBitmap& src, const Rect& srcRect, bool transparent);

    // Draws a UTF-8 string. ASCII (<0x80) uses the bitmap font; bytes 0x80-0xFF
    // are inline colour-change escapes (original convention); codepoints >=0x100
    // are CJK/Unicode rasterised on demand. Returns the final pen x.
    int drawString(const GFont& font, int x, int y, const std::string& utf8,
                   uint8_t frontColor, WriteMode mode = WriteMode::Normal);

    // Palette-index buffer.
    const std::vector<uint8_t>& pixels() const { return px_; }
    std::vector<uint8_t>&       pixelsMut()    { return px_; } // for resource loaders

    // Convert to tightly-packed RGBA8888 (for SDL texture upload / PPM dump).
    void toRGBA(std::vector<uint8_t>& out) const;

private:
    void blitGlyph(const CjkGlyphCache::Glyph& g, int penX, int baseline,
                   uint8_t color, WriteMode mode);

    int w_, h_;
    std::vector<uint8_t> px_;
};

// Measures a UTF-8 string in the given font without drawing (mirrors the
// advance logic of GBitmap::drawString). Used by GetDrawStringSize / centering.
Size measureString(const GFont& font, const std::string& utf8);

} // namespace oc1
