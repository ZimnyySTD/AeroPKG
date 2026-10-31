#include "input.h"
#include "common.h"

InputManager::InputManager() {}

InputManager::~InputManager() {
    if (controller) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
    if (joystick) {
        SDL_JoystickClose(joystick);
        joystick = nullptr;
    }
}

bool InputManager::init() {
    int numJoysticks = SDL_NumJoysticks();
    if (numJoysticks > 0) {
        for (int i = 0; i < numJoysticks; ++i) {
            if (SDL_IsGameController(i)) {
                controller = SDL_GameControllerOpen(i);
                if (controller) {
                    LOGI("Connected GameController: %s", SDL_GameControllerName(controller));
                    break;
                }
            }
        }

        // On PS4, OpenOrbis handles DualShock 4 via standard SDL_Joystick
        if (!controller) {
            joystick = SDL_JoystickOpen(0);
            if (joystick) {
                LOGI("Connected PS4 DualShock 4 Joystick: %s", SDL_JoystickName(joystick));
            }
        }
    }
    return true;
}

void InputManager::update() {
    previousButtons = currentButtons;
    currentButtons = 0;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            quitRequested = true;
        } else if (event.type == SDL_JOYDEVICEADDED && !joystick && !controller) {
            joystick = SDL_JoystickOpen(event.jdevice.which);
        } else if (event.type == SDL_JOYDEVICEREMOVED) {
            if (joystick && event.jdevice.which == SDL_JoystickInstanceID(joystick)) {
                SDL_JoystickClose(joystick);
                joystick = nullptr;
            }
        } else if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) {
            controller = SDL_GameControllerOpen(event.cdevice.which);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
        }
    }

    // 1. OpenOrbis PS4 Joystick mapping (SDL_Joystick)
    if (joystick) {
        // PS4 PAD Button indices:
        // 0: Cross, 1: Circle, 2: Square, 3: Triangle, 4: L1, 5: R1, 9: Options, 13: Up, 14: Down, 15: Left, 16: Right
        if (SDL_JoystickGetButton(joystick, 0))  currentButtons |= BTN_CROSS;
        if (SDL_JoystickGetButton(joystick, 1))  currentButtons |= BTN_CIRCLE;
        if (SDL_JoystickGetButton(joystick, 2))  currentButtons |= BTN_SQUARE;
        if (SDL_JoystickGetButton(joystick, 3))  currentButtons |= BTN_TRIANGLE;
        if (SDL_JoystickGetButton(joystick, 4))  currentButtons |= BTN_L1;
        if (SDL_JoystickGetButton(joystick, 5))  currentButtons |= BTN_R1;
        if (SDL_JoystickGetButton(joystick, 9))  currentButtons |= BTN_OPTIONS;
        if (SDL_JoystickGetButton(joystick, 13)) currentButtons |= BTN_UP;
        if (SDL_JoystickGetButton(joystick, 14)) currentButtons |= BTN_DOWN;
        if (SDL_JoystickGetButton(joystick, 15)) currentButtons |= BTN_LEFT;
        if (SDL_JoystickGetButton(joystick, 16)) currentButtons |= BTN_RIGHT;

        // Check Hat 0 (D-Pad)
        Uint8 hat = SDL_JoystickGetHat(joystick, 0);
        if (hat & SDL_HAT_UP)    currentButtons |= BTN_UP;
        if (hat & SDL_HAT_DOWN)  currentButtons |= BTN_DOWN;
        if (hat & SDL_HAT_LEFT)  currentButtons |= BTN_LEFT;
        if (hat & SDL_HAT_RIGHT) currentButtons |= BTN_RIGHT;
    }

    // 2. SDL_GameController mapping (PC / standard controller)
    if (controller) {
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_UP))    currentButtons |= BTN_UP;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN))  currentButtons |= BTN_DOWN;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT))  currentButtons |= BTN_LEFT;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) currentButtons |= BTN_RIGHT;

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_A))           currentButtons |= BTN_CROSS;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_B))           currentButtons |= BTN_CIRCLE;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_X))           currentButtons |= BTN_SQUARE;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_Y))           currentButtons |= BTN_TRIANGLE;

        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER))  currentButtons |= BTN_L1;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER)) currentButtons |= BTN_R1;
        if (SDL_GameControllerGetButton(controller, SDL_CONTROLLER_BUTTON_START))          currentButtons |= BTN_OPTIONS;
    }

    // Keyboard support for PC testing / simulation
    const Uint8* state = SDL_GetKeyboardState(NULL);
    if (state[SDL_SCANCODE_UP])     currentButtons |= BTN_UP;
    if (state[SDL_SCANCODE_DOWN])   currentButtons |= BTN_DOWN;
    if (state[SDL_SCANCODE_LEFT])   currentButtons |= BTN_LEFT;
    if (state[SDL_SCANCODE_RIGHT])  currentButtons |= BTN_RIGHT;
    if (state[SDL_SCANCODE_RETURN] || state[SDL_SCANCODE_Z]) currentButtons |= BTN_CROSS;
    if (state[SDL_SCANCODE_ESCAPE] || state[SDL_SCANCODE_X]) currentButtons |= BTN_CIRCLE;
    if (state[SDL_SCANCODE_C])      currentButtons |= BTN_SQUARE;
    if (state[SDL_SCANCODE_V])      currentButtons |= BTN_TRIANGLE;
    if (state[SDL_SCANCODE_Q])      currentButtons |= BTN_L1;
    if (state[SDL_SCANCODE_E])      currentButtons |= BTN_R1;

    justPressedButtons = currentButtons & ~previousButtons;
    justReleasedButtons = ~currentButtons & previousButtons;
}

bool InputManager::isPressed(Button btn) const {
    return (currentButtons & btn) != 0;
}

bool InputManager::isJustPressed(Button btn) const {
    return (justPressedButtons & btn) != 0;
}

bool InputManager::isJustReleased(Button btn) const {
    return (justReleasedButtons & btn) != 0;
}
