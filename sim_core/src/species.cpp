#include "sim/species.hpp"

#include <algorithm>
#include <utility>

namespace sim {
namespace {

SpeciesDefinition base_plant(SpeciesId id, const char* key, const char* name) {
    SpeciesDefinition definition;
    definition.id = id;
    definition.key = key;
    definition.display_name = name;
    definition.kind = EntityKind::plant;
    definition.tags = {"plant"};
    definition.food_energy_per_biomass = 4.0;
    definition.stress_loss_fraction_per_hour = 0.00012;
    definition.cruise_height = 0.28;
    return definition;
}

SpeciesDefinition base_animal(SpeciesId id,
                              const char* key,
                              const char* name,
                              EntityKind kind) {
    SpeciesDefinition definition;
    definition.id = id;
    definition.key = key;
    definition.display_name = name;
    definition.kind = kind;
    definition.tags = {"animal"};
    definition.reproduction_energy_fraction = 0.28;
    definition.cruise_height = 0.35;
    definition.home_range_radius = 5.0;
    definition.decision_interval_hours = 2.0;
    definition.rest_duration_hours = 1.0;
    return definition;
}

void scale_spatial_parameters(SpeciesDefinition& definition) {
    if (is_animal(definition.kind)) {
        definition.movement_per_hour *= 220.0;
        definition.perception_radius *= 70.0;
        definition.home_range_radius =
            definition.home_range_radius * definition.home_range_radius * 17.0;
        definition.social_radius *= 80.0;
        definition.flee_radius *= 70.0;
    } else if (definition.kind == EntityKind::plant) {
        definition.seed_dispersal_radius *= 20.0;
    }
}

bool add_scaled_species(SpeciesCatalog& catalog, SpeciesDefinition definition) {
    scale_spatial_parameters(definition);
    return catalog.add(std::move(definition));
}

} // namespace

bool SpeciesCatalog::add(SpeciesDefinition definition) {
    if (definition.id == 0 || definition.key.empty() || find(definition.id) != nullptr ||
        find(definition.key) != nullptr) {
        return false;
    }
    species_.push_back(std::move(definition));
    std::ranges::sort(species_, {}, &SpeciesDefinition::id);
    return true;
}

const SpeciesDefinition* SpeciesCatalog::find(SpeciesId id) const noexcept {
    const auto it = std::ranges::lower_bound(species_, id, {}, &SpeciesDefinition::id);
    return it != species_.end() && it->id == id ? &*it : nullptr;
}

const SpeciesDefinition* SpeciesCatalog::find(std::string_view key) const noexcept {
    const auto it = std::ranges::find(species_, key, &SpeciesDefinition::key);
    return it != species_.end() ? &*it : nullptr;
}

SpeciesCatalog SpeciesCatalog::temperate_island() {
    SpeciesCatalog catalog;

    {
        SpeciesDefinition grass_def = base_plant(species::grass, "grass", "Grass");
        grass_def.tags = {"plant", "ground_cover"};
        grass_def.initial_biomass = 4.0;
        grass_def.max_biomass = 18.0;
        grass_def.food_energy_per_biomass = 5.0;
        grass_def.growth_biomass_per_hour = 0.07;
        grass_def.maturity_hours = 72.0;
        grass_def.lifespan_hours = 24.0 * 365.0;
        grass_def.reproduction_interval_hours = 24.0 * 7.0;
        grass_def.reproduction_chance_per_hour = 0.0010;
        grass_def.carrying_density_per_cell = 0.07;
        grass_def.preferred_moisture = 0.55;
        grass_def.moisture_tolerance = 0.42;
        grass_def.preferred_temperature = 18.0;
        grass_def.temperature_tolerance = 18.0;
        grass_def.shade_preference = -0.55;
        grass_def.litter_organic_per_hour = 0.00008;
        grass_def.seed_dispersal_radius = 1.2;
        add_scaled_species(catalog, std::move(grass_def));
    }

    {
        SpeciesDefinition clover_def = base_plant(species::clover, "clover", "Clover");
        clover_def.tags = {"plant", "ground_cover", "flowering", "nitrogen_fixer"};
        clover_def.initial_biomass = 2.0;
        clover_def.max_biomass = 10.0;
        clover_def.food_energy_per_biomass = 6.0;
        clover_def.growth_biomass_per_hour = 0.045;
        clover_def.maturity_hours = 24.0 * 14.0;
        clover_def.lifespan_hours = 24.0 * 365.0 * 2.0;
        clover_def.reproduction_interval_hours = 24.0 * 10.0;
        clover_def.reproduction_chance_per_hour = 0.0008;
        clover_def.carrying_density_per_cell = 0.028;
        clover_def.preferred_moisture = 0.62;
        clover_def.moisture_tolerance = 0.35;
        clover_def.preferred_temperature = 17.0;
        clover_def.temperature_tolerance = 15.0;
        clover_def.shade_preference = -0.35;
        clover_def.pollination_requirement = 0.18;
        clover_def.litter_organic_per_hour = 0.00005;
        clover_def.seed_dispersal_radius = 1.1;
        add_scaled_species(catalog, std::move(clover_def));
    }

    {
        SpeciesDefinition oak_def = base_plant(species::oak, "oak", "Oak");
        oak_def.tags = {"plant", "tree", "flowering", "canopy"};
        oak_def.initial_biomass = 10.0;
        oak_def.max_biomass = 520.0;
        oak_def.food_energy_per_biomass = 2.0;
        oak_def.growth_biomass_per_hour = 0.005;
        oak_def.stress_loss_fraction_per_hour = 0.000005;
        oak_def.maturity_hours = 24.0 * 365.0 * 8.0;
        oak_def.lifespan_hours = 24.0 * 365.0 * 400.0;
        oak_def.reproduction_interval_hours = 24.0 * 365.0;
        oak_def.reproduction_chance_per_hour = 0.000012;
        oak_def.carrying_density_per_cell = 0.0042;
        oak_def.preferred_moisture = 0.58;
        oak_def.moisture_tolerance = 0.32;
        oak_def.preferred_temperature = 16.0;
        oak_def.temperature_tolerance = 17.0;
        oak_def.canopy_contribution = 0.78;
        oak_def.shade_preference = 0.05;
        oak_def.pollination_requirement = 0.12;
        oak_def.litter_organic_per_hour = 0.00045;
        oak_def.seed_dispersal_radius = 2.4;
        oak_def.cruise_height = 0.55;
        add_scaled_species(catalog, std::move(oak_def));
    }

    {
        SpeciesDefinition birch_def = base_plant(species::birch, "birch", "Birch");
        birch_def.tags = {"plant", "tree", "flowering", "canopy"};
        birch_def.initial_biomass = 7.0;
        birch_def.max_biomass = 220.0;
        birch_def.food_energy_per_biomass = 2.2;
        birch_def.growth_biomass_per_hour = 0.009;
        birch_def.stress_loss_fraction_per_hour = 0.00001;
        birch_def.maturity_hours = 24.0 * 365.0 * 4.0;
        birch_def.lifespan_hours = 24.0 * 365.0 * 90.0;
        birch_def.reproduction_interval_hours = 24.0 * 280.0;
        birch_def.reproduction_chance_per_hour = 0.00003;
        birch_def.carrying_density_per_cell = 0.0038;
        birch_def.preferred_moisture = 0.60;
        birch_def.moisture_tolerance = 0.30;
        birch_def.preferred_temperature = 13.5;
        birch_def.temperature_tolerance = 16.0;
        birch_def.canopy_contribution = 0.62;
        birch_def.shade_preference = -0.1;
        birch_def.pollination_requirement = 0.10;
        birch_def.litter_organic_per_hour = 0.00032;
        birch_def.seed_dispersal_radius = 2.8;
        birch_def.cruise_height = 0.5;
        add_scaled_species(catalog, std::move(birch_def));
    }

    {
        SpeciesDefinition pine_def = base_plant(species::pine, "pine", "Pine");
        pine_def.tags = {"plant", "tree", "canopy"};
        pine_def.initial_biomass = 8.0;
        pine_def.max_biomass = 380.0;
        pine_def.food_energy_per_biomass = 1.6;
        pine_def.growth_biomass_per_hour = 0.0045;
        pine_def.stress_loss_fraction_per_hour = 0.000004;
        pine_def.maturity_hours = 24.0 * 365.0 * 10.0;
        pine_def.lifespan_hours = 24.0 * 365.0 * 250.0;
        pine_def.reproduction_interval_hours = 24.0 * 365.0;
        pine_def.reproduction_chance_per_hour = 0.00001;
        pine_def.carrying_density_per_cell = 0.0036;
        pine_def.preferred_moisture = 0.42;
        pine_def.moisture_tolerance = 0.34;
        pine_def.preferred_temperature = 12.0;
        pine_def.temperature_tolerance = 18.0;
        pine_def.canopy_contribution = 0.84;
        pine_def.shade_preference = 0.15;
        pine_def.litter_organic_per_hour = 0.00038;
        pine_def.seed_dispersal_radius = 3.0;
        pine_def.cruise_height = 0.58;
        add_scaled_species(catalog, std::move(pine_def));
    }

    {
        SpeciesDefinition berry_def =
            base_plant(species::berry_bush, "berry_bush", "Berry bush");
        berry_def.tags = {"plant", "shrub", "flowering", "fruit"};
        berry_def.initial_biomass = 3.5;
        berry_def.max_biomass = 28.0;
        berry_def.food_energy_per_biomass = 7.0;
        berry_def.growth_biomass_per_hour = 0.028;
        berry_def.maturity_hours = 24.0 * 80.0;
        berry_def.lifespan_hours = 24.0 * 365.0 * 12.0;
        berry_def.reproduction_interval_hours = 24.0 * 40.0;
        berry_def.reproduction_chance_per_hour = 0.00045;
        berry_def.carrying_density_per_cell = 0.012;
        berry_def.preferred_moisture = 0.57;
        berry_def.moisture_tolerance = 0.30;
        berry_def.preferred_temperature = 16.0;
        berry_def.temperature_tolerance = 14.0;
        berry_def.canopy_contribution = 0.18;
        berry_def.shade_preference = 0.15;
        berry_def.pollination_requirement = 0.22;
        berry_def.litter_organic_per_hour = 0.00012;
        berry_def.seed_dispersal_radius = 1.6;
        berry_def.cruise_height = 0.38;
        add_scaled_species(catalog, std::move(berry_def));
    }

    {
        SpeciesDefinition fern_def = base_plant(species::fern, "fern", "Fern");
        fern_def.tags = {"plant", "ground_cover", "shade"};
        fern_def.initial_biomass = 1.8;
        fern_def.max_biomass = 9.0;
        fern_def.food_energy_per_biomass = 3.5;
        fern_def.growth_biomass_per_hour = 0.032;
        fern_def.maturity_hours = 24.0 * 40.0;
        fern_def.lifespan_hours = 24.0 * 365.0 * 6.0;
        fern_def.reproduction_interval_hours = 24.0 * 18.0;
        fern_def.reproduction_chance_per_hour = 0.0007;
        fern_def.carrying_density_per_cell = 0.018;
        fern_def.preferred_moisture = 0.68;
        fern_def.moisture_tolerance = 0.28;
        fern_def.preferred_temperature = 15.0;
        fern_def.temperature_tolerance = 12.0;
        fern_def.shade_preference = 0.72;
        fern_def.litter_organic_per_hour = 0.00007;
        fern_def.seed_dispersal_radius = 1.0;
        add_scaled_species(catalog, std::move(fern_def));
    }

    {
        SpeciesDefinition reeds_def = base_plant(species::reeds, "reeds", "Reeds");
        reeds_def.tags = {"plant", "wetland", "ground_cover"};
        reeds_def.initial_biomass = 3.0;
        reeds_def.max_biomass = 16.0;
        reeds_def.food_energy_per_biomass = 3.2;
        reeds_def.growth_biomass_per_hour = 0.055;
        reeds_def.maturity_hours = 24.0 * 30.0;
        reeds_def.lifespan_hours = 24.0 * 365.0 * 4.0;
        reeds_def.reproduction_interval_hours = 24.0 * 12.0;
        reeds_def.reproduction_chance_per_hour = 0.0009;
        reeds_def.carrying_density_per_cell = 0.01;
        reeds_def.preferred_moisture = 0.90;
        reeds_def.moisture_tolerance = 0.22;
        reeds_def.preferred_temperature = 16.0;
        reeds_def.temperature_tolerance = 14.0;
        reeds_def.shade_preference = -0.25;
        reeds_def.litter_organic_per_hour = 0.00016;
        reeds_def.seed_dispersal_radius = 1.8;
        reeds_def.cruise_height = 0.42;
        add_scaled_species(catalog, std::move(reeds_def));
    }

    {
        SpeciesDefinition mushroom_def = base_plant(species::mushroom, "mushroom", "Mushroom");
        mushroom_def.tags = {"plant", "fungus", "decomposer", "shade"};
        mushroom_def.initial_biomass = 0.8;
        mushroom_def.max_biomass = 4.5;
        mushroom_def.food_energy_per_biomass = 5.5;
        mushroom_def.growth_biomass_per_hour = 0.04;
        mushroom_def.stress_loss_fraction_per_hour = 0.0002;
        mushroom_def.maturity_hours = 24.0 * 8.0;
        mushroom_def.lifespan_hours = 24.0 * 365.0;
        mushroom_def.reproduction_interval_hours = 24.0 * 6.0;
        mushroom_def.reproduction_chance_per_hour = 0.0014;
        mushroom_def.carrying_density_per_cell = 0.012;
        mushroom_def.preferred_moisture = 0.70;
        mushroom_def.moisture_tolerance = 0.30;
        mushroom_def.preferred_temperature = 14.0;
        mushroom_def.temperature_tolerance = 12.0;
        mushroom_def.shade_preference = 0.55;
        mushroom_def.organic_growth_factor = 1.0;
        mushroom_def.seed_dispersal_radius = 0.9;
        mushroom_def.cruise_height = 0.16;
        add_scaled_species(catalog, std::move(mushroom_def));
    }

    {
        SpeciesDefinition rabbit_def =
            base_animal(species::rabbit, "rabbit", "Rabbit", EntityKind::herbivore);
        rabbit_def.food_species = {species::grass, species::clover, species::berry_bush};
        rabbit_def.tags = {"animal", "herbivore", "prey"};
        rabbit_def.initial_biomass = 2.0;
        rabbit_def.max_biomass = 2.6;
        rabbit_def.food_energy_per_biomass = 30.0;
        rabbit_def.initial_energy = 13.0;
        rabbit_def.max_energy = 20.0;
        rabbit_def.metabolism_per_hour = 0.02;
        rabbit_def.dehydration_per_hour = 0.002;
        rabbit_def.movement_per_hour = 1.3;
        rabbit_def.perception_radius = 5.5;
        rabbit_def.interaction_radius = 0.35;
        rabbit_def.bite_biomass_per_hour = 0.09;
        rabbit_def.maturity_hours = 24.0 * 90.0;
        rabbit_def.lifespan_hours = 24.0 * 365.0 * 5.0;
        rabbit_def.reproduction_interval_hours = 24.0 * 28.0;
        rabbit_def.reproduction_chance_per_hour = 0.0016;
        rabbit_def.forage_energy_fraction = 0.76;
        rabbit_def.reproduction_energy_fraction = 0.30;
        rabbit_def.carrying_density_per_cell = 0.009;
        rabbit_def.social_radius = 4.0;
        rabbit_def.social_weight = 0.55;
        rabbit_def.flee_radius = 7.0;
        rabbit_def.flee_speed_multiplier = 1.28;
        rabbit_def.activity_peak_hour = 19.0;
        rabbit_def.active_hours_per_day = 14.0;
        rabbit_def.initial_group_size = 6;
        add_scaled_species(catalog, std::move(rabbit_def));
    }

    {
        SpeciesDefinition deer_def =
            base_animal(species::deer, "deer", "Deer", EntityKind::herbivore);
        deer_def.food_species = {species::grass, species::clover, species::fern,
                                 species::berry_bush};
        deer_def.tags = {"animal", "herbivore", "prey"};
        deer_def.initial_biomass = 75.0;
        deer_def.max_biomass = 110.0;
        deer_def.food_energy_per_biomass = 24.0;
        deer_def.initial_energy = 160.0;
        deer_def.max_energy = 240.0;
        deer_def.metabolism_per_hour = 0.05;
        deer_def.dehydration_per_hour = 0.002;
        deer_def.movement_per_hour = 0.75;
        deer_def.perception_radius = 6.0;
        deer_def.interaction_radius = 0.6;
        deer_def.bite_biomass_per_hour = 0.15;
        deer_def.maturity_hours = 24.0 * 365.0 * 2.0;
        deer_def.lifespan_hours = 24.0 * 365.0 * 18.0;
        deer_def.reproduction_interval_hours = 24.0 * 365.0;
        deer_def.reproduction_chance_per_hour = 0.00008;
        deer_def.carrying_density_per_cell = 0.0018;
        deer_def.cruise_height = 0.55;
        deer_def.home_range_radius = 10.0;
        deer_def.decision_interval_hours = 3.5;
        deer_def.rest_duration_hours = 1.8;
        deer_def.social_radius = 8.0;
        deer_def.social_weight = 0.72;
        deer_def.flee_radius = 11.0;
        deer_def.flee_speed_multiplier = 1.45;
        deer_def.activity_peak_hour = 7.0;
        deer_def.active_hours_per_day = 16.0;
        deer_def.initial_group_size = 7;
        add_scaled_species(catalog, std::move(deer_def));
    }

    {
        SpeciesDefinition mouse_def =
            base_animal(species::mouse, "mouse", "Mouse", EntityKind::herbivore);
        mouse_def.food_species = {species::grass, species::mushroom, species::berry_bush};
        mouse_def.tags = {"animal", "herbivore", "prey"};
        mouse_def.initial_biomass = 0.25;
        mouse_def.max_biomass = 0.35;
        mouse_def.food_energy_per_biomass = 28.0;
        mouse_def.initial_energy = 1.8;
        mouse_def.max_energy = 2.6;
        mouse_def.metabolism_per_hour = 0.0038;
        mouse_def.dehydration_per_hour = 0.0024;
        mouse_def.movement_per_hour = 1.6;
        mouse_def.perception_radius = 5.0;
        mouse_def.interaction_radius = 0.22;
        mouse_def.bite_biomass_per_hour = 0.03;
        mouse_def.maturity_hours = 24.0 * 28.0;
        mouse_def.lifespan_hours = 24.0 * 365.0 * 2.0;
        mouse_def.reproduction_interval_hours = 24.0 * 12.0;
        mouse_def.reproduction_chance_per_hour = 0.0030;
        mouse_def.reproduction_energy_fraction = 0.20;
        mouse_def.carrying_density_per_cell = 0.018;
        mouse_def.forage_energy_fraction = 0.78;
        mouse_def.cruise_height = 0.18;
        mouse_def.home_range_radius = 3.0;
        mouse_def.decision_interval_hours = 1.0;
        mouse_def.rest_duration_hours = 0.7;
        mouse_def.social_radius = 3.0;
        mouse_def.social_weight = 0.45;
        mouse_def.flee_radius = 6.0;
        mouse_def.flee_speed_multiplier = 1.25;
        mouse_def.activity_peak_hour = 23.0;
        mouse_def.active_hours_per_day = 12.0;
        mouse_def.initial_group_size = 8;
        add_scaled_species(catalog, std::move(mouse_def));
    }

    {
        SpeciesDefinition hare_def =
            base_animal(species::hare, "hare", "Hare", EntityKind::herbivore);
        hare_def.food_species = {species::grass, species::clover, species::fern,
                                 species::reeds};
        hare_def.tags = {"animal", "herbivore", "prey"};
        hare_def.initial_biomass = 3.4;
        hare_def.max_biomass = 4.6;
        hare_def.food_energy_per_biomass = 26.0;
        hare_def.initial_energy = 16.0;
        hare_def.max_energy = 24.0;
        hare_def.metabolism_per_hour = 0.022;
        hare_def.dehydration_per_hour = 0.002;
        hare_def.movement_per_hour = 1.55;
        hare_def.perception_radius = 6.0;
        hare_def.interaction_radius = 0.4;
        hare_def.bite_biomass_per_hour = 0.10;
        hare_def.maturity_hours = 24.0 * 100.0;
        hare_def.lifespan_hours = 24.0 * 365.0 * 6.0;
        hare_def.reproduction_interval_hours = 24.0 * 36.0;
        hare_def.reproduction_chance_per_hour = 0.0010;
        hare_def.forage_energy_fraction = 0.75;
        hare_def.reproduction_energy_fraction = 0.28;
        hare_def.carrying_density_per_cell = 0.006;
        hare_def.cruise_height = 0.38;
        hare_def.home_range_radius = 7.0;
        hare_def.social_radius = 5.0;
        hare_def.social_weight = 0.35;
        hare_def.flee_radius = 9.0;
        hare_def.flee_speed_multiplier = 1.3;
        hare_def.activity_peak_hour = 19.0;
        hare_def.active_hours_per_day = 13.0;
        hare_def.initial_group_size = 4;
        add_scaled_species(catalog, std::move(hare_def));
    }

    {
        SpeciesDefinition boar_def =
            base_animal(species::boar, "boar", "Boar", EntityKind::omnivore);
        boar_def.food_species = {species::grass, species::berry_bush, species::mushroom,
                                 species::reeds};
        boar_def.tags = {"animal", "omnivore", "prey"};
        boar_def.initial_biomass = 55.0;
        boar_def.max_biomass = 85.0;
        boar_def.food_energy_per_biomass = 20.0;
        boar_def.initial_energy = 130.0;
        boar_def.max_energy = 200.0;
        boar_def.metabolism_per_hour = 0.045;
        boar_def.dehydration_per_hour = 0.0025;
        boar_def.movement_per_hour = 0.7;
        boar_def.perception_radius = 5.5;
        boar_def.interaction_radius = 0.7;
        boar_def.bite_biomass_per_hour = 0.18;
        boar_def.maturity_hours = 24.0 * 365.0 * 1.5;
        boar_def.lifespan_hours = 24.0 * 365.0 * 12.0;
        boar_def.reproduction_interval_hours = 24.0 * 280.0;
        boar_def.reproduction_chance_per_hour = 0.00009;
        boar_def.reproduction_energy_fraction = 0.32;
        boar_def.carrying_density_per_cell = 0.0014;
        boar_def.cruise_height = 0.48;
        boar_def.home_range_radius = 9.0;
        boar_def.decision_interval_hours = 3.0;
        boar_def.rest_duration_hours = 1.6;
        boar_def.social_radius = 7.0;
        boar_def.social_weight = 0.62;
        boar_def.activity_peak_hour = 18.0;
        boar_def.active_hours_per_day = 15.0;
        boar_def.initial_group_size = 6;
        add_scaled_species(catalog, std::move(boar_def));
    }

    {
        SpeciesDefinition wolf_def =
            base_animal(species::wolf, "wolf", "Wolf", EntityKind::carnivore);
        wolf_def.food_species = {species::rabbit, species::deer, species::hare};
        wolf_def.tags = {"animal", "carnivore", "predator"};
        wolf_def.initial_biomass = 38.0;
        wolf_def.max_biomass = 55.0;
        wolf_def.initial_energy = 110.0;
        wolf_def.max_energy = 180.0;
        wolf_def.metabolism_per_hour = 0.02;
        wolf_def.dehydration_per_hour = 0.003;
        wolf_def.movement_per_hour = 1.15;
        wolf_def.perception_radius = 9.0;
        wolf_def.interaction_radius = 0.8;
        wolf_def.bite_biomass_per_hour = 0.16;
        wolf_def.maturity_hours = 24.0 * 365.0 * 2.0;
        wolf_def.lifespan_hours = 24.0 * 365.0 * 13.0;
        wolf_def.reproduction_interval_hours = 24.0 * 365.0;
        wolf_def.reproduction_chance_per_hour = 0.000055;
        wolf_def.carrying_density_per_cell = 0.0007;
        wolf_def.cruise_height = 0.5;
        wolf_def.forage_energy_fraction = 0.68;
        wolf_def.home_range_radius = 14.0;
        wolf_def.decision_interval_hours = 4.0;
        wolf_def.rest_duration_hours = 2.2;
        wolf_def.social_radius = 12.0;
        wolf_def.social_weight = 0.78;
        wolf_def.hunt_speed_multiplier = 1.8;
        wolf_def.activity_peak_hour = 21.0;
        wolf_def.active_hours_per_day = 15.0;
        wolf_def.initial_group_size = 2;
        add_scaled_species(catalog, std::move(wolf_def));
    }

    {
        SpeciesDefinition fox_def =
            base_animal(species::fox, "fox", "Fox", EntityKind::carnivore);
        fox_def.food_species = {species::mouse, species::rabbit, species::hare};
        fox_def.tags = {"animal", "carnivore", "predator"};
        fox_def.initial_biomass = 7.5;
        fox_def.max_biomass = 11.0;
        fox_def.initial_energy = 22.0;
        fox_def.max_energy = 34.0;
        fox_def.metabolism_per_hour = 0.012;
        fox_def.dehydration_per_hour = 0.0028;
        fox_def.movement_per_hour = 1.25;
        fox_def.perception_radius = 7.0;
        fox_def.interaction_radius = 0.5;
        fox_def.bite_biomass_per_hour = 0.05;
        fox_def.maturity_hours = 24.0 * 280.0;
        fox_def.lifespan_hours = 24.0 * 365.0 * 8.0;
        fox_def.reproduction_interval_hours = 24.0 * 220.0;
        fox_def.reproduction_chance_per_hour = 0.00012;
        fox_def.carrying_density_per_cell = 0.0012;
        fox_def.cruise_height = 0.4;
        fox_def.forage_energy_fraction = 0.70;
        fox_def.home_range_radius = 11.0;
        fox_def.decision_interval_hours = 3.0;
        fox_def.rest_duration_hours = 1.8;
        fox_def.hunt_speed_multiplier = 1.75;
        fox_def.activity_peak_hour = 22.0;
        fox_def.active_hours_per_day = 14.0;
        add_scaled_species(catalog, std::move(fox_def));
    }

    {
        SpeciesDefinition bee_def =
            base_animal(species::bee, "bee", "Bee", EntityKind::insect);
        bee_def.food_species = {species::clover, species::berry_bush};
        bee_def.tags = {"animal", "insect", "pollinator", "flying"};
        bee_def.initial_biomass = 0.01;
        bee_def.max_biomass = 0.015;
        bee_def.initial_energy = 0.8;
        bee_def.max_energy = 1.2;
        bee_def.metabolism_per_hour = 0.0007;
        bee_def.dehydration_per_hour = 0.0016;
        bee_def.movement_per_hour = 2.2;
        bee_def.perception_radius = 8.0;
        bee_def.interaction_radius = 0.35;
        bee_def.bite_biomass_per_hour = 0.002;
        bee_def.maturity_hours = 24.0 * 20.0;
        bee_def.lifespan_hours = 24.0 * 365.0 * 2.0;
        bee_def.reproduction_interval_hours = 24.0 * 24.0;
        bee_def.reproduction_chance_per_hour = 0.0009;
        bee_def.reproduction_energy_fraction = 0.18;
        bee_def.carrying_density_per_cell = 0.012;
        bee_def.pollination_deposit_per_hour = 0.22;
        bee_def.nectar_energy_per_hour = 0.012;
        bee_def.forage_energy_fraction = 0.78;
        bee_def.activity_min_temperature = 7.0;
        bee_def.cruise_height = 1.05;
        bee_def.home_range_radius = 7.0;
        bee_def.decision_interval_hours = 0.8;
        bee_def.rest_duration_hours = 0.45;
        bee_def.social_radius = 5.0;
        bee_def.social_weight = 0.58;
        bee_def.activity_peak_hour = 13.0;
        bee_def.active_hours_per_day = 11.0;
        bee_def.initial_group_size = 12;
        add_scaled_species(catalog, std::move(bee_def));
    }

    {
        SpeciesDefinition butterfly_def =
            base_animal(species::butterfly, "butterfly", "Butterfly", EntityKind::insect);
        butterfly_def.food_species = {species::clover, species::berry_bush};
        butterfly_def.tags = {"animal", "insect", "pollinator", "flying"};
        butterfly_def.initial_biomass = 0.008;
        butterfly_def.max_biomass = 0.012;
        butterfly_def.initial_energy = 0.55;
        butterfly_def.max_energy = 0.9;
        butterfly_def.metabolism_per_hour = 0.00055;
        butterfly_def.dehydration_per_hour = 0.0016;
        butterfly_def.movement_per_hour = 1.8;
        butterfly_def.perception_radius = 7.0;
        butterfly_def.interaction_radius = 0.3;
        butterfly_def.bite_biomass_per_hour = 0.0012;
        butterfly_def.maturity_hours = 24.0 * 16.0;
        butterfly_def.lifespan_hours = 24.0 * 365.0;
        butterfly_def.reproduction_interval_hours = 24.0 * 18.0;
        butterfly_def.reproduction_chance_per_hour = 0.0012;
        butterfly_def.reproduction_energy_fraction = 0.16;
        butterfly_def.carrying_density_per_cell = 0.01;
        butterfly_def.pollination_deposit_per_hour = 0.16;
        butterfly_def.nectar_energy_per_hour = 0.008;
        butterfly_def.forage_energy_fraction = 0.76;
        butterfly_def.activity_min_temperature = 8.0;
        butterfly_def.cruise_height = 1.15;
        butterfly_def.home_range_radius = 6.0;
        butterfly_def.decision_interval_hours = 0.7;
        butterfly_def.rest_duration_hours = 0.55;
        butterfly_def.social_radius = 4.0;
        butterfly_def.social_weight = 0.35;
        butterfly_def.activity_peak_hour = 13.0;
        butterfly_def.active_hours_per_day = 10.0;
        butterfly_def.initial_group_size = 6;
        add_scaled_species(catalog, std::move(butterfly_def));
    }

    {
        SpeciesDefinition beetle_def =
            base_animal(species::beetle, "beetle", "Beetle", EntityKind::insect);
        beetle_def.food_species = {species::mushroom};
        beetle_def.tags = {"animal", "insect", "decomposer"};
        beetle_def.initial_biomass = 0.03;
        beetle_def.max_biomass = 0.05;
        beetle_def.food_energy_per_biomass = 12.0;
        beetle_def.initial_energy = 0.9;
        beetle_def.max_energy = 1.4;
        beetle_def.metabolism_per_hour = 0.0012;
        beetle_def.dehydration_per_hour = 0.0015;
        beetle_def.movement_per_hour = 0.55;
        beetle_def.perception_radius = 3.6;
        beetle_def.interaction_radius = 0.18;
        beetle_def.bite_biomass_per_hour = 0.004;
        beetle_def.maturity_hours = 24.0 * 40.0;
        beetle_def.lifespan_hours = 24.0 * 365.0 * 3.0;
        beetle_def.reproduction_interval_hours = 24.0 * 70.0;
        beetle_def.reproduction_chance_per_hour = 0.0004;
        beetle_def.reproduction_energy_fraction = 0.2;
        beetle_def.carrying_density_per_cell = 0.012;
        beetle_def.organic_consumption_per_hour = 0.0045;
        beetle_def.activity_min_temperature = 4.0;
        beetle_def.cruise_height = 0.12;
        beetle_def.home_range_radius = 2.5;
        beetle_def.decision_interval_hours = 1.4;
        beetle_def.rest_duration_hours = 0.9;
        beetle_def.social_radius = 2.5;
        beetle_def.social_weight = 0.35;
        beetle_def.activity_peak_hour = 22.0;
        beetle_def.active_hours_per_day = 12.0;
        beetle_def.initial_group_size = 6;
        add_scaled_species(catalog, std::move(beetle_def));
    }

    {
        SpeciesDefinition ant_def =
            base_animal(species::ant, "ant", "Ant", EntityKind::insect);
        ant_def.tags = {"animal", "insect", "decomposer"};
        ant_def.initial_biomass = 0.006;
        ant_def.max_biomass = 0.01;
        ant_def.food_energy_per_biomass = 10.0;
        ant_def.initial_energy = 0.5;
        ant_def.max_energy = 0.8;
        ant_def.metabolism_per_hour = 0.0007;
        ant_def.dehydration_per_hour = 0.0018;
        ant_def.movement_per_hour = 0.9;
        ant_def.perception_radius = 2.0;
        ant_def.interaction_radius = 0.14;
        ant_def.bite_biomass_per_hour = 0.0;
        ant_def.maturity_hours = 24.0 * 20.0;
        ant_def.lifespan_hours = 24.0 * 365.0 * 1.5;
        ant_def.reproduction_interval_hours = 24.0 * 40.0;
        ant_def.reproduction_chance_per_hour = 0.00055;
        ant_def.reproduction_energy_fraction = 0.16;
        ant_def.carrying_density_per_cell = 0.014;
        ant_def.organic_consumption_per_hour = 0.0035;
        ant_def.activity_min_temperature = 6.0;
        ant_def.cruise_height = 0.1;
        ant_def.home_range_radius = 3.0;
        ant_def.decision_interval_hours = 0.6;
        ant_def.rest_duration_hours = 0.35;
        ant_def.social_radius = 3.5;
        ant_def.social_weight = 0.8;
        ant_def.activity_peak_hour = 14.0;
        ant_def.active_hours_per_day = 15.0;
        ant_def.initial_group_size = 24;
        add_scaled_species(catalog, std::move(ant_def));
    }

    return catalog;
}

} // namespace sim
