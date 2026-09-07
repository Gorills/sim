#include "sim/habitat.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <numeric>
#include <stdexcept>
#include <utility>

namespace sim {
namespace {

double unit_from_bits(std::uint64_t value) noexcept {
    value ^= value >> 30U;
    value *= 0xbf58476d1ce4e5b9ULL;
    value ^= value >> 27U;
    value *= 0x94d049bb133111ebULL;
    value ^= value >> 31U;
    return static_cast<double>(value >> 11U) * (1.0 / 9007199254740992.0);
}

} // namespace

HabitatGrid::HabitatGrid(HabitatConfig config) : config_(config) {
    config_.width = std::max<std::size_t>(1, config_.width);
    config_.height = std::max<std::size_t>(1, config_.height);
    if (!std::isfinite(config_.cell_size) || config_.cell_size <= 0.0) {
        config_.cell_size = 1.0;
    }
    cells_.resize(config_.width * config_.height);
    generate_island();
}

void HabitatGrid::generate_island() {
    const double half_width = static_cast<double>(config_.width) * 0.5;
    const double half_height = static_cast<double>(config_.height) * 0.5;

    for (std::size_t z = 0; z < config_.height; ++z) {
        for (std::size_t x = 0; x < config_.width; ++x) {
            const double nx = (static_cast<double>(x) + 0.5 - half_width) / half_width;
            const double nz = (static_cast<double>(z) + 0.5 - half_height) / half_height;
            const double radius = std::sqrt(nx * nx + nz * nz);
            const double irregularity = (noise(x, z, 17) - 0.5) * 0.14;
            const double coast = radius + irregularity;

            HabitatCell& value = cell(x, z);
            value.water = coast > 0.88;
            value.fresh_water =
                !value.water && coast < 0.72 &&
                std::abs(nx * 0.32 + std::sin(nz * 7.0) * 0.045) < 0.028;
            value.elevation =
                value.water ? -0.08 : std::max(0.0, (0.88 - coast) * 0.9 + noise(x, z, 31) * 0.06);
            value.moisture =
                value.water || value.fresh_water
                    ? 1.0
                    : std::clamp(0.82 - value.elevation * 0.45 +
                                     (noise(x, z, 47) - 0.5) * 0.18,
                                 0.18, 0.95);
            value.nutrients =
                value.water ? 0.2 : std::clamp(0.55 + noise(x, z, 61) * 0.35, 0.0, 1.0);
            value.temperature = 17.0 - value.elevation * 5.0;
            value.canopy = 0.0;
            value.light = 1.0;
            value.organic =
                value.water ? 0.0
                            : std::clamp(0.10 + (noise(x, z, 83) - 0.5) * 0.12, 0.02, 0.22);
            value.pollination = 0.0;
        }
    }
}

void HabitatGrid::advance(double hours, double absolute_hours, const ClimateConfig& climate) {
    if (!std::isfinite(hours) || hours <= 0.0) {
        return;
    }
    const double dt = std::min(hours, 24.0);
    const double year_phase =
        2.0 * std::numbers::pi * std::fmod(std::max(0.0, absolute_hours), 24.0 * 365.0) /
        (24.0 * 365.0);
    const double day_phase =
        2.0 * std::numbers::pi * std::fmod(std::max(0.0, absolute_hours), 24.0) / 24.0;
    const double seasonal = std::sin(year_phase - std::numbers::pi * 0.5);
    const double rain_cycle = std::max(0.0, std::sin(day_phase * 0.37 + year_phase * 11.0));

    for (HabitatCell& value : cells_) {
        if (value.water) {
            value.moisture = 1.0;
            value.temperature =
                climate.mean_temperature + seasonal * climate.seasonal_temperature_amplitude * 0.4;
            value.organic = 0.0;
            value.pollination = 0.0;
            value.light = 1.0;
            continue;
        }

        value.temperature = climate.mean_temperature +
                            seasonal * climate.seasonal_temperature_amplitude +
                            std::sin(day_phase - std::numbers::pi * 0.5) * 2.5 -
                            value.elevation * 5.0;
        const double rainfall = climate.rain_per_hour * (0.35 + rain_cycle * 1.8);
        const double heat_factor = std::clamp((value.temperature + 5.0) / 35.0, 0.1, 1.5);
        value.moisture =
            value.fresh_water
                ? 1.0
                : std::clamp(value.moisture +
                                 (rainfall - climate.evaporation_per_hour * heat_factor) * dt,
                             0.0, 1.0);
        const double mineralize =
            value.organic * climate.organic_mineralization_per_hour * dt;
        value.organic = std::clamp(value.organic - mineralize, 0.0, 1.0);
        value.nutrients =
            std::clamp(value.nutrients +
                           climate.nutrient_regeneration_per_hour * (1.0 - value.nutrients) * dt +
                           mineralize * 0.75,
                       0.0, 1.0);
        value.pollination =
            std::clamp(value.pollination - climate.pollination_decay_per_hour * dt, 0.0, 1.0);
    }
}

void HabitatGrid::clear_canopy() noexcept {
    for (HabitatCell& value : cells_) {
        value.canopy = 0.0;
    }
}

void HabitatGrid::add_canopy(Vec3 world_position, double amount) noexcept {
    HabitatCell* value = cell_at(world_position);
    if (value == nullptr || value->water || amount <= 0.0) {
        return;
    }
    value->canopy = std::clamp(value->canopy + amount, 0.0, 1.0);
}

void HabitatGrid::finalize_light() noexcept {
    for (HabitatCell& value : cells_) {
        value.light = value.water ? 1.0 : std::clamp(1.0 - value.canopy * 0.88, 0.08, 1.0);
    }
}

HabitatCell* HabitatGrid::cell_at(Vec3 world_position) noexcept {
    std::size_t x = 0;
    std::size_t z = 0;
    return coordinates(world_position, x, z) ? &cells_[index(x, z)] : nullptr;
}

const HabitatCell* HabitatGrid::cell_at(Vec3 world_position) const noexcept {
    std::size_t x = 0;
    std::size_t z = 0;
    return coordinates(world_position, x, z) ? &cells_[index(x, z)] : nullptr;
}

HabitatCell& HabitatGrid::cell(std::size_t x, std::size_t z) {
    if (x >= config_.width || z >= config_.height) {
        throw std::out_of_range("habitat cell coordinates");
    }
    return cells_[index(x, z)];
}

const HabitatCell& HabitatGrid::cell(std::size_t x, std::size_t z) const {
    if (x >= config_.width || z >= config_.height) {
        throw std::out_of_range("habitat cell coordinates");
    }
    return cells_[index(x, z)];
}

Vec3 HabitatGrid::cell_center(std::size_t x, std::size_t z) const noexcept {
    return {config_.origin.x + (static_cast<double>(x) + 0.5) * config_.cell_size,
            config_.origin.y,
            config_.origin.z + (static_cast<double>(z) + 0.5) * config_.cell_size};
}

bool HabitatGrid::is_land(Vec3 world_position) const noexcept {
    const HabitatCell* value = cell_at(world_position);
    return value != nullptr && !value->water;
}

std::span<double> HabitatGrid::ensure_layer(std::string name, double initial_value) {
    auto [it, inserted] =
        optional_layers_.try_emplace(std::move(name), cells_.size(), initial_value);
    if (!inserted && it->second.size() != cells_.size()) {
        it->second.assign(cells_.size(), initial_value);
    }
    return it->second;
}

std::span<double> HabitatGrid::layer(std::string_view name) noexcept {
    const auto it = optional_layers_.find(std::string(name));
    return it == optional_layers_.end() ? std::span<double>{} : std::span<double>{it->second};
}

std::span<const double> HabitatGrid::layer(std::string_view name) const noexcept {
    const auto it = optional_layers_.find(std::string(name));
    return it == optional_layers_.end() ? std::span<const double>{}
                                        : std::span<const double>{it->second};
}

double HabitatGrid::mean_moisture() const noexcept {
    return mean_field(&HabitatCell::moisture);
}

double HabitatGrid::mean_temperature() const noexcept {
    return mean_field(&HabitatCell::temperature);
}

double HabitatGrid::mean_canopy() const noexcept {
    return mean_land_field(&HabitatCell::canopy);
}

double HabitatGrid::mean_light() const noexcept {
    return mean_land_field(&HabitatCell::light);
}

double HabitatGrid::mean_organic() const noexcept {
    return mean_land_field(&HabitatCell::organic);
}

double HabitatGrid::mean_pollination() const noexcept {
    return mean_land_field(&HabitatCell::pollination);
}

double HabitatGrid::max_canopy() const noexcept {
    return max_land_field(&HabitatCell::canopy);
}

double HabitatGrid::max_organic() const noexcept {
    return max_land_field(&HabitatCell::organic);
}

double HabitatGrid::max_pollination() const noexcept {
    return max_land_field(&HabitatCell::pollination);
}

double HabitatGrid::mean_field(double HabitatCell::* field) const noexcept {
    if (cells_.empty()) {
        return 0.0;
    }
    const double total = std::accumulate(
        cells_.begin(), cells_.end(), 0.0,
        [field](double sum, const HabitatCell& value) { return sum + value.*field; });
    return total / static_cast<double>(cells_.size());
}

double HabitatGrid::mean_land_field(double HabitatCell::* field) const noexcept {
    double total = 0.0;
    std::size_t count = 0;
    for (const HabitatCell& value : cells_) {
        if (value.water) {
            continue;
        }
        total += value.*field;
        ++count;
    }
    return count == 0 ? 0.0 : total / static_cast<double>(count);
}

double HabitatGrid::max_land_field(double HabitatCell::* field) const noexcept {
    double best = 0.0;
    for (const HabitatCell& value : cells_) {
        if (value.water) {
            continue;
        }
        best = std::max(best, value.*field);
    }
    return best;
}

std::size_t HabitatGrid::index(std::size_t x, std::size_t z) const noexcept {
    return z * config_.width + x;
}

bool HabitatGrid::coordinates(Vec3 position, std::size_t& x, std::size_t& z) const noexcept {
    const double local_x = (position.x - config_.origin.x) / config_.cell_size;
    const double local_z = (position.z - config_.origin.z) / config_.cell_size;
    if (!std::isfinite(local_x) || !std::isfinite(local_z) || local_x < 0.0 || local_z < 0.0 ||
        local_x >= static_cast<double>(config_.width) ||
        local_z >= static_cast<double>(config_.height)) {
        return false;
    }
    x = static_cast<std::size_t>(local_x);
    z = static_cast<std::size_t>(local_z);
    return true;
}

double HabitatGrid::noise(std::size_t x, std::size_t z, std::uint64_t salt) const noexcept {
    const std::uint64_t key = config_.seed ^ (static_cast<std::uint64_t>(x) * 0x9e3779b97f4a7c15ULL) ^
                              (static_cast<std::uint64_t>(z) * 0xc2b2ae3d27d4eb4fULL) ^
                              (salt * 0x165667b19e3779f9ULL);
    return unit_from_bits(key);
}

} // namespace sim
