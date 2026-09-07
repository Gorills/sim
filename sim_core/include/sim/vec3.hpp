#pragma once

#include <cmath>

namespace sim {

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

[[nodiscard]] constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

[[nodiscard]] constexpr Vec3 operator*(Vec3 a, double s) noexcept {
    return {a.x * s, a.y * s, a.z * s};
}

[[nodiscard]] constexpr Vec3 operator*(double s, Vec3 a) noexcept {
    return a * s;
}

[[nodiscard]] constexpr bool operator==(Vec3 a, Vec3 b) noexcept {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

[[nodiscard]] constexpr bool operator!=(Vec3 a, Vec3 b) noexcept {
    return !(a == b);
}

[[nodiscard]] inline Vec3 lerp(Vec3 a, Vec3 b, double t) noexcept {
    return a * (1.0 - t) + b * t;
}

[[nodiscard]] inline double length_squared(Vec3 v) noexcept {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] inline double length(Vec3 v) noexcept {
    return std::sqrt(length_squared(v));
}

} // namespace sim
