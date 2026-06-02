// LanguageTools.h — ported CodeObject (OpenCiv1 LanguageTools.cs).
//
// The text post-processing layer for the game's language pack: word-wrapping a
// text block to a character width, trimming a string to a pixel width (with an
// ellipsis), keyword substitution ($US/$THEM/...), and the language-pack
// lookups that the KING/BLURB/etc. sections feed. The F0_2f4d_* method names
// mirror the C# so cross-referencing stays mechanical.
//
// Deviations from C# (all documented in LanguageTools.cpp):
//   - F0_2f4d_01ad_GetTextBySectionAndKey reads "<SECTION>.TXT" from the
//     resource directory via the TextResource module (the port of the C#
//     ParseSectionFile parsing + key lookup), resolving paths against
//     OpenCiv1Game::resourcePath(). A missing key returns "" (the C# pops a
//     MessageBox; there is no message-box UI in the port yet). The returned
//     English text is then run through the shared Translator so .TXT text is
//     localized at the same chokepoint as DrawString.
//   - ReplaceKeywords substitutes from OpenCiv1Game::Array_30b8 (added to the
//     game shell to mirror the C# global), defaulting to 5 empty strings.
#pragma once
#include "OpenCiv1Game.h"
#include <string>

namespace oc1 {

class LanguageTools {
public:
    explicit LanguageTools(OpenCiv1Game& parent);

    // Word-wraps a text block to at most maxLength characters per line, turning
    // spaces/'^'/'\n' into break points. "^ " / "^_" (and the '\n' variants)
    // introduce a forced new line for presented options. Faithful 1:1 port.
    std::string F0_2f4d_0000_AdjustTextBlockWidth(const std::string& text, int maxLength);

    // Trims characters from the end of text until it fits in maxWidth pixels,
    // appending '.' as an ellipsis. Uses DrawTools::F0_1182_00ef_GetStringWidth.
    std::string F0_2f4d_04f7_TrimStringToWidth(std::string text, int maxWidth);

    // Language-pack lookup by section + key, via TextResource (see header note).
    // The key includes its leading '*' (e.g. "*PALACE"). Result is Translator-localized.
    std::string F0_2f4d_01ad_GetTextBySectionAndKey(const std::string& section,
                                                    const std::string& key);

    // KING-section lookup -> ReplaceKeywords -> AdjustTextBlockWidth.
    std::string F0_2f4d_044f_GetTextFromKingSection(const std::string& key, int maxWidth = 80);

    // Replaces the $US/$THEM/$BUCKS/$RPLC1/$RPLC2 keywords with the parent's
    // Array_30b8 values. Faithful port.
    std::string F0_2f4d_0471_ReplaceKeywords(std::string text);

private:
    OpenCiv1Game& p;
    VCPU& cpu;
};

} // namespace oc1
