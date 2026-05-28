// CommonTools.h — ported CodeObject (OpenCiv1 CommonTools.cs).
//
// The game's grab-bag of low-level services: the main timer, the VGA palette
// cycling/transform engine, the sound-driver shims, screen-blit wrappers and the
// mouse-cursor state. The F0_* method names mirror the C# 1:1 so cross-
// referencing the disassembly stays mechanical.
//
// What is ported faithfully (self-contained, exercised by --commontest):
//   - The palette-cycle SLOT engine: F0_1000_0382_AddPaletteCycleSlot,
//     F0_1000_03fa_StartPaletteCycleSlot, F0_1000_042b_StopPaletteCycleSlot and
//     the per-tick F0_1000_044a_CyclePaletteTimer. These are pure logic over a
//     slot table + the live palette; here the "live palette" is the main
//     screen's GBitmap palette (graphics.screen(0).palette), which is exactly
//     what the C# GDriver.Get/SetPaletteColor read/write.
//   - F0_1000_033e_ResetWaitTimer (tick-count reset).
//   - F0_1000_0bfa_FillRectangle (rect-clipped fill via GDriver.fillRectangle).
//
// STUBS / deviations (each has a // TODO(port) in the .cpp). These depend on
// subsystems not yet ported; the methods exist with a minimal safe body so the
// class compiles and links:
//   - F0_1000_0000_InitializeTimer / F0_1000_0051_StopTimer / the private
//     timer worker: no background thread is started (the C# Timer +
//     VCPU.GraphicsLock threading model is out of scope for the headless port).
//   - The palette TRANSFORM engine (F0_1000_04aa/04d4 + the 0631 timer): needs
//     HSVColor / TransformColor / GBitmap.Color18ToColor, none ported. Stubbed.
//   - Sound shims (F0_1000_0a2b/0a32/0a39/0a40/0a47/0a4e): the Sound CodeObject
//     is not ported; these no-op (SoundWorker/SoundTimer return 0).
//   - Screen-blit wrappers (F0_1000_083f/0846/0797/084d): the GDriver
//     F0_VGA_06b7/0c3e/0d47 entry points are not present; the Var_5402/5403
//     bookkeeping that survives is ported, the blit itself is elided.
//   - Mouse state (F0_1000_163e/1697/16ae/16db/170b/17db): the Var_58xx mouse
//     globals and VCPU.MouseLocation/MouseButtons do not exist; modelled on a
//     small local MouseState mirror so the bookkeeping logic is still ported.
//   - F0_1182_0134_WaitTimer: no Thread.Sleep / DoEvents; resets the tick count
//     only (the C# real-time wait is meaningless headless).
#pragma once
#include "OpenCiv1Game.h"
#include <cstdint>
#include <map>
#include <vector>

namespace oc1 {

class CommonTools {
public:
    explicit CommonTools(OpenCiv1Game& parent);

    // ---- timer (STUB: no background thread) ----
    void F0_1000_0000_InitializeTimer();
    void F0_1000_0051_StopTimer();

    // ---- palette cycling (faithful port) ----
    // Advances every active cycle slot by one tick and rewrites the live palette.
    void F0_1000_044a_CyclePaletteTimer();
    // Defines/replaces a cycle slot (index 0..8) over palette range
    // [fromColorIndex, toColorIndex]. Captures the current palette colours.
    void F0_1000_0382_AddPaletteCycleSlot(int index, int speed,
                                          uint8_t fromColorIndex, uint8_t toColorIndex);
    void F0_1000_03fa_StartPaletteCycleSlot(int index);
    void F0_1000_042b_StopPaletteCycleSlot(int index);

    // ---- palette transform (STUB: needs HSVColor/TransformColor) ----
    void F0_1000_0631_TransformPaletteTimer();
    void F0_1000_04aa_TransformPalette(int speed, const std::vector<uint8_t>& palette);
    void F0_1000_04d4_TransformPaletteToColor(int speed, RGB color);

    // ---- wait-timer (faithful tick-count reset; real sleep elided) ----
    void F0_1000_033e_ResetWaitTimer();
    void F0_1182_0134_WaitTimer(int waitTime);

    // ---- screen-blit wrappers (Var_5402/5403 bookkeeping ported; blit elided) ----
    void F0_1000_083f(int x, int y, int bitmapPtr);
    void F0_1000_0846(int screenID);
    void F0_1000_0797_DrawBitmapToScreen(const CRectangle& rect, int xPos, int yPos, int bitmapPtr);
    void F0_1000_084d_DrawBitmapToScreen(const CRectangle& rect, int xPos, int yPos, int bitmapPtr);

    // ---- sound shims (STUB: Sound CodeObject not ported) ----
    void     F0_1000_0a2b_InitSound();
    void     F0_1000_0a32_PlayTune(int16_t tune, uint16_t param2);
    void     F0_1000_0a39_CloseSound();
    uint16_t F0_1000_0a40_SoundWorker();
    void     F0_1000_0a47_FastSoundWorker();
    uint16_t F0_1000_0a4e_SoundTimer();

    // ---- fill rectangle (faithful port) ----
    void F0_1000_0bfa_FillRectangle(const CRectangle& rect, int x, int y,
                                    int width, int height, uint16_t mode);

    // ---- mouse state (Var_58xx mirror; bookkeeping ported) ----
    void F0_1000_163e_InitMouse();
    void F0_1000_1697(int x, int y, int bitmapPtr);
    void F0_1000_16ae(int x, int y);
    void F0_1000_16db();
    void F0_1000_170b();
    void F0_1000_17db_MouseEvent();

    // --- ported game-state mirrors (the C# stores these on OpenCiv1Game; kept
    //     here so the port is self-contained until those globals are added) ---
    int tickCount = 0;            // Var_5c_TickCount
    int var_5402 = 0;             // Var_5402
    int var_5403 = 0;             // Var_5403
    int mouseNewX = 0;            // Var_586e_MouseNewX
    int mouseNewY = 0;            // Var_5870_MouseNewY
    int mouseNewButtons = 0;      // Var_5872_MouseNewButtons
    int mouseNewButtonsOr = 0;    // Var_5874_MouseNewButtonsOr
    int mouseIcon = 0;            // Var_5876_MouseIcon
    int mouseIconXOffset = 0;     // Var_5878_MouseIconXOffset
    int mouseIconYOffset = 0;     // Var_587a_MouseIconYOffset
    int var_587d = 0;             // Var_587d

private:
    // A palette-cycle slot, ported from PaletteCycleSlot.cs: a captured run of
    // palette colours that is rotated by one entry every `speed` ticks.
    struct PaletteCycleSlot {
        int speed = 0;
        int speedCount = 0;
        int startPosition = 0;     // first palette index this slot owns
        int currentPosition = 0;   // rotation offset
        bool active = false;
        std::vector<RGB> palette;  // the captured colours (length = range size)
    };

    // The live palette the C# GDriver.Get/SetPaletteColor touch == main screen's.
    RGB getPaletteColor(uint8_t index) const;
    void setPaletteColor(uint8_t index, RGB color);

    OpenCiv1Game& p;
    VCPU& cpu;

    std::map<int, PaletteCycleSlot> paletteCycleSlots_;
    bool timerStarted_ = false;
};

} // namespace oc1
