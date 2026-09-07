#pragma once

#include "sim/types.hpp"
#include "sim/vec3.hpp"

#include <variant>

namespace sim {

struct SpawnCommand {
    EntityId id = 0;
    Vec3 position{};
    Vec3 velocity{};
};

struct SpawnOrganismCommand {
    EntityId id = 0;
    SpeciesId species_id = 0;
    Vec3 position{};
    double energy = -1.0;
    double age_hours = 0.0;
};

struct DespawnCommand {
    EntityId id = 0;
};

using Command = std::variant<SpawnCommand, SpawnOrganismCommand, DespawnCommand>;

} // namespace sim
