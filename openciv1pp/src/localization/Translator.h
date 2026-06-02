// Translator.h — runtime English -> 繁體中文 lookup applied at the single
// DrawString chokepoint, mirroring the design validated in the C# route-A
// spike. The table is seeded from the civ1_cht project's hand-checked
// translations (assets/zh_TW.json). Misses fall through to English, so partial
// coverage is always safe.
#pragma once
#include <string>
#include <unordered_map>

namespace oc1 {

class Translator {
public:
    static Translator& instance();

    // Loads/merges a flat {"english":"中文"} JSON file. Returns entry count;
    // safe to call more than once (later files override earlier keys).
    int loadFile(const std::string& path);

    // Returns the translation, or the original when no entry matches. Tries an
    // exact match first, then a whitespace-trimmed match.
    std::string translate(const std::string& text) const;

    bool enabled = true;
    std::size_t count() const { return exact_.size(); }

private:
    Translator() = default;
    std::unordered_map<std::string, std::string> exact_;
    std::unordered_map<std::string, std::string> trimmed_;
};

} // namespace oc1
