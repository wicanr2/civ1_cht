#include "PicLoader.h"
#include "Compression.h"
#include <fstream>
#include <sstream>

namespace oc1 {

namespace {

uint8_t color18to8(int v) { return uint8_t((255 * (v & 0x3F)) / 63); }

struct Reader {
    const std::vector<uint8_t>& d;
    std::size_t i = 0;
    explicit Reader(const std::vector<uint8_t>& data) : d(data) {}
    bool has(std::size_t n) const { return i + n <= d.size(); }
    uint16_t u16() { uint16_t v = uint16_t(d[i] | (d[i + 1] << 8)); i += 2; return v; }
};

} // namespace

std::unique_ptr<GBitmap> loadPic(const std::vector<uint8_t>& bytes, bool preferHiColor) {
    std::unique_ptr<GBitmap> bitmap;
    Reader r(bytes);

    // palette accumulates across blocks (index -> RGB)
    struct PalEntry { int index; uint8_t rr, gg, bb; };
    std::vector<PalEntry> pal;

    while (r.has(4)) {
        uint16_t sig = r.u16();
        uint16_t len = r.u16();
        if (!r.has(len)) break;
        std::size_t blockStart = r.i;
        const uint8_t* block = bytes.data() + blockStart;
        r.i += len;

        switch (sig) {
            case 0x3045: // 8-bit palette
                if (pal.empty() || !preferHiColor) pal.clear();
                break;

            case 0x304d: { // 18-bit palette
                if (!pal.empty() && !preferHiColor) break;
                pal.clear();
                if (len < 2) break;
                int idx = block[0];
                int endx = block[1];
                int count = endx - idx + 1;
                std::size_t p = 2;
                for (int k = 0; k < count; ++k) {
                    if (p + 3 > len) break;
                    pal.push_back({idx + k, color18to8(block[p]), color18to8(block[p + 1]), color18to8(block[p + 2])});
                    p += 3;
                }
                break;
            }

            case 0x3058: { // 8-bit image, RLE+LZW
                if (bitmap && !preferHiColor) break;
                if (len < 5) break;
                int w = block[0] | (block[1] << 8);
                int h = block[2] | (block[3] << 8);
                int maxBits = block[4];
                if (w <= 0 || h <= 0 || maxBits <= 7) break;

                std::vector<uint8_t> lzwIn(block + 5, block + len);
                std::vector<uint8_t> lzwOut, rleOut;
                lzwDecompress(lzwIn, lzwOut, 9, maxBits);
                rleDecompress(lzwOut, rleOut);

                bitmap = std::make_unique<GBitmap>(w, h);
                for (auto& e : pal) bitmap->palette.set(uint8_t(e.index), e.rr, e.gg, e.bb);
                auto& px = bitmap->pixelsMut();
                std::size_t n = std::min(px.size(), rleOut.size());
                for (std::size_t i = 0; i < n; ++i) px[i] = rleOut[i];
                break;
            }

            case 0x3158: { // 4-bit packed image, RLE+LZW
                if (bitmap && preferHiColor) break;
                if (len < 5) break;
                int w = block[0] | (block[1] << 8);
                int h = block[2] | (block[3] << 8);
                int maxBits = block[4];
                if (w <= 0 || h <= 0 || maxBits <= 7) break;

                std::vector<uint8_t> lzwIn(block + 5, block + len);
                std::vector<uint8_t> lzwOut, rleOut;
                lzwDecompress(lzwIn, lzwOut, 9, maxBits);
                rleDecompress(lzwOut, rleOut);

                bitmap = std::make_unique<GBitmap>(w, h);
                for (auto& e : pal) bitmap->palette.set(uint8_t(e.index), e.rr, e.gg, e.bb);
                auto& px = bitmap->pixelsMut();
                std::size_t addr = 0, src = 0;
                for (int y = 0; y < h && src < rleOut.size(); ++y) {
                    for (int x = 0; x < w; ++x) {
                        if (src >= rleOut.size()) break;
                        int c = rleOut[src++];
                        if (addr < px.size()) px[addr++] = uint8_t(c & 0x0F);
                        if (x + 1 < w) { if (addr < px.size()) px[addr++] = uint8_t((c & 0xF0) >> 4); ++x; }
                    }
                }
                break;
            }

            default:
                return nullptr; // undefined block type
        }
    }
    return bitmap;
}

std::unique_ptr<GBitmap> loadPicFile(const std::string& path, bool preferHiColor) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return nullptr;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return loadPic(bytes, preferHiColor);
}

std::vector<uint8_t> buildPic8(const GBitmap& bmp) {
    std::vector<uint8_t> out;
    auto pushU16 = [&](std::vector<uint8_t>& v, uint16_t x) {
        v.push_back(uint8_t(x & 0xFF)); v.push_back(uint8_t(x >> 8));
    };

    // 0x304d palette block: start=0, end=255, 256 * (r,g,b) at 6-bit
    {
        std::vector<uint8_t> blk;
        blk.push_back(0);    // start index
        blk.push_back(255);  // end index
        for (int i = 0; i < 256; ++i) {
            const RGB& c = bmp.palette.colors[i];
            blk.push_back(uint8_t((c.r * 63) / 255));
            blk.push_back(uint8_t((c.g * 63) / 255));
            blk.push_back(uint8_t((c.b * 63) / 255));
        }
        pushU16(out, 0x304d);
        pushU16(out, uint16_t(blk.size()));
        out.insert(out.end(), blk.begin(), blk.end());
    }

    // 0x3058 image block: w,h then LZW(RLE(pixels)) — LZW output starts with maxBits
    {
        std::vector<uint8_t> rleOut, lzwOut;
        rleCompress(bmp.pixels(), rleOut, 4);
        lzwCompress(rleOut, lzwOut, 9, 11);

        std::vector<uint8_t> blk;
        pushU16(blk, uint16_t(bmp.width()));
        pushU16(blk, uint16_t(bmp.height()));
        blk.insert(blk.end(), lzwOut.begin(), lzwOut.end());

        pushU16(out, 0x3058);
        pushU16(out, uint16_t(blk.size()));
        out.insert(out.end(), blk.begin(), blk.end());
    }
    return out;
}

} // namespace oc1
