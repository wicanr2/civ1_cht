// OpenCiv1Game.h — the game shell that owns the VCPU, the graphics driver and
// the shared drawing context, and hosts the ported CodeObjects.
//
// Mirrors OpenCiv1Game.cs: CodeObjects reach the CPU via `cpu`, graphics via
// `graphics`, and the screen-0 drawing context via `var_aa` (the original
// game's "0xaa" CRectangle). New CodeObjects are added as members here as they
// are ported, exactly like the C# reference.
#pragma once
#include "../vcpu/VCPU.h"
#include "../graphics/GDriver.h"
#include "../resource/TextResource.h"
#include <array>
#include <memory>
#include <string>

namespace oc1 {

class DrawTools;     // ported CodeObject (fwd-declared to break the include cycle)
class ImageTools;    // ported CodeObject (fwd-declared to break the include cycle)
class LanguageTools; // ported CodeObject (fwd-declared to break the include cycle)
class MenuBoxDialog; // ported CodeObject (fwd-declared to break the include cycle)
class CommonTools;   // ported CodeObject (fwd-declared to break the include cycle)
class TextBoxDialogs;// ported CodeObject (fwd-declared to break the include cycle)
class GameMenus;     // ported CodeObject (fwd-declared to break the include cycle)
class MainCode;      // ported CodeObject (fwd-declared to break the include cycle)
class MainIntro;     // ported CodeObject (fwd-declared to break the include cycle)
class MapManagement; // ported CodeObject (fwd-declared to break the include cycle)
class UnitManagement;// ported CodeObject (fwd-declared to break the include cycle)
class CheckPlayerTurn;// ported CodeObject (fwd-declared to break the include cycle)
class CityView;      // ported CodeObject (fwd-declared to break the include cycle)
class GameLoadAndSave;// ported CodeObject (fwd-declared to break the include cycle)
class TechResearch;  // ported CodeObject (fwd-declared to break the include cycle)
class NewGameWizard; // A2: DOS-style cascade frame (left portraits + right list)

class OpenCiv1Game {
public:
    VCPU cpu;
    GDriver graphics;
    CRectangle var_aa; // screen-0 drawing context (OpenCiv1's Var_aa_Screen0_Rectangle)

    // ---- SPACE RACE victory ---------------------------------------------
    // Civ1 endgame: when a civ's spaceship arrives at Alpha Centauri, that
    // civ wins the game. CheckPlayerTurn::processEndOfTurn sets this to
    // the winning civ's index; FrontEndFlow can read it to transition the
    // top-level state to DONE. Default -1 == "no victor yet".
    int victoryCiv = -1;

    // Keyword-replacement values used by LanguageTools::ReplaceKeywords
    // ($US/$THEM/$BUCKS/$RPLC1/$RPLC2). Mirrors OpenCiv1GameGlobals.Array_30b8;
    // defaults to 5 empty strings, populated by other (not-yet-ported) CodeObjects.
    std::array<std::string, 5> Array_30b8;

    // Directory that holds the game's "<SECTION>.TXT" language files and other
    // resources. Mirrors OpenCiv1Game.ResourcePath in the C#; LanguageTools'
    // file-backed lookups resolve their paths against this. Defaults to ".".
    const std::string& resourcePath() const { return resourcePath_; }
    void setResourcePath(std::string path) {
        resourcePath_ = std::move(path);
        textResource_.setResourceDir(resourcePath_);
        textResource_.clearCache();
    }

    // The section/key ".TXT" reader backing LanguageTools' language-pack lookups.
    TextResource& textResource() { return textResource_; }

    OpenCiv1Game();
    ~OpenCiv1Game();

    DrawTools& drawTools() { return *drawTools_; }
    ImageTools& imageTools() { return *imageTools_; }
    LanguageTools& languageTools() { return *languageTools_; }
    MenuBoxDialog& menuBoxDialog() { return *menuBoxDialog_; }
    CommonTools& commonTools() { return *commonTools_; }
    TextBoxDialogs& textBoxDialogs() { return *textBoxDialogs_; }
    GameMenus& gameMenus() { return *gameMenus_; }
    MainCode& mainCode() { return *mainCode_; }
    MainIntro& mainIntro() { return *mainIntro_; }
    MapManagement& mapManagement() { return *mapManagement_; }
    UnitManagement& unitManagement() { return *unitManagement_; }
    CheckPlayerTurn& checkPlayerTurn() { return *checkPlayerTurn_; }
    CityView& cityView() { return *cityView_; }
    GameLoadAndSave& gameLoadAndSave() { return *gameLoadAndSave_; }
    TechResearch& techResearch() { return *techResearch_; }
    NewGameWizard& newGameWizard() { return *newGameWizard_; }

private:
    std::string resourcePath_ = ".";
    TextResource textResource_{resourcePath_};
    std::unique_ptr<DrawTools> drawTools_;
    std::unique_ptr<ImageTools> imageTools_;
    std::unique_ptr<LanguageTools> languageTools_;
    std::unique_ptr<MenuBoxDialog> menuBoxDialog_;
    std::unique_ptr<CommonTools> commonTools_;
    std::unique_ptr<TextBoxDialogs> textBoxDialogs_;
    std::unique_ptr<GameMenus> gameMenus_;
    std::unique_ptr<MainCode> mainCode_;
    std::unique_ptr<MainIntro> mainIntro_;
    std::unique_ptr<MapManagement> mapManagement_;
    std::unique_ptr<UnitManagement> unitManagement_;
    std::unique_ptr<CheckPlayerTurn> checkPlayerTurn_;
    std::unique_ptr<CityView> cityView_;
    std::unique_ptr<GameLoadAndSave> gameLoadAndSave_;
    std::unique_ptr<TechResearch> techResearch_;
    std::unique_ptr<NewGameWizard> newGameWizard_;
};

} // namespace oc1
