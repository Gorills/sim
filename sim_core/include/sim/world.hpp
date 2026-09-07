#pragma once

#include "sim/command.hpp"
#include "sim/habitat.hpp"
#include "sim/snapshot.hpp"
#include "sim/species.hpp"
#include "sim/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sim {

struct WorldConfig {
    double tick_dt = 1.0 / 60.0;
    double ecology_hours_per_tick = 1.0 / 60.0;
    Vec3 bounds_min{-24.0, 0.0, -24.0};
    Vec3 bounds_max{24.0, 8.0, 24.0};
    HabitatConfig habitat{};
    ClimateConfig climate{};
    double climate_start_hour = 24.0 * 120.0 + 8.0;
    std::uint64_t seed = 42;
    std::size_t max_entities = 100'000;
};

struct SpeciesPopulation {
    SpeciesId species_id = 0;
    std::size_t count = 0;
    double biomass = 0.0;
    double mean_energy_fraction = 0.0;
    double mean_hydration = 0.0;
    std::uint64_t deaths_age = 0;
    std::uint64_t deaths_starvation = 0;
    std::uint64_t deaths_dehydration = 0;
    std::uint64_t deaths_biomass_loss = 0;
};

struct EcosystemStats {
    double simulated_hours = 0.0;
    double year_phase = 0.0;
    double mean_moisture = 0.0;
    double mean_temperature = 0.0;
    double mean_canopy = 0.0;
    double mean_light = 0.0;
    double mean_organic = 0.0;
    double mean_pollination = 0.0;
    double max_canopy = 0.0;
    double max_organic = 0.0;
    double max_pollination = 0.0;
    double mean_insect_activity = 0.0;
    std::size_t plants = 0;
    std::size_t herbivores = 0;
    std::size_t omnivores = 0;
    std::size_t carnivores = 0;
    std::size_t insects = 0;
    std::size_t decomposers = 0;
    std::size_t resting = 0;
    std::size_t roaming = 0;
    std::size_t socializing = 0;
    std::size_t foraging = 0;
    std::size_t feeding = 0;
    std::size_t drinking = 0;
    std::size_t fleeing = 0;
    std::uint64_t deaths_age = 0;
    std::uint64_t deaths_starvation = 0;
    std::uint64_t deaths_dehydration = 0;
    std::uint64_t deaths_biomass_loss = 0;
    std::vector<SpeciesPopulation> populations{};
};

class World {
public:
    explicit World(WorldConfig config = {},
                   SpeciesCatalog species = SpeciesCatalog::temperate_island());

    [[nodiscard]] EntityId enqueue_spawn(Vec3 position, Vec3 velocity);
    [[nodiscard]] EntityId enqueue_organism(SpeciesId species_id,
                                            Vec3 position,
                                            double energy = -1.0,
                                            double age_hours = 0.0);
    [[nodiscard]] EntityId enqueue_organism(std::string_view species_key,
                                            Vec3 position,
                                            double energy = -1.0,
                                            double age_hours = 0.0);
    void enqueue_despawn(EntityId id);

    void flush_commands();
    void tick();

    void set_paused(bool paused) noexcept;
    void set_tick_dt(double tick_dt);

    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] HabitatSnapshot habitat_snapshot() const;
    [[nodiscard]] std::uint64_t tick_index() const noexcept { return tick_index_; }
    [[nodiscard]] double tick_dt() const noexcept { return config_.tick_dt; }
    [[nodiscard]] bool paused() const noexcept { return paused_; }
    [[nodiscard]] std::size_t entity_count() const noexcept { return entities_.size(); }
    [[nodiscard]] std::size_t entity_count(SpeciesId species_id) const noexcept;
    [[nodiscard]] double simulated_hours() const noexcept { return simulated_hours_; }
    [[nodiscard]] double year_phase() const noexcept;
    [[nodiscard]] double hour_of_day() const noexcept;
    [[nodiscard]] const WorldConfig& config() const noexcept { return config_; }
    [[nodiscard]] const SpeciesCatalog& species() const noexcept { return species_; }
    [[nodiscard]] SpeciesCatalog& species() noexcept { return species_; }
    [[nodiscard]] const HabitatGrid& habitat() const noexcept { return habitat_; }
    [[nodiscard]] HabitatGrid& habitat() noexcept { return habitat_; }
    [[nodiscard]] EcosystemStats ecosystem_stats() const;

private:
    void apply_commands();
    void apply_one(const SpawnCommand& cmd);
    void apply_one(const SpawnOrganismCommand& cmd);
    void apply_one(const DespawnCommand& cmd);
    void integrate();
    void update_canopy_and_light();
    void update_plants(double hours);
    void update_animals(double hours);
    void rebuild_spatial_index();
    void update_one_animal(std::size_t index, double hours);
    void consume_target(Entity& consumer,
                        Entity& food,
                        const SpeciesDefinition& definition,
                        double hours,
                        double activity);
    void reproduce(Entity& parent, const SpeciesDefinition& definition, double hours);
    void record_death(SpeciesId species_id, bool expired, bool starved, bool dehydrated);
    [[nodiscard]] std::optional<std::size_t> nearest_food(
        std::size_t consumer_index,
        const SpeciesDefinition& definition) const;
    [[nodiscard]] std::optional<std::size_t> current_or_nearest_food(
        std::size_t consumer_index,
        const SpeciesDefinition& definition) const;
    [[nodiscard]] std::optional<std::size_t> nearest_threat(
        std::size_t prey_index,
        const SpeciesDefinition& definition) const;
    [[nodiscard]] std::optional<Vec3> local_group_center(
        std::size_t member_index,
        const SpeciesDefinition& definition) const;
    [[nodiscard]] bool is_food(const SpeciesDefinition& consumer,
                               const Entity& candidate) const noexcept;
    [[nodiscard]] std::optional<Vec3> nearest_drink(Vec3 position, double radius) const;
    [[nodiscard]] std::optional<Vec3> nearest_organic(Vec3 position, double radius) const;
    [[nodiscard]] double random_unit() noexcept;
    [[nodiscard]] Vec3 nearby_position(Vec3 center, double radius) noexcept;
    void move_towards(Entity& entity,
                      Vec3 target,
                      const SpeciesDefinition& definition,
                      double hours,
                      double activity,
                      double stop_radius,
                      double speed_multiplier = 1.0);
    void begin_rest(Entity& entity, const SpeciesDefinition& definition);
    void choose_roaming_target(std::size_t entity_index,
                               Entity& entity,
                               const SpeciesDefinition& definition);
    void constrain_to_habitat(Entity& entity, Vec3 previous_position);
    void remove_dead();
    void remove_at(std::size_t index);
    [[nodiscard]] double activity_factor(const SpeciesDefinition& definition,
                                         const HabitatCell* cell) const noexcept;
    [[nodiscard]] double circadian_activity(
        const SpeciesDefinition& definition) const noexcept;
    [[nodiscard]] double cruise_height(const SpeciesDefinition& definition) const noexcept;
    [[nodiscard]] double forage_threshold(const SpeciesDefinition& definition) const noexcept;
    [[nodiscard]] double absolute_hours() const noexcept;

    WorldConfig config_{};
    SpeciesCatalog species_{};
    HabitatGrid habitat_{};
    std::uint64_t tick_index_ = 0;
    double simulated_hours_ = 0.0;
    EntityId next_id_ = 1;
    std::uint64_t random_state_ = 1;
    std::uint64_t deaths_age_ = 0;
    std::uint64_t deaths_starvation_ = 0;
    std::uint64_t deaths_dehydration_ = 0;
    std::uint64_t deaths_biomass_loss_ = 0;
    std::unordered_map<SpeciesId, std::uint64_t> deaths_age_by_species_{};
    std::unordered_map<SpeciesId, std::uint64_t> deaths_starvation_by_species_{};
    std::unordered_map<SpeciesId, std::uint64_t> deaths_dehydration_by_species_{};
    std::unordered_map<SpeciesId, std::uint64_t> deaths_biomass_by_species_{};
    bool paused_ = false;
    std::vector<Entity> entities_{};
    std::unordered_map<EntityId, std::size_t> index_{};
    std::unordered_map<SpeciesId, std::size_t> species_counts_{};
    std::vector<std::vector<std::size_t>> spatial_buckets_{};
    std::vector<Vec3> spatial_positions_{};
    std::vector<Command> commands_{};
};

} // namespace sim
