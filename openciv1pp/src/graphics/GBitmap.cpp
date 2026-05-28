#include "GBitmap.h"

namespace oc1 {

namespace {

// Decode UTF-8 to codepoints. Invalid/standalone high bytes (0x80-0xFF) fall
// through as their raw byte value, which preserves the original game's inline
// colour-escape convention while letting valid multibyte sequences (Chinese)
// decode to real Unicode codepoints.
std::vector<uint32_t> decodeUtf8(const std::string& s) {
    std::vector<uint32_t> out;
    out.reserve(s.size());
    std::size_t i = 0, n = s.size();
    while (i < n) {
        uint8_t c = static_cast<uint8_t>(s[i]);
        if (c < 0x80) { out.push_back(c); ++i; continue; }

        int extra = 0;
        uint32_t cp = 0;
        if ((c & 0xE0) == 0xC0) { extra = 1; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { extra = 2; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { extra = 3; cp = c & 0x07; }
        else { out.push_back(c); ++i; continue; } // lone high byte -> raw

        if (i + extra >= n) { out.push_back(c); ++i; continue; }
        bool ok = true;
        for (int k = 1; k <= extra; ++k) {
            uint8_t cc = static_cast<uint8_t>(s[i + k]);
            if ((cc & 0xC0) != 0x80) { ok = false; break; }
            cp = (cp << 6) | (cc & 0x3F);
        }
        if (!ok) { out.push_back(c); ++i; continue; } // malformed -> raw byte
        out.push_back(cp);
        i += extra + 1;
    }
    return out;
}

} // namespace

void GBitmap::blitGlyph(const CjkGlyphCache::Glyph& g, int penX, int baseline,
                        uint8_t color, WriteMode mode) {
    for (int j = 0; j < g.height; ++j) {
        int sy = baseline - g.top + j;
        if (sy < 0 || sy >= h_) continue;
        for (int k = 0; k < g.width; ++k) {
            if (g.coverage[static_cast<std::size_t>(j) * g.width + k] < 128)
                continue; // palette has no alpha: threshold coverage
            int sx = penX + g.left + k;
            if (sx < 0 || sx >= w_) continue;
            std::size_t idx = static_cast<std::size_t>(sy) * w_ + sx;
            switch (mode) {
                case WriteMode::Normal: px_[idx] = color; break;
                case WriteMode::And:    px_[idx] &= color; break;
                case WriteMode::Or:     px_[idx] |= color; break;
                case WriteMode::Xor:    px_[idx] ^= color; break;
            }
        }
    }
}

int GBitmap::drawString(const GFont& font, int x, int y, const std::string& utf8,
                        uint8_t frontColor, WriteMode mode) {
    std::vector<uint32_t> cps = decodeUtf8(utf8);
    int penX = x;
    int baseline = y + font.pixelHeight; // top-left origin -> baseline below

    for (uint32_t cp : cps) {
        if (cp >= 0x100) {
            const CjkGlyphCache::Glyph* g = CjkGlyphCache::instance().getGlyph(cp, font.pixelHeight);
            if (g) {
                blitGlyph(*g, penX, baseline, frontColor, mode);
                penX += g->advance + font.charSpacing;
            } else {
                penX += font.pixelHeight; // no font: keep layout
            }
            continue;
        }
        if (cp >= 0x80) {                 // inline colour escape (original convention)
            frontColor = static_cast<uint8_t>(cp - 0x80);
            continue;
        }
        const CjkGlyphCache::Glyph* g = font.get(cp);
        if (!g) g = font.get('?');
        if (g) {
            blitGlyph(*g, penX, baseline, frontColor, mode);
            penX += g->advance + font.charSpacing;
        }
    }
    return penX;
}

Size measureString(const GFont& font, const std::string& utf8) {
    Size sz{0, font.pixelHeight + font.lineSpacing};
    int penX = 0;
    for (uint32_t cp : decodeUtf8(utf8)) {
        if (cp >= 0x100) {
            const CjkGlyphCache::Glyph* g = CjkGlyphCache::instance().getGlyph(cp, font.pixelHeight);
            penX += (g ? g->advance : font.pixelHeight) + font.charSpacing;
        } else if (cp >= 0x80) {
            continue; // colour escape: no advance
        } else {
            const CjkGlyphCache::Glyph* g = font.get(cp);
            if (!g) g = font.get('?');
            if (g) penX += g->advance + font.charSpacing;
        }
    }
    sz.w = penX;
    return sz;
}

void GBitmap::setPixel(int x, int y, uint8_t color, WriteMode mode) {
    if (x < 0 || y < 0 || x >= w_ || y >= h_) return;
    std::size_t idx = static_cast<std::size_t>(y) * w_ + x;
    switch (mode) {
        case WriteMode::Normal: px_[idx] = color; break;
        case WriteMode::And:    px_[idx] &= color; break;
        case WriteMode::Or:     px_[idx] |= color; break;
        case WriteMode::Xor:    px_[idx] ^= color; break;
    }
}

void GBitmap::drawLine(int x1, int y1, int x2, int y2, uint8_t color, WriteMode mode) {
    int wd, wdir = 1, ht, hdir = 1;
    if (x1 > x2) { wd = (x1 - x2) + 1; wdir = -1; } else { wd = (x2 - x1) + 1; }
    if (y1 > y2) { ht = (y1 - y2) + 1; hdir = -1; } else { ht = (y2 - y1) + 1; }

    if (wd == 1 && ht == 1) {
        setPixel(x1, y1, color, mode);
    } else if (wd > 1 && ht == 1) {
        for (int i = 0; i < wd; ++i) setPixel(x1 + i * wdir, y1, color, mode);
    } else if (wd == 1 && ht > 1) {
        for (int i = 0; i < ht; ++i) setPixel(x1, y1 + i * hdir, color, mode);
    } else if (wd > ht) {
        double step = double(ht) / double(wd);
        for (int i = 0; i < wd; ++i) setPixel(x1 + i * wdir, y1 + int(step * i) * hdir, color, mode);
    } else {
        double step = double(wd) / double(ht);
        for (int i = 0; i < ht; ++i) setPixel(x1 + int(step * i) * wdir, y1 + i * hdir, color, mode);
    }
}

void GBitmap::fillRect(const Rect& r, uint8_t color, WriteMode mode) {
    Rect c = Rect::intersect(r, rect());
    if (c.empty()) return;
    for (int yy = c.y; yy < c.y + c.h; ++yy) {
        std::size_t base = static_cast<std::size_t>(yy) * w_;
        for (int xx = c.x; xx < c.x + c.w; ++xx) {
            std::size_t idx = base + xx;
            switch (mode) {
                case WriteMode::Normal: px_[idx] = color; break;
                case WriteMode::And:    px_[idx] &= color; break;
                case WriteMode::Or:     px_[idx] |= color; break;
                case WriteMode::Xor:    px_[idx] ^= color; break;
            }
        }
    }
}

void GBitmap::drawRect(const Rect& r, uint8_t color, WriteMode mode) {
    if (r.w <= 0 || r.h <= 0) return;
    int x2 = r.x + r.w - 1, y2 = r.y + r.h - 1;
    drawLine(r.x, r.y, x2, r.y, color, mode);   // top
    drawLine(r.x, y2, x2, y2, color, mode);     // bottom
    drawLine(r.x, r.y, r.x, y2, color, mode);   // left
    drawLine(x2, r.y, x2, y2, color, mode);     // right
}

void GBitmap::replaceColor(const Rect& r, uint8_t oldColor, uint8_t newColor) {
    Rect c = Rect::intersect(r, rect());
    if (c.empty()) return;
    for (int yy = c.y; yy < c.y + c.h; ++yy) {
        std::size_t base = static_cast<std::size_t>(yy) * w_;
        for (int xx = c.x; xx < c.x + c.w; ++xx)
            if (px_[base + xx] == oldColor) px_[base + xx] = newColor;
    }
}

void GBitmap::drawBitmap(int x, int y, const GBitmap& src, bool transparent) {
    drawBitmap(x, y, src, src.rect(), transparent);
}

void GBitmap::drawBitmap(int destX, int destY, const GBitmap& src, const Rect& srcRect, bool transparent) {
    Rect s = Rect::intersect(srcRect, src.rect());
    // clamp negative destination by shrinking the source window (mirrors C#)
    if (destX < 0) { s.x -= destX; s.w += destX; destX = 0; }
    if (destY < 0) { s.y -= destY; s.h += destY; destY = 0; }

    Rect d = Rect::intersect(Rect{destX, destY, s.w, s.h}, rect());
    if (d.empty()) return;
    s.w = d.w; s.h = d.h;

    for (int i = 0; i < d.h; ++i) {
        std::size_t sp = static_cast<std::size_t>(s.y + i) * src.w_ + s.x;
        std::size_t dp = static_cast<std::size_t>(d.y + i) * w_ + d.x;
        for (int j = 0; j < d.w; ++j) {
            uint8_t v = src.px_[sp + j];
            if (v != 0 || !transparent) px_[dp + j] = v;
        }
    }
}

namespace {
uint8_t color18to8(int v) { return uint8_t((255 * (v & 0x3F)) / 63); }
uint8_t color8to18(int v) { return uint8_t((v * 63) / 255); }
} // namespace

void GBitmap::setPaletteFromColorStruct(const std::vector<uint8_t>& cs) {
    if (cs.size() < 6 || cs[0] != 0x4d || cs[1] != 0x30) return; // not a 0x304d struct
    int from = cs[4];
    int to = cs[5];
    int count = to - from + 1;
    std::size_t pos = 6;
    for (int i = 0; i < count; ++i) {
        if (pos + 3 > cs.size()) break;
        palette.set(uint8_t(from + i), color18to8(cs[pos]), color18to8(cs[pos + 1]), color18to8(cs[pos + 2]));
        pos += 3;
    }
}

std::vector<uint8_t> GBitmap::exportPaletteColorStruct(int from, int to) const {
    int count = to - from + 1;
    if (count < 0) count = 0;
    int blockLen = 2 + count * 3; // [from][to] + triples (matches the 0x304d block body)
    std::vector<uint8_t> cs;
    cs.push_back(0x4d);
    cs.push_back(0x30);
    cs.push_back(uint8_t(blockLen & 0xFF));
    cs.push_back(uint8_t((blockLen >> 8) & 0xFF));
    cs.push_back(uint8_t(from));
    cs.push_back(uint8_t(to));
    for (int i = 0; i < count; ++i) {
        const RGB& c = palette.colors[uint8_t(from + i)];
        cs.push_back(color8to18(c.r));
        cs.push_back(color8to18(c.g));
        cs.push_back(color8to18(c.b));
    }
    return cs;
}

void GBitmap::toRGBA(std::vector<uint8_t>& out) const {
    out.resize(static_cast<std::size_t>(w_) * h_ * 4);
    for (std::size_t i = 0; i < px_.size(); ++i) {
        const RGB& c = palette.colors[px_[i]];
        out[i * 4 + 0] = c.r;
        out[i * 4 + 1] = c.g;
        out[i * 4 + 2] = c.b;
        out[i * 4 + 3] = 255;
    }
}

} // namespace oc1
