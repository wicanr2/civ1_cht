// Palette.h — 256-entry RGB palette. The real game loads its palette from the
// original .pal assets at runtime; this provides a sane default so the engine
// renders standalone. Index 0 is the background.
#pragma once
#include <array>
#include <cstdint>

namespace oc1 {

struct RGB { uint8_t r = 0, g = 0, b = 0; };

class Palette {
public:
    std::array<RGB, 256> colors{};

    Palette() { loadDefaultVga(); }

    void set(uint8_t i, uint8_t r, uint8_t g, uint8_t b) { colors[i] = {r, g, b}; }

    // A standard 16-colour VGA set in the low indices, plus a grey ramp, so the
    // demo and early-ported screens have usable colours before .pal loading.
    void loadDefaultVga() {
        static const RGB vga16[16] = {
            {0,0,0},      {0,0,170},    {0,170,0},    {0,170,170},
            {170,0,0},    {170,0,170},  {170,85,0},   {170,170,170},
            {85,85,85},   {85,85,255},  {85,255,85},  {85,255,255},
            {255,85,85},  {255,85,255}, {255,255,85}, {255,255,255},
        };
        for (int i = 0; i < 16; ++i) colors[i] = vga16[i];
        for (int i = 16; i < 256; ++i) {
            uint8_t g = static_cast<uint8_t>((i - 16) * 255 / (255 - 16));
            colors[i] = {g, g, g};
        }
    }
};

} // namespace oc1
