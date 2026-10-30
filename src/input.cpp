#include "input.h"
#include "common.h"

InputManager::InputManager() {}

InputManager::~InputManager() {
    if (controller) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
}

bool InputManager::init() {
    int numJoysticks = SDL_NumJoysticks();
    for (int i = 0; i < numJoysticks; ++i) {
        if (SDL_IsGameController(i)) {
            controller = SDL_GameControllerOpen(i);
            if (controller) {
                LOGI("Connected DualShock 4 / GameController: %s", SDL_GameControllerName(controller));
                break;
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
        } else if (event.type == SDL_CONTROLLERDEVICEADDED && !controller) {
            controller = SDL_GameControllerOpen(event.cdevice.which);
        } else if (event.type == SDL_CONTROLLERDEVICEREMOVED) {
            if (controller && event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller))) {
                SDL_GameControllerClose(controller);
                controller = nullptr;
            }
        }
    }

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
