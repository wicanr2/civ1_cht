// Civilopedia.h — in-game encyclopedia (F1 in DOS Civ1).
//
// Simplified port of OpenCiv1's Encyclopedia.cs. The DOS Encyclopedia ships
// 100+ entries (every tech / improvement / unit / terrain / wonder, plus
// game concepts like Fortify and Veteran). This port ships the iconic
// subset that already has zh_TW.json keys for the LABELS — every label is
// rendered through GDriver::drawString so it is localized automatically.
//
// Each entry also has a short Chinese DESCRIPTION (registered separately via
// zh_TW.json keys like "Settlers desc" -> "拓荒者建造城市並改良地形"). The
// description lookup is wrapped in Translator::translate so EN fallback is
// the desc key itself (still useful for unit tests; the actual UX always
// runs with translation on).
//
// The screen is the C# Encyclopedia layout, headlessly modelled:
//   [TOPIC]                 [Entries...]                 [Description]
//
// Pure / headless-testable: there is NO real SDL input loop. The caller
// drives navigation by calling selectCategory(i) / selectEntry(i) / draw().
// MenuBoxDialog is reused to render the entry-list panel.
//
// Bound by SdlPresenter::pollKey() -> 'e' / 'E' / F1: opens the encyclopedia
// while a game is in progress (TODO: wired in a later slice; for now the
// headless test exercises the API directly).
#pragma once
#include "OpenCiv1Game.h"
#include <string>
#include <vector>

namespace oc1 {

// One Civilopedia category (the left panel). Each category maps to an
// ordered list of english-key entries that already exist in zh_TW.json for
// labels; the description key is "<label> desc" by convention.
enum class CivilopediaCategory : int {
    Units         = 0,
    Buildings     = 1,
    Wonders       = 2,
    Terrain       = 3,
    Governments   = 4,
    Techs         = 5,
    Civilizations = 6,
    Tribes        = 7,
};
constexpr int kCivilopediaCategoryCount = 8;

class Civilopedia {
public:
    explicit Civilopedia(OpenCiv1Game& parent);

    // The category labels (English key; rendered through GDriver -> Chinese).
    static const std::vector<std::string>& categoryLabels();

    // Entries (English keys) for a given category, in display order. The
    // labels go through Translator -> Chinese; the description goes through
    // Translator::translate("<label> desc") -> Chinese.
    static const std::vector<std::string>& entriesOf(CivilopediaCategory c);

    // Description lookup helper: "<label> desc" key, falling back to the
    // English key itself when zh_TW.json has no entry yet.
    static std::string descriptionKey(const std::string& label);
    static std::string descriptionOf(const std::string& label);

    // ---- runtime navigation state (set by selectCategory/selectEntry) ----
    int selectedCategory() const { return selectedCategory_; }
    int selectedEntry() const    { return selectedEntry_; }
    // Returns the current entry's English label, or "" when none selected.
    std::string currentEntryLabel() const;

    // -1 to clear. selectCategory resets selectedEntry to 0 (or -1 if empty).
    void selectCategory(int idx);
    void selectEntry(int idx);

    // Draw the Civilopedia screen (Chinese title bar + category column +
    // entry list + selected entry's description) to screen 0. Safe to call
    // every frame; idempotent.
    void draw();

    // Returns true if (catIdx, entryIdx) is a valid (category, entry) pair.
    static bool isValid(int catIdx, int entryIdx);

private:
    OpenCiv1Game& p;
    int selectedCategory_ = 0; // default: Units
    int selectedEntry_    = 0;
};

} // namespace oc1
