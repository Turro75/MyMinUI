#include "axis_direction.h"

#include <SDL.h>

AxisDirection AxisDirectionRepeater::Get(AxisDirection axis_direction)
{
    const int now = SDL_GetTicks();
    switch (axis_direction.x) {
        case AxisDirectionX::LEFT:
            last_right_ = 0;
            if (now - last_left_ < min_interval_ms_) {
                axis_direction.x = AxisDirectionX::NONE;
            } else {
                last_left_ = now;
            }
            break;
        case AxisDirectionX::RIGHT:
            last_left_ = 0;
            if (now - last_right_ < min_interval_ms_) {
                axis_direction.x = AxisDirectionX::NONE;
            } else {
                last_right_ = now;
            }
            break;
        case AxisDirectionX::NONE: last_left_ = last_right_ = 0; break;
    }
    switch (axis_direction.y) {
        case AxisDirectionY::UP:
            last_down_ = 0;
            if (now - last_up_ < min_interval_ms_) {
                axis_direction.y = AxisDirectionY::NONE;
            } else {
                last_up_ = now;
            }
            break;
        case AxisDirectionY::DOWN:
            last_up_ = 0;
            if (now - last_down_ < min_interval_ms_) {
                axis_direction.y = AxisDirectionY::NONE;
            } else {
                last_down_ = now;
            }
            break;
        case AxisDirectionY::NONE: last_up_ = last_down_ = 0; break;
    }
    return axis_direction;
}
