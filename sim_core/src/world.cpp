#include "sim/world.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <ranges>
#include <utility>
#include <variant>

namespace sim {
namespace {

void bounce_axis(double& position, double& velocity, double min_value, double max_value) {
    if (max_value < min_value) {
        std::swap(min_value, max_value);
    }
    if (position < min_value) {
        position = min_value;
        velocity = std::abs(velocity);
    } else if (position > max_value) {
        position = max_value;
        velocity = -std::abs(velocity);
    }
}

WorldConfig normalize_config(WorldConfig config) {
    if (!std::isfinite(config.tick_dt) || config.tick_dt <= 0.0) {
        config.tick_dt = 1.0 / 60.0;
    }
    if (!std::isfinite(config.ecology_hours_per_tick) || config.ecology_hours_per_tick <= 0.0) {
        config.ecology_hours_per_tick = 1.0 / 60.0;
    }
    config.habitat.seed = config.seed;
    config.max_entities = std::max<std::size_t>(1, config.max_entities);
    return config;
}

double horizontal_distance_squared(Vec3 a, Vec3 b) noexcept {
    const double dx = a.x - b.x;
    const double dz = a.z - b.z;
    return dx * dx + dz * dz;
}

Vec3 horizontal_direction(Vec3 from, Vec3 to) noexcept {
    const double dx = to.x - from.x;
    const double dz = to.z - from.z;
    const double distance = std::sqrt(dx * dx + dz * dz);
    return distance > 1.0e-9 ? Vec3{dx / distance, 0.0, dz / distance} : Vec3{};
}

} // namespace

World::World(WorldConfig config, SpeciesCatalog species)
    : config_(normalize_config(config)),
      species_(std::move(species)),
      habitat_(config_.habitat),
      simulation_lod_grid_(config_.bounds_min, config_.bounds_max, config_.simulation_lod),
      simulation_lod_enabled_(config_.simulation_lod_enabled),
      random_state_(config_.seed == 0 ? 1 : config_.seed) {}

EntityId World::enqueue_spawn(Vec3 position, Vec3 velocity) {
    if (entities_.size() + queued_spawn_count_ >= config_.max_entities) {
        return 0;
    }
    const EntityId id = next_id_++;
    commands_.push_back(SpawnCommand{id, position, velocity});
    ++queued_spawn_count_;
    return id;
}

EntityId World::enqueue_organism(SpeciesId species_id,
                                 Vec3 position,
                                 double energy,
                                 double age_hours) {
    if (species_.find(species_id) == nullptr ||
        entities_.size() + queued_spawn_count_ >= config_.max_entities ||
        !habitat_.is_land(position)) {
        return 0;
    }
    const EntityId id = next_id_++;
    commands_.push_back(
        SpawnOrganismCommand{id, species_id, position, energy, std::max(0.0, age_hours)});
    ++queued_spawn_count_;
    return id;
}

EntityId World::enqueue_organism(std::string_view species_key,
                                 Vec3 position,
                                 double energy,
                                 double age_hours) {
    const SpeciesDefinition* definition = species_.find(species_key);
    return definition == nullptr ? 0
                                 : enqueue_organism(definition->id, position, energy, age_hours);
}

void World::enqueue_despawn(EntityId id) {
    commands_.push_back(DespawnCommand{id});
}

void World::flush_commands() {
    apply_commands();
    update_canopy_and_light();
}

void World::tick() {
    apply_commands();
    if (paused_) {
        return;
    }
    integrate();
    simulated_hours_ += config_.ecology_hours_per_tick;
    ++tick_index_;
}

void World::set_paused(bool paused) noexcept {
    paused_ = paused;
}

void World::set_tick_dt(double tick_dt) {
    config_.tick_dt =
        std::isfinite(tick_dt) && tick_dt > 0.0 ? tick_dt : 1.0 / 60.0;
}

void World::set_ecology_hours_per_tick(double hours) {
    if (!std::isfinite(hours) || hours <= 0.0) {
        return;
    }
    config_.ecology_hours_per_tick = hours;
}

void World::configure_simulation_lod(SimulationLodConfig config,
                                     Vec3 observer,
                                     bool enabled) {
    config_.simulation_lod = config;
    config_.simulation_lod_enabled = enabled;
    simulation_lod_grid_ = SimulationLodGrid(config_.bounds_min, config_.bounds_max, config);
    simulation_observer_ = observer;
    simulation_lod_enabled_ = enabled;
}

void World::set_simulation_observer(Vec3 observer) noexcept {
    if (std::isfinite(observer.x) && std::isfinite(observer.z)) {
        simulation_observer_ = observer;
    }
}

void World::set_simulation_lod_enabled(bool enabled) noexcept {
    simulation_lod_enabled_ = enabled;
    config_.simulation_lod_enabled = enabled;
}

SimulationLodSummary World::simulation_lod_summary() const {
    return simulation_lod_grid_.summary(simulation_observer_, tick_index_ + 1);
}

double World::absolute_hours() const noexcept {
    return simulated_hours_ + config_.climate_start_hour;
}

double World::year_phase() const noexcept {
    const double year = 24.0 * 365.0;
    return std::fmod(std::max(0.0, absolute_hours()), year) / year;
}

double World::hour_of_day() const noexcept {
    return std::fmod(std::max(0.0, absolute_hours()), 24.0);
}

Snapshot World::snapshot() const {
    return snapshot({}, -1.0);
}

Snapshot World::snapshot(Vec3 center, double radius) const {
    Snapshot snap;
    snap.tick = tick_index_;
    snap.tick_dt = config_.tick_dt;
    snap.simulated_hours = simulated_hours_;
    snap.paused = paused_;
    snap.mean_moisture = habitat_.mean_moisture();
    snap.mean_temperature = habitat_.mean_temperature();
    snap.mean_canopy = habitat_.mean_canopy();
    snap.mean_light = habitat_.mean_light();
    snap.mean_organic = habitat_.mean_organic();
    snap.mean_pollination = habitat_.mean_pollination();
    snap.year_phase = year_phase();
    snap.hour_of_day = hour_of_day();

    const bool filtered = std::isfinite(radius) && radius >= 0.0;
    const double radius_squared = radius * radius;
    snap.entities.reserve(filtered ? std::min<std::size_t>(entities_.size(), 4'096)
                                   : entities_.size());
    for (const Entity& entity : entities_) {
        if (filtered && horizontal_distance_squared(entity.position, center) > radius_squared) {
            continue;
        }
        EntityState state;
        state.id = entity.id;
        state.position = entity.position;
        state.velocity = entity.velocity;
        state.species_id = entity.species_id;
        state.kind = entity.kind;
        state.intent = entity.intent;
        state.target_position = entity.movement_target;
        state.target_id = entity.target_id;
        state.biomass = entity.biomass;
        state.energy = entity.energy;
        state.hydration = entity.hydration;
        state.age_hours = entity.age_hours;
        snap.entities.push_back(state);
    }
    return snap;
}

HabitatSnapshot World::habitat_snapshot() const {
    HabitatSnapshot snap;
    const HabitatConfig& habitat_config = habitat_.config();
    snap.width = habitat_config.width;
    snap.height = habitat_config.height;
    snap.cell_size = habitat_config.cell_size;
    snap.origin = habitat_config.origin;
    const std::size_t count = habitat_.size();
    snap.elevation.resize(count);
    snap.moisture.resize(count);
    snap.nutrients.resize(count);
    snap.temperature.resize(count);
    snap.canopy.resize(count);
    snap.light.resize(count);
    snap.organic.resize(count);
    snap.pollination.resize(count);
    snap.surface.resize(count);
    for (std::size_t z = 0; z < habitat_config.height; ++z) {
        for (std::size_t x = 0; x < habitat_config.width; ++x) {
            const HabitatCell& cell = habitat_.cell(x, z);
            const std::size_t i = z * habitat_config.width + x;
            snap.elevation[i] = cell.elevation;
            snap.moisture[i] = cell.moisture;
            snap.nutrients[i] = cell.nutrients;
            snap.temperature[i] = cell.temperature;
            snap.canopy[i] = cell.canopy;
            snap.light[i] = cell.light;
            snap.organic[i] = cell.organic;
            snap.pollination[i] = cell.pollination;
            snap.surface[i] = static_cast<std::uint8_t>(surface_kind(cell));
        }
    }
    return snap;
}

HabitatSnapshot World::habitat_snapshot(Vec3 center, double radius) const {
    const HabitatConfig& habitat_config = habitat_.config();
    if (!std::isfinite(radius) || radius <= 0.0) {
        return habitat_snapshot();
    }

    const double world_min_x = habitat_config.origin.x;
    const double world_min_z = habitat_config.origin.z;
    const double world_max_x =
        world_min_x + static_cast<double>(habitat_config.width) * habitat_config.cell_size;
    const double world_max_z =
        world_min_z + static_cast<double>(habitat_config.height) * habitat_config.cell_size;
    if (center.x + radius < world_min_x || center.x - radius >= world_max_x ||
        center.z + radius < world_min_z || center.z - radius >= world_max_z) {
        HabitatSnapshot empty;
        empty.cell_size = habitat_config.cell_size;
        empty.origin = center;
        return empty;
    }

    const auto cell_index = [](double value, double origin, double cell_size,
                               std::size_t limit) {
        const double local = std::floor((value - origin) / cell_size);
        const auto signed_index = static_cast<std::int64_t>(local);
        return static_cast<std::size_t>(
            std::clamp<std::int64_t>(signed_index, 0, static_cast<std::int64_t>(limit - 1)));
    };
    const std::size_t min_x =
        cell_index(center.x - radius, world_min_x, habitat_config.cell_size, habitat_config.width);
    const std::size_t max_x =
        cell_index(center.x + radius, world_min_x, habitat_config.cell_size, habitat_config.width);
    const std::size_t min_z =
        cell_index(center.z - radius, world_min_z, habitat_config.cell_size, habitat_config.height);
    const std::size_t max_z =
        cell_index(center.z + radius, world_min_z, habitat_config.cell_size, habitat_config.height);

    HabitatSnapshot snap;
    snap.width = max_x - min_x + 1;
    snap.height = max_z - min_z + 1;
    snap.cell_size = habitat_config.cell_size;
    snap.origin = {world_min_x + static_cast<double>(min_x) * habitat_config.cell_size,
                   habitat_config.origin.y,
                   world_min_z + static_cast<double>(min_z) * habitat_config.cell_size};
    const std::size_t count = snap.width * snap.height;
    snap.elevation.resize(count);
    snap.moisture.resize(count);
    snap.nutrients.resize(count);
    snap.temperature.resize(count);
    snap.canopy.resize(count);
    snap.light.resize(count);
    snap.organic.resize(count);
    snap.pollination.resize(count);
    snap.surface.resize(count);

    for (std::size_t z = min_z; z <= max_z; ++z) {
        for (std::size_t x = min_x; x <= max_x; ++x) {
            const HabitatCell& cell = habitat_.cell(x, z);
            const std::size_t i = (z - min_z) * snap.width + (x - min_x);
            snap.elevation[i] = cell.elevation;
            snap.moisture[i] = cell.moisture;
            snap.nutrients[i] = cell.nutrients;
            snap.temperature[i] = cell.temperature;
            snap.canopy[i] = cell.canopy;
            snap.light[i] = cell.light;
            snap.organic[i] = cell.organic;
            snap.pollination[i] = cell.pollination;
            snap.surface[i] = static_cast<std::uint8_t>(surface_kind(cell));
        }
    }
    return snap;
}

OverviewSnapshot World::overview_snapshot(std::size_t resolution) const {
    const HabitatConfig& habitat_config = habitat_.config();
    const std::size_t max_dimension = std::max(habitat_config.width, habitat_config.height);
    resolution = std::clamp<std::size_t>(resolution, 1, max_dimension);
    const std::size_t scale = std::max<std::size_t>(
        1, (max_dimension + resolution - 1) / resolution);

    OverviewSnapshot out;
    out.width = (habitat_config.width + scale - 1) / scale;
    out.height = (habitat_config.height + scale - 1) / scale;
    out.cell_size = habitat_config.cell_size * static_cast<double>(scale);
    out.origin = habitat_config.origin;
    const std::size_t count = out.width * out.height;
    out.surface.assign(count, static_cast<std::uint8_t>(SurfaceKind::ocean));
    out.plants.assign(count, 0);
    out.herbivores.assign(count, 0);
    out.omnivores.assign(count, 0);
    out.carnivores.assign(count, 0);
    out.insects.assign(count, 0);

    std::vector<std::uint32_t> land(count, 0);
    std::vector<std::uint32_t> ocean(count, 0);
    std::vector<std::uint32_t> fresh(count, 0);
    for (std::size_t z = 0; z < habitat_config.height; ++z) {
        for (std::size_t x = 0; x < habitat_config.width; ++x) {
            const std::size_t overview_index = (z / scale) * out.width + (x / scale);
            switch (surface_kind(habitat_.cell(x, z))) {
            case SurfaceKind::land:
                ++land[overview_index];
                break;
            case SurfaceKind::ocean:
                ++ocean[overview_index];
                break;
            case SurfaceKind::fresh_water:
                ++fresh[overview_index];
                break;
            }
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t total = land[i] + ocean[i] + fresh[i];
        if (fresh[i] != 0 && fresh[i] * 4U >= total) {
            out.surface[i] = static_cast<std::uint8_t>(SurfaceKind::fresh_water);
        } else if (land[i] >= ocean[i]) {
            out.surface[i] = static_cast<std::uint8_t>(SurfaceKind::land);
        }
    }

    for (const Entity& entity : entities_) {
        std::size_t x = 0;
        std::size_t z = 0;
        if (!habitat_.coordinates(entity.position, x, z)) {
            continue;
        }
        const std::size_t i = (z / scale) * out.width + (x / scale);
        switch (entity.kind) {
        case EntityKind::plant:
            ++out.plants[i];
            break;
        case EntityKind::herbivore:
            ++out.herbivores[i];
            break;
        case EntityKind::omnivore:
            ++out.omnivores[i];
            break;
        case EntityKind::carnivore:
            ++out.carnivores[i];
            break;
        case EntityKind::insect:
            ++out.insects[i];
            break;
        case EntityKind::generic:
            break;
        }
    }
    return out;
}

std::size_t World::entity_count(SpeciesId species_id) const noexcept {
    const auto it = species_counts_.find(species_id);
    return it == species_counts_.end() ? 0 : it->second;
}

EcosystemStats World::ecosystem_stats() const {
    EcosystemStats stats;
    stats.simulated_hours = simulated_hours_;
    stats.year_phase = year_phase();
    stats.mean_moisture = habitat_.mean_moisture();
    stats.mean_temperature = habitat_.mean_temperature();
    stats.mean_canopy = habitat_.mean_canopy();
    stats.mean_light = habitat_.mean_light();
    stats.mean_organic = habitat_.mean_organic();
    stats.mean_pollination = habitat_.mean_pollination();
    stats.max_canopy = habitat_.max_canopy();
    stats.max_organic = habitat_.max_organic();
    stats.max_pollination = habitat_.max_pollination();
    stats.deaths_age = deaths_age_;
    stats.deaths_starvation = deaths_starvation_;
    stats.deaths_dehydration = deaths_dehydration_;
    stats.deaths_biomass_loss = deaths_biomass_loss_;

    std::vector<SpeciesPopulation> populations;
    populations.reserve(species_.all().size());
    std::unordered_map<SpeciesId, std::size_t> population_index;
    population_index.reserve(species_.all().size());

    for (const SpeciesDefinition& definition : species_.all()) {
        SpeciesPopulation population;
        population.species_id = definition.id;
        const auto age = deaths_age_by_species_.find(definition.id);
        const auto starved = deaths_starvation_by_species_.find(definition.id);
        const auto dehydrated = deaths_dehydration_by_species_.find(definition.id);
        const auto biomass = deaths_biomass_by_species_.find(definition.id);
        population.deaths_age = age == deaths_age_by_species_.end() ? 0 : age->second;
        population.deaths_starvation =
            starved == deaths_starvation_by_species_.end() ? 0 : starved->second;
        population.deaths_dehydration =
            dehydrated == deaths_dehydration_by_species_.end() ? 0 : dehydrated->second;
        population.deaths_biomass_loss =
            biomass == deaths_biomass_by_species_.end() ? 0 : biomass->second;
        population_index.emplace(definition.id, populations.size());
        populations.push_back(population);
    }

    double insect_activity_sum = 0.0;
    std::size_t insect_activity_samples = 0;

    for (const Entity& entity : entities_) {
        const SpeciesDefinition* definition = species_.find(entity.species_id);
        if (definition != nullptr) {
            if (const auto population_it = population_index.find(entity.species_id);
                population_it != population_index.end()) {
                SpeciesPopulation& population = populations[population_it->second];
                ++population.count;
                population.biomass += entity.biomass;
                if (is_animal(entity.kind)) {
                    population.mean_energy_fraction +=
                        entity.energy / std::max(0.001, definition->max_energy);
                    population.mean_hydration += entity.hydration;
                }
            }
            if (has_tag(*definition, "decomposer")) {
                ++stats.decomposers;
            }
        }

        switch (entity.kind) {
        case EntityKind::plant:
            ++stats.plants;
            break;
        case EntityKind::herbivore:
            ++stats.herbivores;
            break;
        case EntityKind::omnivore:
            ++stats.omnivores;
            break;
        case EntityKind::carnivore:
            ++stats.carnivores;
            break;
        case EntityKind::insect:
            ++stats.insects;
            insect_activity_sum +=
                definition != nullptr ? activity_factor(*definition, habitat_.cell_at(entity.position))
                                      : 1.0;
            ++insect_activity_samples;
            break;
        case EntityKind::generic:
            break;
        }

        switch (entity.intent) {
        case BehaviorIntent::resting:
            ++stats.resting;
            break;
        case BehaviorIntent::roaming:
            ++stats.roaming;
            break;
        case BehaviorIntent::socializing:
            ++stats.socializing;
            break;
        case BehaviorIntent::foraging:
            ++stats.foraging;
            break;
        case BehaviorIntent::feeding:
            ++stats.feeding;
            break;
        case BehaviorIntent::drinking:
            ++stats.drinking;
            break;
        case BehaviorIntent::fleeing:
            ++stats.fleeing;
            break;
        case BehaviorIntent::idle:
            break;
        }
    }

    for (SpeciesPopulation& population : populations) {
        const bool had_deaths =
            (population.deaths_age + population.deaths_starvation +
             population.deaths_dehydration + population.deaths_biomass_loss) != 0;
        if (population.count == 0 && !had_deaths) {
            continue;
        }
        const SpeciesDefinition* definition = species_.find(population.species_id);
        if (population.count != 0 && definition != nullptr && is_animal(definition->kind)) {
            population.mean_energy_fraction /= static_cast<double>(population.count);
            population.mean_hydration /= static_cast<double>(population.count);
        }
        stats.populations.push_back(population);
    }

    if (insect_activity_samples != 0) {
        stats.mean_insect_activity =
            insect_activity_sum / static_cast<double>(insect_activity_samples);
    }
    return stats;
}

void World::apply_commands() {
    for (const Command& command : commands_) {
        std::visit([this](const auto& cmd) { apply_one(cmd); }, command);
    }
    commands_.clear();
    queued_spawn_count_ = 0;
}

void World::apply_one(const SpawnCommand& cmd) {
    if (cmd.id == 0 || index_.contains(cmd.id) ||
        entities_.size() >= config_.max_entities) {
        return;
    }
    Entity entity;
    entity.id = cmd.id;
    entity.position = cmd.position;
    entity.velocity = cmd.velocity;
    entity.home_position = cmd.position;
    entity.movement_target = cmd.position;
    entity.last_ecology_tick = tick_index_;
    index_.emplace(cmd.id, entities_.size());
    entities_.push_back(entity);
}

void World::apply_one(const SpawnOrganismCommand& cmd) {
    const SpeciesDefinition* definition = species_.find(cmd.species_id);
    if (definition == nullptr || cmd.id == 0 || index_.contains(cmd.id) ||
        entities_.size() >= config_.max_entities || !habitat_.is_land(cmd.position)) {
        return;
    }

    Entity entity;
    entity.id = cmd.id;
    entity.position = cmd.position;
    entity.position.y = cruise_height(*definition);
    entity.home_position = entity.position;
    entity.movement_target = entity.position;
    entity.species_id = definition->id;
    entity.kind = definition->kind;
    entity.intent =
        is_animal(definition->kind) ? BehaviorIntent::resting : BehaviorIntent::idle;
    entity.biomass = definition->initial_biomass;
    entity.energy = cmd.energy >= 0.0 ? std::min(cmd.energy, definition->max_energy)
                                     : definition->initial_energy;
    entity.hydration = 1.0;
    entity.age_hours = cmd.age_hours;
    entity.reproduction_cooldown_hours = definition->reproduction_interval_hours * random_unit();
    entity.behavior_timer_hours =
        is_animal(definition->kind) ? definition->rest_duration_hours * random_unit() : 0.0;
    entity.last_ecology_tick = tick_index_;
    index_.emplace(cmd.id, entities_.size());
    entities_.push_back(entity);
    ++species_counts_[definition->id];
}

void World::apply_one(const DespawnCommand& cmd) {
    const auto it = index_.find(cmd.id);
    if (it == index_.end()) {
        return;
    }
    remove_at(it->second);
}

void World::integrate() {
    simulation_work_stats_ = {};
    simulation_work_stats_.tick = tick_index_ + 1;

    habitat_.advance(config_.ecology_hours_per_tick, absolute_hours(), config_.climate);
    update_canopy_and_light();
    update_plants();
    rebuild_spatial_index();
    update_animals();
    remove_dead();

    const double dt = config_.tick_dt;
    for (Entity& entity : entities_) {
        if (entity.kind != EntityKind::generic) {
            continue;
        }
        entity.position = entity.position + entity.velocity * dt;
        bounce_axis(entity.position.x, entity.velocity.x, config_.bounds_min.x, config_.bounds_max.x);
        bounce_axis(entity.position.y, entity.velocity.y, config_.bounds_min.y, config_.bounds_max.y);
        bounce_axis(entity.position.z, entity.velocity.z, config_.bounds_min.z, config_.bounds_max.z);
    }
}

void World::update_canopy_and_light() {
    habitat_.clear_canopy();
    for (const Entity& entity : entities_) {
        if (entity.kind != EntityKind::plant) {
            continue;
        }
        const SpeciesDefinition* definition = species_.find(entity.species_id);
        if (definition == nullptr || definition->canopy_contribution <= 0.0) {
            continue;
        }
        const double scale = std::clamp(
            entity.biomass / std::max(0.01, definition->initial_biomass), 0.25, 1.0);
        habitat_.add_canopy(entity.position, definition->canopy_contribution * scale);
    }
    habitat_.finalize_light();
}

void World::update_plants() {
    for (Entity& entity : entities_) {
        if (entity.kind != EntityKind::plant) {
            continue;
        }
        const std::optional<double> scheduled_hours = scheduled_ecology_hours(entity);
        if (!scheduled_hours.has_value()) {
            continue;
        }
        const double hours = *scheduled_hours;
        const SpeciesDefinition* definition = species_.find(entity.species_id);
        HabitatCell* cell = habitat_.cell_at(entity.position);
        if (definition == nullptr || cell == nullptr || cell->water) {
            entity.biomass = 0.0;
            continue;
        }

        entity.age_hours += hours;
        entity.reproduction_cooldown_hours =
            std::max(0.0, entity.reproduction_cooldown_hours - hours);

        const double moisture_fitness =
            std::clamp(1.0 - std::abs(cell->moisture - definition->preferred_moisture) /
                                 std::max(0.01, definition->moisture_tolerance),
                       0.0, 1.0);
        const double temperature_fitness =
            std::clamp(1.0 - std::abs(cell->temperature - definition->preferred_temperature) /
                                 std::max(0.01, definition->temperature_tolerance),
                       0.0, 1.0);
        const double light_target =
            std::clamp(0.5 - 0.5 * definition->shade_preference, 0.0, 1.0);
        const double light_fitness =
            std::clamp(1.0 - std::abs(cell->light - light_target) / 0.8, 0.0, 1.0);
        const double organic_share = std::clamp(definition->organic_growth_factor, 0.0, 1.0);
        const double resource =
            cell->organic * organic_share + cell->nutrients * (1.0 - organic_share);
        const double carrying =
            std::clamp(1.0 - entity.biomass / std::max(0.01, definition->max_biomass), 0.0, 1.0);
        const double fitness =
            moisture_fitness * temperature_fitness * resource * light_fitness;
        const double growth =
            definition->growth_biomass_per_hour * fitness * carrying * hours;
        const double stress_loss =
            definition->initial_biomass * definition->stress_loss_fraction_per_hour *
            (1.0 - fitness) * hours;
        entity.biomass = std::clamp(entity.biomass + growth - stress_loss, 0.0,
                                    definition->max_biomass);
        cell->moisture = std::clamp(cell->moisture - growth * 0.0008, 0.0, 1.0);
        cell->nutrients = std::clamp(cell->nutrients - growth * 0.003, 0.0, 1.0);
        if (organic_share > 0.0 && growth > 0.0) {
            cell->organic = std::clamp(cell->organic - growth * 0.045 * organic_share, 0.0, 1.0);
        }
        if (has_tag(*definition, "nitrogen_fixer")) {
            cell->nutrients = std::clamp(cell->nutrients + growth * 0.0012, 0.0, 1.0);
        }
        if (definition->litter_organic_per_hour > 0.0) {
            const double litter =
                definition->litter_organic_per_hour *
                (entity.biomass / std::max(0.01, definition->max_biomass)) * hours;
            cell->organic = std::clamp(cell->organic + litter, 0.0, 1.0);
        }

        reproduce(entity, *definition, hours);
    }
}

void World::update_animals() {
    const std::size_t count = entities_.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (!is_animal(entities_[i].kind)) {
            continue;
        }
        const std::optional<double> scheduled_hours = scheduled_ecology_hours(entities_[i]);
        if (!scheduled_hours.has_value()) {
            continue;
        }
        update_one_animal(i, *scheduled_hours);
    }
}

std::optional<double> World::scheduled_ecology_hours(Entity& entity) {
    const std::uint64_t step = tick_index_ + 1;
    SimulationLod lod = SimulationLod::individual;
    bool due = true;

    if (simulation_lod_enabled_) {
        const RegionCoord coord = simulation_lod_grid_.region_at(entity.position);
        lod = simulation_lod_grid_.lod_for(coord, simulation_observer_);
        due = simulation_lod_grid_.due(coord, lod, step);
    }

    if (!due) {
        record_lod_entity(lod, false, 0.0);
        return std::nullopt;
    }

    const std::uint64_t last = std::min(entity.last_ecology_tick, step);
    const std::uint64_t elapsed_steps = std::max<std::uint64_t>(1, step - last);
    const double hours = config_.ecology_hours_per_tick * static_cast<double>(elapsed_steps);
    entity.last_ecology_tick = step;
    record_lod_entity(lod, true, hours);
    return hours;
}

void World::record_lod_entity(SimulationLod lod, bool updated, double catchup_hours) {
    ++simulation_work_stats_.organism_entities;
    switch (lod) {
    case SimulationLod::individual:
        ++simulation_work_stats_.individual_entities;
        break;
    case SimulationLod::cohort:
        ++simulation_work_stats_.cohort_entities;
        break;
    case SimulationLod::aggregate:
        ++simulation_work_stats_.aggregate_entities;
        break;
    }
    if (updated) {
        ++simulation_work_stats_.updated_entities;
        simulation_work_stats_.max_catchup_hours =
            std::max(simulation_work_stats_.max_catchup_hours, catchup_hours);
    } else {
        ++simulation_work_stats_.deferred_entities;
    }
}

void World::rebuild_spatial_index() {
    spatial_buckets_.clear();
    spatial_buckets_.resize(habitat_.size());
    spatial_positions_.resize(entities_.size());
    const HabitatConfig& habitat_config = habitat_.config();
    for (std::size_t i = 0; i < entities_.size(); ++i) {
        spatial_positions_[i] = entities_[i].position;
        std::size_t x = 0;
        std::size_t z = 0;
        if (habitat_.coordinates(entities_[i].position, x, z)) {
            spatial_buckets_[z * habitat_config.width + x].push_back(i);
        }
    }
}

void World::consume_target(Entity& consumer,
                           Entity& food,
                           const SpeciesDefinition& definition,
                           double hours,
                           double activity) {
    const SpeciesDefinition* food_definition = species_.find(food.species_id);
    const double feed_rate = std::max(activity, 0.25);
    const double retained_biomass =
        food.kind == EntityKind::plant && food_definition != nullptr
            ? food_definition->initial_biomass * 0.12
            : 0.01;
    const double available = std::max(0.0, food.biomass - retained_biomass);
    const double consumed = std::min(available, definition.bite_biomass_per_hour * hours * feed_rate);
    food.biomass -= consumed;
    if (food_definition != nullptr) {
        consumer.energy =
            std::min(definition.max_energy,
                     consumer.energy + consumed * food_definition->food_energy_per_biomass * 0.75);
        if (has_tag(*food_definition, "flowering")) {
            if (definition.nectar_energy_per_hour > 0.0) {
                consumer.energy = std::min(
                    definition.max_energy,
                    consumer.energy + definition.nectar_energy_per_hour * hours * feed_rate);
            }
            if (definition.pollination_deposit_per_hour > 0.0) {
                if (HabitatCell* visit = habitat_.cell_at(food.position); visit != nullptr) {
                    visit->pollination = std::clamp(
                        visit->pollination +
                            definition.pollination_deposit_per_hour * hours * feed_rate,
                        0.0, 1.0);
                }
            }
        }
    }
    consumer.biomass = std::min(definition.max_biomass, consumer.biomass + consumed * 0.015);
    consumer.velocity = {};
}

void World::update_one_animal(std::size_t entity_index, double hours) {
    Entity& entity = entities_[entity_index];
    const SpeciesDefinition* definition = species_.find(entity.species_id);
    if (definition == nullptr) {
        entity.biomass = 0.0;
        return;
    }

    HabitatCell* cell = habitat_.cell_at(entity.position);
    const double activity = activity_factor(*definition, cell);
    const double hungry = forage_threshold(*definition);

    entity.age_hours += hours;
    entity.reproduction_cooldown_hours =
        std::max(0.0, entity.reproduction_cooldown_hours - hours);
    entity.behavior_timer_hours = std::max(0.0, entity.behavior_timer_hours - hours);
    entity.energy -= definition->metabolism_per_hour * hours * (0.35 + 0.65 * activity);
    entity.hydration -= definition->dehydration_per_hour * hours;

    const Vec3 previous_position = entity.position;
    if (const std::optional<std::size_t> threat =
            nearest_threat(entity_index, *definition);
        threat.has_value()) {
        const Entity& predator = entities_[*threat];
        Vec3 away = horizontal_direction(predator.position, entity.position);
        if (length_squared(away) <= 1.0e-12) {
            const double angle =
                std::fmod(static_cast<double>(entity.id) * 0.61803398875, 1.0) *
                2.0 * std::numbers::pi;
            away = {std::cos(angle), 0.0, std::sin(angle)};
        }
        entity.target_id = predator.id;
        entity.movement_target =
            entity.position + away * std::max(definition->flee_radius, definition->perception_radius);
        entity.intent = BehaviorIntent::fleeing;
        move_towards(entity, entity.movement_target, *definition, hours,
                     std::max(activity, 0.72), 0.0, definition->flee_speed_multiplier);
        constrain_to_habitat(entity, previous_position);
        return;
    }

    const bool urgent =
        entity.hydration < 0.28 || entity.energy < definition->max_energy * 0.34;
    const double needs_activity = urgent ? std::max(activity, 0.75) : activity;
    if (entity.hydration < 0.55) {
        if (const std::optional<Vec3> drink =
                nearest_drink(entity.position, definition->perception_radius * 3.0);
            drink.has_value()) {
            entity.target_id = 0;
            entity.movement_target = *drink;
            entity.intent = BehaviorIntent::drinking;
            const double distance =
                std::sqrt(horizontal_distance_squared(entity.position, *drink));
            const double drink_radius =
                std::max(definition->interaction_radius, habitat_.config().cell_size * 0.75);
            if (distance <= drink_radius) {
                entity.hydration = std::min(1.0, entity.hydration + 0.22 * hours);
                entity.velocity = {};
            } else if (needs_activity >= 0.12) {
                move_towards(entity, *drink, *definition, hours, needs_activity, drink_radius);
                if (std::sqrt(horizontal_distance_squared(entity.position, *drink)) <=
                    drink_radius) {
                    entity.hydration = std::min(1.0, entity.hydration + 0.22 * hours);
                    entity.velocity = {};
                }
            } else {
                entity.velocity = {};
            }
            constrain_to_habitat(entity, previous_position);
            return;
        }
    }

    if (definition->organic_consumption_per_hour > 0.0 && entity.energy < hungry) {
        HabitatCell* organic_cell = habitat_.cell_at(entity.position);
        if (organic_cell != nullptr && organic_cell->organic > 1.0e-5) {
            entity.target_id = 0;
            entity.movement_target = entity.position;
            entity.intent = BehaviorIntent::feeding;
            const double consumed =
                std::min(organic_cell->organic,
                         definition->organic_consumption_per_hour * hours *
                             std::max(needs_activity, 0.25));
            organic_cell->organic = std::max(0.0, organic_cell->organic - consumed);
            organic_cell->nutrients =
                std::clamp(organic_cell->nutrients + consumed * 0.4, 0.0, 1.0);
            entity.energy =
                std::min(definition->max_energy,
                         entity.energy + consumed * definition->food_energy_per_biomass);
            entity.velocity = {};
            constrain_to_habitat(entity, previous_position);
            reproduce(entity, *definition, hours);
            return;
        } else if (needs_activity >= 0.12) {
            if (const std::optional<Vec3> organic =
                    nearest_organic(entity.position, definition->perception_radius * 2.0);
                organic.has_value()) {
                entity.target_id = 0;
                entity.movement_target = *organic;
                entity.intent = BehaviorIntent::foraging;
                move_towards(entity, *organic, *definition, hours, needs_activity,
                             habitat_.config().cell_size * 0.5);
                constrain_to_habitat(entity, previous_position);
                reproduce(entity, *definition, hours);
                return;
            }
        }
    }

    const bool seeking_food = entity.energy < hungry;
    const std::optional<std::size_t> food_index =
        seeking_food ? current_or_nearest_food(entity_index, *definition)
                     : std::optional<std::size_t>{};
    if (food_index.has_value()) {
        Entity& food = entities_[*food_index];
        entity.target_id = food.id;
        entity.movement_target = food.position;
        const double distance = std::sqrt(horizontal_distance_squared(entity.position, food.position));
        if (distance <= definition->interaction_radius) {
            entity.intent = BehaviorIntent::feeding;
            consume_target(entity, food, *definition, hours, activity);
        } else if (needs_activity >= 0.12) {
            entity.intent = BehaviorIntent::foraging;
            const double hunt_speed =
                is_animal(food.kind) ? definition->hunt_speed_multiplier : 1.0;
            move_towards(entity, food.position, *definition, hours, needs_activity,
                         definition->interaction_radius, hunt_speed);
            if (std::sqrt(horizontal_distance_squared(entity.position, food.position)) <=
                definition->interaction_radius) {
                entity.intent = BehaviorIntent::feeding;
                consume_target(entity, food, *definition, hours, activity);
            }
        } else {
            entity.intent = BehaviorIntent::resting;
            entity.velocity = {};
        }
    } else {
        entity.target_id = 0;
        if (activity < 0.12 && !urgent) {
            entity.intent = BehaviorIntent::resting;
            entity.velocity = {};
            entity.behavior_timer_hours =
                std::max(entity.behavior_timer_hours, definition->rest_duration_hours * 0.5);
        } else if (entity.intent == BehaviorIntent::resting &&
                   entity.behavior_timer_hours > 0.0) {
            entity.velocity = {};
        } else {
            const double distance =
                std::sqrt(horizontal_distance_squared(entity.position, entity.movement_target));
            const bool continuing =
                (entity.intent == BehaviorIntent::roaming ||
                 entity.intent == BehaviorIntent::socializing ||
                 (entity.intent == BehaviorIntent::foraging && !food_index.has_value())) &&
                entity.behavior_timer_hours > 0.0 &&
                distance > std::max(0.2, definition->interaction_radius);
            if (!continuing) {
                if (entity.intent == BehaviorIntent::roaming ||
                    entity.intent == BehaviorIntent::socializing ||
                    entity.intent == BehaviorIntent::foraging) {
                    begin_rest(entity, *definition);
                } else {
                    choose_roaming_target(entity_index, entity, *definition);
                    if (seeking_food) {
                        entity.intent = BehaviorIntent::foraging;
                    }
                }
            }
            if (entity.intent != BehaviorIntent::resting) {
                move_towards(entity, entity.movement_target, *definition, hours,
                             needs_activity, std::max(0.2, definition->interaction_radius));
            }
        }
    }

    if (definition->pollination_deposit_per_hour > 0.0) {
        if (HabitatCell* visit = habitat_.cell_at(entity.position); visit != nullptr) {
            visit->pollination = std::clamp(
                visit->pollination +
                    definition->pollination_deposit_per_hour * hours * std::max(activity, 0.15) * 0.25,
                0.0, 1.0);
        }
    }

    constrain_to_habitat(entity, previous_position);
    reproduce(entity, *definition, hours);
}

void World::reproduce(Entity& parent, const SpeciesDefinition& definition, double hours) {
    if (parent.age_hours < definition.maturity_hours ||
        parent.reproduction_cooldown_hours > 0.0 ||
        queued_spawn_count_ + entities_.size() >= config_.max_entities) {
        return;
    }

    if (is_animal(definition.kind)) {
        const HabitatCell* cell = habitat_.cell_at(parent.position);
        if (activity_factor(definition, cell) < 0.4) {
            return;
        }
    }

    if (definition.carrying_density_per_cell > 0.0) {
        const auto capacity = static_cast<std::size_t>(
            std::ceil(static_cast<double>(habitat_.size()) *
                      definition.carrying_density_per_cell));
        if (entity_count(definition.id) >= capacity) {
            return;
        }
    }

    if (definition.kind == EntityKind::plant) {
        if (parent.biomass < definition.initial_biomass * 1.4) {
            return;
        }
        if (definition.pollination_requirement > 0.0) {
            const HabitatCell* cell = habitat_.cell_at(parent.position);
            const double pollination = cell == nullptr ? 0.0 : cell->pollination;
            if (pollination < definition.pollination_requirement * 0.2) {
                return;
            }
        }
    } else {
        if (parent.energy < definition.max_energy * 0.72 || entity_count(definition.id) < 2) {
            return;
        }
    }

    double probability =
        1.0 - std::exp(-std::max(0.0, definition.reproduction_chance_per_hour) * hours);
    if (definition.kind == EntityKind::plant && definition.pollination_requirement > 0.0) {
        const HabitatCell* cell = habitat_.cell_at(parent.position);
        const double pollination = cell == nullptr ? 0.0 : cell->pollination;
        probability *= std::clamp(
            pollination / std::max(0.01, definition.pollination_requirement), 0.15, 1.0);
    }
    if (random_unit() >= probability) {
        return;
    }

    const double dispersal =
        definition.seed_dispersal_radius > 0.0
            ? definition.seed_dispersal_radius
            : (definition.kind == EntityKind::plant ? 1.5 : 0.7);
    const Vec3 child_position = nearby_position(parent.position, dispersal);
    if (!habitat_.is_land(child_position)) {
        return;
    }

    double child_energy = -1.0;
    if (definition.kind == EntityKind::plant) {
        parent.biomass *= 0.88;
    } else {
        const double investment = definition.max_energy * definition.reproduction_energy_fraction;
        parent.energy -= investment;
        child_energy = std::max(definition.initial_energy * 0.5, investment);
    }
    parent.reproduction_cooldown_hours = definition.reproduction_interval_hours;
    static_cast<void>(enqueue_organism(definition.id, child_position, child_energy, 0.0));
}

std::optional<std::size_t> World::nearest_food(
    std::size_t consumer_index,
    const SpeciesDefinition& definition) const {
    const double limit_squared = definition.perception_radius * definition.perception_radius;
    double best_distance = std::numeric_limits<double>::infinity();
    std::optional<std::size_t> best;

    const HabitatConfig& habitat_config = habitat_.config();
    std::size_t center_x = 0;
    std::size_t center_z = 0;
    if (spatial_positions_.size() != entities_.size() ||
        !habitat_.coordinates(spatial_positions_[consumer_index], center_x, center_z) ||
        spatial_buckets_.size() != habitat_.size()) {
        return std::nullopt;
    }
    const auto radius_cells = static_cast<std::size_t>(
        std::ceil(definition.perception_radius / habitat_config.cell_size));
    const std::size_t min_x = center_x > radius_cells ? center_x - radius_cells : 0;
    const std::size_t min_z = center_z > radius_cells ? center_z - radius_cells : 0;
    const std::size_t max_x = std::min(habitat_config.width - 1, center_x + radius_cells);
    const std::size_t max_z = std::min(habitat_config.height - 1, center_z + radius_cells);

    for (std::size_t z = min_z; z <= max_z; ++z) {
        for (std::size_t x = min_x; x <= max_x; ++x) {
            for (const std::size_t i : spatial_buckets_[z * habitat_config.width + x]) {
                if (i == consumer_index || !is_food(definition, entities_[i])) {
                    continue;
                }
                const double distance =
                    horizontal_distance_squared(spatial_positions_[consumer_index],
                                                spatial_positions_[i]);
                if (distance <= limit_squared && distance < best_distance) {
                    best_distance = distance;
                    best = i;
                }
            }
        }
    }
    return best;
}

std::optional<std::size_t> World::current_or_nearest_food(
    std::size_t consumer_index,
    const SpeciesDefinition& definition) const {
    const Entity& consumer = entities_[consumer_index];
    if (consumer.target_id != 0) {
        const auto current = index_.find(consumer.target_id);
        if (current != index_.end() && current->second != consumer_index &&
            is_food(definition, entities_[current->second])) {
            const double retention_radius = definition.perception_radius * 1.35;
            if (spatial_positions_.size() == entities_.size() &&
                horizontal_distance_squared(spatial_positions_[consumer_index],
                                            spatial_positions_[current->second]) <=
                retention_radius * retention_radius) {
                return current->second;
            }
        }
    }
    return nearest_food(consumer_index, definition);
}

std::optional<std::size_t> World::nearest_threat(
    std::size_t prey_index,
    const SpeciesDefinition& definition) const {
    if (definition.flee_radius <= 0.0 || spatial_buckets_.size() != habitat_.size() ||
        spatial_positions_.size() != entities_.size()) {
        return std::nullopt;
    }
    const Entity& prey = entities_[prey_index];
    const HabitatConfig& habitat_config = habitat_.config();
    std::size_t center_x = 0;
    std::size_t center_z = 0;
    if (!habitat_.coordinates(spatial_positions_[prey_index], center_x, center_z)) {
        return std::nullopt;
    }

    const auto radius_cells = static_cast<std::size_t>(
        std::ceil(definition.flee_radius / habitat_config.cell_size));
    const std::size_t min_x = center_x > radius_cells ? center_x - radius_cells : 0;
    const std::size_t min_z = center_z > radius_cells ? center_z - radius_cells : 0;
    const std::size_t max_x = std::min(habitat_config.width - 1, center_x + radius_cells);
    const std::size_t max_z = std::min(habitat_config.height - 1, center_z + radius_cells);
    const double limit_squared = definition.flee_radius * definition.flee_radius;
    double best_distance = std::numeric_limits<double>::infinity();
    std::optional<std::size_t> best;

    for (std::size_t z = min_z; z <= max_z; ++z) {
        for (std::size_t x = min_x; x <= max_x; ++x) {
            for (const std::size_t candidate_index :
                 spatial_buckets_[z * habitat_config.width + x]) {
                if (candidate_index == prey_index) {
                    continue;
                }
                const Entity& candidate = entities_[candidate_index];
                const SpeciesDefinition* predator = species_.find(candidate.species_id);
                if (predator == nullptr ||
                    std::ranges::find(predator->food_species, prey.species_id) ==
                        predator->food_species.end()) {
                    continue;
                }
                const double distance =
                    horizontal_distance_squared(spatial_positions_[prey_index],
                                                spatial_positions_[candidate_index]);
                const bool actively_hunting =
                    candidate.target_id == prey.id &&
                    (candidate.intent == BehaviorIntent::foraging ||
                     candidate.intent == BehaviorIntent::feeding);
                const double immediate_danger =
                    std::max(predator->interaction_radius * 2.0,
                             habitat_config.cell_size * 0.75);
                if (!actively_hunting && distance > immediate_danger * immediate_danger) {
                    continue;
                }
                if (distance <= limit_squared && distance < best_distance) {
                    best_distance = distance;
                    best = candidate_index;
                }
            }
        }
    }
    return best;
}

std::optional<Vec3> World::local_group_center(
    std::size_t member_index,
    const SpeciesDefinition& definition) const {
    if (definition.social_radius <= 0.0 || spatial_buckets_.size() != habitat_.size() ||
        spatial_positions_.size() != entities_.size()) {
        return std::nullopt;
    }
    const Entity& member = entities_[member_index];
    const HabitatConfig& habitat_config = habitat_.config();
    std::size_t center_x = 0;
    std::size_t center_z = 0;
    if (!habitat_.coordinates(spatial_positions_[member_index], center_x, center_z)) {
        return std::nullopt;
    }

    const auto radius_cells = static_cast<std::size_t>(
        std::ceil(definition.social_radius / habitat_config.cell_size));
    const std::size_t min_x = center_x > radius_cells ? center_x - radius_cells : 0;
    const std::size_t min_z = center_z > radius_cells ? center_z - radius_cells : 0;
    const std::size_t max_x = std::min(habitat_config.width - 1, center_x + radius_cells);
    const std::size_t max_z = std::min(habitat_config.height - 1, center_z + radius_cells);
    const double limit_squared = definition.social_radius * definition.social_radius;
    Vec3 center = spatial_positions_[member_index];
    std::size_t count = 1;

    for (std::size_t z = min_z; z <= max_z; ++z) {
        for (std::size_t x = min_x; x <= max_x; ++x) {
            for (const std::size_t candidate_index :
                 spatial_buckets_[z * habitat_config.width + x]) {
                if (candidate_index == member_index) {
                    continue;
                }
                const Entity& candidate = entities_[candidate_index];
                if (candidate.species_id != member.species_id ||
                    horizontal_distance_squared(spatial_positions_[member_index],
                                                spatial_positions_[candidate_index]) >
                        limit_squared) {
                    continue;
                }
                center = center + spatial_positions_[candidate_index];
                ++count;
            }
        }
    }
    return count > 1 ? std::optional<Vec3>{center * (1.0 / static_cast<double>(count))}
                     : std::nullopt;
}

bool World::is_food(const SpeciesDefinition& consumer, const Entity& candidate) const noexcept {
    if (std::ranges::find(consumer.food_species, candidate.species_id) ==
        consumer.food_species.end()) {
        return false;
    }
    const SpeciesDefinition* candidate_definition = species_.find(candidate.species_id);
    const double retained_biomass =
        candidate.kind == EntityKind::plant && candidate_definition != nullptr
            ? candidate_definition->initial_biomass * 0.12
            : 0.01;
    return candidate.biomass > retained_biomass + 1.0e-8;
}

std::optional<Vec3> World::nearest_drink(Vec3 position, double radius) const {
    const HabitatConfig& config = habitat_.config();
    if (config.width == 0 || config.height == 0) {
        return std::nullopt;
    }
    const double radius_squared = radius * radius;
    std::size_t center_x = 0;
    std::size_t center_z = 0;
    if (!habitat_.coordinates(position, center_x, center_z)) {
        return std::nullopt;
    }
    const auto radius_cells = static_cast<std::size_t>(
        std::ceil(radius / std::max(1.0e-6, config.cell_size)));
    const std::size_t min_x = center_x > radius_cells ? center_x - radius_cells : 0;
    const std::size_t min_z = center_z > radius_cells ? center_z - radius_cells : 0;
    const std::size_t max_x = std::min(config.width - 1, center_x + radius_cells);
    const std::size_t max_z = std::min(config.height - 1, center_z + radius_cells);

    double best_score = -std::numeric_limits<double>::infinity();
    std::optional<Vec3> best;
    for (std::size_t z = min_z; z <= max_z; ++z) {
        for (std::size_t x = min_x; x <= max_x; ++x) {
            const HabitatCell& cell = habitat_.cell(x, z);
            if (cell.water || (!cell.fresh_water && cell.moisture < 0.92)) {
                continue;
            }
            const Vec3 candidate = habitat_.cell_center(x, z);
            const double distance = horizontal_distance_squared(position, candidate);
            if (distance > radius_squared) {
                continue;
            }
            const double score = cell.moisture * 4.0 + (cell.fresh_water ? 2.0 : 0.0) -
                                 std::sqrt(distance) / std::max(0.1, radius);
            if (score > best_score) {
                best_score = score;
                best = candidate;
            }
        }
    }
    return best;
}

std::optional<Vec3> World::nearest_organic(Vec3 position, double radius) const {
    const HabitatConfig& config = habitat_.config();
    if (config.width == 0 || config.height == 0) {
        return std::nullopt;
    }
    const double radius_squared = radius * radius;
    std::size_t center_x = 0;
    std::size_t center_z = 0;
    if (!habitat_.coordinates(position, center_x, center_z)) {
        return std::nullopt;
    }
    const auto radius_cells = static_cast<std::size_t>(
        std::ceil(radius / std::max(1.0e-6, config.cell_size)));
    const std::size_t min_x = center_x > radius_cells ? center_x - radius_cells : 0;
    const std::size_t min_z = center_z > radius_cells ? center_z - radius_cells : 0;
    const std::size_t max_x = std::min(config.width - 1, center_x + radius_cells);
    const std::size_t max_z = std::min(config.height - 1, center_z + radius_cells);

    double best_score = -std::numeric_limits<double>::infinity();
    std::optional<Vec3> best;
    for (std::size_t z = min_z; z <= max_z; ++z) {
        for (std::size_t x = min_x; x <= max_x; ++x) {
            const HabitatCell& cell = habitat_.cell(x, z);
            if (cell.water || cell.organic <= 0.015) {
                continue;
            }
            const Vec3 candidate = habitat_.cell_center(x, z);
            const double distance = horizontal_distance_squared(position, candidate);
            if (distance > radius_squared) {
                continue;
            }
            const double score =
                cell.organic * 5.0 - std::sqrt(distance) / std::max(0.1, radius);
            if (score > best_score) {
                best_score = score;
                best = candidate;
            }
        }
    }
    return best;
}

double World::random_unit() noexcept {
    random_state_ ^= random_state_ >> 12U;
    random_state_ ^= random_state_ << 25U;
    random_state_ ^= random_state_ >> 27U;
    const std::uint64_t value = random_state_ * 2685821657736338717ULL;
    return static_cast<double>(value >> 11U) * (1.0 / 9007199254740992.0);
}

void World::move_towards(Entity& entity,
                         Vec3 target,
                         const SpeciesDefinition& definition,
                         double hours,
                         double activity,
                         double stop_radius,
                         double speed_multiplier) {
    const double distance = std::sqrt(horizontal_distance_squared(entity.position, target));
    const double halt = std::max(0.0, stop_radius);
    if (distance <= halt) {
        entity.velocity = {};
        return;
    }
    const double step = definition.movement_per_hour * std::max(activity, 0.05) *
                        std::max(0.0, speed_multiplier) * hours;
    const double travel = std::min(step, std::max(0.0, distance - halt * 0.5));
    const Vec3 direction = horizontal_direction(entity.position, target);
    entity.velocity = hours > 1.0e-12 ? direction * (travel / hours) : direction;
    entity.position = entity.position + direction * travel;
}

void World::begin_rest(Entity& entity, const SpeciesDefinition& definition) {
    entity.intent = BehaviorIntent::resting;
    entity.target_id = 0;
    entity.movement_target = entity.position;
    entity.velocity = {};
    entity.behavior_timer_hours =
        definition.rest_duration_hours * (0.65 + random_unit() * 0.7);
}

void World::choose_roaming_target(std::size_t entity_index,
                                  Entity& entity,
                                  const SpeciesDefinition& definition) {
    Vec3 anchor = entity.home_position;
    entity.intent = BehaviorIntent::roaming;
    if (const std::optional<Vec3> group = local_group_center(entity_index, definition);
        group.has_value()) {
        const double social = std::clamp(definition.social_weight, 0.0, 1.0);
        anchor = entity.home_position * (1.0 - social) + *group * social;
        entity.intent = BehaviorIntent::socializing;
    }

    const double home_distance =
        std::sqrt(horizontal_distance_squared(entity.position, entity.home_position));
    if (home_distance > definition.home_range_radius * 1.15) {
        entity.movement_target = entity.home_position;
    } else {
        entity.movement_target = anchor;
        const double roam_radius = std::max(
            habitat_.config().cell_size,
            definition.home_range_radius * (entity.intent == BehaviorIntent::socializing ? 0.32
                                                                                         : 0.55));
        for (int attempt = 0; attempt < 12; ++attempt) {
            const Vec3 candidate = nearby_position(anchor, roam_radius);
            if (habitat_.is_land(candidate)) {
                entity.movement_target = candidate;
                break;
            }
        }
    }
    entity.behavior_timer_hours =
        definition.decision_interval_hours * (0.7 + random_unit() * 0.6);
}

Vec3 World::nearby_position(Vec3 center, double radius) noexcept {
    const double angle = random_unit() * 2.0 * std::numbers::pi;
    const double distance = std::sqrt(random_unit()) * radius;
    return {center.x + std::cos(angle) * distance, center.y, center.z + std::sin(angle) * distance};
}

void World::constrain_to_habitat(Entity& entity, Vec3 previous_position) {
    entity.position.x = std::clamp(entity.position.x, config_.bounds_min.x, config_.bounds_max.x);
    entity.position.z = std::clamp(entity.position.z, config_.bounds_min.z, config_.bounds_max.z);
    if (!habitat_.is_land(entity.position)) {
        entity.position = previous_position;
        entity.velocity = entity.velocity * -0.35;
    }
    if (entity.kind == EntityKind::generic) {
        return;
    }
    const SpeciesDefinition* definition = species_.find(entity.species_id);
    entity.position.y = definition != nullptr ? cruise_height(*definition) : 0.35;
}

void World::remove_dead() {
    for (std::size_t i = entities_.size(); i-- > 0;) {
        const Entity& entity = entities_[i];
        if (entity.kind == EntityKind::generic) {
            continue;
        }
        const SpeciesDefinition* definition = species_.find(entity.species_id);
        const bool expired = definition == nullptr || entity.age_hours >= definition->lifespan_hours;
        const bool starved = is_animal(entity.kind) && (entity.energy <= 0.0 || entity.hydration <= 0.0);
        const double minimum_biomass =
            definition == nullptr ? 0.0 : std::max(1.0e-8, definition->initial_biomass * 0.05);
        if (expired || starved || entity.biomass <= minimum_biomass) {
            const bool energy_gone = is_animal(entity.kind) && entity.energy <= 0.0;
            const bool water_gone = is_animal(entity.kind) && entity.hydration <= 0.0;
            record_death(entity.species_id, expired, energy_gone, water_gone);
            if (HabitatCell* cell = habitat_.cell_at(entity.position);
                cell != nullptr && !cell->water) {
                cell->organic =
                    std::clamp(cell->organic + entity.biomass * 0.0015, 0.0, 1.0);
                cell->nutrients =
                    std::clamp(cell->nutrients + entity.biomass * 0.00025, 0.0, 1.0);
            }
            remove_at(i);
        }
    }
}

void World::remove_at(std::size_t index) {
    const EntityId removed_id = entities_[index].id;
    const SpeciesId removed_species = entities_[index].species_id;
    const std::size_t last = entities_.size() - 1;
    if (index != last) {
        entities_[index] = entities_[last];
        index_[entities_[index].id] = index;
    }
    entities_.pop_back();
    index_.erase(removed_id);
    if (removed_species != 0) {
        const auto count = species_counts_.find(removed_species);
        if (count != species_counts_.end() && --count->second == 0) {
            species_counts_.erase(count);
        }
    }
}

double World::activity_factor(const SpeciesDefinition& definition,
                              const HabitatCell* cell) const noexcept {
    double temperature_activity = 1.0;
    if (definition.activity_min_temperature > -50.0) {
        const double temperature = cell != nullptr ? cell->temperature : 17.0;
        if (temperature <= definition.activity_min_temperature) {
            temperature_activity = 0.08;
        } else if (temperature < definition.activity_min_temperature + 7.0) {
            temperature_activity = std::clamp(
                (temperature - definition.activity_min_temperature) / 7.0, 0.08, 1.0);
        }
    }
    return temperature_activity * circadian_activity(definition);
}

double World::circadian_activity(const SpeciesDefinition& definition) const noexcept {
    const double active_hours = std::clamp(definition.active_hours_per_day, 0.5, 24.0);
    if (active_hours >= 23.9) {
        return 1.0;
    }
    const double peak = std::fmod(std::max(0.0, definition.activity_peak_hour), 24.0);
    const double difference = std::abs(hour_of_day() - peak);
    const double distance = std::min(difference, 24.0 - difference);
    const double half_window = active_hours * 0.5;
    const double edge = std::min(1.5, half_window * 0.45);
    if (distance <= half_window - edge) {
        return 1.0;
    }
    if (distance >= half_window + edge) {
        return 0.04;
    }
    const double t =
        std::clamp((half_window + edge - distance) / std::max(0.001, edge * 2.0), 0.0, 1.0);
    const double smooth = t * t * (3.0 - 2.0 * t);
    return std::lerp(0.04, 1.0, smooth);
}

double World::cruise_height(const SpeciesDefinition& definition) const noexcept {
    if (definition.cruise_height > 0.0) {
        return definition.cruise_height;
    }
    return definition.kind == EntityKind::insect ? 1.0 : 0.35;
}

double World::forage_threshold(const SpeciesDefinition& definition) const noexcept {
    const double fraction =
        definition.forage_energy_fraction > 0.0 ? definition.forage_energy_fraction : 0.78;
    return definition.max_energy * std::clamp(fraction, 0.2, 0.98);
}

void World::record_death(SpeciesId species_id, bool expired, bool starved, bool dehydrated) {
    if (expired) {
        ++deaths_age_;
        ++deaths_age_by_species_[species_id];
    } else if (starved) {
        ++deaths_starvation_;
        ++deaths_starvation_by_species_[species_id];
    } else if (dehydrated) {
        ++deaths_dehydration_;
        ++deaths_dehydration_by_species_[species_id];
    } else {
        ++deaths_biomass_loss_;
        ++deaths_biomass_by_species_[species_id];
    }
}

} // namespace sim
