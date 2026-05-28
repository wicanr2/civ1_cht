#include "DrawTools.h"

namespace oc1 {

namespace {
void replaceAllChar(std::string& s, char from, char to) {
    for (char& c : s) if (c == from) c = to;
}
void replaceAllStr(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) { s.replace(pos, from.size(), to); pos += to.size(); }
}
std::string trimSpacesNewlines(const std::string& s) {
    std::size_t a = 0, b = s.size();
    auto isws = [](char c) { return c == ' ' || c == '\n'; };
    while (a < b && isws(s[a])) ++a;
    while (b > a && isws(s[b - 1])) --b;
    return s.substr(a, b - a);
}
// Civ text munging: '^' (and "^\n") become hard line breaks; other newlines/CRs
// collapse to spaces. (1:1 with DrawTools.cs.)
std::string normalizeBlockText(std::string text) {
    replaceAllChar(text, '\r', ' ');
    replaceAllStr(text, "^\n", "\r");
    replaceAllChar(text, '\n', ' ');
    replaceAllChar(text, '\r', '\n');
    replaceAllChar(text, '^', '\n');
    while (text.find("  ") != std::string::npos) replaceAllStr(text, "  ", " ");
    if (text.empty() || text.back() != '\n') text += '\n';
    return text;
}
} // namespace

DrawTools::DrawTools(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {}

void DrawTools::F0_1182_002a_DrawString(const std::string& text, int x, int y, uint8_t frontColor) {
    p.var_aa.frontColor = frontColor;
    if (cpu.ReadUInt16(cpu.DS.u16(), 0x6b8c) != 0) // double-width text mode flag
        x *= 2;
    p.graphics.drawString(p.var_aa, x, y, text);
}

void DrawTools::F0_1182_005c_DrawStringToScreen0(const std::string& text, int x, int y, uint8_t frontColor) {
    p.var_aa.flags = 0;
    F0_1182_002a_DrawString(text, x, y, frontColor);
    p.var_aa.flags = 1;
}

void DrawTools::F0_1182_0086_DrawStringWithShadowToScreen0(const std::string& text, int x, int y, uint8_t frontColor) {
    F0_1182_005c_DrawStringToScreen0(text, x, y + 1, 0);
    F0_1182_005c_DrawStringToScreen0(text, x, y, frontColor);
}

void DrawTools::F0_1182_00b3_DrawCenteredStringToScreen0(const std::string& text, int x, int y, uint8_t frontColor) {
    x -= F0_1182_00ef_GetStringWidth(text) / 2;
    p.var_aa.flags = 0;
    F0_1182_002a_DrawString(text, x, y, frontColor);
    p.var_aa.flags = 1;
}

void DrawTools::F0_1182_00b3_DrawCenteredStringWithShadowToScreen0(const std::string& text, int x, int y, uint8_t frontColor) {
    x -= F0_1182_00ef_GetStringWidth(text) / 2;
    p.var_aa.flags = 0;
    F0_1182_002a_DrawString(text, x + 1, y + 1, 0);
    F0_1182_002a_DrawString(text, x, y, frontColor);
    p.var_aa.flags = 1;
}

int DrawTools::F0_1182_00ef_GetStringWidth(const std::string& text) {
    int width = p.graphics.getDrawStringSize(p.var_aa.fontID, text).w;
    cpu.AX.u16(uint16_t(int16_t(width)));
    return width;
}

int DrawTools::DrawTextBlock(int x, int y, std::string text, int maxWidth, uint8_t frontColor) {
    int newY = y, lastSep = -1, cur = 0;
    text = normalizeBlockText(std::move(text));
    int len = int(text.size());

    while (cur < len && newY < 193) {
        std::size_t sp = text.find_first_of(" \n", lastSep + 1);
        if (sp != std::string::npos) {
            char sepChar = text[sp];
            std::string part = text.substr(cur, sp - cur);
            Size ps = p.graphics.getDrawStringSize(p.var_aa.fontID, part);
            if (ps.w > maxWidth) {
                std::string draw = text.substr(cur, lastSep - cur);
                F0_1182_005c_DrawStringToScreen0(draw, x, newY, frontColor);
                cur = lastSep + 1;
                lastSep = int(sp);
                newY += ps.h;
            } else if (sepChar == '\n') {
                F0_1182_005c_DrawStringToScreen0(part, x, newY, frontColor);
                cur = int(sp) + 1;
                lastSep = int(sp);
                newY += ps.h;
            } else {
                lastSep = int(sp);
            }
        } else {
            std::string part = trimSpacesNewlines(text.substr(cur, len - cur));
            Size ps = p.graphics.getDrawStringSize(p.var_aa.fontID, part);
            F0_1182_005c_DrawStringToScreen0(part, x, newY, frontColor);
            cur = len;
            lastSep = len;
            newY += ps.h;
        }
    }
    return newY;
}

int DrawTools::GetTextBlockHeight(std::string text, int y, int maxWidth) {
    int newY = y, lastSep = -1, cur = 0;
    text = normalizeBlockText(std::move(text));
    int len = int(text.size());

    while (cur < len && newY < 193) {
        std::size_t sp = text.find_first_of(" \n", lastSep + 1);
        if (sp != std::string::npos) {
            char sepChar = text[sp];
            std::string part = text.substr(cur, sp - cur);
            Size ps = p.graphics.getDrawStringSize(p.var_aa.fontID, part);
            if (ps.w > maxWidth) {
                cur = lastSep + 1; lastSep = int(sp); newY += ps.h;
            } else if (sepChar == '\n') {
                cur = int(sp) + 1; lastSep = int(sp); newY += ps.h;
            } else {
                lastSep = int(sp);
            }
        } else {
            std::string part = text.substr(cur, len - cur);
            Size ps = p.graphics.getDrawStringSize(p.var_aa.fontID, part);
            cur = len; lastSep = len; newY += ps.h;
        }
    }
    return newY - y;
}

} // namespace oc1
