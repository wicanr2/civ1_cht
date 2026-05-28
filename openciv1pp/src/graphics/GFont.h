// GFont.h — pre-rasterised bitmap font (the ASCII path).
//
// In the original game this is loaded from font.cv; here we bootstrap the ASCII
// range from FreeType so the engine renders standalone. Either way the consumer
// (GBitmap::drawString) is the same: it looks up a coverage Glyph per codepoint.
// CJK codepoints bypass this and go to CjkGlyphCache.
#pragma once
#include "CjkGlyphCache.h"
#include <cstdint>
#include <unordered_map>

namespace oc1 {

class GFont {
public:
    int pixelHeight = 16;
    int charSpacing = 1;
    int lineSpacing = 2;

    // Populate ASCII 0x20..0x7E from the currently loaded CjkGlyphCache font.
    // Returns the number of glyphs built. (Stand-in for the Civ font.cv loader.)
    int buildAsciiFromFreeType(int pixelHeight);

    const CjkGlyphCache::Glyph* get(uint32_t cp) const {
        auto it = chars_.find(cp);
        return it == chars_.end() ? nullptr : &it->second;
    }

private:
    std::unordered_map<uint32_t, CjkGlyphCache::Glyph> chars_;
};

} // namespace oc1
