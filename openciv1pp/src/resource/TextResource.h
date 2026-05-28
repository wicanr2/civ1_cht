// TextResource.h — Civilization section/key ".TXT" reader, ported from the
// OpenCiv1 LanguageTools file parsing (ParseSectionFile / GetTextBySectionAndKey).
//
// A Civ ".TXT" language file begins with a 0x212-byte binary header (a hash
// table the original code does not use reliably), after which the body is plain
// ASCII text. The body is a sequence of entries:
//
//     *KEY            <- one or more key lines, each starting with '*'
//     *ALIAS
//     content line 1  <- content lines (none start with '*')
//     content line 2
//     *NEXTKEY        <- next entry begins
//
// getText(section, key) opens "{section}.TXT" under the configured resource
// directory, scans the body for the line that exactly equals `key` (the leading
// '*' is part of the key, e.g. "*PALACE"; case-insensitive), then concatenates
// the following content lines — trimmed, '\n'-joined, double-spaces collapsed —
// up to (but not including) the next '*' line. This mirrors the C#
// LanguageTools.F0_2f4d_01ad_GetTextBySectionAndKey lookup faithfully.
//
// Parsed files are cached by resolved path so repeated lookups are cheap.
#pragma once
#include <map>
#include <string>
#include <vector>

namespace oc1 {

class TextResource {
public:
    // resourceDir is the directory that holds the "<SECTION>.TXT" files. A
    // trailing separator is optional. The C# uses parent.ResourcePath joined
    // directly with "{section}.TXT".
    explicit TextResource(std::string resourceDir = ".");

    void setResourceDir(std::string resourceDir) { resourceDir_ = std::move(resourceDir); }
    const std::string& resourceDir() const { return resourceDir_; }

    // Looks up `key` (including its leading '*') in "<section>.TXT". Returns the
    // joined content, or "" if the file is missing or the key is not found.
    std::string getText(const std::string& section, const std::string& key);

    // Clears the parsed-file cache (e.g. after changing the resource dir).
    void clearCache() { cache_.clear(); }

private:
    // One parsed file: key (uppercased, '*'-prefixed) -> joined content text.
    using ParsedFile = std::map<std::string, std::string>;

    // Returns the parsed file for `section`, loading+caching it on first use.
    // The bool out-param reports whether the file could be opened at all.
    const ParsedFile* loadSection(const std::string& section, bool& opened);

    std::string resourceDir_;
    std::map<std::string, ParsedFile> cache_; // resolved path -> parsed entries
};

} // namespace oc1
