#pragma once

#include "sim/habitat.hpp"
#include "sim/types.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace sim {

struct EntityState {
    EntityId id = 0;
    Vec3 position{};
    Vec3 velocity{};
    SpeciesId species_id = 0;
    EntityKind kind = EntityKind::generic;
    BehaviorIntent intent = BehaviorIntent::idle;
    Vec3 target_position{};
    EntityId target_id = 0;
    double biomass = 1.0;
    double energy = 0.0;
    double hydration = 1.0;
    double age_hours = 0.0;
};

[[nodiscard]] constexpr bool operator==(const EntityState& a, const EntityState& b) noexcept {
    return a.id == b.id && a.position == b.position && a.velocity == b.velocity &&
           a.species_id == b.species_id && a.kind == b.kind && a.intent == b.intent &&
           a.target_position == b.target_position && a.target_id == b.target_id &&
           a.biomass == b.biomass && a.energy == b.energy && a.hydration == b.hydration &&
           a.age_hours == b.age_hours;
}

struct HabitatSnapshot {
    std::size_t width = 0;
    std::size_t height = 0;
    double cell_size = 0.5;
    Vec3 origin{};
    std::vector<double> elevation{};
    std::vector<double> moisture{};
    std::vector<double> nutrients{};
    std::vector<double> temperature{};
    std::vector<double> canopy{};
    std::vector<double> light{};
    std::vector<double> organic{};
    std::vector<double> pollination{};
    std::vector<std::uint8_t> surface{};
};

struct Snapshot {
    std::uint64_t tick = 0;
    double tick_dt = 0.0;
    double simulated_hours = 0.0;
    bool paused = false;
    double mean_moisture = 0.0;
    double mean_temperature = 0.0;
    double mean_canopy = 0.0;
    double mean_light = 0.0;
    double mean_organic = 0.0;
    double mean_pollination = 0.0;
    double year_phase = 0.0;
    double hour_of_day = 0.0;
    std::vector<EntityState> entities{};
};

[[nodiscard]] inline bool operator==(const Snapshot& a, const Snapshot& b) noexcept {
    return a.tick == b.tick && a.tick_dt == b.tick_dt &&
           a.simulated_hours == b.simulated_hours && a.paused == b.paused &&
           a.mean_moisture == b.mean_moisture && a.mean_temperature == b.mean_temperature &&
           a.mean_canopy == b.mean_canopy && a.mean_light == b.mean_light &&
           a.mean_organic == b.mean_organic && a.mean_pollination == b.mean_pollination &&
           a.year_phase == b.year_phase && a.hour_of_day == b.hour_of_day &&
           a.entities == b.entities;
}

[[nodiscard]] std::optional<EntityState> find_entity(const Snapshot& snapshot, EntityId id);

[[nodiscard]] Snapshot interpolate(const Snapshot& previous, const Snapshot& current, double alpha);

} // namespace sim
