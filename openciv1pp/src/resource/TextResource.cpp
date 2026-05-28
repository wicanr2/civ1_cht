#include "TextResource.h"
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace oc1 {

namespace {

// Offset of the ASCII body inside a Civ ".TXT" file: a 0x212-byte binary header
// precedes the text (a hash table the original engine seeks past). Matches the
// C# `inputStream.Seek(0x212, SeekOrigin.Begin)`.
constexpr std::streamoff kBodyOffset = 0x212;

// Reads one line, stripping a trailing '\r' so CRLF and LF files behave the same
// (the C# StreamReader.ReadLine does this transparently). Returns false at EOF
// with no data.
bool readLine(std::istream& in, std::string& out) {
    if (!std::getline(in, out)) return false;
    if (!out.empty() && out.back() == '\r') out.pop_back();
    return true;
}

bool startsWith(const std::string& s, char c) { return !s.empty() && s[0] == c; }

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string trimEnd(const std::string& s) {
    std::size_t b = s.size();
    while (b > 0 && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(0, b);
}

std::string toUpper(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

// Strips trailing characters in `chars` (mirrors C# string.TrimEnd(params)).
std::string trimEndChars(const std::string& s, const std::string& chars) {
    std::size_t b = s.size();
    while (b > 0 && chars.find(s[b - 1]) != std::string::npos) --b;
    return s.substr(0, b);
}

void collapseDoubleSpaces(std::string& s) {
    std::size_t pos;
    while ((pos = s.find("  ")) != std::string::npos)
        s.replace(pos, 2, " ");
}

} // namespace

TextResource::TextResource(std::string resourceDir) : resourceDir_(std::move(resourceDir)) {}

const TextResource::ParsedFile* TextResource::loadSection(const std::string& section, bool& opened) {
    // Build "{resourceDir}{section}.TXT". The C# concatenates ResourcePath with
    // "{section}.TXT" directly; we add a '/' separator only if one is missing.
    std::string path = resourceDir_;
    if (!path.empty() && path.back() != '/' && path.back() != '\\') path += '/';
    path += section + ".TXT";

    auto it = cache_.find(path);
    if (it != cache_.end()) { opened = true; return &it->second; }

    std::ifstream in(path, std::ios::binary);
    if (!in) { opened = false; return nullptr; }

    in.seekg(kBodyOffset);

    // Parse the body the way the C# per-key scan would resolve it: a group of one
    // or more consecutive '*' key lines shares the content block that follows,
    // up to the next '*' line. Each key (uppercased) maps to that joined block.
    ParsedFile parsed;
    std::string line;
    bool have = readLine(in, line);

    while (have) {
        if (!startsWith(line, '*')) { have = readLine(in, line); continue; }

        // Collect the consecutive key lines (aliases share one content block).
        std::vector<std::string> keys;
        while (have && startsWith(line, '*')) {
            keys.push_back(toUpper(trimEnd(line)));
            have = readLine(in, line);
        }

        // Collect content lines until the next '*' line (or EOF).
        std::string item;
        while (have && !startsWith(line, '*')) {
            std::string t = trim(line);
            if (!t.empty()) {
                item += trimEnd(t);
                item += '\n';
            }
            have = readLine(in, line);
        }

        // Finalize: TrimEnd(' ', '^', '\n') then collapse double spaces.
        item = trimEndChars(item, " ^\n");
        collapseDoubleSpaces(item);

        for (const std::string& k : keys)
            parsed.emplace(k, item); // first definition wins, like the scan
    }

    auto ins = cache_.emplace(path, std::move(parsed));
    opened = true;
    return &ins.first->second;
}

std::string TextResource::getText(const std::string& section, const std::string& key) {
    bool opened = false;
    const ParsedFile* file = loadSection(section, opened);
    if (!file) return "";

    auto it = file->find(toUpper(trimEnd(key)));
    if (it == file->end()) return "";
    return it->second;
}

} // namespace oc1
