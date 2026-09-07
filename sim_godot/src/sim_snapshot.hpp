#pragma once

#include "sim_entity_state.hpp"

#include <cstdint>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/typed_array.hpp>

class SimSnapshot : public godot::RefCounted {
    GDCLASS(SimSnapshot, godot::RefCounted)

public:
    void set_tick(int64_t tick);
    [[nodiscard]] int64_t get_tick() const;

    void set_alpha(double alpha);
    [[nodiscard]] double get_alpha() const;

    void set_tick_dt(double tick_dt);
    [[nodiscard]] double get_tick_dt() const;

    void set_paused(bool paused);
    [[nodiscard]] bool is_paused() const;

    void set_simulated_hours(double simulated_hours);
    [[nodiscard]] double get_simulated_hours() const;

    void set_mean_moisture(double mean_moisture);
    [[nodiscard]] double get_mean_moisture() const;

    void set_mean_temperature(double mean_temperature);
    [[nodiscard]] double get_mean_temperature() const;

    void set_mean_canopy(double mean_canopy);
    [[nodiscard]] double get_mean_canopy() const;

    void set_mean_light(double mean_light);
    [[nodiscard]] double get_mean_light() const;

    void set_mean_organic(double mean_organic);
    [[nodiscard]] double get_mean_organic() const;

    void set_mean_pollination(double mean_pollination);
    [[nodiscard]] double get_mean_pollination() const;

    void set_year_phase(double year_phase);
    [[nodiscard]] double get_year_phase() const;

    void set_hour_of_day(double hour_of_day);
    [[nodiscard]] double get_hour_of_day() const;

    void set_entities(const godot::TypedArray<SimEntityState>& entities);
    [[nodiscard]] godot::TypedArray<SimEntityState> get_entities() const;

protected:
    static void _bind_methods();

private:
    int64_t tick_ = 0;
    double alpha_ = 0.0;
    double tick_dt_ = 0.0;
    double simulated_hours_ = 0.0;
    double mean_moisture_ = 0.0;
    double mean_temperature_ = 0.0;
    double mean_canopy_ = 0.0;
    double mean_light_ = 0.0;
    double mean_organic_ = 0.0;
    double mean_pollination_ = 0.0;
    double year_phase_ = 0.0;
    double hour_of_day_ = 0.0;
    bool paused_ = false;
    godot::TypedArray<SimEntityState> entities_{};
};
