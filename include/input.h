#pragma once

#include <cstdint>
#include <SDL2/SDL.h>

enum Button : uint32_t {
    BTN_NONE     = 0,
    BTN_UP       = 1 << 0,
    BTN_DOWN     = 1 << 1,
    BTN_LEFT     = 1 << 2,
    BTN_RIGHT    = 1 << 3,
    BTN_CROSS    = 1 << 4,
    BTN_CIRCLE   = 1 << 5,
    BTN_SQUARE   = 1 << 6,
    BTN_TRIANGLE = 1 << 7,
    BTN_L1       = 1 << 8,
    BTN_R1       = 1 << 9,
    BTN_L2       = 1 << 10,
    BTN_R2       = 1 << 11,
    BTN_OPTIONS  = 1 << 12,
    BTN_TOUCHPAD = 1 << 13
};

class InputManager {
public:
    InputManager();
    ~InputManager();

    bool init();
    void update();

    bool isPressed(Button btn) const;
    bool isJustPressed(Button btn) const;
    bool isJustReleased(Button btn) const;

    bool shouldQuit() const { return quitRequested; }

private:
    uint32_t currentButtons = 0;
    uint32_t previousButtons = 0;
    uint32_t justPressedButtons = 0;
    uint32_t justReleasedButtons = 0;

    SDL_GameController* controller = nullptr;
    bool quitRequested = false;

    // Repeat timer for smooth menu navigation
    uint32_t holdTime = 0;
    uint32_t lastRepeatTime = 0;
};
