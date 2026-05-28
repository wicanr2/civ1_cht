#include "OpenCiv1Game.h"
#include "DrawTools.h"
#include "ImageTools.h"
#include "LanguageTools.h"
#include "MenuBoxDialog.h"
#include "CommonTools.h"
#include "TextBoxDialogs.h"
#include "GameMenus.h"
#include "MainCode.h"
#include "MainIntro.h"

namespace oc1 {

// ctor/dtor live here where the CodeObjects are complete types (unique_ptr members).
OpenCiv1Game::OpenCiv1Game() {
    drawTools_ = std::make_unique<DrawTools>(*this);
    imageTools_ = std::make_unique<ImageTools>(*this);
    languageTools_ = std::make_unique<LanguageTools>(*this);
    menuBoxDialog_ = std::make_unique<MenuBoxDialog>(*this);
    commonTools_ = std::make_unique<CommonTools>(*this);
    textBoxDialogs_ = std::make_unique<TextBoxDialogs>(*this);
    gameMenus_ = std::make_unique<GameMenus>(*this);
    mainCode_ = std::make_unique<MainCode>(*this);
    mainIntro_ = std::make_unique<MainIntro>(*this);
}
OpenCiv1Game::~OpenCiv1Game() = default;

} // namespace oc1
