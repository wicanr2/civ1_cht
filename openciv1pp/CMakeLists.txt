cmake_minimum_required(VERSION 3.16)
project(openciv1pp CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(FREETYPE REQUIRED freetype2)

add_executable(openciv1pp
  src/main.cpp
  src/vcpu/VCPU.cpp
  src/graphics/GBitmap.cpp
  src/graphics/GFont.cpp
  src/graphics/CjkGlyphCache.cpp
  src/localization/Translator.cpp
  src/platform/SdlPresenter.cpp
  src/resource/Compression.cpp
  src/resource/PicLoader.cpp
  src/resource/TextResource.cpp
  src/game/OpenCiv1Game.cpp
  src/game/DrawTools.cpp
  src/game/ImageTools.cpp
  src/game/LanguageTools.cpp
  src/game/MenuBoxDialog.cpp
  src/game/CommonTools.cpp
  src/game/TextBoxDialogs.cpp
  src/game/FrontEndFlow.cpp
  src/game/GameMenus.cpp
  src/game/MainCode.cpp
  src/game/MainIntro.cpp
  src/game/MapManagement.cpp
  src/game/MiniWorld.cpp
  src/game/TerrainTiles.cpp
  src/game/UnitManagement.cpp
  src/game/CheckPlayerTurn.cpp
  src/game/CityView.cpp
  src/game/GameLoadAndSave.cpp
  src/game/TechResearch.cpp
)

target_include_directories(openciv1pp PRIVATE
  ${SDL2_INCLUDE_DIRS}
  ${FREETYPE_INCLUDE_DIRS}
)
target_link_libraries(openciv1pp PRIVATE
  ${SDL2_LIBRARIES}
  ${FREETYPE_LIBRARIES}
)
target_compile_options(openciv1pp PRIVATE ${SDL2_CFLAGS_OTHER} ${FREETYPE_CFLAGS_OTHER})

# Headless self-tests as CTest cases (run: ctest --test-dir build --output-on-failure)
enable_testing()
foreach(t selftest restest gfxtest gdtest compositetest paltest drawtest imgtest langtest txttest menutest navtest commontest textboxtest flowtest gamemenutest playtest maptest titletest newgametest mousetest introtest realgentest citytest turntest gameflowtest aitest aibehaviortest cityviewtest combattest aimovetest savetest techtest minimaptest)
  add_test(NAME ${t} COMMAND openciv1pp --${t})
  # tests load assets/zh_TW.json by relative path -> run them from the project root
  set_tests_properties(${t} PROPERTIES WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
endforeach()
