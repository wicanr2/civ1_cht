#include "CommonTools.h"
#include "../graphics/GDriver.h"

namespace oc1 {

CommonTools::CommonTools(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {
    (void)cpu; // CPU is the transform-palette read path (stubbed); kept for parity.
}

// The C# GDriver.Get/SetPaletteColor read/write the active 256-colour palette.
// Here that is the main screen's GBitmap palette.
RGB CommonTools::getPaletteColor(uint8_t index) const {
    return p.graphics.screen(GDriver::MainScreen).palette.colors[index];
}
void CommonTools::setPaletteColor(uint8_t index, RGB color) {
    p.graphics.screen(GDriver::MainScreen).palette.colors[index] = color;
}

// ---------------------------------------------------------------------------
// Timer
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_0000_InitializeTimer() {
    // TODO(port): InitializeTimer — the C# spins up a System.Threading.Timer that
    // fires the palette/sound worker every ~10ms under VCPU.GraphicsLock. The
    // headless port has no real-time clock or thread; flag it started so
    // StopTimer is symmetric. Drive the work directly via CyclePaletteTimer().
    timerStarted_ = true;
}

void CommonTools::F0_1000_0051_StopTimer() {
    // TODO(port): StopTimer — no background timer to dispose; just clear the flag.
    timerStarted_ = false;
}

// ---------------------------------------------------------------------------
// Palette cycling (faithful port)
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_044a_CyclePaletteTimer() {
    for (auto& kv : paletteCycleSlots_) {
        PaletteCycleSlot& slot = kv.second;
        if (!slot.active) continue;

        slot.speedCount++;
        if (slot.speedCount > slot.speed) {
            slot.speedCount = 0;
            int len = int(slot.palette.size());
            if (len == 0) continue;
            slot.currentPosition = (slot.currentPosition + 1) % len;

            for (int j = 0; j < len; j++) {
                int index = (slot.currentPosition + j) % len;
                setPaletteColor(uint8_t(slot.startPosition + index), slot.palette[std::size_t(j)]);
            }
        }
    }
}

void CommonTools::F0_1000_0382_AddPaletteCycleSlot(int index, int speed,
                                                   uint8_t fromColorIndex, uint8_t toColorIndex) {
    if (index < 0 || index > 8) return;            // C#: ArgumentOutOfRangeException
    if (fromColorIndex > toColorIndex) return;     // C#: ArgumentOutOfRangeException

    auto buildPalette = [&]() {
        std::vector<RGB> pal(std::size_t(toColorIndex - fromColorIndex) + 1);
        for (std::size_t i = 0; i < pal.size(); i++)
            pal[i] = getPaletteColor(uint8_t(fromColorIndex + int(i)));
        return pal;
    };

    auto it = paletteCycleSlots_.find(index);
    if (it != paletteCycleSlots_.end()) {
        // restore old slot palette to the live palette before replacing it
        PaletteCycleSlot& oldSlot = it->second;
        if (oldSlot.active) {
            for (std::size_t i = 0; i < oldSlot.palette.size(); i++)
                setPaletteColor(uint8_t(oldSlot.startPosition + int(i)), oldSlot.palette[i]);
        }
        PaletteCycleSlot newSlot;
        newSlot.speed = speed;
        newSlot.startPosition = fromColorIndex;
        newSlot.palette = buildPalette();
        it->second = std::move(newSlot);
    } else {
        PaletteCycleSlot slot;
        slot.speed = speed;
        slot.startPosition = fromColorIndex;
        slot.palette = buildPalette();
        paletteCycleSlots_.emplace(index, std::move(slot));
    }
}

void CommonTools::F0_1000_03fa_StartPaletteCycleSlot(int index) {
    auto it = paletteCycleSlots_.find(index);
    if (it != paletteCycleSlots_.end())
        it->second.active = true;
}

void CommonTools::F0_1000_042b_StopPaletteCycleSlot(int index) {
    auto it = paletteCycleSlots_.find(index);
    if (it == paletteCycleSlots_.end()) return;
    PaletteCycleSlot& slot = it->second;
    if (slot.active) {
        // restore the captured (un-rotated) colours to the live palette
        for (std::size_t i = 0; i < slot.palette.size(); i++)
            setPaletteColor(uint8_t(slot.startPosition + int(i)), slot.palette[i]);
    }
    slot.active = false;
}

// ---------------------------------------------------------------------------
// Palette transform (STUB)
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_0631_TransformPaletteTimer() {
    // TODO(port): TransformPaletteTimer — needs the TransformColor interpolation
    // (HSVColor lerp) which is not ported. No-op.
}

void CommonTools::F0_1000_04aa_TransformPalette(int speed, const std::vector<uint8_t>& palette) {
    // TODO(port): TransformPalette — needs HSVColor.FromColor / TransformColor and
    // GBitmap.Color18ToColor (18-bit palette decode). Not ported; no-op so callers
    // link. The 6-byte header skip + 3-bytes-per-colour layout is documented here.
    (void)speed; (void)palette;
}

void CommonTools::F0_1000_04d4_TransformPaletteToColor(int speed, RGB color) {
    // TODO(port): TransformPaletteToColor — same HSVColor/TransformColor dependency
    // as above. Not ported; no-op.
    (void)speed; (void)color;
}

// ---------------------------------------------------------------------------
// Wait-timer
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_033e_ResetWaitTimer() {
    tickCount = 0;
}

void CommonTools::F0_1182_0134_WaitTimer(int waitTime) {
    // TODO(port): WaitTimer — the C# sleeps max(waitTime*12, 1) ms then DoEvents().
    // Headless: reset the tick count only (no real-time wait).
    (void)waitTime;
    F0_1000_033e_ResetWaitTimer();
}

// ---------------------------------------------------------------------------
// Screen-blit wrappers (Var_5402/5403 bookkeeping ported; blit elided)
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_083f(int x, int y, int bitmapPtr) {
    // TODO(port): the C# blits the mouse-cursor bitmap to the screen via
    // GDriver.F0_VGA_0270; that entry point is not present. Bookkeeping ported.
    (void)x; (void)y; (void)bitmapPtr;
    if (var_5403 != 0) var_5402 = 0;
}

void CommonTools::F0_1000_0846(int screenID) {
    // Ported: delegates to the now-available GDriver screen-composite-with-effect.
    p.graphics.F0_VGA_06b7_DrawScreenToMainScreenWithEffect(screenID);
    if (var_5403 != 0) var_5402 = 0;
}

void CommonTools::F0_1000_0797_DrawBitmapToScreen(const CRectangle& rect, int xPos, int yPos, int bitmapPtr) {
    // Ported: delegates to GDriver.F0_VGA_0c3e_DrawBitmapToScreen.
    p.graphics.F0_VGA_0c3e_DrawBitmapToScreen(rect, xPos, yPos, bitmapPtr);
    if (var_5403 != 0) var_5402 = 0;
}

void CommonTools::F0_1000_084d_DrawBitmapToScreen(const CRectangle& rect, int xPos, int yPos, int bitmapPtr) {
    // Ported: delegates to GDriver.F0_VGA_0d47_DrawBitmapToScreen.
    p.graphics.F0_VGA_0d47_DrawBitmapToScreen(rect, xPos, yPos, bitmapPtr);
    if (var_5403 != 0) var_5402 = 0;
}

// ---------------------------------------------------------------------------
// Sound shims (STUB: Sound CodeObject not ported)
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_0a2b_InitSound() {
    // TODO(port): Sound.F0_0000_0048_InitSound — Sound CodeObject not ported.
}
void CommonTools::F0_1000_0a32_PlayTune(int16_t tune, uint16_t param2) {
    // TODO(port): Sound.F0_0000_0062_PlayTune (gated on GameData.GameSettingFlags.Sound).
    (void)tune; (void)param2;
}
void CommonTools::F0_1000_0a39_CloseSound() {
    // TODO(port): Sound.F0_0000_006a_CloseSound.
}
uint16_t CommonTools::F0_1000_0a40_SoundWorker() {
    // TODO(port): Sound.F0_0000_0055_SoundWorker.
    return 0;
}
void CommonTools::F0_1000_0a47_FastSoundWorker() {
    // TODO(port): Sound.F0_0000_005c_FastSoundWorker.
}
uint16_t CommonTools::F0_1000_0a4e_SoundTimer() {
    // TODO(port): Sound.F0_0000_005d_SoundTimer.
    return 0;
}

// ---------------------------------------------------------------------------
// Fill rectangle (faithful port)
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_0bfa_FillRectangle(const CRectangle& rect, int x, int y,
                                             int width, int height, uint16_t mode) {
    if (width > 0 && height > 0) {
        // C#: F0_VGA_040a_FillRectangle(screenID, GRectangle(left+x, top+y, w, h),
        // color = mode & 0xff, fillMode = (mode>>8)&0xff). The rect's left/top
        // origin lives on CRectangle in the C#; the C++ CRectangle does not carry
        // an offset, so x/y are already screen-relative here. The high byte
        // (fill/write mode) maps to GDriver's WriteMode; only Normal (0) is used
        // by the ported callers, so the high byte is honoured as Normal.
        p.graphics.fillRectangle(rect.screenID, Rect{x, y, width, height}, uint8_t(mode & 0xff));
    }
}

// ---------------------------------------------------------------------------
// Mouse state (Var_58xx mirror; bookkeeping ported)
// ---------------------------------------------------------------------------
void CommonTools::F0_1000_163e_InitMouse() {
    // TODO(port): VCPU.MouseLocation / MouseButtons not modelled; seed to 0.
    mouseNewX = 0;
    mouseNewY = 0;
    mouseNewButtons = 0;
    var_587d = -1;
}

void CommonTools::F0_1000_1697(int x, int y, int bitmapPtr) {
    mouseIconXOffset = x;
    mouseIconYOffset = y;
    mouseIcon = bitmapPtr;
}

void CommonTools::F0_1000_16ae(int x, int y) {
    mouseNewX = x;
    mouseNewY = y;
}

void CommonTools::F0_1000_16db() {
    if (mouseIcon != 0 && var_5403 == 0) {
        F0_1000_083f(mouseNewX - mouseIconXOffset, mouseNewY - mouseIconYOffset, mouseIcon);
        var_5403 = 1;
    }
}

void CommonTools::F0_1000_170b() {
    if (mouseIcon != 0 && var_5403 != 0)
        var_5403 = 0;
}

void CommonTools::F0_1000_17db_MouseEvent() {
    // TODO(port): the C# reads CPU.BX/CX/DX/AX from the mouse-interrupt callback.
    // Port the bookkeeping faithfully against the current register contents.
    mouseNewButtonsOr |= cpu.BX.u16();
    mouseNewButtons = cpu.BX.s16();
    mouseNewX = cpu.CX.s16();
    mouseNewY = cpu.DX.s16();

    if ((cpu.AX.u16() & 0x1) != 0 && var_5403 != 0) {
        if (var_5402 == 0) var_5402 = 0;
        else var_5402 = 2;
    }
}

} // namespace oc1
