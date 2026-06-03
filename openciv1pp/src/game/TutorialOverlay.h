// TutorialOverlay.h — A3: one-time first-turn tutorial overlay.
//
// Civ1 DOS shows a schematic on the very first turn pointing out every UI
// region (map, HUD, minimap, year, gold, science). After the player dismisses
// it (any key) the overlay never returns this session AND the dismissed flag
// is persisted in the save file (v17+) so a save/load round-trip on turn 2+
// stays dismissed.
//
// Visuals: 5–7 semitransparent white rectangles outlining UI regions, each
// labelled with a Chinese arrow/string ("← 你的單位", "↑ 食物/盾牌/貿易",
// "→ 小地圖"…). The labels go through the Translator so the English keys
// translate via assets/zh_TW.json. ESC or Enter dismisses.
#pragma once
#include "../graphics/GBitmap.h"
#include "../graphics/GDriver.h"

namespace oc1 {

class TutorialOverlay {
public:
    // True once the player has dismissed the overlay (also persisted in the
    // save file v17+). Save/load helpers set this directly.
    bool shown = false;

    // Whether the overlay should fire on turn 1 at all. Defaults FALSE so unit
    // tests (which build a MiniWorld and call draw() on turn 1) don't get the
    // overlay drawn over the minimap / city pixels they assert on. The real
    // game flow (FrontEndFlow::enterPlaying) sets armed = true so the overlay
    // greets a fresh new-game player exactly like DOS Civ1 does.
    bool armed = false;

    // Layer the schematic on top of the current framebuffer. Draws 5–7
    // semitransparent white callout boxes + Chinese labels. The caller is
    // responsible for having already rendered the normal game frame underneath
    // (so the boxes outline real UI regions). fontId picks the GFont used for
    // labels. Const-correct: this only writes to gd/fb, NOT to *this (the
    // `shown` flag stays unchanged until handleKey dismisses it).
    void show(GDriver& gd, int fontId) const;

    // Process a single nav key. ESC/Enter dismisses (shown=true, returns true).
    // Any other key is treated as "any key dismisses" per the spec — returns
    // true. The play loop should call this only while !shown (it is a no-op
    // and returns false when called after dismissal).
    bool handleKey(int navKey);
};

} // namespace oc1
