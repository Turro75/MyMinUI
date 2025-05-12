#include "window.h"

#include <cstdint>
#include <iostream>

#include "axis_direction.h"
#include "def.h"
#include "resourceManager.h"
#include "screen.h"
#include "sdlutils.h"

#define KEYHOLD_TIMER_FIRST   12
#define KEYHOLD_TIMER         3

CWindow::CWindow(void)
    : m_keyHoldCountdown(0)
    , m_controllerButtonCountdown(0)
    , m_lastPressed(SDLK_0)
    , m_retVal(0)
{
    // Add window to the lists for render
    Globals::g_windows.push_back(this);
}

CWindow::~CWindow(void)
{
    // Remove last window
    Globals::g_windows.pop_back();
}

namespace
{

std::uint32_t frameDeadline = 0;

// Limit FPS to avoid high CPU load, use when v-sync isn't available
void LimitFrameRate()
{
    const int refreshDelay = 1000000 / screen.refreshRate;
    std::uint32_t tc = SDL_GetTicks() * 1000;
    std::uint32_t v = 0;
    if (frameDeadline > tc)
    {
        v = tc % refreshDelay;
        SDL_Delay(v / 1000 + 1); // ceil
    }
    frameDeadline = tc + v + refreshDelay;
}

void ResetFrameDeadline() {
    frameDeadline = 0;
}

} // namespace

int CWindow::execute()
{

    m_retVal = 0;
    SDL_Event event;
    bool l_loop(true);
    bool l_render(true);
    
    // Main loop
    while (l_loop)
    {
        ControllerButton button = ControllerButton::NONE;
        PAD_poll();
        if (PAD_justPressed(BTN_MENU)) button = ControllerButton::MENU;
        if (PAD_justPressed(BTN_SELECT)) button = ControllerButton::SELECT;
        if (PAD_justPressed(BTN_UP)) button = ControllerButton::UP;
        if (PAD_justPressed(BTN_DOWN)) button = ControllerButton::DOWN;
        if (PAD_justPressed(BTN_LEFT)) button = ControllerButton::LEFT;
        if (PAD_justPressed(BTN_RIGHT)) button = ControllerButton::RIGHT;
        if (PAD_justPressed(BTN_A)) button = ControllerButton::A;
        if (PAD_justPressed(BTN_B)) button = ControllerButton::B;
        if (PAD_justPressed(BTN_X)) button = ControllerButton::X;
        if (PAD_justPressed(BTN_Y)) button = ControllerButton::Y;
        if (PAD_justPressed(BTN_L1)) button = ControllerButton::LEFTSTICK;
        if (PAD_justPressed(BTN_R1)) button = ControllerButton::RIGHTSTICK;
        if (PAD_justPressed(BTN_L2)) button = ControllerButton::LEFTSHOULDER;
        if (PAD_justPressed(BTN_R2)) button = ControllerButton::RIGHTSHOULDER;
        if (PAD_justPressed(BTN_START)) button = ControllerButton::START;        
        l_render = this->keyPress(event, SDLK_UNKNOWN, button) || l_render;
        if (m_retVal) l_loop = false;    



 /*       // Handle key press
        while (SDL_PollEvent(&event))
        {
            fprintf(stdout,"Event: %04x - Key = %d - State = %d\n", event.type, event.cbutton.button, event.cbutton.state);fflush(stdout);
            switch (event.type)
            {
                case SDL_QUIT: return m_retVal;
                case SDL_CONTROLLERDEVICEADDED:
                    SDL_GameControllerOpen(event.cdevice.which);
                    break;

                case SDL_CONTROLLERAXISMOTION:
                case SDL_CONTROLLERBUTTONDOWN: {
                    const ControllerButton button = ControllerButtonFromSdlEvent(event);
                    //SDL_utils::setMouseCursorEnabled(false);
                    l_render = this->keyPress(event, SDLK_UNKNOWN, button) || l_render;
                    if (m_retVal) l_loop = false;
                    break;
                }
            }
        }*/
        // Handle key hold
        if (!l_loop) break;

        l_render = this->keyHold() || l_render;
        // Render if necessary
        if (l_render)
        {
            SDL_utils::renderAll();
            screen.flip();
            l_render = false;
        }
        LimitFrameRate();
    }

/*  
#ifdef USE_SDL2
    SDL_StopTextInput();
    if (text_input_was_active) SDL_StartTextInput();
#endif
*/
    // -1 is used to signal cancellation but we must return 0 in that case.
    if (m_retVal == -1) m_retVal = 0;
    return m_retVal;
}

bool CWindow::handleZoomTrigger(const SDL_Event &event)
{
    if (event.type != SDL_KEYDOWN) return false;
    const auto sym = event.key.keysym.sym;
    // Zoom on CTRL +/-
    if ((SDL_GetModState() & KMOD_CTRL) == 0) return false;
    float factor;
    switch (sym) {
        case SDLK_PLUS:
        case SDLK_KP_PLUS: factor = 1.1f; break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS: factor = 1 / 1.1f; break;
        default: return false;
    }
    screen.zoom(factor);
    triggerOnResize();
    return true;
}

void CWindow::triggerOnResize() {
    CResourceManager::instance().onResize();
    for (auto *window : Globals::g_windows) window->onResize();
}

bool CWindow::keyPress(
    const SDL_Event &event, SDLC_Keycode key, ControllerButton button)
{
    // Reset timer if running
    if (m_keyHoldCountdown) m_keyHoldCountdown = 0;
    if (key != SDLK_UNKNOWN) m_lastPressed = key;
    if (button != ControllerButton::NONE) {
        m_controllerButtonCountdown = 0;
        m_lastPressedButton = button;
    }
    return false;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool CWindow::keyHold() { return false; }

#if defined(USE_SDL2)
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool CWindow::gamepadHold([[maybe_unused]] SDL_GameController *controller)
{
    return false;
}
#endif

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
bool CWindow::textInput([[maybe_unused]] const SDL_Event &event)
{
    return false;
}

void CWindow::onResize() { }

bool CWindow::tick(SDLC_Keycode keycode)
{
    if (m_lastPressed != keycode) return false;
#if defined(USE_SDL2)
    const bool held
        = SDL_GetKeyboardState(NULL)[SDL_GetScancodeFromKey(keycode)];
#else
    const bool held = SDL_GetKeyState(NULL)[keycode];
#endif
    if (held) {
        if (m_keyHoldCountdown != 0) {
            --m_keyHoldCountdown;
            if (m_keyHoldCountdown == 0) {
                // Timer continues
                m_keyHoldCountdown = KEYHOLD_TIMER;
                // Trigger!
                return true;
            }
        } else {
            // Start timer
            m_keyHoldCountdown = KEYHOLD_TIMER_FIRST;
        }
    } else {
        // Stop timer if running
        if (m_keyHoldCountdown != 0) m_keyHoldCountdown = 0;
    }
    return false;
}

#if defined(USE_SDL2)
bool CWindow::tick(SDL_GameController *controller, ControllerButton button)
{
    if (controller == nullptr || m_lastPressedButton != button) return false;
    if (IsControllerButtonDown(controller, button)) {
        if (m_controllerButtonCountdown != 0) {
            --m_controllerButtonCountdown;
            if (m_controllerButtonCountdown == 0) {
                // Timer continues
                m_controllerButtonCountdown = KEYHOLD_TIMER;
                // Trigger!
                return true;
            }
        } else {
            // Start timer
            m_controllerButtonCountdown = KEYHOLD_TIMER_FIRST;
        }
    } else {
        // Stop timer if running
        if (m_controllerButtonCountdown != 0) m_controllerButtonCountdown = 0;
    }
    return false;
}
#endif

bool CWindow::mouseDown(int button, int x, int y) { return false; }
bool CWindow::mouseWheel(int dx, int dy) { return false; }

bool CWindow::isFullScreen(void) const
{
    // Default behavior
    return false;
}

bool CWindow::handlesTextInput() const
{
    // Default behavior
    return false;
}
