// MainIntro.h — ported CodeObject (OpenCiv1 MainIntro.cs).
//
// Ports the SEQUENCE of the authentic Civ1 INTRO (the F2_0000_0000 entry point
// and its companion F2_0000_0bd7 / F2_0000_152a / F2_0000_17d9 dialogs) as a
// simple slideshow of real DOS .PIC screens with brief Chinese intro captions.
// The original animation (palette cycling, mirror scroll-on, the credits.txt
// crawl, the planet-zoom + pop-up intro dialogs) is replaced by a one-screen-
// at-a-time advance-on-key/click/timer driven from the SDL presenter.
//
// Assets resolved against OpenCiv1Game::resourcePath() (set from --assets or
// the OPENCIV1_DOS_ASSETS env var). Without assets MainIntro::play() returns
// immediately (no-op fallback path); MainIntro::nextFrame() then returns false.
//
// Sequence (faithful to the C# function-by-function order — names kept):
//   F2_0000_0000      : LOGO.PIC                      (title splash)
//   F2_0000_0000      : PLANET1.PIC, PLANET2.PIC      (the rotating planet
//                       half-screens — drawn as two static frames here)
//   F2_0000_0bd7      : BIRTH0.PIC..BIRTH8.PIC        (the "birth of civ"
//                       sequence; one frame per BIRTHn.PIC)
//   F2_0000_152a      : CREDITS.TXT-backed final card (stubbed to a Chinese
//                       caption line on top of LOGO.PIC)
//
// STUBS (minimal safe no-ops; see // TODO(port) markers in the .cpp):
//   - palette-cycle timer slots / SoundTimer            : not modelled
//   - mirror scroll-on of LOGO.PIC + planet rotation    : not modelled (each
//                                                         frame is the static
//                                                         loaded .pic)
//   - credits.txt fscanf crawl + PlayTune music         : not modelled
//   - F2_0000_17d9 floating intro-dialog pop-ups        : not modelled
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

class MainIntro {
public:
    explicit MainIntro(OpenCiv1Game& parent);

    // The planned screen sequence (one .PIC filename per slide). Empty entries
    // mark slides that have NO backing asset (caption-only / stub frame).
    struct Slide {
        std::string picBase;   // e.g. "LOGO.PIC"; empty for caption-only frame
        std::string caption;   // brief Chinese intro line drawn over the slide
    };

    // The canonical slide list (kept here so tests can assert size + contents
    // without needing to play the intro).
    static const std::vector<Slide>& slides();

    // Faithful entry: render the slideshow into the supplied framebuffer,
    // advancing on key / click / timer. Without assets returns immediately
    // (logoLoaded == false). With assets it draws each slide and waits for the
    // SDL presenter's pollKey/pollMouse to advance (timer-only when headless).
    //
    // `interactive`: when true the function takes ownership of an SDL window via
    // a presenter passed by the caller (so MainIntro doesn't depend on SDL). The
    // caller is the one that owns the SdlPresenter; MainIntro just exposes the
    // per-screen drawing via nextFrame() below.
    //
    // Use nextFrame() to step through slides programmatically (headless / tests).
    void play();

    // Reset the slide cursor to the first slide.
    void reset() { cursor_ = 0; }

    // Draw the next slide into `fb` (must be at least 320x200, the DOS intro fb
    // size). Returns true if a slide was drawn and there are MORE slides to go
    // (cursor advances on each call); false when the slideshow is finished OR
    // when the asset directory is empty / the .PIC failed to load (no-op).
    bool nextFrame(GBitmap& fb);

    // Current slide index (0..slides().size()).
    int cursor() const { return cursor_; }
    int slideCount() const { return int(slides().size()); }

    // True when resourcePath() points at a directory with at least the first
    // intro asset (LOGO.PIC). Used by the no-asset short-circuit in play().
    bool hasAssets() const;

private:
    OpenCiv1Game& p;
    int cursor_ = 0;
};

} // namespace oc1
