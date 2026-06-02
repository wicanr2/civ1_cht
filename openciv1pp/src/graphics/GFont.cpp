#include "GFont.h"

namespace oc1 {

int GFont::buildAsciiFromFreeType(int height) {
    pixelHeight = height;
    chars_.clear();
    int built = 0;
    for (uint32_t cp = 0x20; cp <= 0x7E; ++cp) {
        const CjkGlyphCache::Glyph* g = CjkGlyphCache::instance().getGlyph(cp, height);
        if (g) {
            chars_[cp] = *g; // copy into the bitmap font
            ++built;
        }
    }
    return built;
}

} // namespace oc1
