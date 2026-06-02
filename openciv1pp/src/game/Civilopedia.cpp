// Civilopedia.cpp — see Civilopedia.h for the design notes.
//
// All labels are zh_TW.json keys already loaded by setupGame(); descriptions
// use the convention "<label> desc" (the zh_TW.json must therefore add ~50
// "<label> desc" -> "<中文>" pairs). The Encyclopedia layout is a thin
// composition over MenuBoxDialog (entry list) + DrawTools (title + description).
#include "Civilopedia.h"
#include "DrawTools.h"
#include "MenuBoxDialog.h"
#include "../localization/Translator.h"

namespace oc1 {

Civilopedia::Civilopedia(OpenCiv1Game& parent) : p(parent) {}

const std::vector<std::string>& Civilopedia::categoryLabels() {
    static const std::vector<std::string> kLabels = {
        "Units", "Buildings", "Wonders", "Terrain",
        "Governments", "Techs", "Civilizations", "Tribes",
    };
    return kLabels;
}

const std::vector<std::string>& Civilopedia::entriesOf(CivilopediaCategory c) {
    // The entries below are the zh_TW.json LABEL keys already present in
    // assets/zh_TW.json (verified before authoring). Each label is paired
    // with a "<label> desc" key added at the bottom of zh_TW.json.
    static const std::vector<std::string> kUnits = {
        "Settlers", "Militia", "Phalanx", "Cavalry", "Legion",
        "Knight", "Catapult", "Musketeers", "Cannon", "Trireme",
    };
    static const std::vector<std::string> kBuildings = {
        "Granary", "Barracks", "Walls", "Temple",
        "Marketplace", "Library", "Cathedral", "Aqueduct",
    };
    static const std::vector<std::string> kWonders = {
        "Pyramids", "Hanging Gardens", "Great Wall", "Colossus",
        "Apollo Program", "Magellan's Expedition", "Lighthouse", "Oracle",
    };
    static const std::vector<std::string> kTerrain = {
        "Ocean", "Grassland", "Plains", "Forest", "Hills",
        "Mountains", "Desert", "Tundra", "Arctic", "Swamp",
        "Jungle", "River",
    };
    static const std::vector<std::string> kGovernments = {
        "Anarchy", "Despotism", "Govt Monarchy", "Republic", "Govt Democracy",
    };
    static const std::vector<std::string> kTechs = {
        "Alphabet", "Bronze Working", "Pottery", "The Wheel",
        "Currency", "Writing", "Masonry", "Iron Working",
        "Mathematics", "Feudalism", "Gunpowder", "Metallurgy",
        "Mysticism", "Construction", "Map Making", "Monarchy",
        "Code of Laws",
    };
    static const std::vector<std::string> kCivilizations = {
        "Romans", "Babylonians", "Germans", "Egyptians", "Americans",
        "Greeks", "Indians", "Russians", "Zulus", "French",
        "Aztecs", "Chinese", "English", "Mongols",
    };
    static const std::vector<std::string> kTribes = {
        "Caesar", "Hammurabi", "Frederick", "Ramesses", "Abe Lincoln",
        "Alexander", "M.Gandhi", "Stalin", "Shaka", "Napoleon",
        "Montezuma", "Mao Tse Tung", "Elizabeth I", "Genghis Khan",
    };
    static const std::vector<std::string> kEmpty;
    switch (c) {
        case CivilopediaCategory::Units:         return kUnits;
        case CivilopediaCategory::Buildings:     return kBuildings;
        case CivilopediaCategory::Wonders:       return kWonders;
        case CivilopediaCategory::Terrain:       return kTerrain;
        case CivilopediaCategory::Governments:   return kGovernments;
        case CivilopediaCategory::Techs:         return kTechs;
        case CivilopediaCategory::Civilizations: return kCivilizations;
        case CivilopediaCategory::Tribes:        return kTribes;
    }
    return kEmpty;
}

std::string Civilopedia::descriptionKey(const std::string& label) {
    return label + " desc";
}

std::string Civilopedia::descriptionOf(const std::string& label) {
    // Go through Translator with the "<label> desc" key. When the key is
    // absent the translator returns the key itself (English fallback) — the
    // headless test asserts on the translated string when translation is on.
    return Translator::instance().translate(descriptionKey(label));
}

bool Civilopedia::isValid(int catIdx, int entryIdx) {
    if (catIdx < 0 || catIdx >= kCivilopediaCategoryCount) return false;
    const auto& es = entriesOf(static_cast<CivilopediaCategory>(catIdx));
    return entryIdx >= 0 && entryIdx < int(es.size());
}

std::string Civilopedia::currentEntryLabel() const {
    if (!isValid(selectedCategory_, selectedEntry_)) return "";
    const auto& es = entriesOf(static_cast<CivilopediaCategory>(selectedCategory_));
    return es[std::size_t(selectedEntry_)];
}

void Civilopedia::selectCategory(int idx) {
    if (idx < 0 || idx >= kCivilopediaCategoryCount) { selectedCategory_ = -1; selectedEntry_ = -1; return; }
    selectedCategory_ = idx;
    const auto& es = entriesOf(static_cast<CivilopediaCategory>(idx));
    selectedEntry_ = es.empty() ? -1 : 0;
}

void Civilopedia::selectEntry(int idx) {
    if (!isValid(selectedCategory_, idx)) { selectedEntry_ = -1; return; }
    selectedEntry_ = idx;
}

void Civilopedia::draw() {
    // Layout (640x480 framebuffer): banner across the top, three columns
    //   - title bar (Y=0..18)
    //   - left:   category list   (X=10,  Y=30)
    //   - middle: entry list      (X=170, Y=30)
    //   - right:  selected entry's label + description (X=350, Y=30)
    GBitmap& fb = p.graphics.screen(GDriver::MainScreen);
    fb.clear(1);

    // Title bar: "文明百科" centered in the top band (color 14 = bright accent).
    p.drawTools().F0_1182_00b3_DrawCenteredStringWithShadowToScreen0(
        "Civilopedia", fb.width() / 2, 6, 14);

    // Category column header + items.
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Categories", 10, 30, 15);
    int y = 50;
    const int lineH = 18;
    const auto& cats = categoryLabels();
    for (std::size_t i = 0; i < cats.size(); ++i) {
        uint8_t color = (int(i) == selectedCategory_) ? 14 : 11;
        p.drawTools().F0_1182_005c_DrawStringToScreen0(cats[i], 10, y, color);
        y += lineH;
    }

    // Entry list column.
    p.drawTools().F0_1182_005c_DrawStringToScreen0("Entries", 170, 30, 15);
    if (selectedCategory_ >= 0 && selectedCategory_ < kCivilopediaCategoryCount) {
        const auto& es = entriesOf(static_cast<CivilopediaCategory>(selectedCategory_));
        y = 50;
        for (std::size_t i = 0; i < es.size(); ++i) {
            uint8_t color = (int(i) == selectedEntry_) ? 14 : 11;
            p.drawTools().F0_1182_005c_DrawStringToScreen0(es[i], 170, y, color);
            y += lineH;
        }
    }

    // Right column: selected entry label + description.
    std::string label = currentEntryLabel();
    if (!label.empty()) {
        p.drawTools().F0_1182_005c_DrawStringToScreen0(label, 350, 30, 14);
        // Word-wrap description into a 270px column starting at y=60.
        std::string desc = descriptionOf(label);
        if (!desc.empty()) {
            p.drawTools().DrawTextBlock(350, 60, desc, 270, 15);
        }
    }
}

} // namespace oc1
