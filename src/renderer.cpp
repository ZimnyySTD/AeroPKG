#include "renderer.h"

Renderer::Renderer() {}

Renderer::~Renderer() {
    shutdown();
}

bool Renderer::init(int width, int height) {
    screenWidth = width;
    screenHeight = height;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) != 0) {
        LOGE("SDL_Init Failed: %s", SDL_GetError());
        return false;
    }

    if (TTF_Init() != 0) {
        LOGE("TTF_Init Failed: %s", TTF_GetError());
        return false;
    }

    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if ((IMG_Init(imgFlags) & imgFlags) != imgFlags) {
        LOGE("IMG_Init Failed: %s", IMG_GetError());
    }

    window = SDL_CreateWindow(
        APP_NAME,
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        screenWidth,
        screenHeight,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        LOGE("SDL_CreateWindow Failed: %s", SDL_GetError());
        return false;
    }

    sdlRenderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!sdlRenderer) {
        LOGE("SDL_CreateRenderer Failed: %s", SDL_GetError());
        return false;
    }

    SDL_SetRenderDrawBlendMode(sdlRenderer, SDL_BLENDMODE_BLEND);
    LOGI("Renderer initialized successfully at %dx%d (VSync 60 FPS)", screenWidth, screenHeight);
    return true;
}

void Renderer::shutdown() {
    for (auto& pair : textureCache) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    textureCache.clear();

    for (auto& pair : fonts) {
        if (pair.second) {
            TTF_CloseFont(pair.second);
        }
    }
    fonts.clear();

    if (sdlRenderer) {
        SDL_DestroyRenderer(sdlRenderer);
        sdlRenderer = nullptr;
    }

    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}

void Renderer::beginFrame() {
    Color bg = Color::DarkBg();
    SDL_SetRenderDrawColor(sdlRenderer, bg.r, bg.g, bg.b, bg.a);
    SDL_RenderClear(sdlRenderer);
}

void Renderer::endFrame() {
    SDL_RenderPresent(sdlRenderer);
}

void Renderer::fillRect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(sdlRenderer, c.r, c.g, c.b, c.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(sdlRenderer, &rect);
}

void Renderer::drawRect(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(sdlRenderer, c.r, c.g, c.b, c.a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(sdlRenderer, &rect);
}

void Renderer::drawLine(int x1, int y1, int x2, int y2, Color c) {
    SDL_SetRenderDrawColor(sdlRenderer, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLine(sdlRenderer, x1, y1, x2, y2);
}

void Renderer::drawRoundedRect(int x, int y, int w, int h, int radius, Color c) {
    // Smooth card background with rounded aesthetics
    fillRect(x + radius, y, w - 2 * radius, h, c);
    fillRect(x, y + radius, w, h - 2 * radius, c);
}

TTF_Font* Renderer::getFont(int size) {
    if (fonts.find(size) != fonts.end()) {
        return fonts[size];
    }

    // Default system font or bundled font
    const char* fontPaths[] = {
        "/app0/assets/font.ttf",
        "/system/common/font/DFHEIB5-MP-R-01.ttf",
        "/system/common/font/default.ttf"
    };

    TTF_Font* font = nullptr;
    for (const char* path : fontPaths) {
        font = TTF_OpenFont(path, size);
        if (font) break;
    }

    if (!font) {
        // Fallback placeholder font handling
        return nullptr;
    }

    fonts[size] = font;
    return font;
}

void Renderer::drawText(const std::string& text, int x, int y, int fontSize, Color color, bool centered) {
    if (text.empty()) return;

    TTF_Font* font = getFont(fontSize);
    if (!font) return;

    SDL_Color sdlColor = {color.r, color.g, color.b, color.a};
    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
    if (texture) {
        int drawX = centered ? x - (surface->w / 2) : x;
        SDL_Rect dest = {drawX, y, surface->w, surface->h};
        SDL_RenderCopy(sdlRenderer, texture, nullptr, &dest);
        SDL_DestroyTexture(texture);
    }
    SDL_FreeSurface(surface);
}

int Renderer::getTextWidth(const std::string& text, int fontSize) {
    TTF_Font* font = getFont(fontSize);
    if (!font || text.empty()) return 0;
    int w = 0, h = 0;
    TTF_SizeUTF8(font, text.c_str(), &w, &h);
    return w;
}

SDL_Texture* Renderer::loadTexture(const std::string& path) {
    if (textureCache.find(path) != textureCache.end()) {
        return textureCache[path];
    }

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(sdlRenderer, surface);
    SDL_FreeSurface(surface);

    if (texture) {
        textureCache[path] = texture;
    }
    return texture;
}

void Renderer::drawTexture(SDL_Texture* texture, int x, int y, int w, int h) {
    if (!texture) return;
    SDL_Rect dest = {x, y, w, h};
    SDL_RenderCopy(sdlRenderer, texture, nullptr, &dest);
}

void Renderer::drawImage(const std::string& path, int x, int y, int w, int h) {
    SDL_Texture* tex = loadTexture(path);
    if (tex) {
        drawTexture(tex, x, y, w, h);
    } else {
        // Fallback placeholder icon box
        fillRect(x, y, w, h, Color::CardSelected());
        drawRect(x, y, w, h, Color::AccentBlue());
    }
}
