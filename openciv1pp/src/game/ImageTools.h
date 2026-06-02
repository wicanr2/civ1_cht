// ImageTools.h — ported CodeObject (OpenCiv1 ImageTools.cs).
//
// The .pic/.map image + icon loader. The actual .pic decoding (RLE+LZW, blocks,
// 4-/8-bit images) lives in the PicLoader module and is reused here untouched.
// This layer only wires a loaded image onto a GDriver screen (and copies its
// palette across), or registers it as a standalone bitmap ("icon"). The
// F0_2fa1_* method names mirror the C# so cross-referencing stays mechanical.
//
// Deviation from C#: the C# threads an `out byte[] palette` back into CPU memory
// (DS:0xba06/0xba08) and SetColorsFromColorStruct. The C++ GDriver has no such
// CPU-side palette registers; this port simplifies the palette plumbing to
// copying the loaded image's palette onto the target screen. See ImageTools.cpp.
#pragma once
#include "OpenCiv1Game.h"
#include <string>

namespace oc1 {

class ImageTools {
public:
    explicit ImageTools(OpenCiv1Game& parent);

    // Reads the filename from CPU memory (DS:filenamePtr), then delegates to the
    // std::string overload. palettePtr is accepted for signature parity with the
    // C# but, per the simplification above, only selects whether the loaded
    // palette is applied to the screen (mirrors the palettePtr==1 case).
    void F0_2fa1_01a2_LoadBitmapOrPalette(short screenID, uint16_t xPos, uint16_t yPos,
                                          uint16_t filenamePtr, uint16_t palettePtr);

    // Loads a .pic/.map onto the given screen at (xPos,yPos) and copies the
    // loaded image's palette onto that screen.
    void F0_2fa1_01a2_LoadBitmapOrPalette(short screenID, uint16_t xPos, uint16_t yPos,
                                          const std::string& filename, uint16_t palettePtr);

    // Loads an icon (a .pic) as a standalone GDriver bitmap and returns its id.
    int F0_2fa1_044c_LoadIcon(uint16_t filenamePtr);
    int F0_2fa1_044c_LoadIcon(const std::string& filename);

private:
    OpenCiv1Game& p;
    VCPU& cpu;
};

} // namespace oc1
