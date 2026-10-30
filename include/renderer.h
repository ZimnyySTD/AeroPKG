#pragma once

#include "common.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>
#include <unordered_map>

struct Color {
    Uint8 r, g, b, a;
    static Color White()       { return {255, 255, 255, 255}; }
    static Color Black()       { return {0, 0, 0, 255}; }
    static Color DarkBg()      { return {18, 20, 28, 255}; }
    static Color CardBg()      { return {28, 32, 44, 255}; }
    static Color CardSelected(){ return {45, 55, 78, 255}; }
    static Color AccentBlue()  { return {0, 150, 255, 255}; }
    static Color AccentCyan()  { return {0, 220, 220, 255}; }
    static Color TextMuted()   { return {160, 170, 185, 255}; }
    static Color Success()     { return {50, 205, 50, 255}; }
    static Color Warning()     { return {255, 165, 0, 255}; }
};

class Renderer {
public:
    Renderer();
    ~Renderer();

    bool init(int width = 1920, int height = 1080);
    void shutdown();

    void beginFrame();
    void endFrame();

    // Drawing primitives
    void drawRect(int x, int y, int w, int h, Color color);
    void fillRect(int x, int y, int w, int h, Color color);
    void drawRoundedRect(int x, int y, int w, int h, int radius, Color color);
    void drawLine(int x1, int y1, int x2, int y2, Color color);

    // Text rendering
    void drawText(const std::string& text, int x, int y, int fontSize = 24, Color color = Color::White(), bool centered = false);
    int getTextWidth(const std::string& text, int fontSize = 24);

    // Texture & Image rendering with LRU cache
    SDL_Texture* loadTexture(const std::string& path);
    void drawTexture(SDL_Texture* texture, int x, int y, int w, int h);
    void drawImage(const std::string& path, int x, int y, int w, int h);

    // Dimensions
    int getWidth() const { return screenWidth; }
    int getHeight() const { return screenHeight; }

private:
    SDL_Window* window = nullptr;
    SDL_Renderer* sdlRenderer = nullptr;
    std::unordered_map<int, TTF_Font*> fonts;
    std::unordered_map<std::string, SDL_Texture*> textureCache;

    int screenWidth = 1920;
    int screenHeight = 1080;
    TTF_Font* getFont(int size);
};
