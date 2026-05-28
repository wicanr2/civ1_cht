#include "CjkGlyphCache.h"
#include <cstdlib>
#include <cstdio>
#include <ft2build.h>
#include FT_FREETYPE_H

namespace oc1 {

CjkGlyphCache& CjkGlyphCache::instance() {
    static CjkGlyphCache c;
    return c;
}

CjkGlyphCache::~CjkGlyphCache() {
    if (face_)    FT_Done_Face(static_cast<FT_Face>(face_));
    if (library_) FT_Done_FreeType(static_cast<FT_Library>(library_));
}

bool CjkGlyphCache::loadFont(const std::string& path, int faceIndex) {
    if (!library_) {
        FT_Library lib = nullptr;
        if (FT_Init_FreeType(&lib) != 0) return false;
        library_ = lib;
    }
    FT_Face face = nullptr;
    if (FT_New_Face(static_cast<FT_Library>(library_), path.c_str(), faceIndex, &face) != 0)
        return false;

    if (face_) FT_Done_Face(static_cast<FT_Face>(face_));
    face_ = face;
    fontLoaded_ = true;
    cache_.clear();
    return true;
}

std::string CjkGlyphCache::autoLoad() {
    const char* env = std::getenv("OPENCIV1_CJK_FONT");
    std::vector<std::string> candidates;
    if (env && env[0]) candidates.emplace_back(env);
    candidates.insert(candidates.end(), {
        "assets/zh_font.ttf",
        "assets/zh_font.ttc",
        "/usr/share/fonts/truetype/arphic/uming.ttc",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSansCJK-Regular.ttc",
        "C:/Windows/Fonts/mingliu.ttc",
        "C:/Windows/Fonts/msjh.ttc",
        "/System/Library/Fonts/PingFang.ttc",
    });
    for (const auto& p : candidates) {
        if (loadFont(p)) return p;
    }
    std::fprintf(stderr,
        "[CjkGlyphCache] No CJK font found. Set OPENCIV1_CJK_FONT or install "
        "AR PL UMing / Noto Sans CJK. Chinese text will render blank.\n");
    return {};
}

const CjkGlyphCache::Glyph* CjkGlyphCache::getGlyph(uint32_t codepoint, int pixelHeight) {
    if (!fontLoaded_ || pixelHeight <= 0) return nullptr;
    uint64_t key = (uint64_t(pixelHeight) << 32) | codepoint;
    auto it = cache_.find(key);
    if (it != cache_.end()) return &it->second;
    Glyph g = rasterize(codepoint, pixelHeight);
    auto res = cache_.emplace(key, std::move(g));
    return &res.first->second;
}

CjkGlyphCache::Glyph CjkGlyphCache::rasterize(uint32_t codepoint, int pixelHeight) {
    Glyph g;
    FT_Face face = static_cast<FT_Face>(face_);
    if (FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixelHeight)) != 0)
        return g;
    // Hinted monochrome (1-bit) rendering: the palette framebuffer can only
    // store on/off ink, so a crisp hinted bitmap beats thresholding an
    // anti-aliased mask — especially for thin Latin strokes at small sizes.
    if (FT_Load_Char(face, codepoint, FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) != 0)
        return g;

    FT_GlyphSlot slot = face->glyph;
    g.advance = static_cast<int>(slot->advance.x >> 6);
    g.left    = slot->bitmap_left;
    g.top     = slot->bitmap_top;
    g.width   = static_cast<int>(slot->bitmap.width);
    g.height  = static_cast<int>(slot->bitmap.rows);

    if (g.advance <= 0) g.advance = pixelHeight;
    if (g.width > 0 && g.height > 0) {
        g.coverage.assign(static_cast<std::size_t>(g.width) * g.height, 0);
        const unsigned char* src = slot->bitmap.buffer;
        int pitch = slot->bitmap.pitch; // bytes per row (may be negative)
        if (slot->bitmap.pixel_mode == FT_PIXEL_MODE_MONO) {
            for (int y = 0; y < g.height; ++y) {
                const unsigned char* row = src + static_cast<std::ptrdiff_t>(y) * pitch;
                for (int x = 0; x < g.width; ++x) {
                    int bit = (row[x >> 3] >> (7 - (x & 7))) & 1;
                    g.coverage[static_cast<std::size_t>(y) * g.width + x] = bit ? 255 : 0;
                }
            }
        } else { // FT_PIXEL_MODE_GRAY fallback
            for (int y = 0; y < g.height; ++y) {
                const unsigned char* row = src + static_cast<std::ptrdiff_t>(y) * pitch;
                for (int x = 0; x < g.width; ++x)
                    g.coverage[static_cast<std::size_t>(y) * g.width + x] = row[x];
            }
        }
    }
    return g;
}

} // namespace oc1
