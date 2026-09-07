#pragma once

#include "sim/species.hpp"
#include "sim/vec3.hpp"

#include <cstdint>

namespace sim {

using EntityId = std::uint64_t;

enum class BehaviorIntent : std::uint8_t {
    idle,
    resting,
    roaming,
    socializing,
    foraging,
    feeding,
    drinking,
    fleeing,
};

struct Entity {
    EntityId id = 0;
    Vec3 position{};
    Vec3 velocity{};
    Vec3 home_position{};
    Vec3 movement_target{};
    EntityId target_id = 0;
    SpeciesId species_id = 0;
    EntityKind kind = EntityKind::generic;
    BehaviorIntent intent = BehaviorIntent::idle;
    double biomass = 1.0;
    double energy = 0.0;
    double hydration = 1.0;
    double age_hours = 0.0;
    double reproduction_cooldown_hours = 0.0;
    double behavior_timer_hours = 0.0;

    // Last world ecology step applied to this organism. Simulation LOD can
    // defer work for a region, then advance the entity by the exact accumulated
    // simulated interval when that region becomes due again.
    std::uint64_t last_ecology_tick = 0;
};

} // namespace sim
