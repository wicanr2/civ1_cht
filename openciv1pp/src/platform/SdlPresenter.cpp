#include "SdlPresenter.h"
#include <SDL.h>
#include <cstdio>

namespace oc1 {

bool SdlPresenter::init(const char* title, int fbWidth, int fbHeight, int scale) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "[SDL] init failed: %s\n", SDL_GetError());
        return false;
    }
    fbW_ = fbWidth;
    fbH_ = fbHeight;

    SDL_Window* win = SDL_CreateWindow(
        title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        fbWidth * scale, fbHeight * scale, SDL_WINDOW_SHOWN);
    if (!win) { std::fprintf(stderr, "[SDL] window: %s\n", SDL_GetError()); return false; }
    window_ = win;

    SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { std::fprintf(stderr, "[SDL] renderer: %s\n", SDL_GetError()); return false; }
    renderer_ = ren;
    SDL_RenderSetLogicalSize(ren, fbWidth, fbHeight);

    SDL_Texture* tex = SDL_CreateTexture(
        ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, fbWidth, fbHeight);
    if (!tex) { std::fprintf(stderr, "[SDL] texture: %s\n", SDL_GetError()); return false; }
    texture_ = tex;
    return true;
}

void SdlPresenter::shutdown() {
    if (texture_)  SDL_DestroyTexture(static_cast<SDL_Texture*>(texture_));
    if (renderer_) SDL_DestroyRenderer(static_cast<SDL_Renderer*>(renderer_));
    if (window_)   SDL_DestroyWindow(static_cast<SDL_Window*>(window_));
    texture_ = renderer_ = window_ = nullptr;
    SDL_Quit();
}

bool SdlPresenter::pumpEvents() {
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) quit_ = true;
        else if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) quit_ = true;
    }
    return !quit_;
}

int SdlPresenter::pollKey() {
    // Mapped codes mirror MenuBoxDialog::NavKey (kept as literals so this SDL
    // layer stays free of any game-header dependency): 1=Up,2=Down,3=Enter,4=Esc;
    // 5=Left,6=Right extend it for the map-scroller.
    SDL_Event ev;
    while (SDL_PollEvent(&ev)) {
        if (ev.type == SDL_QUIT) { quit_ = true; return 0; }
        if (ev.type == SDL_KEYDOWN) {
            switch (ev.key.keysym.sym) {
                case SDLK_UP:     return 1;
                case SDLK_DOWN:   return 2;
                case SDLK_RETURN:
                case SDLK_KP_ENTER: return 3;
                case SDLK_ESCAPE: quit_ = true; return 4;
                case SDLK_LEFT:   return 5;
                case SDLK_RIGHT:  return 6;
                default: break;
            }
        }
    }
    return 0;
}

bool SdlPresenter::present(const GBitmap& fb) {
    if (!pumpEvents()) return false;

    fb.toRGBA(rgba_);
    SDL_Texture* tex = static_cast<SDL_Texture*>(texture_);
    SDL_UpdateTexture(tex, nullptr, rgba_.data(), fbW_ * 4);

    SDL_Renderer* ren = static_cast<SDL_Renderer*>(renderer_);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, nullptr, nullptr);
    SDL_RenderPresent(ren);
    return !quit_;
}

} // namespace oc1
