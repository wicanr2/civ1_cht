#include "ImageTools.h"
#include "../resource/PicLoader.h"
#include <algorithm>
#include <cctype>

namespace oc1 {

namespace {
// Lowercased file extension including the dot (".pic"), or "" if none.
std::string extLower(const std::string& path) {
    std::size_t dot = path.find_last_of('.');
    std::size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
    std::string e = path.substr(dot);
    std::transform(e.begin(), e.end(), e.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    return e;
}
} // namespace

ImageTools::ImageTools(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {}

void ImageTools::F0_2fa1_01a2_LoadBitmapOrPalette(short screenID, uint16_t xPos, uint16_t yPos,
                                                  uint16_t filenamePtr, uint16_t palettePtr) {
    F0_2fa1_01a2_LoadBitmapOrPalette(screenID, xPos, yPos,
                                     cpu.ReadString(cpu.DS.u16(), filenamePtr), palettePtr);
}

void ImageTools::F0_2fa1_01a2_LoadBitmapOrPalette(short screenID, uint16_t xPos, uint16_t yPos,
                                                  const std::string& filename, uint16_t palettePtr) {
    // C# prefixes ResourcePath + uppercases the DOS filename. The C++ resource
    // path is supplied directly by callers/tests, so use the filename as given.
    const std::string& filename1 = filename;
    std::string ext = extLower(filename1);

    if (screenID >= 0 && (ext == ".pic" || ext == ".map")) {
        if (!p.graphics.hasScreen(screenID))
            throw std::runtime_error("The page is not allocated");

        // PicLoader replaces screen->LoadPIC + GBitmap.FromPICFile (no reimpl).
        std::unique_ptr<GBitmap> loaded = loadPicFile(filename1, true);
        if (!loaded) throw std::runtime_error("failed to load .pic: " + filename1);

        GBitmap& scr = p.graphics.screen(screenID);
        // Simplification of the C# `out byte[] palette` -> CPU memory plumbing:
        // we copy the loaded palette onto the screen. The C# applied the palette
        // to the live display only for palettePtr==1/0xba06; mirror that gate.
        if (palettePtr == 1 || palettePtr == 0xba06)
            scr.copyPaletteFrom(*loaded);
        scr.drawBitmap(xPos, yPos, *loaded, false);
    }
    // else: C# reads only the palette (GBitmap.ReadPaletteFromPICFile). With the
    // palette simplified to the screen, there is no CPU-side palette buffer to
    // fill, so this branch is a no-op in the port.
}

int ImageTools::F0_2fa1_044c_LoadIcon(uint16_t filenamePtr) {
    return F0_2fa1_044c_LoadIcon(cpu.ReadString(VCPU::ToLinearAddress(cpu.DS.u16(), filenamePtr)));
}

int ImageTools::F0_2fa1_044c_LoadIcon(const std::string& filename) {
    // C# GDriver.LoadIcon decodes the image and stores it in the icon registry,
    // returning its id. The C++ analogue is GDriver.addBitmap (bitmap registry,
    // ids from 0xb000). Returns -1 on failure.
    std::unique_ptr<GBitmap> loaded = loadPicFile(filename, true);
    if (!loaded) return -1;
    return p.graphics.addBitmap(std::move(*loaded));
}

} // namespace oc1
