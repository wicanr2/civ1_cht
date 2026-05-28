// CityView.h — ported CodeObject (OpenCiv1 CityView.cs, F19_0000_*).
//
// The "city interior" screen the player sees when clicking on a city on the
// world map: it shows the city name (with founding year), population (one
// little person sprite per population unit), founding-year, owner tribe name,
// current production progress, and a tile-yield mini-grid of the 21 city-
// radius tiles around the city centre.
//
// Faithful where tractable. The SCREEN LAYOUT (background panel, top title
// bar, bottom info bar, side population area, 21-tile mini-grid) mirrors the
// shape of OpenCiv1's F19_0000_0000_ShowCityLayout — the panels/positions and
// which info goes where match the C#. Function names that map 1:1 keep the
// F0_/F19_ tag for cross-referencing.
//
// SIMPLIFIED (// TODO(port) below):
//   * The faithful "city as buildings/roads/wonders" procedural layout
//     (cityLayout[19,12] cellular generator + wonderRectangles + roads) is
//     replaced by a simple grid of OWNED tiles + a city centre marker. Civ1
//     specialists/happiness/trade/food math is OUT OF SCOPE here.
//   * CBACK*.PIC backdrop is loaded via ImageTools when assets are present
//     (open(cityId) loads it lazily); otherwise the screen falls back to a
//     plain colored panel.
//   * F19_0000_111f_DrawCityPopulation draws ONE small sprite/dot per ActualSize
//     point (== City.units + 1, so a freshly-founded city shows 1 dot).
//
// All labels go through DrawTools / Translator so the screen is Chinese for
// free (the new keys "City"/"Population:"/"Founded:"/"Owner:"/"Tiles:" are
// added to assets/zh_TW.json).
#pragma once
#include "../graphics/GBitmap.h"
#include "../resource/PicLoader.h"
#include <memory>
#include <string>

namespace oc1 {

class OpenCiv1Game;
struct City;

class CityView {
public:
    explicit CityView(OpenCiv1Game& parent);

    // Open/close: drives the (open?) flag the main loop uses to route input.
    // open(cityId) is a no-op (returns false) when cityId is out of range.
    bool open(int cityId);
    void close();
    bool isOpen() const { return open_; }
    int  cityId() const { return cityId_; }

    // Render the city screen onto `screen`. Reads the City record from the
    // host game's UnitManagement using cityId(). When the host game's
    // MapManagement has terrain, the 21-tile mini-grid samples real terrain
    // around (city.x, city.y); otherwise it falls back to a neutral grass fill.
    // When the host's resourcePath() contains CBACK*.PIC the backdrop is used.
    // Mirrors F19_0000_0000_ShowCityLayout's screen-build (simplified — see
    // header comment for the omitted deep sim).
    void draw(GBitmap& screen, const City& city, int fontId);

    // Same as above, but resolves City via cityId() and isOpen() (a no-op when
    // !isOpen()). Used by the interactive main loop.
    void draw(GBitmap& screen, int fontId);

    // Handle keyboard input. Returns true when the city view consumed the
    // event and the caller should re-render. ESC closes the view.
    bool handleKey(int navKey);

    // Handle mouse click in framebuffer coords. Closes the view when the
    // click falls OUTSIDE the city panel (mirrors the Civ1 behaviour:
    // click-outside-city-screen returns to the map). Returns true if handled.
    bool handleClick(int fbX, int fbY);

private:
    OpenCiv1Game& p;
    bool open_ = false;
    int  cityId_ = -1;
    // Cached CBACK*.PIC backdrop (loaded lazily on open()). nullptr when assets
    // absent (fallback path used in draw()).
    std::unique_ptr<GBitmap> backdrop_;
    bool triedLoadBackdrop_ = false;
    // Cached TER257.PIC tileset (loaded lazily on open()) — drives the 21-tile
    // mini-grid's real-tile blit path. nullptr falls back to per-terrain
    // distinct palette indices in draw().
    std::unique_ptr<GBitmap> tileset_;
    bool triedLoadTileset_ = false;
};

} // namespace oc1
