// Compression.h — Civilization's RLE + LZW codecs, ported from OpenCiv1
// (Compression/RLE.cs, Compression/LZW.cs). Used to decode .pic image blocks.
//
// Pipeline (per OpenCiv1): raw -> RLE.Compress -> LZW.Compress -> stored;
// decode reverses it: stored -> LZW.Decompress -> RLE.Decompress -> raw.
#pragma once
#include <cstdint>
#include <vector>

namespace oc1 {

// RLE: 0x90 is the run marker. `0x90 0x00` is a literal 0x90; `b ... 0x90 n`
// means the preceding byte repeats to a total run length of n.
void rleCompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int minCount);
void rleDecompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

// LZW variant: codes are LSB-first, bit width grows from startBits..maxBits then
// the dictionary resets. Compress emits maxBits as its first output byte.
void lzwCompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int startBits, int maxBits);
void lzwDecompress(const std::vector<uint8_t>& in, std::vector<uint8_t>& out, int startBits, int maxBits);

} // namespace oc1
