#ifndef AXIS_DIRECTION_H_
#define AXIS_DIRECTION_H_

#include <cstdint>

enum class AxisDirectionX : std::uint8_t { NONE, LEFT, RIGHT };
enum class AxisDirectionY : std::uint8_t { NONE, UP, DOWN };

// 8-way direction of a D-Pad or a thumb stick.
struct AxisDirection {
    AxisDirectionX x;
    AxisDirectionY y;
};

// Returns a non-empty AxisDirection at most once per the given time interval.
class AxisDirectionRepeater {
  public:
    AxisDirectionRepeater(int min_interval_ms = 160)
        : last_left_(0)
        , last_right_(0)
        , last_up_(0)
        , last_down_(0)
        , min_interval_ms_(min_interval_ms)
    {
    }

    AxisDirection Get(AxisDirection axis_direction);

  private:
    int last_left_;
    int last_right_;
    int last_up_;
    int last_down_;
    int min_interval_ms_;
};

#endif // AXIS_DIRECTION_H_
