#pragma once

#include <cstdint>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

class SimHabitatGrid : public godot::RefCounted {
    GDCLASS(SimHabitatGrid, godot::RefCounted)

public:
    void set_width(int32_t width);
    [[nodiscard]] int32_t get_width() const;

    void set_height(int32_t height);
    [[nodiscard]] int32_t get_height() const;

    void set_cell_size(double cell_size);
    [[nodiscard]] double get_cell_size() const;

    void set_origin(godot::Vector3 origin);
    [[nodiscard]] godot::Vector3 get_origin() const;

    void set_elevation(const godot::PackedFloat32Array& elevation);
    [[nodiscard]] godot::PackedFloat32Array get_elevation() const;

    void set_moisture(const godot::PackedFloat32Array& moisture);
    [[nodiscard]] godot::PackedFloat32Array get_moisture() const;

    void set_nutrients(const godot::PackedFloat32Array& nutrients);
    [[nodiscard]] godot::PackedFloat32Array get_nutrients() const;

    void set_temperature(const godot::PackedFloat32Array& temperature);
    [[nodiscard]] godot::PackedFloat32Array get_temperature() const;

    void set_canopy(const godot::PackedFloat32Array& canopy);
    [[nodiscard]] godot::PackedFloat32Array get_canopy() const;

    void set_light(const godot::PackedFloat32Array& light);
    [[nodiscard]] godot::PackedFloat32Array get_light() const;

    void set_organic(const godot::PackedFloat32Array& organic);
    [[nodiscard]] godot::PackedFloat32Array get_organic() const;

    void set_pollination(const godot::PackedFloat32Array& pollination);
    [[nodiscard]] godot::PackedFloat32Array get_pollination() const;

    void set_surface(const godot::PackedByteArray& surface);
    [[nodiscard]] godot::PackedByteArray get_surface() const;

protected:
    static void _bind_methods();

private:
    int32_t width_ = 0;
    int32_t height_ = 0;
    double cell_size_ = 0.5;
    godot::Vector3 origin_{};
    godot::PackedFloat32Array elevation_{};
    godot::PackedFloat32Array moisture_{};
    godot::PackedFloat32Array nutrients_{};
    godot::PackedFloat32Array temperature_{};
    godot::PackedFloat32Array canopy_{};
    godot::PackedFloat32Array light_{};
    godot::PackedFloat32Array organic_{};
    godot::PackedFloat32Array pollination_{};
    godot::PackedByteArray surface_{};
};
