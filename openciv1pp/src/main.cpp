// main.cpp — entry point and bring-up demo for the OpenCiv1 C++/SDL2 port.
//
// Modes:
//   (default)        open an SDL2 window and present the localized demo screen
//   --dump <f.ppm>   render the localized demo offscreen (no SDL) -> PPM
//   --english        disable translation (show original English)
//   --selftest       run VCPU ALU self-tests and exit
//   --restest        round-trip the .pic codec (RLE+LZW) and exit
//   --pic <f.pic>    load a real .pic asset and dump it to <f.pic>.ppm
#include "graphics/GBitmap.h"
#include "graphics/GDriver.h"
#include "graphics/CjkGlyphCache.h"
#include "game/OpenCiv1Game.h"
#include "game/DrawTools.h"
#include "game/ImageTools.h"
#include "game/LanguageTools.h"
#include "game/MenuBoxDialog.h"
#include "game/CommonTools.h"
#include "game/TextBoxDialogs.h"
#include "game/FrontEndFlow.h"
#include "game/GameMenus.h"
#include "game/MainCode.h"
#include "game/MiniWorld.h"
#include "localization/Translator.h"
#include "platform/SdlPresenter.h"
#include "resource/PicLoader.h"
#include "resource/TextResource.h"
#include "vcpu/VCPU.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

using namespace oc1;

// ---------------- demo screen ----------------
// Rendered through GDriver.drawString (the real engine entry point): translation
// happens at that single chokepoint, then the screen composites ASCII + CJK.
static void drawDemo(GDriver& gd, GFont& font, bool translate) {
    Translator::instance().enabled = translate;
    GBitmap& fb = gd.screen(GDriver::MainScreen);
    fb.clear(1);
    const int x = 22;
    const int dy = font.pixelHeight + font.lineSpacing + 4;
    int y = 12;

    gd.drawString(0, font, x, y, "CIVILIZATION for Windows", 14);
    y += dy + 6;
    const char* menu[] = {
        "Start a New Game", "Load a Saved Game", "Play on EARTH",
        "Customize World", "View Hall of Fame", "Quit",
    };
    for (const char* it : menu) { gd.drawString(0, font, x, y, it, 15); y += dy; }
    y += 6;
    gd.drawString(0, font, x, y, "Difficulty Level...", 11); y += dy;
    gd.drawString(0, font, x, y, "Emperor (toughest)", 11);  y += dy;
    gd.drawString(0, font, x, y, "Solomon the Wise", 10);    y += dy;
    gd.drawString(0, font, x, y, "(c) 1991 MicroProse", 7);
}

static bool dumpPPM(const GBitmap& fb, const char* path) {
    std::vector<uint8_t> rgba;
    fb.toRGBA(rgba);
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    std::fprintf(f, "P6\n%d %d\n255\n", fb.width(), fb.height());
    const std::size_t n = static_cast<std::size_t>(fb.width()) * fb.height();
    for (std::size_t i = 0; i < n; ++i) {
        std::fputc(rgba[i * 4 + 0], f);
        std::fputc(rgba[i * 4 + 1], f);
        std::fputc(rgba[i * 4 + 2], f);
    }
    std::fclose(f);
    return true;
}

// ---------------- VCPU self-test ----------------
static int selftest() {
    VCPU c;
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    // registers
    c.AX.lo(0xCD); c.AX.hi(0xAB);
    chk(c.AX.u16() == 0xABCD, "AL/AH compose AX");
    c.BX.u32(0x12345678); chk(c.BX.u16() == 0x5678 && c.BX.hiU16() == 0x1234, "u32 views");

    // add/sub/cmp flags
    c.SUB_UInt16(5, 5);                 chk(c.flags.Z && !c.flags.C, "5-5 Z, !C");
    c.SUB_UInt16(3, 5);                 chk(c.flags.C && c.flags.S, "3-5 borrow+sign");
    chk(c.ADD_UInt16(0x7FFF, 1) == 0x8000 && c.flags.O && c.flags.S, "0x7FFF+1 overflow");
    chk(c.ADD_UInt8(0xFF, 1) == 0 && c.flags.C && c.flags.Z, "0xFF+1 carry+zero");
    c.CMP_UInt16(2, 9);                 chk(c.flags.L() && c.flags.B(), "cmp 2,9 less/below");
    c.CMP_UInt16(9, 2);                 chk(c.flags.G() && c.flags.A(), "cmp 9,2 greater/above");

    // mul/div/imul/idiv
    c.AX.u16(0x1000); c.MUL_UInt16(c.DX, c.AX, 0x10);
    chk(c.AX.u16() == 0x0000 && c.DX.u16() == 0x0001 && c.flags.C, "MUL 0x1000*0x10");
    c.AX.u16(0xFFFF); c.IMUL_UInt16(c.AX, c.DX, 2);
    chk(c.AX.u16() == 0xFFFE && c.DX.u16() == 0xFFFF, "IMUL -1*2");
    c.DX.u16(0x0001); c.AX.u16(0x0000); c.DIV_UInt16(c.AX, c.DX, 0x10);
    chk(c.AX.u16() == 0x1000 && c.DX.u16() == 0x0000, "DIV 0x10000/0x10");

    // shifts / rotates
    chk(c.SHL_UInt16(0x0001, 4) == 0x0010, "SHL16");
    chk(c.SHR_UInt16(0x8000, 4) == 0x0800, "SHR16");
    chk(c.SAR_UInt16(0x8000, 4) == 0xF800, "SAR16 sign-extend");
    chk(c.ROL_UInt8(0x80, 1) == 0x01 && c.flags.C, "ROL8 wrap");
    chk(c.ROR_UInt8(0x01, 1) == 0x80, "ROR8 wrap");
    chk(c.NEG_UInt16(1) == 0xFFFF && c.flags.C, "NEG16");
    chk(c.XOR_UInt16(0xFFFF, 0xFFFF) == 0 && c.flags.Z, "XOR self -> 0");

    // memory + stack
    c.WriteUInt16(0x1000, 0x0020, 0xBEEF);
    chk(c.ReadUInt16(0x1000, 0x0020) == 0xBEEF, "mem u16 round-trip");
    c.WriteString(VCPU::ToLinearAddress(0x2000, 0x0000), "Hi");
    chk(c.ReadString(0x2000, 0x0000) == "Hi", "string round-trip");
    c.PUSH_UInt16(0x1234); c.PUSH_UInt16(0xCAFE);
    chk(c.POP_UInt16() == 0xCAFE && c.POP_UInt16() == 0x1234, "stack LIFO");

    // string op: STOS advances DI, writes ES:DI
    c.flags.D = false; c.ES.u16(0x3000); c.DI.u16(0x0000); c.AX.lo(0x7E);
    c.STOS_UInt8();
    chk(c.ReadUInt8(0x3000, 0x0000) == 0x7E && c.DI.u16() == 0x0001, "STOS8 + DI inc");

    std::printf(fail ? "SELFTEST: %d failure(s)\n" : "SELFTEST: all pass\n", fail);
    return fail ? 1 : 0;
}

// ---------------- resource codec round-trip ----------------
static int restest() {
    const int W = 53, H = 31; // odd dims to exercise 4-bit packing edge later
    GBitmap img(W, H);
    for (int i = 0; i < 16; ++i) img.palette.set(uint8_t(i), uint8_t(i * 16), uint8_t(255 - i * 16), uint8_t(i * 8));
    auto& px = img.pixelsMut();
    for (std::size_t i = 0; i < px.size(); ++i) {
        int m = int(i) % 37;
        if (m < 10) px[i] = 0x90;                          // run incl. RLE marker byte
        else if (m < 20) px[i] = 0x55;                     // constant run
        else px[i] = uint8_t((i * 2654435761u) >> 13);     // pseudo-random (LZW)
    }

    std::vector<uint8_t> pic = buildPic8(img);
    std::unique_ptr<GBitmap> dec = loadPic(pic, true);
    int fail = 0;
    if (!dec) { std::printf("  FAIL: loadPic returned null\n"); ++fail; }
    else {
        if (dec->width() != W || dec->height() != H) { std::printf("  FAIL: dims %dx%d\n", dec->width(), dec->height()); ++fail; }
        else {
            std::size_t mism = 0;
            for (std::size_t i = 0; i < px.size(); ++i) if (dec->pixels()[i] != px[i]) ++mism;
            if (mism) { std::printf("  FAIL: %zu/%zu pixels differ\n", mism, px.size()); ++fail; }
        }
        // palette is 18-bit (6 bits/channel): lossy by format design. Verify the
        // decoded value equals the original passed through encode (8->6) + decode
        // (6->8), i.e. exactly what a faithful loader must reproduce.
        auto enc6  = [](int v8) { return (v8 * 63) / 255; };
        auto dec8  = [](int v6) { return (255 * (v6 & 0x3F)) / 63; };
        for (int i = 0; i < 16; ++i) {
            RGB a = img.palette.colors[i], b = dec->palette.colors[i];
            if (uint8_t(dec8(enc6(a.r))) != b.r || uint8_t(dec8(enc6(a.g))) != b.g) {
                std::printf("  FAIL: palette[%d]\n", i); ++fail; break;
            }
        }
    }
    std::printf(fail ? "RESTEST: %d failure(s)\n" : "RESTEST: all pass (pic encode->decode pixel-exact)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- GBitmap drawing-primitive self-test ----------------
static int gfxtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    GBitmap b(32, 24);
    b.clear(0);

    // setPixel / getPixel / bounds
    b.setPixel(5, 6, 7);
    chk(b.getPixel(5, 6) == 7, "setPixel/getPixel");
    chk(b.getPixel(-1, 0) == 0 && b.getPixel(100, 0) == 0, "getPixel OOB -> 0");
    b.setPixel(1000, 1000, 9); // OOB write must be a no-op (no crash)

    // write modes
    b.setPixel(5, 6, 0x0F, WriteMode::And); chk(b.getPixel(5, 6) == (7 & 0x0F), "And mode");
    b.setPixel(5, 6, 0x30, WriteMode::Or);  chk(b.getPixel(5, 6) == ((7 & 0x0F) | 0x30), "Or mode");
    b.setPixel(5, 6, 0xFF, WriteMode::Xor); chk(b.getPixel(5, 6) == (((7 & 0x0F) | 0x30) ^ 0xFF), "Xor mode");

    // horizontal / vertical lines
    b.clear(0);
    b.drawLine(2, 10, 8, 10, 5);
    { bool ok = true; for (int x = 2; x <= 8; ++x) ok &= (b.getPixel(x, 10) == 5); chk(ok && b.getPixel(9, 10) == 0, "horizontal line"); }
    b.drawLine(4, 2, 4, 7, 6);
    { bool ok = true; for (int y = 2; y <= 7; ++y) ok &= (b.getPixel(4, y) == 6); chk(ok, "vertical line"); }
    // diagonal endpoints
    b.clear(0); b.drawLine(0, 0, 6, 6, 4);
    chk(b.getPixel(0, 0) == 4 && b.getPixel(6, 6) == 4 && b.getPixel(3, 3) == 4, "diagonal line");

    // fillRect + clipping (rect extends past edges)
    b.clear(0);
    b.fillRect(Rect{-2, -2, 6, 6}, 8);
    chk(b.getPixel(0, 0) == 8 && b.getPixel(3, 3) == 8 && b.getPixel(4, 4) == 0, "fillRect + clip");
    { std::size_t cnt = 0; for (auto p : b.pixels()) if (p == 8) ++cnt; chk(cnt == 16, "fillRect clipped area = 4x4"); }

    // fillRect Xor
    b.clear(0x0F); b.fillRect(Rect{0, 0, 4, 4}, 0xFF, WriteMode::Xor);
    chk(b.getPixel(0, 0) == (0x0F ^ 0xFF), "fillRect Xor");

    // drawRect outline (corners set, interior clear)
    b.clear(0); b.drawRect(Rect{2, 2, 5, 5}, 3);
    chk(b.getPixel(2, 2) == 3 && b.getPixel(6, 6) == 3 && b.getPixel(6, 2) == 3 && b.getPixel(2, 6) == 3, "drawRect corners");
    chk(b.getPixel(4, 4) == 0, "drawRect hollow interior");

    // replaceColor inside a rect only
    b.clear(0); b.fillRect(Rect{0, 0, 10, 10}, 5); b.setPixel(15, 15, 5);
    b.replaceColor(Rect{0, 0, 10, 10}, 5, 2);
    chk(b.getPixel(0, 0) == 2 && b.getPixel(9, 9) == 2 && b.getPixel(15, 15) == 5, "replaceColor scoped to rect");

    // drawBitmap with transparency (index 0 = transparent)
    GBitmap spr(4, 4); spr.clear(0);
    spr.setPixel(0, 0, 9); spr.setPixel(3, 3, 9); // only two non-zero pixels
    b.clear(1);
    b.drawBitmap(10, 10, spr, true);
    chk(b.getPixel(10, 10) == 9 && b.getPixel(13, 13) == 9, "drawBitmap copies non-zero");
    chk(b.getPixel(11, 10) == 1, "drawBitmap transparent skips zero");
    // opaque copy writes zeros too
    b.clear(1); b.drawBitmap(10, 10, spr, false);
    chk(b.getPixel(11, 10) == 0, "drawBitmap opaque copies zero");

    // drawBitmap negative-offset clipping (no crash, partial copy)
    GBitmap spr2(4, 4); spr2.clear(7);
    b.clear(0); b.drawBitmap(-2, -2, spr2, false);
    chk(b.getPixel(0, 0) == 7 && b.getPixel(1, 1) == 7 && b.getPixel(2, 2) == 0, "drawBitmap negative clip");

    std::printf(fail ? "GFXTEST: %d failure(s)\n" : "GFXTEST: all pass (drawing primitives)\n", fail);
    return fail ? 1 : 0;
}

// A visual scene exercising every primitive, for eyeball confirmation.
static void drawGfxScene(GBitmap& b) {
    for (int i = 0; i < 16; ++i) b.palette.set(uint8_t(i), uint8_t(i * 17), uint8_t((i * 11) & 0xFF), uint8_t(255 - i * 16));
    b.clear(1);
    b.fillRect(Rect{8, 8, 120, 60}, 4);
    b.drawRect(Rect{8, 8, 120, 60}, 15);
    b.fillRect(Rect{20, 20, 40, 36}, 10);
    b.replaceColor(Rect{20, 20, 40, 18}, 10, 12); // recolor top half of the inner box
    // an X of diagonal lines
    b.drawLine(140, 10, 220, 70, 14);
    b.drawLine(220, 10, 140, 70, 11);
    // a Xor band over everything
    b.fillRect(Rect{0, 80, b.width(), 8}, 0xFF, WriteMode::Xor);
    // a transparent sprite stamped repeatedly
    GBitmap spr(12, 12); spr.clear(0);
    spr.drawLine(0, 0, 11, 11, 9); spr.drawLine(11, 0, 0, 11, 9);
    spr.drawRect(Rect{0, 0, 12, 12}, 13);
    for (int k = 0; k < 6; ++k) b.drawBitmap(20 + k * 18, 100, spr, true);
}

// ---------------- palette color-struct round-trip ----------------
static int paltest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };
    auto q = [](int v8) { return (255 * ((v8 * 63) / 255)) / 63; }; // 8->6->8 expected

    GBitmap a(8, 8);
    for (int i = 10; i <= 20; ++i) a.palette.set(uint8_t(i), uint8_t(i * 9), uint8_t(255 - i * 7), uint8_t(i * 3));

    std::vector<uint8_t> cs = a.exportPaletteColorStruct(10, 20);
    chk(cs.size() >= 6 && cs[0] == 0x4d && cs[1] == 0x30, "struct signature");
    chk(cs[4] == 10 && cs[5] == 20, "struct from/to");
    chk(cs.size() == std::size_t(6 + 11 * 3), "struct length");

    GBitmap b(8, 8); // default palette; index 0 should remain untouched
    RGB before0 = b.palette.colors[0];
    b.setPaletteFromColorStruct(cs);

    bool ok = true;
    for (int i = 10; i <= 20; ++i) {
        RGB src = a.palette.colors[i], dst = b.palette.colors[i];
        ok &= (dst.r == uint8_t(q(src.r)) && dst.g == uint8_t(q(src.g)) && dst.b == uint8_t(q(src.b)));
    }
    chk(ok, "palette 10..20 transferred (6-bit quantised)");
    chk(b.palette.colors[0].r == before0.r && b.palette.colors[9].r == GBitmap(1, 1).palette.colors[9].r,
        "indices outside [10,20] untouched");

    std::printf(fail ? "PALTEST: %d failure(s)\n" : "PALTEST: all pass (palette color-struct)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- GDriver screen-manager self-test ----------------
static int gdtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    GDriver gd;
    gd.addScreen(0, 64, 48);
    gd.addScreen(1, 32, 32);
    chk(gd.hasScreen(0) && gd.hasScreen(1) && !gd.hasScreen(2), "screen allocation");

    GBitmap& s1 = gd.screen(1);
    s1.clear(0);
    s1.fillRect(Rect{0, 0, 16, 16}, 5);
    s1.setPixel(20, 20, 9);

    gd.screen(0).clear(0);
    gd.drawImage(1, Rect{0, 0, 32, 32}, 0, 10, 8); // blit screen1 -> screen0 at (10,8)
    chk(gd.screen(0).getPixel(10, 8) == 5, "drawImage top-left");
    chk(gd.screen(0).getPixel(30, 28) == 9, "drawImage offset pixel");
    chk(gd.screen(0).getPixel(10 + 16, 8) == 0, "drawImage outside fill");
    chk(gd.screen(0).getPixel(9, 8) == 0, "drawImage left edge untouched");

    int id = gd.screenToBitmap(0, Rect{10, 8, 32, 32}); // capture what we just blitted
    chk(id >= 0xb000 && gd.hasBitmap(id), "screenToBitmap id range");
    chk(gd.bitmap(id).getPixel(0, 0) == 5 && gd.bitmap(id).getPixel(20, 20) == 9, "captured bitmap content");

    gd.screen(0).clear(1);
    gd.drawBitmapToScreen(0, 0, 0, id);
    chk(gd.screen(0).getPixel(0, 0) == 5 && gd.screen(0).getPixel(20, 20) == 9, "drawBitmapToScreen");

    chk(gd.bitmapCount() == 1 && gd.screenCount() == 2, "counts");

    // high-level entry points keyed by screen id
    gd.screen(0).clear(0);
    gd.setPixel(0, 3, 3, 7);                 chk(gd.getPixel(0, 3, 3) == 7, "GDriver setPixel/getPixel");
    gd.fillRectangle(0, Rect{0, 0, 5, 5}, 2); chk(gd.getPixel(0, 0, 0) == 2 && gd.getPixel(0, 3, 3) == 2, "GDriver fillRectangle");
    gd.drawLine(0, 0, 10, 10, 10, 9);         chk(gd.getPixel(0, 0, 10) == 9 && gd.getPixel(0, 10, 10) == 9, "GDriver drawLine");
    gd.replaceColor(0, Rect{0, 0, 5, 5}, 2, 4); chk(gd.getPixel(0, 0, 0) == 4, "GDriver replaceColor");

    std::printf(fail ? "GDTEST: %d failure(s)\n" : "GDTEST: all pass (screen manager + entry points)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- GDriver screen-composite self-test ----------------
// Exercises the ported screen->main-screen composite and bitmap->screen blits.
static int compositetest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    GDriver gd;
    gd.addScreen(0, 64, 48);   // main screen
    gd.addScreen(1, 64, 48);   // offscreen, same size

    // Paint a known, unambiguous pattern on screen 1.
    GBitmap& s1 = gd.screen(1);
    s1.clear(3);
    s1.fillRect(Rect{4, 4, 20, 12}, 7);
    s1.setPixel(0, 0, 11);
    s1.setPixel(63, 47, 9);
    gd.screen(0).clear(1); // main starts as something different

    // Composite screen 1 onto main screen 0 (full-frame, no transparency).
    gd.F0_VGA_06b7_DrawScreenToMainScreen(1);

    // Assert screen 0 is now pixel-exact equal to screen 1 over the whole frame.
    bool exact = true;
    for (int y = 0; y < 48 && exact; ++y)
        for (int x = 0; x < 64; ++x)
            if (gd.screen(0).getPixel(x, y) != gd.screen(1).getPixel(x, y)) { exact = false; break; }
    chk(exact, "DrawScreenToMainScreen pixel-exact composite");
    chk(gd.screen(0).getPixel(0, 0) == 11 && gd.screen(0).getPixel(10, 8) == 7 &&
        gd.screen(0).getPixel(63, 47) == 9 && gd.screen(0).getPixel(40, 40) == 3,
        "DrawScreenToMainScreen sampled pixels");

    // screenID 0 is a no-op; non-allocated screen throws.
    gd.screen(0).clear(2);
    gd.F0_VGA_06b7_DrawScreenToMainScreen(0);
    chk(gd.screen(0).getPixel(0, 0) == 2, "DrawScreenToMainScreen(0) no-op");
    bool threw = false;
    try { gd.F0_VGA_06b7_DrawScreenToMainScreen(99); } catch (const std::exception&) { threw = true; }
    chk(threw, "DrawScreenToMainScreen unallocated throws");

    // WithEffect yields the same final composited frame (animation stubbed).
    gd.screen(0).clear(1);
    gd.F0_VGA_06b7_DrawScreenToMainScreenWithEffect(1);
    chk(gd.screen(0).getPixel(0, 0) == 11 && gd.screen(0).getPixel(10, 8) == 7,
        "DrawScreenToMainScreenWithEffect final frame");

    // ---- bitmap-to-screen path (F0_VGA_0c3e / 0d47) ----
    // Build a stored bitmap with a transparent (index 0) border around a fill.
    GBitmap bmp(8, 8);
    bmp.clear(0);                       // 0 == transparent
    bmp.fillRect(Rect{2, 2, 4, 4}, 12); // opaque centre
    int bid = gd.addBitmap(std::move(bmp));

    CRectangle rc; rc.screenID = 0; rc.left = 5; rc.top = 6;
    gd.screen(0).clear(1);
    gd.F0_VGA_0c3e_DrawBitmapToScreen(rc, 3, 4, bid); // draws at (5+3, 6+4)=(8,10)
    chk(gd.screen(0).getPixel(8 + 2, 10 + 2) == 12, "0c3e centre drawn at rect offset");
    chk(gd.screen(0).getPixel(8, 10) == 1, "0c3e transparent border preserved");

    gd.screen(0).clear(1);
    gd.F0_VGA_0d47_DrawBitmapToScreen(rc, 0, 0, bid); // draws at (5,6)
    chk(gd.screen(0).getPixel(5 + 2, 6 + 2) == 12, "0d47 centre drawn at rect offset");
    chk(gd.screen(0).getPixel(5, 6) == 1, "0d47 transparent border preserved");

    // Missing bitmap is a silent no-op (mirrors C# log-and-skip).
    gd.screen(0).clear(1);
    gd.F0_VGA_0c3e_DrawBitmapToScreen(rc, 0, 0, 0xDEAD);
    chk(gd.screen(0).getPixel(5, 6) == 1, "0c3e missing bitmap is no-op");

    std::printf(fail ? "COMPOSITETEST: %d failure(s)\n" : "COMPOSITETEST: all pass\n", fail);
    return fail ? 1 : 0;
}

// Visual: build one tile on an offscreen screen, then tile it across the main
// screen via screen->screen blits (the pattern the map renderer uses).
static void drawGdScene(GDriver& gd) {
    gd.addScreen(0, 256, 128);
    gd.addScreen(1, 16, 16);
    GBitmap& main = gd.screen(0);
    for (int i = 0; i < 16; ++i) main.palette.set(uint8_t(i), uint8_t(i * 16), uint8_t(255 - i * 9), uint8_t((i * 5) & 0xFF));
    GBitmap& tile = gd.screen(1);
    tile.copyPaletteFrom(main);
    tile.clear(4);
    tile.drawRect(Rect{0, 0, 16, 16}, 15);
    tile.drawLine(0, 0, 15, 15, 11);
    tile.setPixel(8, 8, 14);

    main.clear(1);
    for (int ty = 0; ty < 128 / 16; ++ty)
        for (int tx = 0; tx < 256 / 16; ++tx)
            if (((tx + ty) & 1) == 0) // checkerboard of tiles
                gd.drawImage(1, Rect{0, 0, 16, 16}, 0, tx * 16, ty * 16);
}

// ---------------- DrawTools (first ported CodeObject) ----------------
static void setupGame(OpenCiv1Game& g, int w, int h) {
    CjkGlyphCache::instance().autoLoad();
    Translator::instance().loadFile("assets/zh_TW.json");
    GFont font; font.buildAsciiFromFreeType(16);
    g.graphics.addScreen(GDriver::MainScreen, w, h);
    g.graphics.addFont(1, std::move(font));
    g.var_aa.screenID = 0;
    g.var_aa.fontID = 1;
}

static int drawtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    OpenCiv1Game g;
    setupGame(g, 320, 200);
    Translator::instance().enabled = true;
    DrawTools& dt = g.drawTools();

    int w = dt.F0_1182_00ef_GetStringWidth("Quit"); // -> 離開
    chk(w > 0, "GetStringWidth > 0");
    chk(g.cpu.AX.u16() == uint16_t(int16_t(w)), "GetStringWidth writes AX (CPU side-effect)");

    g.graphics.screen(0).clear(0);
    dt.F0_1182_00b3_DrawCenteredStringToScreen0("Start a New Game", 160, 20, 15);
    std::size_t ink = 0; for (auto px : g.graphics.screen(0).pixels()) if (px) ++ink;
    chk(ink > 0, "centered string drew ink");

    // a centered string is left-shifted by half its width
    g.graphics.screen(0).clear(0);
    dt.F0_1182_002a_DrawString("Quit", 0, 40, 15); // left-anchored
    std::size_t leftInk = 0; for (auto px : g.graphics.screen(0).pixels()) if (px) ++leftInk;
    chk(leftInk > 0, "plain string drew ink");

    // word-wrap: a long paragraph wraps to several lines
    g.graphics.screen(0).clear(0);
    std::string para = "The Senate has overruled your decision and the peace treaty is now signed by all the great powers.";
    int y0 = 10;
    int y1 = dt.DrawTextBlock(4, y0, para, 110, 15);
    chk(y1 > y0 + (16 + 2), "DrawTextBlock wrapped to multiple lines");
    chk(dt.GetTextBlockHeight(para, y0, 110) == y1 - y0, "GetTextBlockHeight matches DrawTextBlock");

    std::printf(fail ? "DRAWTEST: %d failure(s)\n" : "DRAWTEST: all pass (DrawTools CodeObject)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- ImageTools (.pic loader CodeObject) ----------------
static int imgtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    // Build a small bitmap with a known pattern + palette, encode to a .pic.
    const int W = 24, H = 16;
    GBitmap src(W, H);
    for (int i = 0; i < 16; ++i) src.palette.set(uint8_t(i), uint8_t(i * 16), uint8_t(255 - i * 16), uint8_t(i * 8));
    auto& sp = src.pixelsMut();
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            sp[std::size_t(y) * W + x] = uint8_t((x + y * 2) % 16); // deterministic, indices 0..15

    std::vector<uint8_t> pic = buildPic8(src);
    const char* path = "/tmp/imgtest.pic";
    { FILE* o = std::fopen(path, "wb");
      if (!o) { std::printf("  FAIL: cannot write %s\n", path); ++fail; }
      else { std::fwrite(pic.data(), 1, pic.size(), o); std::fclose(o); } }

    // Load it onto a GDriver screen at an offset via ImageTools.
    OpenCiv1Game g;
    g.graphics.addScreen(GDriver::MainScreen, 320, 200);
    GBitmap& scr = g.graphics.screen(GDriver::MainScreen);
    scr.clear(0);
    const int ox = 40, oy = 24;
    // palettePtr==1 -> also copy palette to screen (mirrors C# live-palette gate)
    g.imageTools().F0_2fa1_01a2_LoadBitmapOrPalette(GDriver::MainScreen, ox, oy, std::string(path), 1);

    // Assert the screen pixels at the offset match the original pattern.
    bool px_ok = true;
    for (int y = 0; y < H && px_ok; ++y)
        for (int x = 0; x < W; ++x)
            if (scr.getPixel(ox + x, oy + y) != sp[std::size_t(y) * W + x]) { px_ok = false; break; }
    chk(px_ok, "loaded screen pixels match source pattern at offset");
    chk(scr.getPixel(0, 0) == 0, "area outside the blit untouched");

    // Palette was copied onto the screen (6-bit quantised, like the .pic format).
    auto q = [](int v8) { return uint8_t((255 * ((v8 * 63) / 255)) / 63); };
    bool pal_ok = true;
    for (int i = 0; i < 16; ++i) {
        RGB s = src.palette.colors[i], d = scr.palette.colors[i];
        if (d.r != q(s.r) || d.g != q(s.g) || d.b != q(s.b)) { pal_ok = false; break; }
    }
    chk(pal_ok, "screen palette matches loaded .pic palette (6-bit quantised)");

    // LoadIcon registers the same .pic as a standalone bitmap.
    int iconId = g.imageTools().F0_2fa1_044c_LoadIcon(std::string(path));
    chk(iconId >= 0xb000 && g.graphics.hasBitmap(iconId), "LoadIcon returns valid bitmap id");
    if (iconId >= 0xb000 && g.graphics.hasBitmap(iconId)) {
        GBitmap& icon = g.graphics.bitmap(iconId);
        chk(icon.width() == W && icon.height() == H, "icon dimensions match");
        chk(icon.getPixel(0, 0) == sp[0] && icon.getPixel(W - 1, H - 1) == sp[std::size_t(H - 1) * W + (W - 1)],
            "icon pixels match source");
    }

    std::printf(fail ? "IMGTEST: %d failure(s)\n" : "IMGTEST: all pass (ImageTools .pic loader CodeObject)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- LanguageTools (text post-processing CodeObject) ----------------
static int langtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    OpenCiv1Game g;
    LanguageTools& lt = g.languageTools();

    // ReplaceKeywords: $US/$THEM/$BUCKS/$RPLC1/$RPLC2 -> Array_30b8 values.
    g.Array_30b8 = {"Rome", "Egypt", "100", "alpha", "beta"};
    chk(lt.F0_2f4d_0471_ReplaceKeywords("$US offers $BUCKS to $THEM") == "Rome offers 100 to Egypt",
        "ReplaceKeywords substitutes keywords");
    chk(lt.F0_2f4d_0471_ReplaceKeywords("$RPLC1/$RPLC2") == "alpha/beta", "ReplaceKeywords RPLC1/2");
    chk(lt.F0_2f4d_0471_ReplaceKeywords("no keywords here") == "no keywords here", "ReplaceKeywords passthrough");

    // AdjustTextBlockWidth: word-wrap to <= maxLength chars/line. With words of
    // 3 chars and maxLength=5, each word lands on its own line (see port trace).
    chk(lt.F0_2f4d_0000_AdjustTextBlockWidth("aaa bbb ccc", 5) == "aaa\nbbb\nccc\n",
        "AdjustTextBlockWidth wraps 3-char words at width 5");
    // A single short word that already fits: trailing newline appended, no breaks.
    chk(lt.F0_2f4d_0000_AdjustTextBlockWidth("hello", 80) == "hello\n",
        "AdjustTextBlockWidth single word gets trailing newline");
    // '^' followed by space introduces a forced option line break.
    chk(lt.F0_2f4d_0000_AdjustTextBlockWidth("Quit^ Yes", 80) == "Quit\n Yes\n",
        "AdjustTextBlockWidth forced option break on '^ '");

    // TrimStringToWidth: needs a font (DrawTools). Trim to a narrow pixel width
    // shortens the string and appends a '.' ellipsis; result must fit maxWidth.
    setupGame(g, 320, 200);
    Translator::instance().enabled = false; // measure the literal ASCII text
    // No spaces -> every trim hits the "replace last 2 chars with '.'" branch,
    // so the result deterministically ends with the ellipsis dot.
    std::string full = "AAAAAAAAAAAAAAAAAAAA";
    int fullW = g.drawTools().F0_1182_00ef_GetStringWidth(full);
    int target = fullW / 2;
    std::string trimmed = lt.F0_2f4d_04f7_TrimStringToWidth(full, target);
    chk(trimmed.size() < full.size(), "TrimStringToWidth shortens the string");
    chk(g.drawTools().F0_1182_00ef_GetStringWidth(trimmed) <= target, "TrimStringToWidth fits maxWidth");
    chk(!trimmed.empty() && trimmed.back() == '.', "TrimStringToWidth ends with ellipsis dot");

    std::printf(fail ? "LANGTEST: %d failure(s)\n" : "LANGTEST: all pass (LanguageTools CodeObject)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- TextResource (.TXT section/key reader) ----------------
// Writes a synthetic "KING.TXT" in the real format (0x212-byte binary header
// followed by ASCII '*KEY'/content entries), then exercises both the raw
// TextResource lookup and the LanguageTools KING-section path through it.
static int txttest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    const char* dir = "/tmp/oc1_txttest";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::string path = std::string(dir) + "/KING.TXT";
    {
        FILE* o = std::fopen(path.c_str(), "wb");
        if (!o) { std::printf("  FAIL: cannot write %s\n", path.c_str()); ++fail; }
        else {
            // 0x212-byte binary header (the engine seeks past it).
            std::vector<uint8_t> header(0x212, 0x00);
            std::fwrite(header.data(), 1, header.size(), o);
            // Body: entries in the real format. *PALACE has two content lines;
            // *EXCHANGE and its alias *EXCH share one block (consecutive keys).
            const char* body =
                "*PALACE\n"
                "Your palace is complete, O great $US.\n"
                "The people rejoice.\n"
                "*EXCHANGE\n"
                "*EXCH\n"
                "We propose an exchange of technologies.\n"
                "*END\n";
            std::fwrite(body, 1, std::strlen(body), o);
            std::fclose(o);
        }
    }

    // Raw TextResource lookup (no translation layer).
    TextResource tr(dir);
    std::string palace = tr.getText("KING", "*PALACE");
    chk(palace == "Your palace is complete, O great $US.\nThe people rejoice.",
        "TextResource joins content lines of *PALACE");
    chk(tr.getText("KING", "*EXCHANGE") == "We propose an exchange of technologies.",
        "TextResource reads *EXCHANGE");
    chk(tr.getText("KING", "*EXCH") == tr.getText("KING", "*EXCHANGE"),
        "TextResource alias key shares the content block");
    chk(tr.getText("KING", "*NOPE").empty(), "TextResource missing key -> empty");
    chk(tr.getText("MISSINGSECTION", "*PALACE").empty(), "TextResource missing file -> empty");
    // case-insensitive key match (C# uses CurrentCultureIgnoreCase)
    chk(tr.getText("KING", "*palace") == palace, "TextResource key match is case-insensitive");

    // LanguageTools KING-section path: GetTextBySectionAndKey -> ReplaceKeywords
    // -> AdjustTextBlockWidth, with the resource dir pointed at the synthetic file.
    OpenCiv1Game g;
    g.setResourcePath(dir);
    Translator::instance().enabled = false; // keep the literal English for the assert
    g.Array_30b8 = {"Caesar", "", "", "", ""};
    std::string king = g.languageTools().F0_2f4d_044f_GetTextFromKingSection("*PALACE", 80);
    chk(!king.empty(), "LanguageTools king-section path returns non-empty");
    chk(king.find("Caesar") != std::string::npos, "LanguageTools king path applied $US keyword replacement");
    chk(king.find("$US") == std::string::npos, "LanguageTools king path consumed the $US keyword");
    chk(king.back() == '\n', "LanguageTools king path width-adjusted (trailing newline)");

    std::printf(fail ? "TXTTEST: %d failure(s)\n" : "TXTTEST: all pass (TextResource .TXT reader)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- MenuBoxDialog (pop-up menu render CodeObject) ----------------
// Builds the real main-menu, draws the boxed menu via the ported render path,
// and asserts the menu region has ink. To PROVE the items are localized, it
// renders the same menu twice — once with the Translator on, once off — and
// asserts the two pixel buffers differ (translation visibly changed the text).
// Dumps the translated render to /tmp/menubox.ppm for eyeballing.
static int menutest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    const std::vector<std::string> items = {
        "Start a New Game", "Load a Saved Game", "Play on EARTH",
        "Customize World", "View Hall of Fame", "Quit",
    };
    const int mx = 30, my = 20;

    // Render the menu into a fresh game's screen 0 and return the pixel buffer.
    auto render = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game g;
        setupGame(g, 320, 200);        // screen 0 320x200, font 1 from FreeType(16), zh_TW.json
        Translator::instance().enabled = translate;
        g.graphics.screen(0).clear(1); // background = palette index 1 (non-zero)
        MenuBoxDialog& mb = g.menuBoxDialog();
        mb.forcedSelection = 2;        // drive the (stubbed) input loop selection
        int sel = mb.F0_2d05_0031_ShowMenuBox(items, mx, my, /*windowFrame*/ true, /*helpOption*/ false);
        chk(sel == 2, "ShowMenuBox returns the forced selection");
        return g.graphics.screen(0).pixels();
    };

    // 1) Chinese render (Translator enabled): this is the shipping output.
    std::vector<uint8_t> zh = render(true);
    // 2) English render (Translator disabled): same layout, untranslated text.
    std::vector<uint8_t> en = render(false);

    // Localization proof: the two buffers MUST differ — if the menu drew raw
    // (untranslated) text, enabling the Translator would not change a pixel.
    chk(zh.size() == en.size() && !zh.empty(), "both renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0,
        "translated vs untranslated pixels DIFFER (menu items are localized)");

    // Reconstruct a framebuffer view over the Chinese render for the region/ink
    // asserts and the PPM dump.
    GBitmap fb(320, 200);
    fb.pixelsMut() = zh;

    // Count ink (non-background pixels, background == 1) inside the menu region.
    const int rx = mx - 2, ry = my - 2, rw = 220, rh = 120; // generous menu region
    std::size_t ink = 0;
    for (int yy = ry; yy < ry + rh && yy < fb.height(); ++yy)
        for (int xx = rx; xx < rx + rw && xx < fb.width(); ++xx)
            if (fb.getPixel(xx, yy) != 1) ++ink;
    chk(ink > 0, "menu region has ink (border/text drawn)");

    // The box fill (index 7) should exist; the selected line's background is
    // swapped 7->11 (the C# highlight bar; the 22->3 swap matches nothing here).
    bool hasFill = false, hasHighlight = false;
    for (int yy = ry; yy < ry + rh && yy < fb.height(); ++yy)
        for (int xx = rx; xx < rx + rw && xx < fb.width(); ++xx) {
            uint8_t px = fb.getPixel(xx, yy);
            if (px == 7) hasFill = true;
            if (px == 11) hasHighlight = true;
        }
    chk(hasFill, "box background fill present (index 7)");
    chk(hasHighlight, "selected option highlighted (bar index 11)");

    dumpPPM(fb, "/tmp/menubox.ppm");

    Translator::instance().enabled = true; // restore the default translating state

    if (fail)
        std::printf("MENUTEST: %d failure(s)\n", fail);
    else
        std::printf("MENUTEST: all pass (MenuBoxDialog render CodeObject; "
                    "%zu localized pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// ---------------- MenuBoxDialog navigation core (headless) ----------------
// Drives the pure navStep() state machine with the abstract NavKey codes and
// asserts the C# main-menu input behaviour: DOWN advances the highlight, ENTER
// returns the highlighted index, ESC cancels (-1). No SDL, no rendering.
static const std::vector<std::string>& mainMenuItems() {
    static const std::vector<std::string> items = {
        "Start a New Game", "Load a Saved Game", "Play on EARTH",
        "Customize World", "View Hall of Fame", "Quit",
    };
    return items;
}

static int navtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    OpenCiv1Game g;
    setupGame(g, 320, 200);
    MenuBoxDialog& mb = g.menuBoxDialog();
    const int n = int(mainMenuItems().size());

    // From the default highlight (option 0), DOWN twice advances by 2.
    int start = mb.setupNav(n, /*disabled*/ 0, /*startIndex*/ 0);
    chk(start == 0, "setupNav default highlight is option 0");
    chk(mb.navStep(MenuBoxDialog::KeyDown) == MenuBoxDialog::NavNone, "DOWN keeps navigating");
    chk(mb.navStep(MenuBoxDialog::KeyDown) == MenuBoxDialog::NavNone, "DOWN keeps navigating");
    chk(mb.highlight == start + 2, "DOWN,DOWN advances highlight by 2");

    // ENTER returns the now-highlighted index.
    int sel = mb.navStep(MenuBoxDialog::KeyEnter);
    chk(sel == start + 2, "ENTER returns the highlighted index");

    // Clamp: DOWN past the last option stays on the last option.
    mb.setupNav(n, 0, n - 1);
    mb.navStep(MenuBoxDialog::KeyDown);
    chk(mb.highlight == n - 1, "DOWN clamps at last option");
    // UP past the first stays at 0.
    mb.setupNav(n, 0, 0);
    mb.navStep(MenuBoxDialog::KeyUp);
    chk(mb.highlight == 0, "UP clamps at first option");

    // Disabled options are skipped: disable option 1, DOWN from 0 -> 2.
    mb.setupNav(n, /*disable bit1*/ uint32_t(1) << 1, 0);
    mb.navStep(MenuBoxDialog::KeyDown);
    chk(mb.highlight == 2, "DOWN skips a disabled option");

    // Fresh menu + ESC returns -1 (cancel).
    mb.setupNav(n, 0, 0);
    chk(mb.navStep(MenuBoxDialog::KeyEsc) == MenuBoxDialog::NavCancel, "ESC returns -1 (cancel)");
    chk(mb.highlight == -1, "ESC clears the highlight");

    std::printf(fail ? "NAVTEST: %d failure(s)\n" : "NAVTEST: all pass (MenuBoxDialog navigation core)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- CommonTools (timer/palette/mouse services CodeObject) ----------------
// Exercises the self-contained ports: the palette-cycle SLOT engine (known
// seed/sequence on the live palette), the rect-clipped FillRectangle, the
// tick-count wait-timer reset, and the register-driven MouseEvent bookkeeping.
static int commontest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    OpenCiv1Game g;
    g.graphics.addScreen(GDriver::MainScreen, 64, 48);
    CommonTools& ct = g.commonTools();
    Palette& pal = g.graphics.screen(GDriver::MainScreen).palette;

    // --- Palette-cycle slot: a known 4-colour run rotates by one entry per tick.
    // Seed palette indices 10..13 with distinct, identifiable colours.
    for (int i = 0; i < 4; ++i) pal.set(uint8_t(10 + i), uint8_t(10 + i), 0, 0); // r = 10,11,12,13
    // speed=0 -> advances on every tick (speedCount++ then >speed).
    ct.F0_1000_0382_AddPaletteCycleSlot(/*index*/ 0, /*speed*/ 0, /*from*/ 10, /*to*/ 13);
    ct.F0_1000_03fa_StartPaletteCycleSlot(0);

    // One tick (currentPosition 0->1): for j in 0..3, pal[10+(1+j)%4]=captured[j].
    // -> pal[11]=10, pal[12]=11, pal[13]=12, pal[10]=13 (the run shifts +1 index).
    ct.F0_1000_044a_CyclePaletteTimer();
    chk(pal.colors[10].r == 13 && pal.colors[11].r == 10 &&
        pal.colors[12].r == 11 && pal.colors[13].r == 12,
        "CyclePaletteTimer rotates the run by 1 after one tick");

    // Second tick (currentPosition 1->2): pal[10+(2+j)%4]=captured[j].
    // -> pal[12]=10, pal[13]=11, pal[10]=12, pal[11]=13.
    ct.F0_1000_044a_CyclePaletteTimer();
    chk(pal.colors[10].r == 12 && pal.colors[11].r == 13 &&
        pal.colors[12].r == 10 && pal.colors[13].r == 11,
        "CyclePaletteTimer rotates by 2 after two ticks");

    // Stop restores the originally-captured (un-rotated) colours to the palette.
    ct.F0_1000_042b_StopPaletteCycleSlot(0);
    chk(pal.colors[10].r == 10 && pal.colors[11].r == 11 &&
        pal.colors[12].r == 12 && pal.colors[13].r == 13,
        "StopPaletteCycleSlot restores the captured palette");

    // A stopped slot does not move the palette on a tick.
    ct.F0_1000_044a_CyclePaletteTimer();
    chk(pal.colors[10].r == 10 && pal.colors[13].r == 13, "stopped slot is inert on tick");

    // out-of-range index is rejected (no slot created -> Start is a no-op).
    ct.F0_1000_0382_AddPaletteCycleSlot(/*index*/ 9, 0, 10, 13); // index > 8 ignored
    ct.F0_1000_03fa_StartPaletteCycleSlot(9);
    ct.F0_1000_044a_CyclePaletteTimer();
    chk(pal.colors[10].r == 10, "out-of-range slot index ignored");

    // --- FillRectangle: rect-clipped fill of the main screen with a colour.
    GBitmap& scr = g.graphics.screen(GDriver::MainScreen);
    scr.clear(0);
    ct.F0_1000_0bfa_FillRectangle(g.var_aa, /*x*/ 4, /*y*/ 4, /*w*/ 8, /*h*/ 6, /*mode*/ 0x0007);
    chk(scr.getPixel(4, 4) == 7 && scr.getPixel(11, 9) == 7, "FillRectangle fills the rect (colour=mode&0xff)");
    chk(scr.getPixel(3, 4) == 0 && scr.getPixel(12, 9) == 0, "FillRectangle leaves the outside untouched");
    { std::size_t cnt = 0; for (auto px : scr.pixels()) if (px == 7) ++cnt; chk(cnt == 8 * 6, "FillRectangle filled exactly w*h pixels"); }
    // zero/negative size is a no-op
    scr.clear(0);
    ct.F0_1000_0bfa_FillRectangle(g.var_aa, 4, 4, 0, 6, 0x0007);
    { std::size_t cnt = 0; for (auto px : scr.pixels()) if (px == 7) ++cnt; chk(cnt == 0, "FillRectangle zero width is a no-op"); }

    // --- WaitTimer / ResetWaitTimer: tick count resets.
    ct.tickCount = 123;
    ct.F0_1000_033e_ResetWaitTimer();
    chk(ct.tickCount == 0, "ResetWaitTimer zeroes the tick count");
    ct.tickCount = 99;
    ct.F0_1182_0134_WaitTimer(5);
    chk(ct.tickCount == 0, "WaitTimer resets the tick count");

    // --- MouseEvent: reads the VCPU registers and updates the mouse mirror.
    g.cpu.BX.u16(0x0003); g.cpu.CX.u16(120); g.cpu.DX.u16(80); g.cpu.AX.u16(0x0000);
    ct.mouseNewButtonsOr = 0;
    ct.F0_1000_17db_MouseEvent();
    chk(ct.mouseNewButtons == 3 && ct.mouseNewX == 120 && ct.mouseNewY == 80, "MouseEvent reads BX/CX/DX");
    chk(ct.mouseNewButtonsOr == 0x0003, "MouseEvent ORs the button mask");
    g.cpu.BX.u16(0x0001);
    ct.F0_1000_17db_MouseEvent();
    chk(ct.mouseNewButtonsOr == 0x0003, "MouseEvent button OR accumulates");

    std::printf(fail ? "COMMONTEST: %d failure(s)\n" : "COMMONTEST: all pass (CommonTools services CodeObject)\n", fail);
    return fail ? 1 : 0;
}

// ---------------- TextBoxDialogs (message/text pop-up render CodeObject) ----------------
// Builds a message box with a translatable title, a word-wrapped message body
// and option buttons, draws it via the ported render path, and asserts the box
// region has ink + the box fill. To PROVE the text is localized it renders the
// SAME box twice — Translator on vs off — and asserts the two pixel buffers
// DIFFER (translation visibly changed the title/message/buttons). Dumps the
// translated render to /tmp/textbox.ppm.
static int textboxtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    const std::string title   = "Quit";
    const std::string message =
        "The Senate has overruled your decision and the peace treaty is now signed by all the great powers.";
    const std::vector<std::string> buttons = {"Yes", "No"};
    const int bx = 30, by = 40, bw = 200;

    // Render the box into a fresh game's screen 0 and return the pixel buffer.
    auto render = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game g;
        setupGame(g, 320, 200);        // screen 0 320x200, font 1 from FreeType(16), zh_TW.json
        Translator::instance().enabled = translate;
        g.graphics.screen(0).clear(1); // background = palette index 1 (non-zero)
        TextBoxDialogs& tb = g.textBoxDialogs();
        tb.forcedSelection = 0;        // drive the (stubbed) input loop -> first button
        int sel = tb.F23_0000_0000_ShowTextBox(title, message, buttons, bx, by, bw);
        chk(sel == 0, "ShowTextBox returns the forced selection");
        return g.graphics.screen(0).pixels();
    };

    // 1) Chinese render (Translator enabled): the shipping output.
    std::vector<uint8_t> zh = render(true);
    // 2) English render (Translator disabled): same layout, untranslated text.
    std::vector<uint8_t> en = render(false);

    // Localization proof: the two buffers MUST differ — if the box drew raw
    // (untranslated) text, enabling the Translator would not change a pixel.
    chk(zh.size() == en.size() && !zh.empty(), "both renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0,
        "translated vs untranslated pixels DIFFER (box text is localized)");

    // Reconstruct a framebuffer view over the Chinese render for the region asserts.
    GBitmap fb(320, 200);
    fb.pixelsMut() = zh;

    // Count ink (non-background, background == 1) inside a generous box region.
    const int rx = bx - 2, ry = by - 2, rw = bw + 4, rh = 160;
    std::size_t ink = 0;
    bool hasFill = false, hasHighlight = false;
    for (int yy = ry; yy < ry + rh && yy < fb.height(); ++yy)
        for (int xx = rx; xx < rx + rw && xx < fb.width(); ++xx) {
            uint8_t px = fb.getPixel(xx, yy);
            if (px != 1) ++ink;
            if (px == 15) hasFill = true;       // box background fill
            if (px == 11) hasHighlight = true;  // selected-button highlight
        }
    chk(ink > 0, "box region has ink (border/title/message/buttons drawn)");
    chk(hasFill, "box background fill present (index 15)");
    chk(hasHighlight, "selected button highlighted (index 11)");

    dumpPPM(fb, "/tmp/textbox.ppm");

    Translator::instance().enabled = true; // restore the default translating state

    if (fail)
        std::printf("TEXTBOXTEST: %d failure(s)\n", fail);
    else
        std::printf("TEXTBOXTEST: all pass (TextBoxDialogs render CodeObject; "
                    "%zu localized pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// ---------------- mouse hit-test / dispatch (headless) ----------------
// Drives the PURE mouse paths (MenuBoxDialog::itemAt/handleMouse and
// MiniWorld::screenToTile/handleMouseClick) without opening an SDL window.
// Exercises a real menu render (so the cached rects are populated by the
// same code path the interactive loop hits) and a real MiniWorld draw.
static int mousetest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    // ---- MenuBoxDialog: itemAt + handleMouse ----
    {
        OpenCiv1Game g;
        setupGame(g, 320, 200);
        Translator::instance().enabled = true;
        g.graphics.screen(0).clear(1);
        MenuBoxDialog& mb = g.menuBoxDialog();
        mb.setupNav(int(mainMenuItems().size()), /*disabled*/ 0, /*startIndex*/ 0);
        // Same setup the menutest uses (mx=30, my=20, windowFrame on).
        mb.F0_2d05_0031_ShowMenuBox(mainMenuItems(), 30, 20, /*windowFrame*/ true, /*helpOption*/ false);

        // Cached rects: one per option, in option order.
        const auto& rects = mb.lastItemRects();
        chk(rects.size() == mainMenuItems().size(),
            "lastItemRects size == option count after ShowMenuBox");

        // itemAt at the centre of each rect returns the right option index.
        bool allHit = true;
        for (std::size_t i = 0; i < rects.size(); ++i) {
            int cx = rects[i].x + rects[i].w / 2;
            int cy = rects[i].y + rects[i].h / 2;
            if (mb.itemAt(cx, cy) != int(i)) { allHit = false; break; }
        }
        chk(allHit, "itemAt(centre of each item rect) returns the option index");

        // itemAt far outside the box returns -1.
        chk(mb.itemAt(5, 5) == -1, "itemAt(5,5) outside the box -> -1");
        chk(mb.itemAt(319, 199) == -1, "itemAt(bottom-right corner) outside -> -1");

        // Left-click on option 2 sets highlight + writes outSelection=2.
        int sel = -42;
        int cx2 = rects[2].x + rects[2].w / 2;
        int cy2 = rects[2].y + rects[2].h / 2;
        MenuBoxDialog::MouseEvent click{cx2, cy2, 1, true, false};
        bool consumed = mb.handleMouse(click, &sel);
        chk(consumed && sel == 2, "left-click on item 2 returns selection 2");
        chk(mb.highlight == 2, "left-click sets highlight to clicked item");

        // Hover (motion) on option 4 moves the highlight without selecting.
        sel = -42;
        int cx4 = rects[4].x + rects[4].w / 2;
        int cy4 = rects[4].y + rects[4].h / 2;
        MenuBoxDialog::MouseEvent hover{cx4, cy4, 0, false, true};
        chk(!mb.handleMouse(hover, &sel), "hover does not return a selection");
        chk(sel == -42, "hover does not write outSelection");
        chk(mb.highlight == 4, "hover moves the highlight");

        // Click OUTSIDE the box -> cancel (-1).
        sel = -42;
        MenuBoxDialog::MouseEvent outside{5, 5, 1, true, false};
        chk(mb.handleMouse(outside, &sel) && sel == -1,
            "left-click outside the box cancels (-1)");

        // Right-click anywhere -> cancel (-1).
        sel = -42;
        MenuBoxDialog::MouseEvent rclick{cx2, cy2, 3, true, false};
        chk(mb.handleMouse(rclick, &sel) && sel == -1,
            "right-click cancels (-1)");
    }

    // ---- MiniWorld: screenToTile round-trip + handleMouseClick ----
    {
        const int W = 40, H = 30;
        OpenCiv1Game g;
        setupGame(g, 480, 300);
        Translator::instance().enabled = true;
        MiniWorld world(W, H, 12345u);
        world.draw(g.graphics, 1, 12); // populates the cached viewport math.

        // Round-trip: pick a couple of known framebuffer pixels and assert the
        // resulting tile maps back to the same pixel band.
        int tx = -1, ty = -1;
        chk(world.screenToTile(0, 0, tx, ty), "screenToTile(0,0) is inside the viewport");
        chk(tx >= 0 && ty >= 0, "screenToTile(0,0) returns sensible tile coords");
        int tx2 = -1, ty2 = -1;
        chk(world.screenToTile(13, 25, tx2, ty2),
            "screenToTile(13,25) is inside the viewport");
        // (13,25) at tileSize=12 -> tile delta (1,2) from the top-left visible.
        chk(tx2 == tx + 1 && ty2 == ty + 2,
            "screenToTile advances by 1 tile per tileSize pixels");
        int dummy = 0;
        chk(!world.screenToTile(-1, 0, dummy, dummy),
            "screenToTile rejects negative x (off-map)");
        chk(!world.screenToTile(0, 299, dummy, dummy),
            "screenToTile rejects HUD-bar y (below the viewport)");

        // Click east of the unit -> moves (+1, 0).
        int ux = world.unitX(), uy = world.unitY();
        // Build a pixel coord clearly east of the unit, inside the viewport.
        // Camera centres on the unit so unit-pixel ~= (cols/2 * tileSize, rows/2 * tileSize).
        int unitPxX = (ux - 0) * 0 + ((/*viewW*/ 480) / 12 / 2) * 12 + 6;
        int unitPxY = (uy - 0) * 0 + ((/*viewH*/ (300 - 36)) / 12 / 2) * 12 + 6;
        // Actual unit pixel is camCentre*tileSize + tileSize/2; cheat by going
        // via screenToTile from the unit's known map coords. Use the unit-pixel
        // estimate above + a deliberate offset of (+24, 0) -> two tiles east.
        // Reset the world (clean clone with fresh moves).
        MiniWorld w2(W, H, 12345u);
        w2.draw(g.graphics, 1, 12);
        int beforeX = w2.unitX(), beforeY = w2.unitY();
        chk(w2.handleMouseClick(unitPxX + 24, unitPxY),
            "handleMouseClick east of the unit moves the unit");
        chk(w2.unitX() == beforeX + 1 && w2.unitY() == beforeY,
            "click east of unit -> moveUnit(+1, 0)");

        // Click south-west of the unit -> moves (-1, +1).
        MiniWorld w3(W, H, 12345u);
        w3.draw(g.graphics, 1, 12);
        int bx3 = w3.unitX(), by3 = w3.unitY();
        chk(w3.handleMouseClick(unitPxX - 24, unitPxY + 24),
            "handleMouseClick south-west of the unit moves the unit");
        chk(w3.unitX() == bx3 - 1 && w3.unitY() == by3 + 1,
            "click south-west of unit -> moveUnit(-1, +1)");

        // Click on the unit's own pixel: no movement.
        MiniWorld w4(W, H, 12345u);
        w4.draw(g.graphics, 1, 12);
        int bx4 = w4.unitX(), by4 = w4.unitY();
        chk(!w4.handleMouseClick(unitPxX, unitPxY),
            "handleMouseClick on the unit's own tile returns false");
        chk(w4.unitX() == bx4 && w4.unitY() == by4,
            "click on the unit's tile does not move the unit");

        // Click on the HUD bar (below the map): no movement.
        MiniWorld w5(W, H, 12345u);
        w5.draw(g.graphics, 1, 12);
        int bx5 = w5.unitX(), by5 = w5.unitY();
        chk(!w5.handleMouseClick(100, 290),
            "handleMouseClick on the HUD (y>=viewH) returns false");
        chk(w5.unitX() == bx5 && w5.unitY() == by5,
            "HUD click does not move the unit");
    }

    if (fail) std::printf("MOUSETEST: %d failure(s)\n", fail);
    else      std::printf("MOUSETEST: all pass (mouse hit-test/dispatch)\n");
    return fail ? 1 : 0;
}

// ---------------- interactive Chinese main menu (SDL) ----------------
// Same setup as menutest (translation ON), but opens the real SDL window and
// loops draw -> present -> pollKey -> navStep until ENTER (>=0) or ESC (-1).
static int menuInteractive() {
    OpenCiv1Game g;
    setupGame(g, 320, 200);
    Translator::instance().enabled = true;
    const std::vector<std::string>& items = mainMenuItems();
    const int mx = 30, my = 20;
    const int n = int(items.size());

    GBitmap& fb = g.graphics.screen(0);

    SdlPresenter pres;
    if (!pres.init("OpenCiv1++ Main Menu (zh-TW)", fb.width(), fb.height(), 3)) return 1;

    MenuBoxDialog& mb = g.menuBoxDialog();
    mb.setupNav(n, /*disabled*/ 0, /*startIndex*/ 0);

    auto redraw = [&]() {
        fb.clear(1);
        mb.defaultOptionIndex = mb.highlight;   // pre-select the highlighted option
        mb.forcedSelection = mb.highlight;      // draw the highlight bar on it
        mb.F0_2d05_0031_ShowMenuBox(items, mx, my, /*windowFrame*/ true, /*helpOption*/ false);
    };
    redraw();

    int result = MenuBoxDialog::NavNone;
    while (result == MenuBoxDialog::NavNone) {
        if (!pres.present(fb)) { result = MenuBoxDialog::NavCancel; break; }
        int prev = mb.highlight;
        // Mouse: hover moves highlight; click selects (caller treats outSel as ENTER).
        SdlPresenter::MouseEvent me;
        while (pres.pollMouse(me)) {
            MenuBoxDialog::MouseEvent mm{me.x, me.y, me.button, me.down, me.motion};
            int outSel = MenuBoxDialog::NavNone;
            if (mb.handleMouse(mm, &outSel)) {
                result = (outSel < 0) ? MenuBoxDialog::NavCancel : outSel;
                break;
            }
        }
        if (result != MenuBoxDialog::NavNone) break;
        int key = pres.pollKey();
        if (key != 0) result = mb.navStep(key);
        if (result == MenuBoxDialog::NavNone && mb.highlight != prev) redraw();
    }
    pres.shutdown();

    if (result >= 0)
        std::printf("selected: %d (%s)\n", result, items[std::size_t(result)].c_str());
    else
        std::printf("cancelled\n");
    return 0;
}

// ---------------- front-end start-sequence flow ----------------
// FrontEndFlow composes MenuBoxDialog (Chinese menu + navStep) and
// TextBoxDialogs into Civ1's start sequence:
//   TITLE -> MAIN_MENU -> DIFFICULTY -> TRIBE -> NAME -> STARTING -> DONE.

// Headless flow transition test. Drives the PURE FrontEndFlow::handleKey() and
// asserts the state machine, the remembered difficulty, ESC-back, and that the
// difficulty menu renders different pixels translate-on vs -off (Chinese). No
// SDL. Mirrors the menutest/navtest style.
static int flowtest() {
    using State = FrontEndFlow::State;
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    OpenCiv1Game g;
    setupGame(g, 320, 200);

    // 1) MAIN_MENU + ENTER (on item 0, "Start a New Game") -> DIFFICULTY.
    {
        FrontEndFlow flow(g);
        chk(flow.state() == State::MAIN_MENU, "starts in MAIN_MENU");
        State s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::DIFFICULTY, "ENTER on item 0 -> DIFFICULTY");

        // DOWN x4, ENTER -> TRIBE with chosenDifficulty == 4 (Emperor).
        flow.handleKey(MenuBoxDialog::KeyDown);
        flow.handleKey(MenuBoxDialog::KeyDown);
        flow.handleKey(MenuBoxDialog::KeyDown);
        flow.handleKey(MenuBoxDialog::KeyDown);
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::TRIBE, "DOWNx4 + ENTER -> TRIBE");
        chk(flow.chosenDifficulty() == 4, "chosenDifficulty == 4 (Emperor)");

        // ENTER on tribe 0 -> NAME, then ENTER -> STARTING.
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::NAME, "ENTER on a tribe -> NAME");
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::STARTING, "ENTER on NAME -> STARTING");

        // Any key dismisses the STARTING message box -> DONE.
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::DONE, "key on STARTING -> DONE");
    }

    // 2) Fresh flow + (in DIFFICULTY) ESC -> back to MAIN_MENU.
    {
        FrontEndFlow flow(g);
        flow.handleKey(MenuBoxDialog::KeyEnter);             // -> DIFFICULTY
        chk(flow.state() == State::DIFFICULTY, "entered DIFFICULTY");
        State s = flow.handleKey(MenuBoxDialog::KeyEsc);      // ESC -> back
        chk(s == State::MAIN_MENU, "ESC in DIFFICULTY -> MAIN_MENU");
    }

    // 3) MAIN_MENU: ENTER on the last item ("Quit") -> QUIT.
    {
        FrontEndFlow flow(g);
        flow.handleKey(MenuBoxDialog::KeyUp); // wraps/clamps onto item 0
        for (int i = 0; i < int(FrontEndFlow::mainMenuItems().size()) - 1; ++i)
            flow.handleKey(MenuBoxDialog::KeyDown);          // walk to "Quit"
        State s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::QUIT, "ENTER on \"Quit\" -> QUIT");
    }

    // 4) Localization proof: the DIFFICULTY menu must render different pixels
    //    with the Translator on (Chinese) vs off (English).
    auto renderDifficulty = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game gg;
        setupGame(gg, 320, 200);
        Translator::instance().enabled = translate;
        FrontEndFlow flow(gg);
        flow.handleKey(MenuBoxDialog::KeyEnter); // MAIN_MENU -> DIFFICULTY
        flow.draw();
        return gg.graphics.screen(0).pixels();
    };
    std::vector<uint8_t> zh = renderDifficulty(true);
    std::vector<uint8_t> en = renderDifficulty(false);
    chk(zh.size() == en.size() && !zh.empty(), "both difficulty renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0, "difficulty menu Chinese vs English pixels DIFFER");

    {
        GBitmap fb(320, 200);
        fb.pixelsMut() = zh;
        dumpPPM(fb, "/tmp/menuflow_difficulty.ppm");
    }

    Translator::instance().enabled = true; // restore the default translating state

    if (fail)
        std::printf("FLOWTEST: %d failure(s)\n", fail);
    else
        std::printf("FLOWTEST: all pass (FrontEndFlow start-sequence state machine; "
                    "%zu localized pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// ---------------- new-game flow (title -> difficulty -> tribe -> name -> start) ----------------
// Headless verification of the full ported new-game flow. Drives
// FrontEndFlow::handleKey() from MAIN_MENU through DIFFICULTY -> TRIBE ->
// NAME -> STARTING, asserts the captured chosenDifficulty / chosenTribe /
// chosenName, ESC-back, and renders the DIFFICULTY menu twice (translate on
// vs off) to prove the labels are Chinese (the localization pixel-diff).
static int newgametest() {
    using State = FrontEndFlow::State;
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    OpenCiv1Game g;
    setupGame(g, 320, 200);

    // 1) Full transition: MAIN_MENU -> DIFFICULTY (Prince) -> TRIBE (Romans) ->
    //    NAME (default = "Caesar") -> STARTING -> DONE.
    {
        FrontEndFlow flow(g);
        chk(flow.state() == State::MAIN_MENU, "starts in MAIN_MENU");
        chk(flow.handleKey(MenuBoxDialog::KeyEnter) == State::DIFFICULTY,
            "ENTER 'Start a New Game' -> DIFFICULTY");

        // DOWNx2, ENTER picks Prince (index 2) and advances to TRIBE.
        flow.handleKey(MenuBoxDialog::KeyDown);
        flow.handleKey(MenuBoxDialog::KeyDown);
        State s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::TRIBE, "ENTER on a difficulty -> TRIBE");
        chk(flow.chosenDifficulty() == 2, "chosenDifficulty == 2 (Prince)");

        // ENTER on tribe 0 (Romans) -> NAME.
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::NAME, "ENTER on a tribe -> NAME");
        chk(flow.chosenTribe() == 0, "chosenTribe == 0 (Romans)");

        // ENTER on NAME -> STARTING with chosenName == "Caesar" (tribe leader).
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::STARTING, "ENTER on NAME -> STARTING");
        chk(flow.chosenName() == "Caesar",
            "chosenName defaulted to the tribe's leader (Caesar)");

        // Dismiss STARTING -> DONE.
        s = flow.handleKey(MenuBoxDialog::KeyEnter);
        chk(s == State::DONE, "key on STARTING -> DONE");
    }

    // 2) ESC backs up: TRIBE ESC -> DIFFICULTY; NAME ESC -> TRIBE.
    {
        FrontEndFlow flow(g);
        flow.handleKey(MenuBoxDialog::KeyEnter);     // -> DIFFICULTY
        flow.handleKey(MenuBoxDialog::KeyEnter);     // -> TRIBE
        State s = flow.handleKey(MenuBoxDialog::KeyEsc);
        chk(s == State::DIFFICULTY, "ESC in TRIBE -> DIFFICULTY");
        flow.handleKey(MenuBoxDialog::KeyEnter);     // -> TRIBE again
        flow.handleKey(MenuBoxDialog::KeyEnter);     // -> NAME
        s = flow.handleKey(MenuBoxDialog::KeyEsc);
        chk(s == State::TRIBE, "ESC in NAME -> TRIBE");
    }

    // 3) Localization proof: render the DIFFICULTY menu twice (Translator on
    //    vs off); pixel buffers must differ (Chinese labels visibly change).
    auto renderDifficulty = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game gg;
        setupGame(gg, 320, 200);
        Translator::instance().enabled = translate;
        FrontEndFlow flow(gg);
        flow.handleKey(MenuBoxDialog::KeyEnter);     // MAIN_MENU -> DIFFICULTY
        flow.draw();
        return gg.graphics.screen(0).pixels();
    };
    std::vector<uint8_t> zh = renderDifficulty(true);
    std::vector<uint8_t> en = renderDifficulty(false);
    chk(zh.size() == en.size() && !zh.empty(), "both difficulty renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0, "DIFFICULTY menu Chinese vs English pixels DIFFER");

    // 4) MainCode::F0_11a8_087c_NewGameMenu: a direct call with forced choices
    //    returns the captured difficulty/tribe/name (default = leader). This
    //    exercises the ported menu-building code path end-to-end.
    {
        OpenCiv1Game gg; setupGame(gg, 320, 200);
        Translator::instance().enabled = true;
        int d = -1, t = -1; std::string n;
        bool ok = gg.mainCode().F0_11a8_087c_NewGameMenu(
            /*forcedDifficulty*/ 4, /*forcedTribeIndex*/ 13,
            /*chosenName*/ "", &d, &t, &n);
        chk(ok && d == 4 && t == 13 && n == "Genghis Khan",
            "F0_11a8_087c_NewGameMenu captures forced choices + default name");
    }

    // 5) Dump the localized TRIBE menu for eyeballing.
    {
        OpenCiv1Game gg; setupGame(gg, 320, 200);
        Translator::instance().enabled = true;
        FrontEndFlow flow(gg);
        flow.handleKey(MenuBoxDialog::KeyEnter);  // -> DIFFICULTY
        flow.handleKey(MenuBoxDialog::KeyEnter);  // -> TRIBE
        flow.draw();
        dumpPPM(gg.graphics.screen(0), "/tmp/newgame_tribe.ppm");
    }

    Translator::instance().enabled = true; // restore default

    if (fail)
        std::printf("NEWGAMETEST: %d failure(s)\n", fail);
    else
        std::printf("NEWGAMETEST: all pass (new-game flow TITLE/MAIN/DIFFICULTY/"
                    "TRIBE/NAME/STARTING; %zu localized pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// Interactive Chinese new-game flow (SDL). setupGame, translation ON, optional
// LOGO.PIC via --assets <dir>; runs the full TITLE -> ... -> STARTING flow,
// printing each state transition + the final chosen difficulty / tribe / name.
static int newgameInteractive(const std::string& assetDir) {
    OpenCiv1Game g;
    setupGame(g, 320, 200);
    Translator::instance().enabled = true;
    if (!assetDir.empty()) g.setResourcePath(assetDir);

    GBitmap& fb = g.graphics.screen(0);
    SdlPresenter pres;
    if (!pres.init("OpenCiv1++ New Game (zh-TW)", fb.width(), fb.height(), 3)) return 1;

    FrontEndFlow flow(g);
    flow.enterTitle();              // start at the logo+menu splash.
    auto stateName = [](FrontEndFlow::State s) -> const char* {
        switch (s) {
            case FrontEndFlow::State::TITLE:      return "TITLE";
            case FrontEndFlow::State::MAIN_MENU:  return "MAIN_MENU";
            case FrontEndFlow::State::DIFFICULTY: return "DIFFICULTY";
            case FrontEndFlow::State::TRIBE:      return "TRIBE";
            case FrontEndFlow::State::NAME:       return "NAME";
            case FrontEndFlow::State::STARTING:   return "STARTING";
            case FrontEndFlow::State::DONE:       return "DONE";
            case FrontEndFlow::State::QUIT:       return "QUIT";
        }
        return "?";
    };
    flow.draw();

    while (true) {
        if (!pres.present(fb)) break;
        FrontEndFlow::State prev = flow.state();
        // Mouse: dispatch into the current menu (if any) as a click==Enter.
        SdlPresenter::MouseEvent me;
        bool consumedMouse = false;
        while (pres.pollMouse(me)) {
            MenuBoxDialog::MouseEvent mm{me.x, me.y, me.button, me.down, me.motion};
            int outSel = MenuBoxDialog::NavNone;
            if (g.menuBoxDialog().handleMouse(mm, &outSel)) {
                int navKey = (outSel < 0) ? MenuBoxDialog::KeyEsc : MenuBoxDialog::KeyEnter;
                FrontEndFlow::State s = flow.handleKey(navKey);
                if (s != prev) std::printf("[newgame] -> %s\n", stateName(s));
                consumedMouse = true;
                if (s == FrontEndFlow::State::DONE || s == FrontEndFlow::State::QUIT) {
                    flow.draw(); pres.present(fb); pres.shutdown();
                    std::printf("[newgame] ended in %s; difficulty=%d tribe=%d name=\"%s\"\n",
                                stateName(flow.state()),
                                flow.chosenDifficulty(), flow.chosenTribe(),
                                flow.chosenName().c_str());
                    return 0;
                }
                break;
            }
        }
        if (consumedMouse) { flow.draw(); continue; }
        int key = pres.pollKey();
        if (key == 0) continue;
        FrontEndFlow::State s = flow.handleKey(key);
        if (s != prev) std::printf("[newgame] -> %s\n", stateName(s));
        if (s == FrontEndFlow::State::DONE || s == FrontEndFlow::State::QUIT) {
            flow.draw(); pres.present(fb); break;
        }
        flow.draw();
    }
    pres.shutdown();
    std::printf("[newgame] ended in %s; difficulty=%d tribe=%d name=\"%s\"\n",
                stateName(flow.state()),
                flow.chosenDifficulty(), flow.chosenTribe(),
                flow.chosenName().c_str());
    return 0;
}

// ---------------- GameMenus (in-game top-menu system CodeObject) ----------------
// Exercises the ported GameMenus build/draw/dispatch paths. Drives a menu via
// GameMenus::F0_2c84_0000_ShowTopMenu (which delegates rendering to the ported
// MenuBoxDialog, whose forcedSelection stub drives the chosen option). Asserts:
//   - the menu region has ink (border/text drawn);
//   - the SAME menu rendered translate-on (Chinese) vs translate-off (English)
//     produces DIFFERENT pixel buffers (proves the menu items are localized);
//   - the Game/World/Advisors/Encyclopedia menus set the right shortcut key /
//     dispatch from the (forced) selection.
// Dumps the translated World menu to /tmp/gamemenu.ppm for eyeballing.
static int gamemenutest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    const int wmX = 144, wmY = 8; // World menu coordinates (from the C#)

    // Render the World menu into a fresh game's screen 0; pick "World Map".
    auto render = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game g;
        setupGame(g, 320, 200);             // screen 0 320x200, font 1, zh_TW.json
        Translator::instance().enabled = translate;
        g.graphics.screen(0).clear(1);      // background = palette index 1 (non-zero)
        g.gameMenus().spaceshipFlags = 0xfe00; // enable the 'SpaceShips' option
        g.menuBoxDialog().forcedSelection = 3; // drive selection -> "World Map (F10)"
        g.gameMenus().F0_2c84_0000_ShowTopMenu(/*playerID*/ 0, /*unitID*/ -1, /*menuIndex*/ 3);
        chk(g.gameMenus().lastDispatch == GameMenus::Dispatch::WorldMap,
            "World menu selection 3 dispatches WorldMap");
        return g.graphics.screen(0).pixels();
    };

    std::vector<uint8_t> zh = render(true);
    std::vector<uint8_t> en = render(false);

    // Localization proof: translated vs untranslated buffers MUST differ.
    chk(zh.size() == en.size() && !zh.empty(), "both renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0, "translated vs untranslated pixels DIFFER (menu items are localized)");

    // Ink in the World menu region.
    GBitmap fb(320, 200);
    fb.pixelsMut() = zh;
    std::size_t ink = 0;
    bool hasFill = false;
    for (int yy = wmY - 2; yy < 200; ++yy)
        for (int xx = wmX - 2; xx < 320; ++xx) {
            uint8_t px = fb.getPixel(xx, yy);
            if (px != 1) ++ink;
            if (px == 7) hasFill = true; // MenuBoxDialog box fill (index 7)
        }
    chk(ink > 0, "World menu region has ink (border/text drawn)");
    chk(hasFill, "box background fill present (index 7)");
    dumpPPM(fb, "/tmp/gamemenu.ppm");

    // ---- Game menu: each selection maps to the right shortcut key. ----
    {
        OpenCiv1Game g; setupGame(g, 320, 200);
        Translator::instance().enabled = true;
        g.graphics.screen(0).clear(1);
        g.menuBoxDialog().forcedSelection = 0; // Tax Rate
        g.gameMenus().F0_2c84_0000_ShowTopMenu(0, -1, 0);
        chk(g.gameMenus().Var_d4ca_MenuShortcutKey == '=', "Game menu 'Tax Rate' -> '='");
    }
    {
        OpenCiv1Game g; setupGame(g, 320, 200);
        Translator::instance().enabled = true;
        g.graphics.screen(0).clear(1);
        g.menuBoxDialog().forcedSelection = 8; // QUIT to DOS
        g.gameMenus().F0_2c84_0000_ShowTopMenu(0, -1, 0);
        chk(g.gameMenus().Var_d4ca_MenuShortcutKey == 0x1000, "Game menu 'QUIT to DOS' -> 0x1000");
    }

    // ---- Advisors menu: selection dispatches the right report. ----
    {
        OpenCiv1Game g; setupGame(g, 320, 200);
        Translator::instance().enabled = true;
        g.graphics.screen(0).clear(1);
        g.menuBoxDialog().forcedSelection = 4; // Trade Advisor
        g.gameMenus().F0_2c84_0000_ShowTopMenu(0, -1, 2);
        chk(g.gameMenus().lastDispatch == GameMenus::Dispatch::TradeReport,
            "Advisors menu 'Trade Advisor' -> TradeReport");
    }

    // ---- Encyclopedia menu: selection records the topic. ----
    {
        OpenCiv1Game g; setupGame(g, 320, 200);
        Translator::instance().enabled = true;
        g.graphics.screen(0).clear(1);
        g.menuBoxDialog().forcedSelection = 2; // City Improvements
        g.gameMenus().F0_2c84_0000_ShowTopMenu(0, -1, 4);
        chk(g.gameMenus().lastDispatch == GameMenus::Dispatch::Encyclopedia &&
            g.gameMenus().lastEncyclopediaTopic == 2,
            "Encyclopedia menu records selected topic 2");
    }

    // ---- Orders menu: builds for a Settlers unit, returns the hotkey char. ----
    {
        OpenCiv1Game g; setupGame(g, 320, 200);
        Translator::instance().enabled = true;
        g.graphics.screen(0).clear(1);
        GameMenus& gm = g.gameMenus();
        gm.orderContext.valid = true;
        gm.orderContext.isSettlers = true;
        gm.orderContext.isLand = true;
        g.menuBoxDialog().forcedSelection = 0; // "No Orders" -> hotkey ' '
        gm.F0_2c84_0000_ShowTopMenu(0, /*unitID*/ 0, 1);
        chk(gm.Var_d4ca_MenuShortcutKey == ' ', "Orders menu 'No Orders' -> ' '");
    }

    Translator::instance().enabled = true; // restore default

    if (fail)
        std::printf("GAMEMENUTEST: %d failure(s)\n", fail);
    else
        std::printf("GAMEMENUTEST: all pass (GameMenus in-game menu system; "
                    "%zu localized pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// Interactive Chinese start-sequence flow (SDL). setupGame, translation ON, run
// the flow with the real window: draw current screen -> present -> pollKey ->
// handleKey, until DONE or QUIT.
static int menuflowInteractive() {
    OpenCiv1Game g;
    setupGame(g, 320, 200);
    Translator::instance().enabled = true;

    GBitmap& fb = g.graphics.screen(0);
    SdlPresenter pres;
    if (!pres.init("OpenCiv1++ Start (zh-TW)", fb.width(), fb.height(), 3)) return 1;

    FrontEndFlow flow(g);
    auto stateName = [](FrontEndFlow::State s) -> const char* {
        switch (s) {
            case FrontEndFlow::State::TITLE:      return "TITLE";
            case FrontEndFlow::State::MAIN_MENU:  return "MAIN_MENU";
            case FrontEndFlow::State::DIFFICULTY: return "DIFFICULTY";
            case FrontEndFlow::State::TRIBE:      return "TRIBE";
            case FrontEndFlow::State::NAME:       return "NAME";
            case FrontEndFlow::State::STARTING:   return "STARTING";
            case FrontEndFlow::State::DONE:       return "DONE";
            case FrontEndFlow::State::QUIT:       return "QUIT";
        }
        return "?";
    };
    flow.draw();

    while (true) {
        if (!pres.present(fb)) break;          // window closed
        FrontEndFlow::State prevState = flow.state();
        // Mouse: route the active menu's click as Enter (or cancel as Esc).
        SdlPresenter::MouseEvent me;
        bool consumedMouse = false;
        while (pres.pollMouse(me)) {
            MenuBoxDialog::MouseEvent mm{me.x, me.y, me.button, me.down, me.motion};
            int outSel = MenuBoxDialog::NavNone;
            if (g.menuBoxDialog().handleMouse(mm, &outSel)) {
                int navKey = (outSel < 0) ? MenuBoxDialog::KeyEsc : MenuBoxDialog::KeyEnter;
                FrontEndFlow::State s = flow.handleKey(navKey);
                if (s != prevState) {
                    std::printf("[flow] -> %s\n", stateName(s));
                    if (s == FrontEndFlow::State::STARTING)
                        std::printf("selected difficulty %d\n", flow.chosenDifficulty());
                }
                consumedMouse = true;
                if (s == FrontEndFlow::State::DONE || s == FrontEndFlow::State::QUIT) {
                    flow.draw(); pres.present(fb); pres.shutdown();
                    std::printf("[flow] ended in %s\n", stateName(flow.state()));
                    return 0;
                }
                break;
            }
        }
        if (consumedMouse) { flow.draw(); continue; }
        int key = pres.pollKey();
        if (key == 0) continue;                // present() vsyncs the loop
        FrontEndFlow::State s = flow.handleKey(key);
        if (s != prevState) {
            std::printf("[flow] -> %s\n", stateName(s));
            if (s == FrontEndFlow::State::STARTING)
                std::printf("selected difficulty %d\n", flow.chosenDifficulty());
        }
        if (s == FrontEndFlow::State::DONE || s == FrontEndFlow::State::QUIT) {
            flow.draw();
            pres.present(fb);
            break;
        }
        flow.draw();
    }
    pres.shutdown();
    std::printf("[flow] ended in %s\n", stateName(flow.state()));
    return 0;
}

// Forward decl: resolves the DOS-assets dir (explicit arg / OPENCIV1_DOS_ASSETS
// env). Defined with the MiniWorld section further down.
static std::string resolveAssetDir(const char* explicitDir);

// ---------------- MainCode boot-screen (LOGO.PIC + Chinese main menu) ----------------
// Ports the boot-screen path of MainCode: render the authentic Civ1 title —
// the real LOGO.PIC on screen 0 — then draw the (Chinese) main game-type menu
// via MenuBoxDialog on top. With no DOS assets the logo is skipped (colored
// background) and the menu still draws — graceful fallback.

// Headless verification of MainCode::F0_11a8_0486_LogoAndMainGameMenu.
//   (a) WITHOUT assets: render the logo+menu path, assert the menu drew ink and
//       that translate-on vs -off pixels DIFFER (the menu is localized Chinese).
//   (b) IF OPENCIV1_DOS_ASSETS/LOGO.PIC exists: point resourcePath at it, render
//       again, assert the logo CONTRIBUTED pixels (the screen is not just the
//       menu over a flat background) and dump /tmp/title.ppm.
static int titletest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    // (MainCode places the menu at 100,140 internally; the test scans the whole
    // 320x200 screen for ink so no menu-coord constants are needed here.)

    // Render the boot screen into a fresh game's screen 0 (no assets) and return
    // the pixel buffer. Background is flat index 1 so logo/menu ink is detectable.
    auto renderNoAssets = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game g;
        setupGame(g, 320, 200);
        Translator::instance().enabled = translate;
        g.graphics.screen(0).clear(1);
        bool logo = true;
        int sel = g.mainCode().F0_11a8_0486_LogoAndMainGameMenu(/*forcedSelection*/ 0, &logo);
        chk(sel == 0, "logo+menu returns the forced selection");
        chk(!logo, "no-assets render did NOT load a logo (graceful fallback)");
        return g.graphics.screen(0).pixels();
    };

    // 1) Chinese render (Translator on): the shipping output.
    std::vector<uint8_t> zh = renderNoAssets(true);
    // 2) English render (Translator off): same layout, untranslated text.
    std::vector<uint8_t> en = renderNoAssets(false);

    chk(zh.size() == en.size() && !zh.empty(), "both renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0, "translated vs untranslated pixels DIFFER (menu is Chinese)");

    // The menu must have drawn ink over the (index-1) background.
    {
        GBitmap fb(320, 200);
        fb.pixelsMut() = zh;
        std::size_t ink = 0;
        bool hasFill = false;
        for (int yy = 0; yy < 200; ++yy)
            for (int xx = 0; xx < 320; ++xx) {
                uint8_t px = fb.getPixel(xx, yy);
                if (px != 1) ++ink;
                if (px == 7) hasFill = true; // MenuBoxDialog box fill (index 7)
            }
        chk(ink > 0, "menu drew ink (border/text) over the background");
        chk(hasFill, "box background fill present (index 7)");
    }

    // (b) Real-asset sub-check: only when OPENCIV1_DOS_ASSETS is set AND LOGO.PIC
    // exists there. Otherwise SKIP (headless tests stay green with NO assets).
    {
        std::string dir = resolveAssetDir(nullptr); // env only, in this context
        std::error_code ec;
        if (!dir.empty() &&
            std::filesystem::exists(std::filesystem::path(dir) / "LOGO.PIC", ec)) {
            OpenCiv1Game g;
            setupGame(g, 320, 200);
            Translator::instance().enabled = true;
            g.setResourcePath(dir);
            g.graphics.screen(0).clear(1);
            bool logo = false;
            g.mainCode().F0_11a8_0486_LogoAndMainGameMenu(/*forcedSelection*/ 0, &logo);
            chk(logo, "LOGO.PIC loaded from OPENCIV1_DOS_ASSETS");

            // Prove the logo contributed: pixels OUTSIDE the menu region must be
            // something other than the flat index-1 background (the logo art).
            GBitmap fb(320, 200);
            fb.pixelsMut() = g.graphics.screen(0).pixels();
            std::size_t logoInk = 0;
            for (int yy = 0; yy < 130; ++yy)            // above the menu (my=140)
                for (int xx = 0; xx < 320; ++xx)
                    if (fb.getPixel(xx, yy) != 1) ++logoInk;
            chk(logoInk > 0, "logo contributed pixels outside the menu region");

            dumpPPM(fb, "/tmp/title.ppm");
            std::printf("  (real LOGO + Chinese menu -> /tmp/title.ppm)\n");
        } else {
            std::printf("  (no DOS assets; real-LOGO sub-check skipped)\n");
        }
    }

    Translator::instance().enabled = true; // restore default

    if (fail)
        std::printf("TITLETEST: %d failure(s)\n", fail);
    else
        std::printf("TITLETEST: all pass (MainCode boot-screen; "
                    "%zu localized pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// Interactive Chinese title screen (SDL): setupGame + resourcePath(assets) +
// translation ON, render the LOGO.PIC + main menu, then loop draw -> present ->
// pollKey -> navStep until ENTER (>=0) or ESC (-1). Prints the selected item.
static int titleInteractive(const std::string& assetDir) {
    OpenCiv1Game g;
    setupGame(g, 320, 200);
    Translator::instance().enabled = true;
    if (!assetDir.empty()) {
        g.setResourcePath(assetDir);
        std::error_code ec;
        if (std::filesystem::exists(std::filesystem::path(assetDir) / "LOGO.PIC", ec))
            std::printf("[title] real Civ1 LOGO.PIC from %s\n", assetDir.c_str());
        else
            std::printf("[title] no LOGO.PIC in %s; colored background\n", assetDir.c_str());
    }

    const std::vector<std::string>& items = MainCode::mainMenuItems();
    const int n = int(items.size());

    GBitmap& fb = g.graphics.screen(0);
    SdlPresenter pres;
    if (!pres.init("OpenCiv1++ Title (zh-TW)", fb.width(), fb.height(), 3)) return 1;

    MenuBoxDialog& mb = g.menuBoxDialog();
    mb.setupNav(n, /*disabled*/ 0, /*startIndex*/ 0);

    auto redraw = [&]() {
        fb.clear(1);
        mb.defaultOptionIndex = mb.highlight;
        // The logo+menu path applies the logo background and draws the menu with
        // the current highlight as the forced selection.
        g.mainCode().F0_11a8_0486_LogoAndMainGameMenu(mb.highlight, nullptr);
    };
    redraw();

    int result = MenuBoxDialog::NavNone;
    while (result == MenuBoxDialog::NavNone) {
        if (!pres.present(fb)) { result = MenuBoxDialog::NavCancel; break; }
        int prev = mb.highlight;
        // Mouse: hover moves highlight; click selects (matches menuInteractive).
        SdlPresenter::MouseEvent me;
        while (pres.pollMouse(me)) {
            MenuBoxDialog::MouseEvent mm{me.x, me.y, me.button, me.down, me.motion};
            int outSel = MenuBoxDialog::NavNone;
            if (mb.handleMouse(mm, &outSel)) {
                result = (outSel < 0) ? MenuBoxDialog::NavCancel : outSel;
                break;
            }
        }
        if (result != MenuBoxDialog::NavNone) break;
        int key = pres.pollKey();
        if (key != 0) result = mb.navStep(key);
        if (result == MenuBoxDialog::NavNone && mb.highlight != prev) redraw();
    }
    pres.shutdown();

    if (result >= 0)
        std::printf("selected: %d (%s)\n", result, items[std::size_t(result)].c_str());
    else
        std::printf("cancelled\n");
    return 0;
}

// ---------------- MiniWorld playable slice ----------------
// NOTE: MiniWorld is an original minimal playable slice (NOT a faithful Civ1
// CodeObject port): a procedurally-generated tile map + a movable unit + turn
// counter + a Chinese HUD, built on the verified GDriver/Translator engine.

// Resolve the DOS-assets directory for the playable map's real tileset:
// an explicit dir (from --assets <dir>) wins; otherwise the OPENCIV1_DOS_ASSETS
// environment variable; otherwise empty (fallback to colored rects).
static std::string resolveAssetDir(const char* explicitDir) {
    if (explicitDir && explicitDir[0]) return std::string(explicitDir);
    if (const char* env = std::getenv("OPENCIV1_DOS_ASSETS"); env && env[0])
        return std::string(env);
    return std::string();
}

// Headless verification of the playable world. Asserts the pure API
// (deterministic map, clamped movement, turn advance, terrain name keys) and
// PROVES the HUD is localized by rendering the world twice (Translator on vs
// off) and asserting the pixel buffers differ. Dumps the Chinese render to
// /tmp/play.ppm.
static int playtest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    const int W = 40, H = 30;
    const uint32_t seed = 12345u;

    // Deterministic map: same seed -> same terrain at a sample of tiles.
    MiniWorld a(W, H, seed), b(W, H, seed);
    bool sameMap = true;
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            if (a.terrainAt(x, y) != b.terrainAt(x, y)) sameMap = false;
    chk(sameMap, "same seed -> identical map (deterministic)");
    MiniWorld c(W, H, seed + 1);
    bool differs = false;
    for (int y = 0; y < H && !differs; ++y)
        for (int x = 0; x < W; ++x)
            if (a.terrainAt(x, y) != c.terrainAt(x, y)) { differs = true; break; }
    chk(differs, "different seed -> different map");

    // terrainNameKey returns the expected English key for a known tile.
    chk(std::string(MiniWorld::terrainNameKey(Terrain::Water)) == "Ocean" &&
        std::string(MiniWorld::terrainNameKey(Terrain::Mountains)) == "Mountains",
        "terrainNameKey returns expected English keys");
    chk(std::string(MiniWorld::terrainNameKey(a.terrainAt(a.unitX(), a.unitY()))).size() > 0,
        "current tile has a terrain name key");

    // moveUnit right increases x by 1; clamps at the right edge.
    {
        MiniWorld m(W, H, seed);
        int x0 = m.unitX();
        chk(m.moveUnit(1, 0) && m.unitX() == x0 + 1, "moveUnit(+1,0) increases x by 1");
        // walk to the right edge; further right is clamped (no change -> false).
        while (m.unitX() < W - 1) m.moveUnit(1, 0);
        chk(m.unitX() == W - 1, "moveUnit clamps at right edge");
        chk(!m.moveUnit(1, 0) && m.unitX() == W - 1, "moveUnit past right edge is a no-op");
        // can't go below 0 on the left.
        while (m.unitX() > 0) m.moveUnit(-1, 0);
        chk(m.unitX() == 0, "moveUnit reaches left edge (x==0)");
        chk(!m.moveUnit(-1, 0) && m.unitX() == 0, "moveUnit can't go below 0");
        // vertical clamps too.
        while (m.unitY() > 0) m.moveUnit(0, -1);
        chk(m.unitY() == 0 && !m.moveUnit(0, -1), "moveUnit clamps at top (y==0)");
    }

    // endTurn increments the turn counter.
    {
        MiniWorld m(W, H, seed);
        int t0 = m.turn();
        m.endTurn(); m.endTurn();
        chk(m.turn() == t0 + 2, "endTurn increments the turn counter");
    }

    // Localization proof: render the world Chinese vs English; pixels must differ.
    auto render = [&](bool translate) -> std::vector<uint8_t> {
        OpenCiv1Game g;
        setupGame(g, 480, 300);
        Translator::instance().enabled = translate;
        MiniWorld m(W, H, seed);
        m.draw(g.graphics, 1, 12);
        return g.graphics.screen(0).pixels();
    };
    std::vector<uint8_t> zh = render(true);
    std::vector<uint8_t> en = render(false);
    chk(zh.size() == en.size() && !zh.empty(), "both renders produced a buffer");
    std::size_t diffPixels = 0;
    for (std::size_t i = 0; i < zh.size() && i < en.size(); ++i)
        if (zh[i] != en[i]) ++diffPixels;
    chk(diffPixels > 0, "HUD Chinese vs English pixels DIFFER (HUD is localized)");

    // The map region must have ink (terrain colours were drawn).
    {
        GBitmap fb(480, 300);
        fb.pixelsMut() = zh;
        bool hasTerrain = false;
        for (auto px : fb.pixels()) if (px >= 200 && px <= 206) { hasTerrain = true; break; }
        chk(hasTerrain, "map region drew terrain tiles");
        bool hasUnit = false;
        for (auto px : fb.pixels()) if (px == 209) { hasUnit = true; break; }
        chk(hasUnit, "unit marker drawn");
        dumpPPM(fb, "/tmp/play.ppm");
    }

    // Real-asset tileset sub-check: only when OPENCIV1_DOS_ASSETS is set AND
    // TER257.PIC exists there. Otherwise SKIP (headless tests must stay green
    // with NO assets). loadTileset must always be a no-throw no-op when absent.
    {
        MiniWorld noAssets(W, H, seed);
        chk(!noAssets.loadTileset("/nonexistent/dir") && !noAssets.hasTileset(),
            "loadTileset on a missing dir fails gracefully (no tileset)");

        std::string dir = resolveAssetDir(nullptr); // env only, in this context
        std::error_code ec;
        if (!dir.empty() &&
            std::filesystem::exists(std::filesystem::path(dir) / "TER257.PIC", ec)) {
            MiniWorld m(W, H, seed);
            chk(m.loadTileset(dir), "loadTileset succeeds when TER257.PIC present");
            chk(m.hasTileset(), "tileset is loaded");
            chk(m.tilesetWidth() == 320 && m.tilesetHeight() == 200,
                "TER257 tileset is 320x200");
            // Render one frame with the real tiles and dump it for eyeballing.
            OpenCiv1Game g;
            setupGame(g, 480, 300);
            Translator::instance().enabled = true;
            m.draw(g.graphics, 1, 16);
            dumpPPM(g.graphics.screen(0), "/tmp/playmap.ppm");
            std::printf("  (real-tile frame -> /tmp/playmap.ppm)\n");
        } else {
            std::printf("  (no DOS assets; real-tile sub-check skipped)\n");
        }
    }

    Translator::instance().enabled = true; // restore default

    if (fail)
        std::printf("PLAYTEST: %d failure(s)\n", fail);
    else
        std::printf("PLAYTEST: all pass (MiniWorld playable slice; "
                    "%zu localized HUD pixels differ)\n", diffPixels);
    return fail ? 1 : 0;
}

// Headless verification of the faithful TER257.PIC (col,row) mapping in
// TerrainTiles.h/.cpp. Three checks:
//   (1) terrainToTileXY returns Civ1's documented (col=0, row=terrainID) for
//       the land terrains (1:1 with Array_b886[terrain,0] in OpenCiv1
//       StartGameMenu.F5_0000_1455_LoadBitmaps);
//   (2) determinism: MiniWorld(seed)==MiniWorld(seed) (sanity);
//   (3) with OPENCIV1_DOS_ASSETS pointing at a real DOS install, render one
//       frame using the faithful tiles and dump it to /tmp/realmap.ppm,
//       asserting the frame has non-zero ink. Without assets the sub-check
//       is skipped (the test still passes).
static int maptest() {
    int fail = 0;
    auto chk = [&](bool ok, const char* m) { if (!ok) { std::printf("  FAIL: %s\n", m); ++fail; } };

    // (1) Faithful (col,row) mapping for the base tile of each terrain. These
    // are the documented Civ1 values: Array_b886[terrain,0] = ScreenToBitmap(
    // 1, 0, terrain*16, 16, 16). Same expected values for Ocean/Grassland/
    // Forest/Mountain as the task spec calls out.
    {
        TileXY ocean = terrainToTileXY(Terrain::Water);
        TileXY grass = terrainToTileXY(Terrain::Grassland);
        TileXY forest = terrainToTileXY(Terrain::Forest);
        TileXY mtn = terrainToTileXY(Terrain::Mountains);
        TileXY desert = terrainToTileXY(Terrain::Desert);
        TileXY plains = terrainToTileXY(Terrain::Plains);
        TileXY hills = terrainToTileXY(Terrain::Hills);
        // Water uses coastal/overlay regions (Array_d294 at y=176, overlay at
        // 256,120) — the base (col,row) returned is the closest aligned 16x16
        // ocean cell in TER257.PIC (col=15, row=7).
        chk(ocean.col == 15 && ocean.row == 7,
            "Water base tile is the (15,7) ocean cell (closest aligned 16x16 to TER257 ocean overlay)");
        // Land terrains: faithful (col=0, row=terrainID) from Array_b886.
        chk(grass.col == 0  && grass.row == 2,  "Grassland -> (0,2)");
        chk(forest.col == 0 && forest.row == 3, "Forest    -> (0,3)");
        chk(mtn.col == 0    && mtn.row == 5,    "Mountains -> (0,5)");
        chk(desert.col == 0 && desert.row == 0, "Desert    -> (0,0)");
        chk(plains.col == 0 && plains.row == 1, "Plains    -> (0,1)");
        chk(hills.col == 0  && hills.row == 4,  "Hills     -> (0,4)");
        // Tundra/Arctic/Swamp/Jungle: rows 6..9, col 0 (also faithful).
        chk(terrainToTileXY(Terrain::Tundra).row == 6 &&
            terrainToTileXY(Terrain::Arctic).row == 7 &&
            terrainToTileXY(Terrain::Swamp).row  == 8 &&
            terrainToTileXY(Terrain::Jungle).row == 9,
            "Tundra/Arctic/Swamp/Jungle base tiles -> rows 6..9");
    }

    // (2) Determinism sanity (same seed -> same map) using MiniWorld's
    // existing seed-based generator. Cheap full-grid compare.
    {
        const int W = 32, H = 24;
        const uint32_t seed = 0xC1Au;
        MiniWorld a(W, H, seed), b(W, H, seed);
        bool same = true;
        for (int y = 0; y < H && same; ++y)
            for (int x = 0; x < W; ++x)
                if (a.terrainAt(x, y) != b.terrainAt(x, y)) { same = false; break; }
        chk(same, "MiniWorld(seed) is deterministic across two builds");
    }

    // (3) Real-asset path: render a frame using the faithful TER257 mapping
    // and dump it to /tmp/realmap.ppm. Only when OPENCIV1_DOS_ASSETS points
    // at a directory containing TER257.PIC; otherwise skip (the test still
    // passes — headless CI must stay green with no assets).
    {
        std::string dir = resolveAssetDir(nullptr);
        std::error_code ec;
        if (!dir.empty() &&
            std::filesystem::exists(std::filesystem::path(dir) / "TER257.PIC", ec)) {
            OpenCiv1Game g;
            setupGame(g, 480, 300);
            Translator::instance().enabled = true;
            MiniWorld m(40, 30, 12345u);
            chk(m.loadTileset(dir), "real TER257.PIC loads from OPENCIV1_DOS_ASSETS");
            chk(m.hasTileset(), "tileset is present after loadTileset");
            m.draw(g.graphics, 1, 16);
            // Non-zero ink: at least one map pixel must be set (clear() uses
            // index 208 for HUD bg; any other index counts as ink).
            const GBitmap& fb = g.graphics.screen(0);
            std::size_t ink = 0;
            for (auto px : fb.pixels()) if (px != 208) ++ink;
            chk(ink > 0, "real-tile render produced non-zero ink");
            dumpPPM(fb, "/tmp/realmap.ppm");
            std::printf("  (real-tile frame -> /tmp/realmap.ppm)\n");
        } else {
            std::printf("  (no DOS assets at OPENCIV1_DOS_ASSETS; real-tile sub-check skipped)\n");
            // Fallback path must still pass: colored-rect render produces
            // ink in the 200..210 palette range and the unit marker (209).
            OpenCiv1Game g;
            setupGame(g, 480, 300);
            MiniWorld m(40, 30, 12345u);
            m.draw(g.graphics, 1, 12);
            const GBitmap& fb = g.graphics.screen(0);
            bool hasTerrain = false, hasUnit = false;
            for (auto px : fb.pixels()) {
                if (px >= 200 && px <= 206) hasTerrain = true;
                if (px == 209) hasUnit = true;
                if (hasTerrain && hasUnit) break;
            }
            chk(hasTerrain, "fallback colored-rect render drew terrain");
            chk(hasUnit, "fallback render drew the unit marker");
        }
    }

    if (fail)
        std::printf("MAPTEST: %d failure(s)\n", fail);
    else
        std::printf("MAPTEST: all pass (faithful TER257 base-tile mapping verified)\n");
    return fail ? 1 : 0;
}

// Interactive playable slice (SDL). setupGame, translation ON, build a world,
// then loop draw -> present -> pollKey -> (arrows move / Enter endTurn / Esc
// quit). Under the dummy SDL driver pollKey blocks for input — that's expected.
static int playInteractive(const std::string& assetDir) {
    OpenCiv1Game g;
    setupGame(g, 480, 300);
    Translator::instance().enabled = true;

    MiniWorld world(40, 30, 12345u);
    if (!assetDir.empty()) {
        if (world.loadTileset(assetDir))
            std::printf("[play] real Civ1 tiles from %s\n", assetDir.c_str());
        else
            std::printf("[play] no TER257.PIC in %s; using colored rects\n", assetDir.c_str());
    }
    GBitmap& fb = g.graphics.screen(0);

    SdlPresenter pres;
    if (!pres.init("OpenCiv1++ Play (zh-TW)", fb.width(), fb.height(), 2)) return 1;

    world.draw(g.graphics, 1, world.hasTileset() ? 16 : 12);
    while (true) {
        if (!pres.present(fb)) break;           // window closed
        bool dirty = false;
        // Mouse: left-click on a tile moves the unit ONE step toward it.
        SdlPresenter::MouseEvent me;
        while (pres.pollMouse(me)) {
            if (!me.motion && me.down && me.button == 1) {
                if (world.handleMouseClick(me.x, me.y)) dirty = true;
            }
        }
        int key = pres.pollKey();
        if (key == 0) {
            if (dirty) world.draw(g.graphics, 1, world.hasTileset() ? 16 : 12);
            continue;
        }
        switch (key) {
            case SdlPresenter::KeyUp:    dirty = world.moveUnit(0, -1) || dirty; break;
            case SdlPresenter::KeyDown:  dirty = world.moveUnit(0,  1) || dirty; break;
            case SdlPresenter::KeyLeft:  dirty = world.moveUnit(-1, 0) || dirty; break;
            case SdlPresenter::KeyRight: dirty = world.moveUnit( 1, 0) || dirty; break;
            case SdlPresenter::KeyEnter: world.endTurn(); dirty = true; break;
            case SdlPresenter::KeyEsc:   pres.shutdown(); return 0;
            default: break;
        }
        if (dirty) world.draw(g.graphics, 1, world.hasTileset() ? 16 : 12);
    }
    pres.shutdown();
    return 0;
}

// Headless one-frame render of the playable map to a PPM (no SDL window). Loads
// the real tileset when assetDir is non-empty + TER257.PIC is present; otherwise
// renders the colored-rect fallback. Used by `--play --assets <dir> --dump <ppm>`.
static int playDump(const std::string& assetDir, const char* ppmPath) {
    OpenCiv1Game g;
    setupGame(g, 480, 300);
    Translator::instance().enabled = true;
    MiniWorld world(40, 30, 12345u);
    bool real = !assetDir.empty() && world.loadTileset(assetDir);
    world.draw(g.graphics, 1, world.hasTileset() ? 16 : 12);
    if (!dumpPPM(g.graphics.screen(0), ppmPath)) {
        std::fprintf(stderr, "playdump: cannot write %s\n", ppmPath);
        return 1;
    }
    std::printf("[playdump] wrote %s (%s)\n", ppmPath,
                real ? "real Civ1 tiles" : "colored-rect fallback");
    return 0;
}

static void drawScene(OpenCiv1Game& g) {
    GBitmap& s = g.graphics.screen(0);
    s.clear(1);
    DrawTools& dt = g.drawTools();
    Translator::instance().enabled = true;

    dt.F0_1182_00b3_DrawCenteredStringWithShadowToScreen0("CIVILIZATION for Windows", 160, 10, 14);
    const char* menu[] = {"Start a New Game", "Load a Saved Game", "Play on EARTH", "Quit"};
    int y = 40;
    for (const char* it : menu) { dt.F0_1182_00b3_DrawCenteredStringToScreen0(it, 160, y, 15); y += 20; }
    // word-wrapped block (English paragraph demonstrates wrapping)
    dt.DrawTextBlock(10, 130, "The Senate has overruled your decision and the peace treaty is now signed.", 150, 11);
}

int main(int argc, char** argv) {
    bool dump = false, english = false, test = false, res = false, gfx = false;
    bool play = false, title = false, newgame = false;
    const char* dumpPath = nullptr;
    const char* picPath = nullptr;
    const char* gfxDumpPath = nullptr;
    const char* assetsDir = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--dump") && i + 1 < argc) { dump = true; dumpPath = argv[++i]; }
        else if (!std::strcmp(argv[i], "--assets") && i + 1 < argc) { assetsDir = argv[++i]; }
        else if (!std::strcmp(argv[i], "--play")) { play = true; }
        else if (!std::strcmp(argv[i], "--title")) { title = true; }
        else if (!std::strcmp(argv[i], "--newgame")) { newgame = true; }
        else if (!std::strcmp(argv[i], "--pic") && i + 1 < argc) picPath = argv[++i];
        else if (!std::strcmp(argv[i], "--gfxdraw") && i + 1 < argc) gfxDumpPath = argv[++i];
        else if (!std::strcmp(argv[i], "--english")) english = true;
        else if (!std::strcmp(argv[i], "--selftest")) test = true;
        else if (!std::strcmp(argv[i], "--restest")) res = true;
        else if (!std::strcmp(argv[i], "--gfxtest")) gfx = true;
        else if (!std::strcmp(argv[i], "--gdtest")) { return gdtest(); }
        else if (!std::strcmp(argv[i], "--compositetest")) { return compositetest(); }
        else if (!std::strcmp(argv[i], "--paltest")) { return paltest(); }
        else if (!std::strcmp(argv[i], "--drawtest")) { return drawtest(); }
        else if (!std::strcmp(argv[i], "--imgtest")) { return imgtest(); }
        else if (!std::strcmp(argv[i], "--langtest")) { return langtest(); }
        else if (!std::strcmp(argv[i], "--txttest")) { return txttest(); }
        else if (!std::strcmp(argv[i], "--menutest")) { return menutest(); }
        else if (!std::strcmp(argv[i], "--navtest")) { return navtest(); }
        else if (!std::strcmp(argv[i], "--commontest")) { return commontest(); }
        else if (!std::strcmp(argv[i], "--textboxtest")) { return textboxtest(); }
        else if (!std::strcmp(argv[i], "--flowtest")) { return flowtest(); }
        else if (!std::strcmp(argv[i], "--gamemenutest")) { return gamemenutest(); }
        else if (!std::strcmp(argv[i], "--playtest")) { return playtest(); }
        else if (!std::strcmp(argv[i], "--maptest")) { return maptest(); }
        else if (!std::strcmp(argv[i], "--titletest")) { return titletest(); }
        else if (!std::strcmp(argv[i], "--newgametest")) { return newgametest(); }
        else if (!std::strcmp(argv[i], "--mousetest")) { return mousetest(); }
        else if (!std::strcmp(argv[i], "--playdump") && i + 2 < argc) {
            // --playdump <dosAssetDir> <out.ppm>: headless real-tile map frame.
            const char* dir = argv[++i]; const char* out = argv[++i];
            return playDump(resolveAssetDir(dir), out);
        }
        else if (!std::strcmp(argv[i], "--menu")) { return menuInteractive(); }
        else if (!std::strcmp(argv[i], "--menuflow")) { return menuflowInteractive(); }
        else if (!std::strcmp(argv[i], "--drawscene") && i + 1 < argc) {
            OpenCiv1Game g; setupGame(g, 320, 200); drawScene(g);
            dumpPPM(g.graphics.screen(0), argv[++i]);
            std::printf("[drawscene] wrote %s\n", argv[i]); return 0;
        }
        else if (!std::strcmp(argv[i], "--gddraw") && i + 1 < argc) {
            GDriver gd; drawGdScene(gd); dumpPPM(gd.screen(0), argv[++i]);
            std::printf("[gddraw] wrote %s\n", argv[i]); return 0;
        }
    }

    // --play: real Civ1 tiles when an asset dir is given (--assets <dir> or the
    // OPENCIV1_DOS_ASSETS env), else the colored-rect fallback. With --dump it
    // renders one headless frame to the PPM instead of opening an SDL window.
    if (play) {
        std::string dir = resolveAssetDir(assetsDir);
        if (dump && dumpPath) return playDump(dir, dumpPath);
        return playInteractive(dir);
    }

    // --title: the authentic Civ1 boot screen — LOGO.PIC (when a DOS asset dir is
    // given via --assets <dir> or OPENCIV1_DOS_ASSETS) + the Chinese main menu,
    // navigable in an SDL window.
    if (title) {
        return titleInteractive(resolveAssetDir(assetsDir));
    }

    // --newgame: the full Chinese new-game flow in one SDL window — TITLE
    // (LOGO.PIC when assets present, fallback otherwise) -> MAIN_MENU ->
    // DIFFICULTY -> TRIBE -> NAME -> STARTING. Prints state transitions +
    // the captured difficulty/tribe/name at the end.
    if (newgame) {
        return newgameInteractive(resolveAssetDir(assetsDir));
    }

    // run the whole headless suite; nonzero if any fails (CI entry point)
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--test")) {
            int f = 0;
            f += selftest(); f += restest(); f += gfxtest(); f += gdtest(); f += compositetest(); f += paltest(); f += drawtest(); f += imgtest(); f += langtest(); f += txttest(); f += menutest(); f += navtest(); f += commontest(); f += textboxtest(); f += flowtest(); f += gamemenutest(); f += playtest(); f += maptest(); f += titletest(); f += newgametest(); f += mousetest();
            std::printf(f ? "==> SUITE FAILED (%d)\n" : "==> SUITE: ALL PASS\n", f);
            return f ? 1 : 0;
        }
    }

    if (test) return selftest();
    if (res)  return restest();
    if (gfx)  return gfxtest();

    if (gfxDumpPath) {
        GBitmap scene(256, 128);
        drawGfxScene(scene);
        dumpPPM(scene, gfxDumpPath);
        std::printf("[gfxdraw] wrote %s (%dx%d)\n", gfxDumpPath, scene.width(), scene.height());
        return 0;
    }

    // dev: render the demo and encode it to a real .pic on disk (exporter +
    // end-to-end exercise of the file path). Usage: --makepic out.pic
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--makepic") && i + 1 < argc) {
            CjkGlyphCache::instance().autoLoad();
            Translator::instance().loadFile("assets/zh_TW.json");
            GFont f; f.buildAsciiFromFreeType(16);
            GDriver gd; gd.addScreen(0, 440, 280); drawDemo(gd, f, true);
            std::vector<uint8_t> pic = buildPic8(gd.screen(0));
            FILE* o = std::fopen(argv[i + 1], "wb");
            if (!o) { std::fprintf(stderr, "cannot write %s\n", argv[i + 1]); return 1; }
            std::fwrite(pic.data(), 1, pic.size(), o); std::fclose(o);
            std::printf("[makepic] wrote %s (%zu bytes)\n", argv[i + 1], pic.size());
            return 0;
        }
    }

    if (picPath) {
        std::unique_ptr<GBitmap> bmp = loadPicFile(picPath, true);
        if (!bmp) { std::fprintf(stderr, "failed to load %s\n", picPath); return 1; }
        std::string out = std::string(picPath) + ".ppm";
        dumpPPM(*bmp, out.c_str());
        std::printf("[pic] %s -> %s (%dx%d)\n", picPath, out.c_str(), bmp->width(), bmp->height());
        return 0;
    }

    std::string fontPath = CjkGlyphCache::instance().autoLoad();
    int entries = Translator::instance().loadFile("assets/zh_TW.json");
    std::printf("[i18n] %d entries loaded; CJK font: %s\n", entries, fontPath.empty() ? "(none)" : fontPath.c_str());

    GFont font;
    int glyphs = font.buildAsciiFromFreeType(16);
    std::printf("[font] %d ASCII glyphs built at %dpx\n", glyphs, font.pixelHeight);

    GDriver gd;
    gd.addScreen(GDriver::MainScreen, 440, 280);
    drawDemo(gd, font, !english);
    GBitmap& fb = gd.screen(GDriver::MainScreen);

    if (dump) {
        if (!dumpPPM(fb, dumpPath)) { std::fprintf(stderr, "dump failed\n"); return 1; }
        std::printf("[dump] wrote %s (%dx%d)\n", dumpPath, fb.width(), fb.height());
        return 0;
    }

    SdlPresenter pres;
    if (!pres.init("OpenCiv1++ (zh-TW)", fb.width(), fb.height(), 3)) return 1;
    while (pres.present(fb)) { }
    pres.shutdown();
    return 0;
}
