#pragma once

#include <cstdint>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/vector3.hpp>

class SimEntityState : public godot::RefCounted {
    GDCLASS(SimEntityState, godot::RefCounted)

public:
    void set_id(int64_t id);
    [[nodiscard]] int64_t get_id() const;

    void set_position(godot::Vector3 position);
    [[nodiscard]] godot::Vector3 get_position() const;

    void set_velocity(godot::Vector3 velocity);
    [[nodiscard]] godot::Vector3 get_velocity() const;

    void set_species_id(int64_t species_id);
    [[nodiscard]] int64_t get_species_id() const;

    void set_kind(int32_t kind);
    [[nodiscard]] int32_t get_kind() const;

    void set_intent(int32_t intent);
    [[nodiscard]] int32_t get_intent() const;

    void set_target_position(godot::Vector3 target_position);
    [[nodiscard]] godot::Vector3 get_target_position() const;

    void set_target_id(int64_t target_id);
    [[nodiscard]] int64_t get_target_id() const;

    void set_biomass(double biomass);
    [[nodiscard]] double get_biomass() const;

    void set_energy(double energy);
    [[nodiscard]] double get_energy() const;

    void set_hydration(double hydration);
    [[nodiscard]] double get_hydration() const;

protected:
    static void _bind_methods();

private:
    int64_t id_ = 0;
    godot::Vector3 position_{};
    godot::Vector3 velocity_{};
    int64_t species_id_ = 0;
    int32_t kind_ = 0;
    int32_t intent_ = 0;
    godot::Vector3 target_position_{};
    int64_t target_id_ = 0;
    double biomass_ = 1.0;
    double energy_ = 0.0;
    double hydration_ = 1.0;
};
