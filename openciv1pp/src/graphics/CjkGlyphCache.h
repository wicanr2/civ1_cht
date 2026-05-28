// CjkGlyphCache.h — FreeType-backed glyph rasteriser + cache.
//
// Produces 8-bit coverage masks for any Unicode codepoint, cached per
// (codepoint, pixel height). Backend-agnostic: GBitmap blits the coverage into
// its palette framebuffer, so the same masks work under any presenter (SDL2,
// offscreen PPM dump, a future backend). The original Civilization bitmap fonts
// only carry ASCII, so all translated Chinese text needs this path.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace oc1 {

class CjkGlyphCache {
public:
    struct Glyph {
        int width = 0;        // coverage box width
        int height = 0;       // coverage box height
        int advance = 0;      // horizontal pen advance (px)
        int left = 0;         // x bearing from pen
        int top = 0;          // y bearing above baseline
        std::vector<uint8_t> coverage; // row-major, width*height, 0..255
    };

    static CjkGlyphCache& instance();

    // Opens a TTF/TTC/OTF. Returns false if it could not be loaded.
    bool loadFont(const std::string& path, int faceIndex = 0);

    // Searches OPENCIV1_CJK_FONT, then common system locations. Returns the
    // path that loaded, or empty on failure.
    std::string autoLoad();

    bool fontAvailable() const { return fontLoaded_; }

    // Cached coverage for codepoint at pixelHeight. Returns nullptr if no font.
    const Glyph* getGlyph(uint32_t codepoint, int pixelHeight);

    ~CjkGlyphCache();

private:
    CjkGlyphCache() = default;
    Glyph rasterize(uint32_t codepoint, int pixelHeight);

    bool fontLoaded_ = false;
    void* library_ = nullptr; // FT_Library
    void* face_ = nullptr;    // FT_Face
    std::unordered_map<uint64_t, Glyph> cache_;
};

} // namespace oc1
