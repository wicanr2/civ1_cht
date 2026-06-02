// HallOfFame.cpp — see HallOfFame.h for the design notes.
//
// Storage format: a flat JSON array of objects:
//   [{"name":"...","tribe":"...","score":N,"year":Y,"result":0|1|2,"date":"..."},
//    ...]
// Tiny hand-rolled writer / parser — same approach as Translator.cpp so we
// don't pull in a JSON library dependency.
#include "HallOfFame.h"
#include "DrawTools.h"
#include "../localization/Translator.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace oc1 {

HallOfFame::HallOfFame(OpenCiv1Game& parent) : p(parent) {}

int HallOfFame::computeScore(int citiesPop, int wonders, int techs,
                             int landPercent, HallOfFameResult result) {
    int base = citiesPop + 2 * wonders + techs + landPercent;
    if (base < 0) base = 0;
    switch (result) {
        case HallOfFameResult::Conquest: return base;
        case HallOfFameResult::Space:    return base * 2;
        case HallOfFameResult::Defeat:   return base / 2;
    }
    return base;
}

void HallOfFame::addRecord(const HallOfFameRecord& r) {
    records_.push_back(r);
    sortByScore();
}

void HallOfFame::sortByScore() {
    std::stable_sort(records_.begin(), records_.end(),
        [](const HallOfFameRecord& a, const HallOfFameRecord& b) {
            return a.score > b.score;
        });
}

const char* HallOfFame::resultKey(HallOfFameResult r) {
    switch (r) {
        case HallOfFameResult::Conquest: return "Conquest Victory";
        case HallOfFameResult::Space:    return "Space Race Victory";
        case HallOfFameResult::Defeat:   return "Defeat";
    }
    return "Defeat";
}

std::string HallOfFame::storagePath() const {
    if (!storageDir_.empty()) {
        return storageDir_ + "/halloffame.json";
    }
    const char* home = std::getenv("HOME");
    std::string base = home ? std::string(home) + "/.openciv1pp" : std::string(".");
    std::error_code ec;
    std::filesystem::create_directories(base, ec); // best-effort
    return base + "/halloffame.json";
}

namespace {
// Escape a UTF-8 string for JSON (just the seven mandatory escapes — no \uXXXX
// for non-ASCII, matching Translator.cpp's pragmatic flat-JSON convention).
std::string jsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out.push_back(c); break;
        }
    }
    return out;
}
} // namespace

bool HallOfFame::save() const {
    std::string path = storagePath();
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f << "[\n";
    for (std::size_t i = 0; i < records_.size(); ++i) {
        const auto& r = records_[i];
        f << "  {"
          << "\"name\":\""   << jsonEscape(r.name)  << "\","
          << "\"tribe\":\""  << jsonEscape(r.tribe) << "\","
          << "\"score\":"    << r.score             << ","
          << "\"year\":"     << r.year              << ","
          << "\"result\":"   << int(r.result)       << ","
          << "\"date\":\""   << jsonEscape(r.date)  << "\""
          << "}";
        if (i + 1 < records_.size()) f << ",";
        f << "\n";
    }
    f << "]\n";
    return f.good();
}

namespace {
// Tiny JSON-array parser tuned for HallOfFame's exact format. Returns true on
// success; on failure leaves `out` empty.
struct ArrParser {
    const std::string& s;
    std::size_t i = 0;
    explicit ArrParser(const std::string& src) : s(src) {}

    void ws() {
        while (i < s.size()) {
            char c = s[i];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') { ++i; continue; }
            break;
        }
    }
    bool peek(char c) { ws(); return i < s.size() && s[i] == c; }
    bool eat(char c) { if (!peek(c)) return false; ++i; return true; }

    bool parseStr(std::string& out) {
        ws();
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
                    default:   out.push_back(e);    break;
                }
            } else {
                out.push_back(c);
            }
        }
        return false;
    }
    bool parseInt(int& out) {
        ws();
        std::size_t start = i;
        if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        if (i == start) return false;
        out = std::atoi(s.substr(start, i - start).c_str());
        return true;
    }
};
} // namespace

bool HallOfFame::load() {
    std::string path = storagePath();
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    std::string src = ss.str();

    ArrParser p2(src);
    if (!p2.eat('[')) return false;
    std::vector<HallOfFameRecord> tmp;
    if (p2.peek(']')) { p2.eat(']'); records_.swap(tmp); records_ = tmp; sortByScore(); return true; }
    while (true) {
        if (!p2.eat('{')) return false;
        HallOfFameRecord r{};
        // Loop over comma-separated key:value pairs.
        while (true) {
            std::string k;
            if (!p2.parseStr(k)) return false;
            if (!p2.eat(':')) return false;
            if (k == "name")       { if (!p2.parseStr(r.name))  return false; }
            else if (k == "tribe") { if (!p2.parseStr(r.tribe)) return false; }
            else if (k == "score") { if (!p2.parseInt(r.score)) return false; }
            else if (k == "year")  { if (!p2.parseInt(r.year))  return false; }
            else if (k == "result"){ int v=0; if (!p2.parseInt(v)) return false;
                                     r.result = static_cast<HallOfFameResult>(v); }
            else if (k == "date")  { if (!p2.parseStr(r.date))  return false; }
            else {
                // Unknown key: try to skip a string OR number.
                std::string discard;
                if (!p2.parseStr(discard)) {
                    int dn = 0;
                    if (!p2.parseInt(dn)) return false;
                }
            }
            if (p2.eat(',')) continue;
            if (p2.eat('}')) break;
            return false;
        }
        tmp.push_back(r);
        if (p2.eat(',')) continue;
        if (p2.eat(']')) break;
        return false;
    }
    records_.swap(tmp);
    sortByScore();
    return true;
}

void HallOfFame::draw() {
    GBitmap& fb = p.graphics.screen(GDriver::MainScreen);
    fb.clear(1);

    p.drawTools().F0_1182_00b3_DrawCenteredStringWithShadowToScreen0(
        "Hall of Fame", fb.width() / 2, 6, 14);

    // Column headers (English keys; rendered translated).
    const int yHeader = 40;
    const int xRank  = 20;
    const int xName  = 80;
    const int xCiv   = 220;
    const int xYear  = 340;
    const int xScore = 430;
    const int xRes   = 510;
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Rank",   xRank,  yHeader, 15);
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Name",   xName,  yHeader, 15);
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Civ",    xCiv,   yHeader, 15);
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Year",   xYear,  yHeader, 15);
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Score",  xScore, yHeader, 15);
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Result", xRes,   yHeader, 15);

    if (records_.empty()) {
        p.drawTools().F0_1182_00b3_DrawCenteredStringToScreen0(
            "No records yet", fb.width() / 2, 110, 11);
        return;
    }

    // Up to 8 rows (the screen would scroll for more — not in scope).
    const int lineH = 22;
    int y = yHeader + 24;
    int maxRows = int(records_.size());
    if (maxRows > 8) maxRows = 8;
    for (int i = 0; i < maxRows; ++i) {
        const auto& r = records_[std::size_t(i)];
        char buf[64];

        std::snprintf(buf, sizeof(buf), "%d", i + 1);
        p.drawTools().F0_1182_005c_DrawStringToScreen0(buf, xRank, y, 11);

        p.drawTools().F0_1182_005c_DrawStringToScreen0(r.name, xName, y, 11);
        p.drawTools().F0_1182_005c_DrawStringToScreen0(r.tribe, xCiv, y, 11);

        // Year: AD/BC suffix; translated.
        if (r.year < 0) std::snprintf(buf, sizeof(buf), "%d BC", -r.year);
        else            std::snprintf(buf, sizeof(buf), "%d AD",  r.year);
        p.drawTools().F0_1182_005c_DrawStringToScreen0(buf, xYear, y, 11);

        std::snprintf(buf, sizeof(buf), "%d", r.score);
        p.drawTools().F0_1182_005c_DrawStringToScreen0(buf, xScore, y, 11);

        p.drawTools().F0_1182_005c_DrawStringToScreen0(resultKey(r.result), xRes, y, 11);
        y += lineH;
    }
}

} // namespace oc1
