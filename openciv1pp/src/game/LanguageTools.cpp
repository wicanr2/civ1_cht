#include "LanguageTools.h"
#include "DrawTools.h"
#include "../localization/Translator.h"
#include <array>
#include <string>
#include <vector>

namespace oc1 {

namespace {
void replaceAllStr(std::string& s, const std::string& from, const std::string& to) {
    if (from.empty()) return;
    std::size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
}
} // namespace

LanguageTools::LanguageTools(OpenCiv1Game& parent) : p(parent), cpu(parent.cpu) {
    (void)cpu; // file-backed lookups (which would touch CPU memory) are stubbed.
}

std::string LanguageTools::F0_2f4d_0000_AdjustTextBlockWidth(const std::string& text, int maxLength) {
    // function body — faithful 1:1 port of the C# index logic. newText is built
    // char-by-char; previousDelimiter indexes the last space in newText that can
    // be promoted to a newline when the current line exceeds maxLength.
    int previousDelimiter = 0;
    int currentTextLength = 0;
    std::vector<char> newText;

    int len = int(text.size());
    for (int i = 0; i < len; i++) {
        char ch = text[std::size_t(i)];
        char nextCh = (i + 1 < len) ? text[std::size_t(i + 1)] : '\0';

        if (ch == ' ' || ch == '^' || ch == '\n') {
            if ((ch == '^' || ch == '\n') && (nextCh == ' ' || nextCh == '_')) {
                // the case where an option is presented
                newText.push_back('\n');
                newText.push_back(nextCh);
                currentTextLength = 1;
                previousDelimiter = 0;
                i++;
            } else if (!newText.empty() && currentTextLength > 0) {
                newText.push_back(' ');
                currentTextLength++;
                previousDelimiter = int(newText.size()) - 1;
            } else {
                previousDelimiter = 0;
            }
        } else {
            newText.push_back(ch);
            currentTextLength++;

            if (currentTextLength > maxLength && previousDelimiter > 0) {
                newText[std::size_t(previousDelimiter)] = '\n';
                previousDelimiter = 0;
                currentTextLength = i - previousDelimiter;
            }
        }
    }

    if (currentTextLength > 0) {
        while (!newText.empty() && newText.back() == ' ')
            newText.pop_back();
        newText.push_back('\n');
    }

    return std::string(newText.begin(), newText.end());
}

std::string LanguageTools::F0_2f4d_04f7_TrimStringToWidth(std::string text, int maxWidth) {
    // function body
    while (text.size() > 1 && p.drawTools().F0_1182_00ef_GetStringWidth(text) > maxWidth) {
        if (text[text.size() - 2] != ' ') {
            text = text.substr(0, text.size() - 2) + '.';
        } else {
            text = text.substr(0, text.size() - 1);
        }
    }
    return text;
}

std::string LanguageTools::F0_2f4d_01ad_GetTextBySectionAndKey(const std::string& section,
                                                               const std::string& key) {
    // function body
    // The C# opens "{ResourcePath}{section}.TXT", seeks to 0x212, scans for the
    // key line and concatenates the following lines. That parsing + lookup now
    // lives in the TextResource resource module, which resolves the file under
    // OpenCiv1Game::resourcePath() and caches parsed files.
    //
    // Deviation from C#: a missing key returns "" (the C# pops a MessageBox; the
    // C++ port has no UI message box yet — callers already tolerate "").
    //
    // Localization: the .TXT body is the original English. Route it through the
    // shared Translator (the same chokepoint as DrawString) so loaded text is
    // localized like everything else; a miss falls through to the English text.
    std::string text = p.textResource().getText(section, key);
    return Translator::instance().translate(text);
}

std::string LanguageTools::F0_2f4d_044f_GetTextFromKingSection(const std::string& key, int maxWidth) {
    // function body
    return F0_2f4d_0000_AdjustTextBlockWidth(
        F0_2f4d_0471_ReplaceKeywords(F0_2f4d_01ad_GetTextBySectionAndKey("KING", key)), maxWidth);
}

std::string LanguageTools::F0_2f4d_0471_ReplaceKeywords(std::string text) {
    // function body
    static const std::array<const char*, 5> Array_30ae = {"$US", "$THEM", "$BUCKS", "$RPLC1", "$RPLC2"};
    for (std::size_t i = 0; i < Array_30ae.size(); i++) {
        replaceAllStr(text, Array_30ae[i], p.Array_30b8[i]);
    }
    return text;
}

} // namespace oc1
