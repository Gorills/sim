#include "sim/sim.hpp"

#include <iostream>
#include <limits>
#include <optional>

namespace {

int g_failures = 0;

void check(bool condition, const char* expression, const char* file, int line) {
    if (!condition) {
        std::cerr << "FAIL " << file << ":" << line << "  " << expression << '\n';
        ++g_failures;
    }
}

#define CHECK(expr) check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

void test_commands_apply_on_tick() {
    sim::World world;
    const sim::EntityId id = world.enqueue_spawn({1.0, 2.0, 3.0}, {0.0, 0.0, 0.0});
    CHECK(world.entity_count() == 0);
    CHECK(world.snapshot().entities.empty());
    world.tick();
    CHECK(world.entity_count() == 1);
    const sim::Snapshot snap = world.snapshot();
    const std::optional<sim::EntityState> entity = sim::find_entity(snap, id);
    CHECK(entity.has_value());
    CHECK(entity->position.x == 1.0);
}

void test_flush_applies_without_ticking() {
    sim::World world;
    static_cast<void>(world.enqueue_spawn({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}));
    world.flush_commands();
    CHECK(world.entity_count() == 1);
    CHECK(world.tick_index() == 0);
    CHECK(world.snapshot().entities[0].position.x == 0.0);
}

void test_integration_and_interpolation() {
    sim::WorldConfig config;
    config.tick_dt = 0.5;
    sim::World world(config);
    static_cast<void>(world.enqueue_spawn({0.0, 1.0, 0.0}, {4.0, 0.0, 0.0}));
    world.flush_commands();
    const sim::Snapshot previous = world.snapshot();
    world.tick();
    const sim::Snapshot current = world.snapshot();
    CHECK(current.tick == 1);
    CHECK(current.entities[0].position.x == 2.0);

    const sim::Snapshot mid = sim::interpolate(previous, current, 0.5);
    CHECK(mid.entities[0].position.x == 1.0);
    CHECK(sim::interpolate(previous, current, 0.0).entities[0].position == previous.entities[0].position);
    CHECK(sim::interpolate(previous, current, 1.0).entities[0].position == current.entities[0].position);
}

void test_bounds_bounce() {
    sim::WorldConfig config;
    config.tick_dt = 0.1;
    config.bounds_min = {0.0, 0.0, 0.0};
    config.bounds_max = {1.0, 1.0, 1.0};
    sim::World world(config);
    static_cast<void>(world.enqueue_spawn({0.95, 0.5, 0.5}, {10.0, 0.0, 0.0}));
    world.tick();
    const sim::Snapshot snap = world.snapshot();
    const sim::EntityState& entity = snap.entities.at(0);
    CHECK(entity.position.x == 1.0);
    CHECK(entity.velocity.x < 0.0);
}

void test_despawn() {
    sim::World world;
    const sim::EntityId keep = world.enqueue_spawn({0.0, 0.0, 0.0}, {});
    const sim::EntityId drop = world.enqueue_spawn({1.0, 0.0, 0.0}, {});
    world.flush_commands();
    world.enqueue_despawn(drop);
    world.tick();
    CHECK(world.entity_count() == 1);
    const sim::Snapshot snap = world.snapshot();
    CHECK(sim::find_entity(snap, keep).has_value());
    CHECK(!sim::find_entity(snap, drop).has_value());
}

void test_determinism() {
    auto run = []() {
        sim::World world;
        static_cast<void>(world.enqueue_spawn({0.0, 0.4, 0.0}, {1.0, 0.2, -0.5}));
        static_cast<void>(world.enqueue_spawn({1.0, 0.4, 1.0}, {-0.3, 0.0, 0.8}));
        for (int i = 0; i < 120; ++i) {
            world.tick();
        }
        return world.snapshot();
    };
    CHECK(run() == run());
}

void test_pause_skips_integration() {
    sim::World world;
    static_cast<void>(world.enqueue_spawn({0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}));
    world.flush_commands();
    world.set_paused(true);
    world.tick();
    CHECK(world.tick_index() == 0);
    CHECK(world.snapshot().entities[0].position.x == 0.0);
}

void test_stepper_fixed_dt_and_spiral_cap() {
    sim::Stepper stepper(0.05, 8);
    int ticks = 0;
    const int steps = stepper.advance(1.0, [&]() { ++ticks; });
    CHECK(steps == 8);
    CHECK(ticks == 8);
    CHECK(stepper.alpha() <= 1.0);

    sim::Stepper exact(0.25, 16);
    int exact_ticks = 0;
    CHECK(exact.advance(1.0, [&]() { ++exact_ticks; }) == 4);
    CHECK(exact_ticks == 4);
    CHECK(exact.alpha() == 0.0);
}

void test_stepper_can_honor_sixteen_times_speed() {
    sim::Stepper limited(1.0 / 60.0, 8);
    int limited_ticks = 0;
    CHECK(limited.advance(16.0 / 60.0, [&]() { ++limited_ticks; }) == 8);

    sim::Stepper honest(1.0 / 60.0, 24);
    int honest_ticks = 0;
    CHECK(honest.advance(16.0 / 60.0, [&]() { ++honest_ticks; }) == 16);
    CHECK(honest_ticks == 16);
    honest.set_max_steps(32);
    CHECK(honest.max_steps() == 32);
}

void test_invalid_time_values_fall_back_safely() {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    sim::WorldConfig config;
    config.tick_dt = nan;
    sim::World world(config);
    CHECK(world.tick_dt() == 1.0 / 60.0);
    world.set_tick_dt(infinity);
    CHECK(world.tick_dt() == 1.0 / 60.0);

    sim::Stepper stepper(nan);
    CHECK(stepper.tick_dt() == 1.0 / 60.0);
    int ticks = 0;
    CHECK(stepper.advance(nan, [&]() { ++ticks; }) == 0);
    CHECK(ticks == 0);
    CHECK(stepper.advance(stepper.tick_dt(), [&]() { ++ticks; }) == 1);
    CHECK(ticks == 1);
}

void test_habitat_and_extensible_layers_are_deterministic() {
    sim::HabitatConfig config;
    config.width = 16;
    config.height = 12;
    config.seed = 99;
    sim::HabitatGrid first(config);
    sim::HabitatGrid second(config);
    CHECK(first.size() == 16U * 12U);
    CHECK(first.mean_moisture() == second.mean_moisture());

    bool has_ocean = false;
    bool has_fresh_water = false;
    for (std::size_t z = 0; z < config.height; ++z) {
        for (std::size_t x = 0; x < config.width; ++x) {
            has_ocean = has_ocean || first.cell(x, z).water;
            has_fresh_water = has_fresh_water || first.cell(x, z).fresh_water;
        }
    }
    CHECK(has_ocean);
    CHECK(has_fresh_water);

    std::span<double> mana = first.ensure_layer("mana", 0.25);
    CHECK(mana.size() == first.size());
    mana[3] = 0.75;
    CHECK(first.layer("mana")[3] == 0.75);
    CHECK(first.layer("unknown").empty());
}

void test_habitat_large_steps_cover_full_interval() {
    sim::HabitatConfig config;
    config.width = 12;
    config.height = 12;
    config.seed = 11;
    sim::HabitatGrid combined(config);
    sim::HabitatGrid split(config);
    sim::ClimateConfig climate;

    combined.advance(48.0, 100.0, climate);
    split.advance(24.0, 100.0, climate);
    split.advance(24.0, 124.0, climate);

    CHECK(combined.mean_moisture() == split.mean_moisture());
    CHECK(combined.mean_temperature() == split.mean_temperature());
    CHECK(combined.mean_organic() == split.mean_organic());
}

void test_species_catalog_data_driven_web() {
    const sim::SpeciesCatalog catalog = sim::SpeciesCatalog::temperate_island();
    CHECK(catalog.all().size() >= 15);
    CHECK(catalog.find("grass") != nullptr);
    CHECK(catalog.find("birch") != nullptr);
    CHECK(catalog.find("pine") != nullptr);
    CHECK(catalog.find("berry_bush") != nullptr);
    CHECK(catalog.find("fern") != nullptr);
    CHECK(catalog.find("reeds") != nullptr);
    CHECK(catalog.find("mushroom") != nullptr);
    CHECK(catalog.find("mouse") != nullptr);
    CHECK(catalog.find("hare") != nullptr);
    CHECK(catalog.find("boar") != nullptr);
    CHECK(catalog.find("fox") != nullptr);
    CHECK(catalog.find("butterfly") != nullptr);
    CHECK(catalog.find("beetle") != nullptr);
    CHECK(catalog.find("ant") != nullptr);
    CHECK(catalog.find("wolf") != nullptr);
    CHECK(catalog.find("wildflower") != nullptr);
    CHECK(catalog.find("willow") != nullptr);
    CHECK(catalog.find("vole") != nullptr);
    CHECK(catalog.find("hedgehog") != nullptr);
    CHECK(catalog.find("robin") != nullptr);
    CHECK(catalog.find("owl") != nullptr);
    CHECK(catalog.find("frog") != nullptr);
    CHECK(catalog.find("bumblebee") != nullptr);
    CHECK(catalog.find("moth") != nullptr);
    CHECK(catalog.all().size() >= 29);

    CHECK(catalog.find("clover")->kind == sim::EntityKind::plant);
    CHECK(sim::has_tag(*catalog.find("clover"), "nitrogen_fixer"));
    CHECK(sim::has_tag(*catalog.find("clover"), "flowering"));
    CHECK(catalog.find("boar")->kind == sim::EntityKind::omnivore);
    CHECK(catalog.find("boar")->food_species.size() >= 3);
    CHECK(sim::has_tag(*catalog.find("bee"), "pollinator"));
    CHECK(sim::has_tag(*catalog.find("butterfly"), "pollinator"));
    CHECK(sim::has_tag(*catalog.find("beetle"), "decomposer"));
    CHECK(sim::has_tag(*catalog.find("mushroom"), "decomposer"));
    CHECK(catalog.find("oak")->canopy_contribution > 0.0);
    CHECK(catalog.find("fern")->shade_preference > 0.4);
    CHECK(catalog.find("mushroom")->organic_growth_factor > 0.0);
    CHECK(catalog.find("bee")->pollination_deposit_per_hour > 0.0);
    CHECK(catalog.find("bee")->nectar_energy_per_hour > 0.0);
    CHECK(catalog.find("bee")->perception_radius >= 800.0);
    CHECK(catalog.find("bee")->home_range_radius >= 1'500.0);
    CHECK(catalog.find("butterfly")->nectar_energy_per_hour > 0.0);
    CHECK(catalog.find("butterfly")->lifespan_hours >= 24.0 * 300.0);
    CHECK(sim::has_tag(*catalog.find("wildflower"), "flowering"));
    CHECK(sim::has_tag(*catalog.find("willow"), "tree"));
    CHECK(catalog.find("vole")->kind == sim::EntityKind::herbivore);
    CHECK(catalog.find("hedgehog")->kind == sim::EntityKind::omnivore);
    CHECK(sim::has_tag(*catalog.find("robin"), "flying"));
    CHECK(sim::has_tag(*catalog.find("owl"), "flying"));
    CHECK(sim::has_tag(*catalog.find("bumblebee"), "pollinator"));
    CHECK(sim::has_tag(*catalog.find("moth"), "pollinator"));
    CHECK(catalog.find("frog")->preferred_moisture > 0.8);
    CHECK(catalog.find("wildflower")->initial_group_size > 1);
    CHECK(catalog.find("grass")->initial_group_size > 1);

    const sim::SpeciesDefinition* rabbit = catalog.find("rabbit");
    const sim::SpeciesDefinition* deer = catalog.find("deer");
    const sim::SpeciesDefinition* wolf = catalog.find("wolf");
    const sim::SpeciesDefinition* fox = catalog.find("fox");
    CHECK(rabbit != nullptr && rabbit->home_range_radius >= 300.0);
    CHECK(rabbit != nullptr && rabbit->movement_per_hour >= 200.0);
    CHECK(rabbit != nullptr && rabbit->interaction_radius < 1.0);
    CHECK(deer != nullptr && deer->home_range_radius >= 1'000.0);
    CHECK(wolf != nullptr && wolf->home_range_radius >= 3'000.0);
    CHECK(wolf != nullptr && wolf->perception_radius < wolf->home_range_radius);
    CHECK(wolf != nullptr && wolf->initial_group_size == 4);
    CHECK(fox != nullptr && fox->home_range_radius >= 1'500.0);

    for (const sim::SpeciesDefinition& definition : catalog.all()) {
        for (const sim::SpeciesId food_id : definition.food_species) {
            CHECK(catalog.find(food_id) != nullptr);
        }
    }
}

void configure_compact_island(sim::WorldConfig& config) {
    config.habitat.width = 48;
    config.habitat.height = 48;
    config.habitat.cell_size = 150.0;
    config.habitat.origin = {-3'600.0, 0.0, -3'600.0};
    config.bounds_min = {-3'600.0, 0.0, -3'600.0};
    config.bounds_max = {3'600.0, 500.0, 3'600.0};
}

sim::IslandScenarioConfig empty_island_populations() {
    sim::IslandScenarioConfig island;
    island.grass = 0;
    island.clover = 0;
    island.oak = 0;
    island.birch = 0;
    island.pine = 0;
    island.berry_bush = 0;
    island.fern = 0;
    island.reeds = 0;
    island.mushroom = 0;
    island.wildflower = 0;
    island.willow = 0;
    island.rabbit = 0;
    island.deer = 0;
    island.mouse = 0;
    island.hare = 0;
    island.boar = 0;
    island.vole = 0;
    island.bee = 0;
    island.butterfly = 0;
    island.beetle = 0;
    island.ant = 0;
    island.bumblebee = 0;
    island.moth = 0;
    island.robin = 0;
    island.hedgehog = 0;
    island.frog = 0;
    island.wolf = 0;
    island.fox = 0;
    island.owl = 0;
    return island;
}

sim::IslandScenarioConfig compact_island_populations() {
    sim::IslandScenarioConfig island = empty_island_populations();
    island.grass = 48;
    island.clover = 22;
    island.oak = 4;
    island.birch = 3;
    island.pine = 3;
    island.berry_bush = 10;
    island.fern = 10;
    island.reeds = 8;
    island.mushroom = 10;
    island.rabbit = 8;
    island.deer = 3;
    island.mouse = 10;
    island.hare = 6;
    island.boar = 2;
    island.wolf = 2;
    island.fox = 2;
    island.bee = 8;
    island.butterfly = 6;
    island.beetle = 8;
    island.ant = 8;
    return island;
}

void test_default_island_scale_and_habitat_snapshot() {
    sim::World world;
    const sim::HabitatConfig& config = world.habitat().config();
    CHECK(config.width >= 192);
    CHECK(config.width <= 320);
    CHECK(config.height == config.width);
    CHECK(config.cell_size >= 50.0);
    CHECK(config.cell_size <= 100.0);
    const double extent_m = static_cast<double>(config.width) * config.cell_size;
    CHECK(extent_m >= 15'000.0);
    CHECK(extent_m <= 25'000.0);

    const sim::HabitatSnapshot habitat = world.habitat_snapshot();
    CHECK(habitat.width == config.width);
    CHECK(habitat.height == config.height);
    CHECK(habitat.elevation.size() == world.habitat().size());
    CHECK(habitat.moisture.size() == habitat.elevation.size());
    CHECK(habitat.surface.size() == habitat.elevation.size());
    CHECK(habitat.canopy.size() == habitat.elevation.size());
    CHECK(habitat.organic.size() == habitat.elevation.size());
    CHECK(habitat.pollination.size() == habitat.elevation.size());

    bool has_ocean = false;
    bool has_land = false;
    bool has_fresh_water = false;
    std::size_t land_cells = 0;
    for (std::size_t i = 0; i < habitat.surface.size(); ++i) {
        has_ocean = has_ocean || habitat.surface[i] == 1;
        has_land = has_land || habitat.surface[i] == 0;
        has_fresh_water = has_fresh_water || habitat.surface[i] == 2;
        if (habitat.surface[i] != 1) {
            ++land_cells;
        } else {
            CHECK(habitat.elevation[i] <= 0.0);
        }
    }
    const double land_area_km2 =
        static_cast<double>(land_cells) * config.cell_size * config.cell_size / 1'000'000.0;
    CHECK(land_area_km2 >= 150.0);
    CHECK(land_area_km2 <= 300.0);
    CHECK(has_ocean);
    CHECK(has_land);
    CHECK(has_fresh_water);
    CHECK(world.snapshot().mean_organic > 0.0);

    sim::IslandScenarioConfig island;
    CHECK(island.wolf == 6);
    CHECK(island.fox >= island.wolf * 2);
    CHECK(island.rabbit >= island.wolf * 50);
    CHECK(island.deer >= island.wolf * 10);
    CHECK(island.wildflower >= island.clover);
    CHECK(island.vole >= island.rabbit);
    CHECK(island.bumblebee + island.moth + island.bee + island.butterfly >= 700);

    const std::size_t total =
        island.grass + island.clover + island.oak + island.birch + island.pine +
        island.berry_bush + island.fern + island.reeds + island.mushroom +
        island.wildflower + island.willow + island.rabbit + island.deer +
        island.mouse + island.hare + island.boar + island.vole + island.bee +
        island.butterfly + island.beetle + island.ant + island.bumblebee +
        island.moth + island.robin + island.hedgehog + island.frog + island.wolf +
        island.fox + island.owl;
    CHECK(total == 9'017);
}

void test_food_linked_seeding_preserves_consumer_habitat() {
    sim::WorldConfig config;
    config.habitat.width = 16;
    config.habitat.height = 16;
    config.habitat.cell_size = 1.0;
    config.habitat.origin = {-8.0, 0.0, -8.0};
    config.bounds_min = {-8.0, 0.0, -8.0};
    config.bounds_max = {8.0, 8.0, 8.0};

    sim::SpeciesCatalog catalog;
    sim::SpeciesDefinition food;
    food.id = sim::species::grass;
    food.key = "dry_food";
    food.display_name = "Dry food";
    food.kind = sim::EntityKind::plant;
    food.initial_biomass = 1.0;
    food.max_biomass = 2.0;
    food.preferred_moisture = 0.2;
    food.moisture_tolerance = 0.02;
    food.preferred_temperature = 18.0;
    food.temperature_tolerance = 100.0;
    CHECK(catalog.add(food));

    sim::SpeciesDefinition consumer;
    consumer.id = sim::species::rabbit;
    consumer.key = "wet_consumer";
    consumer.display_name = "Wet consumer";
    consumer.kind = sim::EntityKind::herbivore;
    consumer.food_species = {sim::species::grass};
    consumer.initial_biomass = 1.0;
    consumer.max_biomass = 2.0;
    consumer.initial_energy = 1.0;
    consumer.max_energy = 2.0;
    consumer.preferred_moisture = 0.9;
    consumer.moisture_tolerance = 0.02;
    consumer.preferred_temperature = 18.0;
    consumer.temperature_tolerance = 100.0;
    consumer.home_range_radius = 1.0;
    CHECK(catalog.add(consumer));

    sim::World world(config, std::move(catalog));
    for (std::size_t z = 0; z < config.habitat.height; ++z) {
        for (std::size_t x = 0; x < config.habitat.width; ++x) {
            sim::HabitatCell& cell = world.habitat().cell(x, z);
            cell.water = false;
            cell.fresh_water = false;
            cell.temperature = 18.0;
            cell.moisture = x <= 6 ? 0.2 : (x >= 10 ? 0.9 : 0.5);
        }
    }

    sim::IslandScenarioConfig island = empty_island_populations();
    island.seed = 91;
    island.grass = 1;
    island.rabbit = 1;
    const sim::ScenarioSeedResult seeded = sim::seed_temperate_island(world, island);
    CHECK(seeded.complete());

    std::optional<sim::Vec3> consumer_position;
    for (const sim::EntityState& entity : world.snapshot().entities) {
        if (entity.species_id == sim::species::rabbit) {
            consumer_position = entity.position;
            break;
        }
    }
    CHECK(consumer_position.has_value());
    if (consumer_position.has_value()) {
        const sim::HabitatCell* cell = world.habitat().cell_at(*consumer_position);
        CHECK(cell != nullptr);
        CHECK(cell == nullptr || cell->moisture == 0.9);
    }
}

void test_consumers_seed_near_existing_food() {
    sim::World world;
    sim::IslandScenarioConfig island = empty_island_populations();
    island.seed = 77;
    island.grass = 1;
    island.rabbit = 1;
    const sim::ScenarioSeedResult seeded = sim::seed_temperate_island(world, island);
    CHECK(seeded.complete());
    CHECK(seeded.total_seeded() == 2);

    std::optional<sim::Vec3> grass_position;
    std::optional<sim::Vec3> rabbit_position;
    for (const sim::EntityState& entity : world.snapshot().entities) {
        if (entity.species_id == sim::species::grass) {
            grass_position = entity.position;
        } else if (entity.species_id == sim::species::rabbit) {
            rabbit_position = entity.position;
        }
    }
    CHECK(grass_position.has_value());
    CHECK(rabbit_position.has_value());
    if (grass_position.has_value() && rabbit_position.has_value()) {
        CHECK(sim::length(*rabbit_position - *grass_position) <= 150.0);
    }
}

void test_interest_snapshots_and_overview() {
    sim::World world;
    const sim::EntityId near_id = world.enqueue_spawn({0.0, 1.0, 0.0}, {});
    const sim::EntityId far_id = world.enqueue_spawn({4'000.0, 1.0, 0.0}, {});

    sim::Vec3 animal_position{};
    bool found_land = false;
    const sim::HabitatConfig& config = world.habitat().config();
    for (std::size_t z = 0; z < config.height && !found_land; ++z) {
        for (std::size_t x = 0; x < config.width && !found_land; ++x) {
            if (!world.habitat().cell(x, z).water) {
                animal_position = world.habitat().cell_center(x, z);
                found_land = true;
            }
        }
    }
    CHECK(found_land);
    CHECK(world.enqueue_organism(sim::species::rabbit, animal_position) != 0);
    world.flush_commands();

    const sim::Snapshot local = world.snapshot({0.0, 0.0, 0.0}, 750.0);
    CHECK(sim::find_entity(local, near_id).has_value());
    CHECK(!sim::find_entity(local, far_id).has_value());

    const sim::HabitatSnapshot region = world.habitat_snapshot({0.0, 0.0, 0.0}, 1'000.0);
    CHECK(region.width > 0);
    CHECK(region.height > 0);
    CHECK(region.width < config.width);
    CHECK(region.height < config.height);

    const sim::OverviewSnapshot overview = world.overview_snapshot(64);
    CHECK(overview.width <= 64);
    CHECK(overview.height <= 64);
    CHECK(overview.surface.size() == overview.width * overview.height);
    CHECK(overview.herbivores.size() == overview.surface.size());
    std::uint64_t herbivores = 0;
    for (const std::uint32_t count : overview.herbivores) {
        herbivores += count;
    }
    CHECK(herbivores == 1);
}

void test_biome_layers_affect_dynamics() {
    sim::WorldConfig config;
    config.seed = 42;
    config.ecology_hours_per_tick = 1.0;
    config.habitat.width = 16;
    config.habitat.height = 16;
    config.habitat.cell_size = 1.0;
    config.habitat.origin = {-8.0, 0.0, -8.0};
    config.bounds_min = {-8.0, 0.0, -8.0};
    config.bounds_max = {8.0, 8.0, 8.0};
    sim::World world(config);

    sim::Vec3 oak_position{};
    bool found_land = false;
    for (std::size_t z = 0; z < 16 && !found_land; ++z) {
        for (std::size_t x = 0; x < 16 && !found_land; ++x) {
            if (!world.habitat().cell(x, z).water) {
                oak_position = world.habitat().cell_center(x, z);
                found_land = true;
            }
        }
    }
    CHECK(found_land);
    CHECK(world.enqueue_organism(sim::species::oak, oak_position) != 0);
    world.flush_commands();
    world.tick();
    const sim::HabitatCell* oak_cell = world.habitat().cell_at(oak_position);
    CHECK(oak_cell != nullptr);
    CHECK(oak_cell->canopy > 0.0);
    CHECK(oak_cell->light < 1.0);
    CHECK(world.snapshot().mean_canopy > 0.0);

    sim::HabitatCell* litter = world.habitat().cell_at(oak_position);
    CHECK(litter != nullptr);
    litter->organic = 0.9;
    const sim::EntityId mushroom =
        world.enqueue_organism(sim::species::mushroom, oak_position, -1.0, 24.0);
    CHECK(mushroom != 0);
    world.flush_commands();
    const double mushroom_before = world.snapshot().entities.back().biomass;
    for (int i = 0; i < 24; ++i) {
        world.tick();
    }
    const auto mushroom_state = sim::find_entity(world.snapshot(), mushroom);
    CHECK(mushroom_state.has_value());
    CHECK(mushroom_state->biomass >= mushroom_before);
    CHECK(world.ecosystem_stats().mean_organic > 0.0);
}

void test_animals_do_not_overshoot_food_on_long_ticks() {
    sim::WorldConfig config;
    config.seed = 42;
    config.ecology_hours_per_tick = 1.0;
    config.habitat.width = 16;
    config.habitat.height = 16;
    config.habitat.cell_size = 1.0;
    config.habitat.origin = {-8.0, 0.0, -8.0};
    config.bounds_min = {-8.0, 0.0, -8.0};
    config.bounds_max = {8.0, 8.0, 8.0};
    sim::World world(config);

    sim::Vec3 grass_at{};
    sim::Vec3 rabbit_at{};
    bool placed = false;
    for (std::size_t z = 2; z < 14 && !placed; ++z) {
        for (std::size_t x = 2; x + 1 < 14 && !placed; ++x) {
            if (!world.habitat().cell(x, z).water && !world.habitat().cell(x + 1, z).water) {
                grass_at = world.habitat().cell_center(x, z);
                rabbit_at = world.habitat().cell_center(x + 1, z);
                placed = true;
            }
        }
    }
    CHECK(placed);
    CHECK(world.enqueue_organism(sim::species::grass, grass_at) != 0);
    const sim::EntityId rabbit =
        world.enqueue_organism(sim::species::rabbit, rabbit_at, 4.0, 24.0 * 120.0);
    CHECK(rabbit != 0);
    world.flush_commands();
    world.tick();
    const auto after = sim::find_entity(world.snapshot(), rabbit);
    CHECK(after.has_value());
    CHECK(after->energy > 4.0);
}

void test_prey_flees_from_a_nearby_predator() {
    sim::WorldConfig config;
    config.seed = 42;
    config.ecology_hours_per_tick = 0.25;
    config.climate_start_hour = 24.0 * 120.0 + 19.0;
    config.habitat.width = 20;
    config.habitat.height = 20;
    config.habitat.cell_size = 50.0;
    config.habitat.origin = {-500.0, 0.0, -500.0};
    config.bounds_min = {-500.0, 0.0, -500.0};
    config.bounds_max = {500.0, 500.0, 500.0};
    sim::World world(config);

    sim::Vec3 rabbit_at{};
    sim::Vec3 wolf_at{};
    bool placed = false;
    for (std::size_t z = 2; z < 18 && !placed; ++z) {
        for (std::size_t x = 4; x + 3 < 18 && !placed; ++x) {
            if (!world.habitat().cell(x - 2, z).water &&
                !world.habitat().cell(x - 1, z).water &&
                !world.habitat().cell(x, z).water &&
                !world.habitat().cell(x + 1, z).water &&
                !world.habitat().cell(x + 2, z).water &&
                !world.habitat().cell(x + 3, z).water) {
                rabbit_at = world.habitat().cell_center(x, z);
                wolf_at = world.habitat().cell_center(x + 3, z);
                placed = true;
            }
        }
    }
    CHECK(placed);
    const sim::EntityId rabbit =
        world.enqueue_organism(sim::species::rabbit, rabbit_at, 20.0, 24.0 * 120.0);
    CHECK(rabbit != 0);
    const sim::EntityId wolf =
        world.enqueue_organism(sim::species::wolf, wolf_at, 20.0, 24.0 * 800.0);
    CHECK(wolf != 0);
    world.flush_commands();
    world.tick();
    const sim::Snapshot before = world.snapshot();
    const auto before_flight = sim::find_entity(before, rabbit);
    const auto before_wolf = sim::find_entity(before, wolf);
    CHECK(before_flight.has_value());
    CHECK(before_wolf.has_value());
    world.tick();
    const sim::Snapshot after_snapshot = world.snapshot();
    const auto after = sim::find_entity(after_snapshot, rabbit);
    const auto after_wolf = sim::find_entity(after_snapshot, wolf);
    CHECK(after.has_value());
    CHECK(after_wolf.has_value());
    if (before_flight.has_value() && before_wolf.has_value() &&
        after.has_value() && after_wolf.has_value()) {
        CHECK(after->intent == sim::BehaviorIntent::fleeing);
        const sim::Vec3 away = before_flight->position - before_wolf->position;
        const sim::Vec3 movement = after->position - before_flight->position;
        CHECK(away.x * movement.x + away.z * movement.z > 0.0);
    }
}

void test_roaming_keeps_a_persistent_local_target() {
    sim::WorldConfig config;
    config.seed = 17;
    config.ecology_hours_per_tick = 0.25;
    config.climate_start_hour = 24.0 * 120.0 + 21.0;
    config.habitat.width = 24;
    config.habitat.height = 24;
    config.habitat.cell_size = 300.0;
    config.habitat.origin = {-3'600.0, 0.0, -3'600.0};
    config.bounds_min = {-3'600.0, 0.0, -3'600.0};
    config.bounds_max = {3'600.0, 500.0, 3'600.0};
    sim::World world(config);

    sim::Vec3 position{};
    bool placed = false;
    for (std::size_t z = 4; z < 20 && !placed; ++z) {
        for (std::size_t x = 4; x < 20 && !placed; ++x) {
            if (!world.habitat().cell(x, z).water) {
                position = world.habitat().cell_center(x, z);
                placed = true;
            }
        }
    }
    CHECK(placed);
    const sim::EntityId wolf =
        world.enqueue_organism(sim::species::wolf, position, 180.0, 24.0 * 800.0);
    CHECK(wolf != 0);
    world.flush_commands();

    std::optional<sim::EntityState> moving;
    for (int tick = 0; tick < 20 && !moving.has_value(); ++tick) {
        world.tick();
        const auto state = sim::find_entity(world.snapshot(), wolf);
        if (state.has_value() &&
            (state->intent == sim::BehaviorIntent::roaming ||
             state->intent == sim::BehaviorIntent::socializing)) {
            moving = state;
        }
    }
    CHECK(moving.has_value());
    if (moving.has_value()) {
        world.tick();
        const auto next = sim::find_entity(world.snapshot(), wolf);
        CHECK(next.has_value());
        if (next.has_value()) {
            CHECK(next->intent == moving->intent);
            CHECK(next->target_position == moving->target_position);
            CHECK(sim::length(next->position - next->target_position) <
                  sim::length(moving->position - moving->target_position));
        }
    }
}

void test_insect_activity_follows_time_of_day() {
    auto activity_at = [](double hour) {
        sim::WorldConfig config;
        config.seed = 42;
        config.climate_start_hour = 24.0 * 180.0 + hour;
        sim::World world(config);
        sim::Vec3 position{};
        bool placed = false;
        for (std::size_t z = 0; z < config.habitat.height && !placed; ++z) {
            for (std::size_t x = 0; x < config.habitat.width && !placed; ++x) {
                if (!world.habitat().cell(x, z).water) {
                    position = world.habitat().cell_center(x, z);
                    placed = true;
                }
            }
        }
        CHECK(placed);
        CHECK(world.enqueue_organism(sim::species::bee, position) != 0);
        world.flush_commands();
        return world.ecosystem_stats().mean_insect_activity;
    };

    CHECK(activity_at(13.0) > activity_at(1.0) * 4.0);
}

void test_animals_seek_fresh_water_when_thirsty() {
    sim::WorldConfig config;
    config.seed = 42;
    config.ecology_hours_per_tick = 1.0;
    config.climate_start_hour = 24.0 * 120.0 + 20.0;
    sim::World world(config);

    sim::Vec3 position{};
    bool placed = false;
    for (std::size_t z = 0; z < config.habitat.height && !placed; ++z) {
        for (std::size_t x = 0; x < config.habitat.width && !placed; ++x) {
            if (world.habitat().cell(x, z).fresh_water) {
                position = world.habitat().cell_center(x, z);
                placed = true;
            }
        }
    }
    CHECK(placed);
    const sim::EntityId fox =
        world.enqueue_organism(sim::species::fox, position, 34.0, 24.0 * 400.0);
    CHECK(fox != 0);
    world.flush_commands();

    bool observed_drinking = false;
    for (int tick = 0; tick < 240 && !observed_drinking; ++tick) {
        world.tick();
        const auto state = sim::find_entity(world.snapshot(), fox);
        CHECK(state.has_value());
        if (!state.has_value()) {
            break;
        }
        observed_drinking = state->intent == sim::BehaviorIntent::drinking;
    }
    CHECK(observed_drinking);
}

void test_organic_feeding_is_a_single_observable_action() {
    sim::WorldConfig config;
    config.seed = 42;
    config.ecology_hours_per_tick = 1.0;
    config.climate_start_hour = 24.0 * 180.0 + 22.0;
    sim::World world(config);

    sim::Vec3 position{};
    bool placed = false;
    for (std::size_t z = 0; z < config.habitat.height && !placed; ++z) {
        for (std::size_t x = 0; x < config.habitat.width && !placed; ++x) {
            if (!world.habitat().cell(x, z).water) {
                position = world.habitat().cell_center(x, z);
                placed = true;
            }
        }
    }
    CHECK(placed);
    sim::HabitatCell* cell = world.habitat().cell_at(position);
    CHECK(cell != nullptr);
    if (cell != nullptr) {
        cell->organic = 0.8;
    }
    const sim::EntityId beetle =
        world.enqueue_organism(sim::species::beetle, position, 0.1, 24.0 * 80.0);
    CHECK(beetle != 0);
    world.flush_commands();
    world.tick();
    const auto state = sim::find_entity(world.snapshot(), beetle);
    CHECK(state.has_value());
    if (state.has_value()) {
        CHECK(state->intent == sim::BehaviorIntent::feeding);
        CHECK(state->energy > 0.1);
    }
    CHECK(cell == nullptr || cell->organic < 0.8);
}

void test_scenario_reports_capacity_rejection() {
    sim::WorldConfig config;
    config.max_entities = 5;
    sim::World world(config);
    const sim::ScenarioSeedResult seeded = sim::seed_temperate_island(world);
    CHECK(!seeded.complete());
    CHECK(seeded.total_seeded() == 5);
    CHECK(world.entity_count() == 5);
}

void test_generic_agents_respect_capacity() {
    sim::WorldConfig config;
    config.max_entities = 2;
    sim::World world(config);

    CHECK(world.enqueue_spawn({0.0, 0.0, 0.0}, {}) != 0);
    CHECK(world.enqueue_spawn({1.0, 0.0, 0.0}, {}) != 0);
    CHECK(world.enqueue_spawn({2.0, 0.0, 0.0}, {}) == 0);
    world.flush_commands();
    CHECK(world.entity_count() == 2);
    CHECK(world.enqueue_spawn({3.0, 0.0, 0.0}, {}) == 0);
}

void test_species_catalog_and_island_food_chain() {
    sim::WorldConfig config;
    config.seed = 42;
    config.ecology_hours_per_tick = 1.0;
    configure_compact_island(config);
    sim::World world(config);
    sim::IslandScenarioConfig island = compact_island_populations();
    island.seed = 42;
    const sim::ScenarioSeedResult seeded = sim::seed_temperate_island(world, island);
    CHECK(seeded.complete());
    CHECK(seeded.total_seeded() == 181);
    CHECK(world.entity_count(sim::species::grass) == 48);
    CHECK(world.entity_count(sim::species::wolf) == 2);
    CHECK(world.entity_count(sim::species::fox) == 2);
    CHECK(world.entity_count(sim::species::mushroom) == 10);
    CHECK(world.habitat().max_canopy() > 0.0);
    CHECK(world.ecosystem_stats().mean_organic > 0.0);

    for (int i = 0; i < 40 * 24; ++i) {
        world.tick();
    }
    const sim::EcosystemStats stats = world.ecosystem_stats();
    CHECK(stats.plants > 0);
    CHECK(stats.herbivores > 0);
    CHECK(stats.carnivores > 0);
    CHECK(stats.insects > 0);
    CHECK(stats.decomposers > 0);
    CHECK(world.entity_count(sim::species::rabbit) > 0);
    CHECK(world.entity_count(sim::species::hare) > 0);
    CHECK(world.entity_count(sim::species::mouse) > 0);
    CHECK(world.entity_count(sim::species::deer) > 0);
    CHECK(world.entity_count(sim::species::wolf) > 0);
    CHECK(world.entity_count(sim::species::bee) > 0);
    CHECK(world.entity_count(sim::species::butterfly) > 0);
    CHECK(stats.mean_moisture > 0.0);
    CHECK(stats.mean_moisture <= 1.0);
    CHECK(stats.mean_organic >= 0.0);
    CHECK(stats.mean_canopy >= 0.0);
    CHECK(stats.max_pollination > 0.0);
}

void test_island_determinism() {
    auto run = []() {
        sim::WorldConfig config;
        config.seed = 7;
        config.ecology_hours_per_tick = 0.5;
        configure_compact_island(config);
        sim::World world(config);
        sim::IslandScenarioConfig island = compact_island_populations();
        island.seed = 7;
        static_cast<void>(sim::seed_temperate_island(world, island));
        for (int i = 0; i < 240; ++i) {
            world.tick();
        }
        return world.snapshot();
    };
    CHECK(run() == run());
}

void test_generic_agent_mode_still_independent() {
    sim::WorldConfig config;
    config.tick_dt = 0.25;
    sim::World world(config);
    const sim::EntityId id = world.enqueue_spawn({0.0, 1.0, 0.0}, {2.0, 0.0, 0.0});
    world.flush_commands();
    world.tick();
    const auto entity = sim::find_entity(world.snapshot(), id);
    CHECK(entity.has_value());
    CHECK(entity->kind == sim::EntityKind::generic);
    CHECK(entity->species_id == 0);
    CHECK(entity->position.x == 0.5);
}

} // namespace

int main() {
    test_commands_apply_on_tick();
    test_flush_applies_without_ticking();
    test_integration_and_interpolation();
    test_bounds_bounce();
    test_despawn();
    test_determinism();
    test_pause_skips_integration();
    test_stepper_fixed_dt_and_spiral_cap();
    test_stepper_can_honor_sixteen_times_speed();
    test_invalid_time_values_fall_back_safely();
    test_habitat_and_extensible_layers_are_deterministic();
    test_habitat_large_steps_cover_full_interval();
    test_species_catalog_data_driven_web();
    test_default_island_scale_and_habitat_snapshot();
    test_food_linked_seeding_preserves_consumer_habitat();
    test_consumers_seed_near_existing_food();
    test_interest_snapshots_and_overview();
    test_biome_layers_affect_dynamics();
    test_animals_do_not_overshoot_food_on_long_ticks();
    test_prey_flees_from_a_nearby_predator();
    test_roaming_keeps_a_persistent_local_target();
    test_insect_activity_follows_time_of_day();
    test_animals_seek_fresh_water_when_thirsty();
    test_organic_feeding_is_a_single_observable_action();
    test_scenario_reports_capacity_rejection();
    test_generic_agents_respect_capacity();
    test_species_catalog_and_island_food_chain();
    test_island_determinism();
    test_generic_agent_mode_still_independent();

    if (g_failures != 0) {
        std::cerr << g_failures << " check(s) failed\n";
        return 1;
    }
    std::cout << "sim_core tests passed\n";
    return 0;
}
