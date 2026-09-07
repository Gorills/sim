#pragma once

#include "sim/vec3.hpp"

#include <godot_cpp/variant/vector3.hpp>

namespace sim_godot {

[[nodiscard]] inline godot::Vector3 to_godot(sim::Vec3 value) {
    return godot::Vector3(static_cast<float>(value.x), static_cast<float>(value.y),
                          static_cast<float>(value.z));
}

[[nodiscard]] inline sim::Vec3 from_godot(godot::Vector3 value) {
    return sim::Vec3{static_cast<double>(value.x), static_cast<double>(value.y),
                     static_cast<double>(value.z)};
}

} // namespace sim_godot
