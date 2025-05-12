#include "controller_buttons.h"

#if defined(USE_SDL2)
namespace {
bool trigger_left_is_down;
bool trigger_right_is_down;
} // namespace

ControllerButton ControllerButtonFromSdlEvent(const SDL_Event &event)
{
    switch (event.type) {
        case SDL_CONTROLLERAXISMOTION:
            switch (event.caxis.axis) {
                case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
                    if (event.caxis.value < 8192) { // 25% pressed
                        trigger_left_is_down = false;
                    }
                    if (event.caxis.value > 16384
                        && !trigger_left_is_down) { // 50% pressed
                        trigger_left_is_down = true;
                        return ControllerButton::TRIGGERLEFT;
                    }
                    return ControllerButton::NONE;
                case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
                    if (event.caxis.value < 8192) { // 25% pressed
                        trigger_right_is_down = false;
                    }
                    if (event.caxis.value > 16384
                        && !trigger_right_is_down) { // 50% pressed
                        trigger_right_is_down = true;
                        return ControllerButton::TRIGGERRIGHT;
                    }
                    return ControllerButton::NONE;
            }
            break;
        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
            switch (event.cbutton.button) {
                case SDL_CONTROLLER_BUTTON_A: return ControllerButton::A;
                case SDL_CONTROLLER_BUTTON_B: return ControllerButton::B;
                case SDL_CONTROLLER_BUTTON_X: return ControllerButton::X;
                case SDL_CONTROLLER_BUTTON_Y: return ControllerButton::Y;
                case SDL_CONTROLLER_BUTTON_LEFTSTICK:
                    return ControllerButton::LEFTSTICK;
                case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
                    return ControllerButton::RIGHTSTICK;
                case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
                    return ControllerButton::LEFTSHOULDER;
                case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
                    return ControllerButton::RIGHTSHOULDER;
                case SDL_CONTROLLER_BUTTON_START:
                    return ControllerButton::START;
                case SDL_CONTROLLER_BUTTON_BACK:
                    return ControllerButton::SELECT;
                case SDL_CONTROLLER_BUTTON_DPAD_UP: return ControllerButton::UP;
                case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
                    return ControllerButton::DOWN;
                case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
                    return ControllerButton::LEFT;
                case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
                    return ControllerButton::RIGHT;
                default: break;
            }
        default: break;
    }
    return ControllerButton::NONE;
}

bool IsControllerButtonDown(
    SDL_GameController *controller, ControllerButton button)
{
    switch (button) {
        case ControllerButton::UP:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_DPAD_UP);
        case ControllerButton::DOWN:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        case ControllerButton::LEFT:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        case ControllerButton::RIGHT:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        case ControllerButton::A:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_A);
        case ControllerButton::B:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_B);
        case ControllerButton::X:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_X);
        case ControllerButton::Y:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_Y);
        case ControllerButton::LEFTSHOULDER:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        case ControllerButton::RIGHTSHOULDER:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        case ControllerButton::TRIGGERLEFT: return trigger_left_is_down;
        case ControllerButton::TRIGGERRIGHT: return trigger_right_is_down;
        case ControllerButton::LEFTSTICK:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_LEFTSTICK);
        case ControllerButton::RIGHTSTICK:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
        case ControllerButton::START:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_START);
        case ControllerButton::SELECT:
            return SDL_GameControllerGetButton(
                controller, SDL_CONTROLLER_BUTTON_BACK);
    }
    return false;
}
#endif
