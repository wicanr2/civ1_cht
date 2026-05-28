// DrawTools.h — first ported CodeObject (OpenCiv1 DrawTools.cs).
//
// The text-layout layer the menus/dialogs sit on: plain / shadowed / centered
// strings, string measuring, and word-wrapped text blocks. Faithful port; the
// F0_* method names match the C# so cross-referencing stays mechanical. All
// text flows through GDriver (translation chokepoint), so this layer is
// localized for free.
#pragma once
#include "OpenCiv1Game.h"
#include <string>

namespace oc1 {

class DrawTools {
public:
    explicit DrawTools(OpenCiv1Game& parent);

    void F0_1182_002a_DrawString(const std::string& text, int x, int y, uint8_t frontColor);
    void F0_1182_005c_DrawStringToScreen0(const std::string& text, int x, int y, uint8_t frontColor);
    void F0_1182_0086_DrawStringWithShadowToScreen0(const std::string& text, int x, int y, uint8_t frontColor);
    void F0_1182_00b3_DrawCenteredStringToScreen0(const std::string& text, int x, int y, uint8_t frontColor);
    void F0_1182_00b3_DrawCenteredStringWithShadowToScreen0(const std::string& text, int x, int y, uint8_t frontColor);
    int  F0_1182_00ef_GetStringWidth(const std::string& text);
    int  DrawTextBlock(int x, int y, std::string text, int maxWidth, uint8_t frontColor);
    int  GetTextBlockHeight(std::string text, int y, int maxWidth);

private:
    OpenCiv1Game& p;
    VCPU& cpu;
};

} // namespace oc1
