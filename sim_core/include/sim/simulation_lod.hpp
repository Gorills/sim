#pragma once

#include "sim/vec3.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sim {

enum class SimulationLod : std::uint8_t {
    individual = 0,
    cohort = 1,
    aggregate = 2,
};

struct SimulationLodConfig {
    // Spatial simulation partition. This is independent from render chunks.
    double region_size = 600.0;
    double individual_radius = 1'200.0;
    double cohort_radius = 4'800.0;

    // Temporal cadence for each representation. Regions are phase-shifted so
    // lower-frequency work is spread across ticks instead of forming spikes.
    std::uint32_t individual_period_ticks = 1;
    std::uint32_t cohort_period_ticks = 4;
    std::uint32_t aggregate_period_ticks = 20;
};

struct RegionCoord {
    std::int32_t x = 0;
    std::int32_t z = 0;

    [[nodiscard]] constexpr bool operator==(const RegionCoord&) const noexcept = default;
};

struct RegionSchedule {
    RegionCoord coord{};
    Vec3 center{};
    SimulationLod lod = SimulationLod::aggregate;
    std::uint32_t period_ticks = 1;
    bool due = false;
};

struct SimulationLodSummary {
    std::size_t total_regions = 0;
    std::size_t individual_regions = 0;
    std::size_t cohort_regions = 0;
    std::size_t aggregate_regions = 0;
    std::size_t due_regions = 0;
};

class SimulationLodGrid {
public:
    SimulationLodGrid(Vec3 bounds_min, Vec3 bounds_max, SimulationLodConfig config = {});

    [[nodiscard]] const SimulationLodConfig& config() const noexcept { return config_; }
    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] std::size_t region_count() const noexcept { return columns_ * rows_; }

    [[nodiscard]] RegionCoord region_at(Vec3 position) const noexcept;
    [[nodiscard]] Vec3 region_center(RegionCoord coord) const noexcept;
    [[nodiscard]] SimulationLod lod_for(RegionCoord coord, Vec3 observer) const noexcept;
    [[nodiscard]] std::uint32_t period_for(SimulationLod lod) const noexcept;
    [[nodiscard]] bool due(RegionCoord coord, SimulationLod lod, std::uint64_t tick) const noexcept;

    [[nodiscard]] std::vector<RegionSchedule> schedule(Vec3 observer, std::uint64_t tick) const;
    [[nodiscard]] SimulationLodSummary summary(Vec3 observer, std::uint64_t tick) const;

private:
    [[nodiscard]] double distance_to_region(RegionCoord coord, Vec3 observer) const noexcept;
    [[nodiscard]] std::uint32_t phase_for(RegionCoord coord, std::uint32_t period) const noexcept;
    [[nodiscard]] RegionCoord clamp_coord(RegionCoord coord) const noexcept;

    Vec3 bounds_min_{};
    Vec3 bounds_max_{};
    SimulationLodConfig config_{};
    std::size_t columns_ = 1;
    std::size_t rows_ = 1;
};

} // namespace sim
