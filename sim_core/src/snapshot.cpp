#include "sim/snapshot.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <unordered_map>

namespace sim {

std::optional<EntityState> find_entity(const Snapshot& snapshot, EntityId id) {
    for (const EntityState& entity : snapshot.entities) {
        if (entity.id == id) {
            return entity;
        }
    }
    return std::nullopt;
}

Snapshot interpolate(const Snapshot& previous, const Snapshot& current, double alpha) {
    Snapshot out;
    out.tick = current.tick;
    out.tick_dt = current.tick_dt;
    out.simulated_hours = current.simulated_hours;
    out.paused = current.paused;
    out.mean_moisture = current.mean_moisture;
    out.mean_temperature = current.mean_temperature;
    out.mean_canopy = current.mean_canopy;
    out.mean_light = current.mean_light;
    out.mean_organic = current.mean_organic;
    out.mean_pollination = current.mean_pollination;
    out.year_phase = current.year_phase;
    out.hour_of_day = current.hour_of_day;
    out.entities.reserve(current.entities.size());

    const double t = std::clamp(alpha, 0.0, 1.0);
    std::unordered_map<EntityId, const EntityState*> prev_by_id;
    prev_by_id.reserve(previous.entities.size());
    for (const EntityState& entity : previous.entities) {
        prev_by_id.emplace(entity.id, &entity);
    }

    for (const EntityState& curr : current.entities) {
        EntityState state = curr;
        if (const auto it = prev_by_id.find(curr.id); it != prev_by_id.end()) {
            state.position = lerp(it->second->position, curr.position, t);
            state.velocity = lerp(it->second->velocity, curr.velocity, t);
            state.biomass = std::lerp(it->second->biomass, curr.biomass, t);
            state.energy = std::lerp(it->second->energy, curr.energy, t);
            state.hydration = std::lerp(it->second->hydration, curr.hydration, t);
            state.age_hours = std::lerp(it->second->age_hours, curr.age_hours, t);
        }
        out.entities.push_back(state);
    }
    return out;
}

} // namespace sim
