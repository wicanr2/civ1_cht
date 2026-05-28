// SdlPresenter.h — the SDL2 display/input backend.
//
// Thin layer: uploads the GBitmap's RGBA conversion to a streaming SDL_Texture
// and presents it scaled to the window. This is the *only* place SDL2 appears;
// the engine and localization never reference it, so the backend can be swapped
// without touching game logic. Offscreen rendering (the PPM dumper) does not use
// this at all.
#pragma once
#include "../graphics/GBitmap.h"
#include <functional>

namespace oc1 {

class SdlPresenter {
public:
    // scale: integer upscale of the palette framebuffer (e.g. 320x200 -> x3).
    bool init(const char* title, int fbWidth, int fbHeight, int scale);
    void shutdown();

    // Presents one frame from the framebuffer. Returns false on quit request.
    bool present(const GBitmap& fb);

    // Pumps events; returns false when the user asked to quit.
    bool pumpEvents();

    // Polls one keyboard event and returns a mapped navigation code matching
    // MenuBoxDialog::NavKey (1=Up,2=Down,3=Enter,4=Esc), plus 5=Left,6=Right for
    // the map-scroller, or 0 if no relevant key is pending. SDL_QUIT and ESC
    // still flag quit() as before. ESC maps to KeyEsc (4) AND requests quit, so
    // an interactive menu can cancel cleanly.
    static constexpr int KeyUp = 1, KeyDown = 2, KeyEnter = 3, KeyEsc = 4,
                         KeyLeft = 5, KeyRight = 6;
    int pollKey();

    // ---- mouse input ----
    // Framebuffer-space (logical) coordinates. button: 1=left, 2=middle, 3=right
    // (matches SDL's button numbering). For motion events button is 0 and down
    // reflects the current button mask (true if any button held).
    struct MouseEvent { int x = 0, y = 0, button = 0; bool down = false; bool motion = false; };

    // Pulls one pending mouse event (button down/up/motion). Returns true on
    // success; out is populated. Coordinates are mapped back from the window's
    // SDL coords to the logical framebuffer coords (SetLogicalSize is set in
    // init), so x/y are always inside [0,fbW) x [0,fbH).
    bool pollMouse(MouseEvent& out);

    // True once the user asked to quit (SDL_QUIT or pressed ESC).
    bool quit() const { return quit_; }

private:
    void* window_  = nullptr; // SDL_Window*
    void* renderer_ = nullptr; // SDL_Renderer*
    void* texture_ = nullptr;  // SDL_Texture*
    int fbW_ = 0, fbH_ = 0;
    bool quit_ = false;
    std::vector<uint8_t> rgba_;
};

} // namespace oc1
