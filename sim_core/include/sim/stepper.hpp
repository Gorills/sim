#pragma once

#include <algorithm>
#include <cmath>
#include <utility>

namespace sim {

class Stepper {
public:
    explicit Stepper(double tick_dt = 1.0 / 60.0, int max_steps = 8)
        : tick_dt_(valid_tick_dt(tick_dt)), max_steps_(std::max(1, max_steps)) {}

    void set_tick_dt(double tick_dt) {
        tick_dt_ = valid_tick_dt(tick_dt);
        accumulator_ = std::min(accumulator_, tick_dt_);
    }

    void set_max_steps(int max_steps) {
        max_steps_ = std::max(1, max_steps);
    }

    [[nodiscard]] double tick_dt() const noexcept { return tick_dt_; }
    [[nodiscard]] int max_steps() const noexcept { return max_steps_; }
    [[nodiscard]] double accumulator() const noexcept { return accumulator_; }

    [[nodiscard]] double alpha() const noexcept {
        if (tick_dt_ <= 0.0) {
            return 0.0;
        }
        return std::clamp(accumulator_ / tick_dt_, 0.0, 1.0);
    }

    void reset() noexcept { accumulator_ = 0.0; }

    template<typename TickFn>
    int advance(double frame_dt, TickFn&& tick) {
        if (!std::isfinite(frame_dt) || frame_dt < 0.0) {
            frame_dt = 0.0;
        }
        accumulator_ += frame_dt;
        int steps = 0;
        while (accumulator_ >= tick_dt_ && steps < max_steps_) {
            tick();
            accumulator_ -= tick_dt_;
            ++steps;
        }
        if (steps == max_steps_ && accumulator_ > tick_dt_) {
            accumulator_ = tick_dt_;
        }
        return steps;
    }

private:
    [[nodiscard]] static double valid_tick_dt(double tick_dt) noexcept {
        return std::isfinite(tick_dt) && tick_dt > 0.0 ? tick_dt : 1.0 / 60.0;
    }

    double tick_dt_ = 1.0 / 60.0;
    double accumulator_ = 0.0;
    int max_steps_ = 8;
};

} // namespace sim
