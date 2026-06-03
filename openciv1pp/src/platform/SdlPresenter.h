// SdlPresenter.h — the SDL2 display/input backend.
//
// Thin layer: uploads the GBitmap's RGBA conversion to a streaming SDL_Texture
// and presents it scaled to the window. This is the *only* place SDL2 appears;
// the engine and localization never reference it, so the backend can be swapped
// without touching game logic. Offscreen rendering (the PPM dumper) does not use
// this at all.
//
// Native framebuffer: 640x480 (the port's new logical size — see the
// 320x200->640x480 upscale push in main.cpp). When fb == 640x480 the renderer's
// logical size equals the window size and there's no letterbox; if a caller
// still passes a smaller fb (e.g. 320x200 for the intro slideshow), SDL
// letterboxes/scales the fb into the window.
#pragma once
#include "../graphics/GBitmap.h"
#include <functional>
#include <string>

namespace oc1 {

class SdlPresenter {
public:
    // Default window size: 1280x960 (2x the 640x480 native framebuffer).
    // Modern hi-DPI screens make the 1x native size painfully small, so the
    // out-of-the-box experience opens at 2x. `--window WxH` still overrides.
    // Logical FB stays 640x480, so coordinates / hit-tests are unaffected.
    static constexpr int DefaultScale = 2;
    static constexpr int DefaultWindowW = 640 * DefaultScale;
    static constexpr int DefaultWindowH = 480 * DefaultScale;

    // Clamp helper for the zoom hotkey logic, exposed as a static helper so
    // a headless test can exercise it without spinning up SDL.
    static int clampScale(int s) { return s < 1 ? 1 : (s > 4 ? 4 : s); }

    // fbWidth/fbHeight = framebuffer (logical) size. winW/winH = SDL window
    // size on screen (defaults to 640x480; pass 0 to fall back to defaults).
    // When fb == window the rendering is 1:1; otherwise SDL_RenderSetLogicalSize
    // scales/letterboxes the fb into the window.
    bool init(const char* title, int fbWidth, int fbHeight,
              int winW = DefaultWindowW, int winH = DefaultWindowH);
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
                         KeyLeft = 5, KeyRight = 6, KeyB = 7, KeyM = 8,
                         KeyR = 9, KeyI = 10, KeyW = 11, KeyP = 12,
                         KeyF = 13,  // SDLK_f — Fortify (+50% defense)
                         // ---- Tax/Luxury/Science rate slider keys ----
                         // SDLK_t cycles TAX +1 (decrements largest of the
                         // other two so sum stays 10). SDLK_l cycles LUX +1.
                         // SDLK_y cycles SCIENCE +1 (Y picked because SDLK_s
                         // is reserved for future Sentry; matches the
                         // documented "scYence" mnemonic in the task spec).
                         KeyT = 14, KeyL = 15, KeyY = 16;
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

    // ---- B6: TEXT INPUT (SDL_TEXTINPUT for name entry) ------------------
    // SDL2 normally suppresses SDL_TEXTINPUT events until StartTextInput()
    // is called (per SDL docs). The FrontEndFlow NAME state calls
    // startTextInput() on entry and stopTextInput() on leave so the IME /
    // dead-key composition path is active ONLY while a name field is open.
    void startTextInput();
    void stopTextInput();

    // Drains all SDL_TEXTINPUT events currently in the queue and returns
    // the concatenated UTF-8 bytes. Returns an empty string when no text
    // events are pending. Returns the accumulated TEST-FEED buffer as well
    // (set via appendTextInput) so headless tests don't need a real SDL
    // event loop.
    std::string pollTextInput();

    // Headless test hook: append UTF-8 bytes to an internal feed buffer.
    // The next pollTextInput() drains and returns them (mixed with any
    // real SDL_TEXTINPUT events). Used by the nameinputtest to simulate
    // typing without spinning up an SDL window. NOT thread-safe.
    void appendTextInput(const char* utf8);

    // ---- Zoom hotkey (+/- and F11) ----
    // setScale(s) clamps to [1,4] and resizes the SDL window to (640*s, 480*s),
    // re-centering it. The renderer's logical size stays at 640x480, so the
    // engine's coordinates are untouched. Returns the resulting clamped scale.
    // No-op (but updates the cached scale_) when the SDL window isn't open
    // (e.g. headless tests using SDL_VIDEODRIVER=dummy with no window).
    int setScale(int s);
    // Current window scale, 1..4. Initialised to DefaultScale (2).
    int scale() const { return scale_; }
    // Toggle fullscreen (SDL_WINDOW_FULLSCREEN_DESKTOP). Remembers the prior
    // windowed scale so exiting fullscreen restores it.
    void toggleFullscreen();
    bool isFullscreen() const { return fullscreen_; }

private:
    void* window_  = nullptr; // SDL_Window*
    void* renderer_ = nullptr; // SDL_Renderer*
    void* texture_ = nullptr;  // SDL_Texture*
    int fbW_ = 0, fbH_ = 0;
    bool quit_ = false;
    std::vector<uint8_t> rgba_;
    std::string textFeed_;  // B6: test-only feed (appended by appendTextInput)
    // ---- zoom / fullscreen state ----
    int  scale_ = DefaultScale;       // current windowed scale (1..4)
    int  prevScale_ = DefaultScale;   // remembered scale to restore on F11 exit
    bool fullscreen_ = false;         // FULLSCREEN_DESKTOP currently active
};

} // namespace oc1
