// PicLoader.h — Civilization .pic image loader, ported from OpenCiv1
// (GBitmap.FromPICFile). A .pic is a sequence of blocks:
//   0x3045  8-bit palette        (ignored when preferHiColor)
//   0x304d  18-bit (6bpc) palette
//   0x3058  8-bit image, RLE+LZW
//   0x3158  4-bit packed image, RLE+LZW
#pragma once
#include "../graphics/GBitmap.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace oc1 {

// Decode a .pic from memory. Returns null on malformed input. The returned
// bitmap carries the palette parsed from the file.
std::unique_ptr<GBitmap> loadPic(const std::vector<uint8_t>& bytes, bool preferHiColor = true);

// Decode a .pic file from disk.
std::unique_ptr<GBitmap> loadPicFile(const std::string& path, bool preferHiColor = true);

// Encode an 8-bit GBitmap into a .pic (0x304d palette + 0x3058 image). Used for
// round-trip testing without copyrighted assets; also a usable exporter.
std::vector<uint8_t> buildPic8(const GBitmap& bmp);

} // namespace oc1
