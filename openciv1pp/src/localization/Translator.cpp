#include "Translator.h"
#include <fstream>
#include <sstream>
#include <cstdint>

namespace oc1 {

namespace {

// Minimal JSON-string reader for a flat object of "key":"value" pairs.
// Handles the escapes the generator emits (\" \\ \/ \b \f \n \r \t \uXXXX);
// UTF-8 bytes pass through untouched. Not a general JSON parser — just enough
// for zh_TW.json, with no external dependency.
struct MiniJson {
    const std::string& s;
    std::size_t i = 0;
    explicit MiniJson(const std::string& src) : s(src) {}

    void skipWs() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
            break;
        }
    }

    static void appendUtf8(std::string& out, uint32_t cp) {
        if (cp <= 0x7F) {
            out.push_back(char(cp));
        } else if (cp <= 0x7FF) {
            out.push_back(char(0xC0 | (cp >> 6)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(char(0xE0 | (cp >> 12)));
            out.push_back(char(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(char(0x80 | (cp & 0x3F)));
        }
    }

    bool parseString(std::string& out) {
        if (i >= s.size() || s[i] != '"') return false;
        ++i;
        out.clear();
        while (i < s.size()) {
            char c = s[i++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i >= s.size()) return false;
                char e = s[i++];
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'n':  out.push_back('\n'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'u': {
                        if (i + 4 > s.size()) return false;
                        uint32_t cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = s[i++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= uint32_t(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= uint32_t(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= uint32_t(h - 'A' + 10);
                            else return false;
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: out.push_back(e); break;
                }
            } else {
                out.push_back(c); // raw byte (incl. UTF-8 continuation bytes)
            }
        }
        return false;
    }
};

std::string trim(const std::string& s) {
    std::size_t a = 0, b = s.size();
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    while (a < b && ws(s[a])) ++a;
    while (b > a && ws(s[b - 1])) --b;
    return s.substr(a, b - a);
}

} // namespace

Translator& Translator::instance() {
    static Translator t;
    return t;
}

int Translator::loadFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return 0;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string json = ss.str();

    MiniJson p(json);
    p.skipWs();
    if (p.i >= json.size() || json[p.i] != '{') return 0;
    ++p.i;

    int added = 0;
    while (true) {
        p.skipWs();
        if (p.i < json.size() && json[p.i] == '}') { ++p.i; break; }

        std::string key, val;
        if (!p.parseString(key)) break;
        p.skipWs();
        if (p.i >= json.size() || json[p.i] != ':') break;
        ++p.i;
        p.skipWs();
        if (!p.parseString(val)) break;

        if (!key.empty() && key[0] != '_') {
            exact_[key] = val;
            std::string tk = trim(key);
            if (!tk.empty()) trimmed_[tk] = val;
            ++added;
        }

        p.skipWs();
        if (p.i < json.size() && json[p.i] == ',') { ++p.i; continue; }
        if (p.i < json.size() && json[p.i] == '}') { ++p.i; break; }
        break;
    }
    return added;
}

std::string Translator::translate(const std::string& text) const {
    if (!enabled || text.empty()) return text;

    auto it = exact_.find(text);
    if (it != exact_.end()) return it->second;

    std::string t = trim(text);
    if (t.size() != text.size()) {
        auto it2 = trimmed_.find(t);
        if (it2 != trimmed_.end()) return it2->second;
    }
    return text;
}

} // namespace oc1
