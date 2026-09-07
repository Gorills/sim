#pragma once

#include "sim/vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sim {

struct HabitatCell {
    double elevation = 0.0;
    double moisture = 0.5;
    double nutrients = 0.7;
    double temperature = 18.0;
    double canopy = 0.0;
    double light = 1.0;
    double organic = 0.0;
    double pollination = 0.0;
    bool water = false;
    bool fresh_water = false;
};

enum class SurfaceKind : std::uint8_t {
    land = 0,
    ocean = 1,
    fresh_water = 2,
};

[[nodiscard]] constexpr SurfaceKind surface_kind(const HabitatCell& cell) noexcept {
    if (cell.water) {
        return SurfaceKind::ocean;
    }
    if (cell.fresh_water) {
        return SurfaceKind::fresh_water;
    }
    return SurfaceKind::land;
}

struct ClimateConfig {
    double mean_temperature = 17.0;
    double seasonal_temperature_amplitude = 8.0;
    double rain_per_hour = 0.00045;
    double evaporation_per_hour = 0.0009;
    double nutrient_regeneration_per_hour = 0.00008;
    double organic_mineralization_per_hour = 0.00045;
    double pollination_decay_per_hour = 0.035;
};

struct HabitatConfig {
    std::size_t width = 96;
    std::size_t height = 96;
    double cell_size = 0.5;
    Vec3 origin{-24.0, 0.0, -24.0};
    std::uint64_t seed = 1;
};

class HabitatGrid {
public:
    explicit HabitatGrid(HabitatConfig config = {});

    void generate_island();
    void advance(double hours, double absolute_hours, const ClimateConfig& climate);
    void clear_canopy() noexcept;
    void add_canopy(Vec3 world_position, double amount) noexcept;
    void finalize_light() noexcept;

    [[nodiscard]] HabitatCell* cell_at(Vec3 world_position) noexcept;
    [[nodiscard]] const HabitatCell* cell_at(Vec3 world_position) const noexcept;
    [[nodiscard]] HabitatCell& cell(std::size_t x, std::size_t z);
    [[nodiscard]] const HabitatCell& cell(std::size_t x, std::size_t z) const;
    [[nodiscard]] Vec3 cell_center(std::size_t x, std::size_t z) const noexcept;
    [[nodiscard]] bool coordinates(Vec3 position,
                                   std::size_t& x,
                                   std::size_t& z) const noexcept;
    [[nodiscard]] bool is_land(Vec3 world_position) const noexcept;

    std::span<double> ensure_layer(std::string name, double initial_value = 0.0);
    [[nodiscard]] std::span<double> layer(std::string_view name) noexcept;
    [[nodiscard]] std::span<const double> layer(std::string_view name) const noexcept;

    [[nodiscard]] const HabitatConfig& config() const noexcept { return config_; }
    [[nodiscard]] std::size_t size() const noexcept { return cells_.size(); }
    [[nodiscard]] double mean_moisture() const noexcept;
    [[nodiscard]] double mean_temperature() const noexcept;
    [[nodiscard]] double mean_canopy() const noexcept;
    [[nodiscard]] double mean_light() const noexcept;
    [[nodiscard]] double mean_organic() const noexcept;
    [[nodiscard]] double mean_pollination() const noexcept;
    [[nodiscard]] double max_canopy() const noexcept;
    [[nodiscard]] double max_organic() const noexcept;
    [[nodiscard]] double max_pollination() const noexcept;

private:
    [[nodiscard]] std::size_t index(std::size_t x, std::size_t z) const noexcept;
    [[nodiscard]] double noise(std::size_t x, std::size_t z, std::uint64_t salt) const noexcept;
    [[nodiscard]] double mean_field(double HabitatCell::* field) const noexcept;
    [[nodiscard]] double mean_land_field(double HabitatCell::* field) const noexcept;
    [[nodiscard]] double max_land_field(double HabitatCell::* field) const noexcept;

    HabitatConfig config_{};
    std::vector<HabitatCell> cells_{};
    std::unordered_map<std::string, std::vector<double>> optional_layers_{};
};

} // namespace sim
