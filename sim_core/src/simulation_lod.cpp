#include "sim/simulation_lod.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace sim {

namespace {

double sane_positive(double value, double fallback) {
    return std::isfinite(value) && value > 0.0 ? value : fallback;
}

std::uint32_t sane_period(std::uint32_t value) {
    return std::max<std::uint32_t>(1, value);
}

} // namespace

SimulationLodGrid::SimulationLodGrid(Vec3 bounds_min,
                                     Vec3 bounds_max,
                                     SimulationLodConfig config)
    : bounds_min_(bounds_min), bounds_max_(bounds_max), config_(config) {
    config_.region_size = sane_positive(config_.region_size, 600.0);
    config_.individual_radius = std::max(0.0, std::isfinite(config_.individual_radius)
                                                 ? config_.individual_radius
                                                 : 1'200.0);
    config_.cohort_radius = std::max(
        config_.individual_radius,
        std::isfinite(config_.cohort_radius) ? config_.cohort_radius : 4'800.0);
    config_.individual_period_ticks = sane_period(config_.individual_period_ticks);
    config_.cohort_period_ticks = sane_period(config_.cohort_period_ticks);
    config_.aggregate_period_ticks = sane_period(config_.aggregate_period_ticks);

    const double width = std::max(0.0, bounds_max_.x - bounds_min_.x);
    const double depth = std::max(0.0, bounds_max_.z - bounds_min_.z);
    columns_ = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(width / config_.region_size)));
    rows_ = std::max<std::size_t>(
        1, static_cast<std::size_t>(std::ceil(depth / config_.region_size)));
}

RegionCoord SimulationLodGrid::clamp_coord(RegionCoord coord) const noexcept {
    coord.x = std::clamp<std::int32_t>(
        coord.x, 0, static_cast<std::int32_t>(columns_ - 1));
    coord.z = std::clamp<std::int32_t>(
        coord.z, 0, static_cast<std::int32_t>(rows_ - 1));
    return coord;
}

RegionCoord SimulationLodGrid::region_at(Vec3 position) const noexcept {
    const auto index_for = [this](double value, double origin, std::size_t limit) {
        if (!std::isfinite(value)) {
            return std::int32_t{0};
        }
        const double local = std::floor((value - origin) / config_.region_size);
        const auto index = static_cast<std::int64_t>(local);
        return static_cast<std::int32_t>(
            std::clamp<std::int64_t>(index, 0, static_cast<std::int64_t>(limit - 1)));
    };
    return {
        index_for(position.x, bounds_min_.x, columns_),
        index_for(position.z, bounds_min_.z, rows_),
    };
}

Vec3 SimulationLodGrid::region_center(RegionCoord coord) const noexcept {
    coord = clamp_coord(coord);
    const double min_x = bounds_min_.x + static_cast<double>(coord.x) * config_.region_size;
    const double min_z = bounds_min_.z + static_cast<double>(coord.z) * config_.region_size;
    const double max_x = std::min(bounds_max_.x, min_x + config_.region_size);
    const double max_z = std::min(bounds_max_.z, min_z + config_.region_size);
    return {
        (min_x + max_x) * 0.5,
        (bounds_min_.y + bounds_max_.y) * 0.5,
        (min_z + max_z) * 0.5,
    };
}

double SimulationLodGrid::distance_to_region(RegionCoord coord, Vec3 observer) const noexcept {
    coord = clamp_coord(coord);
    const double min_x = bounds_min_.x + static_cast<double>(coord.x) * config_.region_size;
    const double min_z = bounds_min_.z + static_cast<double>(coord.z) * config_.region_size;
    const double max_x = std::min(bounds_max_.x, min_x + config_.region_size);
    const double max_z = std::min(bounds_max_.z, min_z + config_.region_size);
    const double closest_x = std::clamp(observer.x, min_x, max_x);
    const double closest_z = std::clamp(observer.z, min_z, max_z);
    const double dx = observer.x - closest_x;
    const double dz = observer.z - closest_z;
    return std::sqrt(dx * dx + dz * dz);
}

SimulationLod SimulationLodGrid::lod_for(RegionCoord coord, Vec3 observer) const noexcept {
    const double distance = distance_to_region(coord, observer);
    if (distance <= config_.individual_radius) {
        return SimulationLod::individual;
    }
    if (distance <= config_.cohort_radius) {
        return SimulationLod::cohort;
    }
    return SimulationLod::aggregate;
}

std::uint32_t SimulationLodGrid::period_for(SimulationLod lod) const noexcept {
    switch (lod) {
    case SimulationLod::individual:
        return config_.individual_period_ticks;
    case SimulationLod::cohort:
        return config_.cohort_period_ticks;
    case SimulationLod::aggregate:
        return config_.aggregate_period_ticks;
    }
    return config_.aggregate_period_ticks;
}

std::uint32_t SimulationLodGrid::phase_for(RegionCoord coord, std::uint32_t period) const noexcept {
    if (period <= 1) {
        return 0;
    }
    const std::uint32_t x = static_cast<std::uint32_t>(coord.x);
    const std::uint32_t z = static_cast<std::uint32_t>(coord.z);
    const std::uint32_t hash = (x * 73'856'093U) ^ (z * 19'349'663U);
    return hash % period;
}

bool SimulationLodGrid::due(RegionCoord coord,
                            SimulationLod lod,
                            std::uint64_t tick) const noexcept {
    const std::uint32_t period = period_for(lod);
    return tick % period == phase_for(clamp_coord(coord), period);
}

std::vector<RegionSchedule> SimulationLodGrid::schedule(Vec3 observer,
                                                        std::uint64_t tick) const {
    std::vector<RegionSchedule> out;
    out.reserve(region_count());
    for (std::size_t z = 0; z < rows_; ++z) {
        for (std::size_t x = 0; x < columns_; ++x) {
            const RegionCoord coord{static_cast<std::int32_t>(x),
                                    static_cast<std::int32_t>(z)};
            const SimulationLod lod = lod_for(coord, observer);
            out.push_back({
                coord,
                region_center(coord),
                lod,
                period_for(lod),
                due(coord, lod, tick),
            });
        }
    }
    return out;
}

SimulationLodSummary SimulationLodGrid::summary(Vec3 observer,
                                                std::uint64_t tick) const {
    SimulationLodSummary out;
    out.total_regions = region_count();
    for (std::size_t z = 0; z < rows_; ++z) {
        for (std::size_t x = 0; x < columns_; ++x) {
            const RegionCoord coord{static_cast<std::int32_t>(x),
                                    static_cast<std::int32_t>(z)};
            const SimulationLod lod = lod_for(coord, observer);
            switch (lod) {
            case SimulationLod::individual:
                ++out.individual_regions;
                break;
            case SimulationLod::cohort:
                ++out.cohort_regions;
                break;
            case SimulationLod::aggregate:
                ++out.aggregate_regions;
                break;
            }
            if (due(coord, lod, tick)) {
                ++out.due_regions;
            }
        }
    }
    return out;
}

} // namespace sim
