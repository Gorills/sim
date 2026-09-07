#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace sim {

using SpeciesId = std::uint32_t;

enum class EntityKind : std::uint8_t {
    generic,
    plant,
    herbivore,
    carnivore,
    insect,
    omnivore,
};

struct SpeciesDefinition {
    SpeciesId id = 0;
    std::string key{};
    std::string display_name{};
    EntityKind kind = EntityKind::generic;
    std::vector<SpeciesId> food_species{};
    std::vector<std::string> tags{};

    double initial_biomass = 1.0;
    double max_biomass = 1.0;
    double food_energy_per_biomass = 1.0;

    double initial_energy = 1.0;
    double max_energy = 1.0;
    double metabolism_per_hour = 0.0;
    double dehydration_per_hour = 0.0;

    double movement_per_hour = 0.0;
    double perception_radius = 0.0;
    double interaction_radius = 0.25;
    double bite_biomass_per_hour = 0.0;
    double home_range_radius = 4.0;
    double decision_interval_hours = 2.0;
    double rest_duration_hours = 1.0;
    double social_radius = 0.0;
    double social_weight = 0.0;
    double flee_radius = 0.0;
    double flee_speed_multiplier = 1.35;
    double hunt_speed_multiplier = 1.0;
    double activity_peak_hour = 12.0;
    double active_hours_per_day = 24.0;
    std::size_t initial_group_size = 1;

    double growth_biomass_per_hour = 0.0;
    double stress_loss_fraction_per_hour = 0.00012;
    double maturity_hours = 0.0;
    double lifespan_hours = 24.0 * 365.0;
    double reproduction_interval_hours = 24.0;
    double reproduction_chance_per_hour = 0.0;
    double reproduction_energy_fraction = 0.25;
    double carrying_density_per_cell = 0.0;

    double preferred_moisture = 0.55;
    double moisture_tolerance = 0.45;
    double preferred_temperature = 20.0;
    double temperature_tolerance = 20.0;

    double canopy_contribution = 0.0;
    double shade_preference = 0.0;
    double organic_growth_factor = 0.0;
    double litter_organic_per_hour = 0.0;
    double organic_consumption_per_hour = 0.0;
    double pollination_deposit_per_hour = 0.0;
    double pollination_requirement = 0.0;
    double nectar_energy_per_hour = 0.0;
    double forage_energy_fraction = 0.78;
    double activity_min_temperature = -100.0;
    double seed_dispersal_radius = 0.0;
    double cruise_height = 0.0;
};

class SpeciesCatalog {
public:
    bool add(SpeciesDefinition definition);

    [[nodiscard]] const SpeciesDefinition* find(SpeciesId id) const noexcept;
    [[nodiscard]] const SpeciesDefinition* find(std::string_view key) const noexcept;
    [[nodiscard]] const std::vector<SpeciesDefinition>& all() const noexcept { return species_; }

    [[nodiscard]] static SpeciesCatalog temperate_island();

private:
    std::vector<SpeciesDefinition> species_{};
};

namespace species {
inline constexpr SpeciesId grass = 1;
inline constexpr SpeciesId clover = 2;
inline constexpr SpeciesId oak = 3;
inline constexpr SpeciesId birch = 4;
inline constexpr SpeciesId pine = 5;
inline constexpr SpeciesId berry_bush = 6;
inline constexpr SpeciesId fern = 7;
inline constexpr SpeciesId reeds = 8;
inline constexpr SpeciesId mushroom = 9;
inline constexpr SpeciesId rabbit = 10;
inline constexpr SpeciesId deer = 11;
inline constexpr SpeciesId mouse = 12;
inline constexpr SpeciesId hare = 13;
inline constexpr SpeciesId boar = 14;
inline constexpr SpeciesId wildflower = 15;
inline constexpr SpeciesId willow = 16;
inline constexpr SpeciesId vole = 17;
inline constexpr SpeciesId hedgehog = 18;
inline constexpr SpeciesId robin = 19;
inline constexpr SpeciesId wolf = 20;
inline constexpr SpeciesId fox = 21;
inline constexpr SpeciesId owl = 22;
inline constexpr SpeciesId frog = 23;
inline constexpr SpeciesId bee = 30;
inline constexpr SpeciesId butterfly = 31;
inline constexpr SpeciesId beetle = 32;
inline constexpr SpeciesId ant = 33;
inline constexpr SpeciesId bumblebee = 34;
inline constexpr SpeciesId moth = 35;
} // namespace species

[[nodiscard]] constexpr bool is_animal(EntityKind kind) noexcept {
    return kind == EntityKind::herbivore || kind == EntityKind::carnivore ||
           kind == EntityKind::insect || kind == EntityKind::omnivore;
}

[[nodiscard]] inline bool has_tag(const SpeciesDefinition& definition,
                                  std::string_view tag) noexcept {
    return std::ranges::find(definition.tags, tag) != definition.tags.end();
}

} // namespace sim
