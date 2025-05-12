#ifndef CONTROLLER_BUTTONS_H_
#define CONTROLLER_BUTTONS_H_

#include <cstdint>

#include <SDL_events.h>
#include <SDL_version.h>

#if defined(USE_SDL2)
#include <SDL_gamecontroller.h>
#endif

// Similar to SDL_GameControllerButton but also defines triggers (L2/R2).
//
// NOTE: A, B, X, Y refer to physical positions on an XBox 360 controller.
// A<->B and X<->Y are reversed on a Nintendo controller.
enum class ControllerButton : std::uint8_t {
    NONE,
    UP,
    DOWN,
    LEFT,
    RIGHT,
    A,             // Bottom button
    B,             // Right button
    X,             // Left button
    Y,             // Top button
    LEFTSHOULDER,  // aka L1
    RIGHTSHOULDER, // aka R1
    TRIGGERLEFT,   // aka L2, ZL
    TRIGGERRIGHT,  // aka R2, ZR
    LEFTSTICK,     // aka L3
    RIGHTSTICK,    // aka R3
    START,
    SELECT,
    MENU,
};

#if defined(USE_SDL2)
// Must be called exactly once per SDL event.
ControllerButton ControllerButtonFromSdlEvent(const SDL_Event &event);
bool IsControllerButtonDown(
    SDL_GameController *controller, ControllerButton button);
#endif

#endif // CONTROLLER_BUTTONS_H_
