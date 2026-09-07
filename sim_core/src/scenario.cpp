#include "sim/scenario.hpp"

#include "sim/world.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <numeric>
#include <optional>
#include <unordered_map>

namespace sim {
namespace {

class ScenarioRandom {
public:
    explicit ScenarioRandom(std::uint64_t seed) : state_(seed == 0 ? 1 : seed) {}

    [[nodiscard]] double unit() noexcept {
        state_ += 0x9e3779b97f4a7c15ULL;
        std::uint64_t value = state_;
        value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
        value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
        value ^= value >> 31U;
        return static_cast<double>(value >> 11U) * (1.0 / 9007199254740992.0);
    }

private:
    std::uint64_t state_;
};

double habitat_fitness(const HabitatCell& cell, const SpeciesDefinition& definition) noexcept {
    if (cell.water) {
        return -1.0;
    }
    const double moisture =
        std::clamp(1.0 - std::abs(cell.moisture - definition.preferred_moisture) /
                             std::max(0.01, definition.moisture_tolerance),
                   0.0, 1.0);
    const double temperature =
        std::clamp(1.0 - std::abs(cell.temperature - definition.preferred_temperature) /
                             std::max(0.01, definition.temperature_tolerance),
                   0.0, 1.0);
    double fitness = moisture * temperature;
    if (definition.preferred_moisture >= 0.82 && (cell.fresh_water || cell.moisture > 0.78)) {
        fitness = std::max(fitness, 0.72);
    }
    if (definition.organic_growth_factor > 0.0) {
        fitness = std::max(fitness * 0.35, fitness * (0.4 + cell.organic));
    }
    return fitness;
}

Vec3 random_land_position(const HabitatGrid& habitat,
                          ScenarioRandom& random,
                          const SpeciesDefinition& definition) {
    const HabitatConfig& config = habitat.config();
    Vec3 best = habitat.cell_center(config.width / 2, config.height / 2);
    double best_fit = -1.0;

    for (int attempt = 0; attempt < 1'200; ++attempt) {
        const std::size_t x =
            std::min(config.width - 1,
                     static_cast<std::size_t>(random.unit() * static_cast<double>(config.width)));
        const std::size_t z =
            std::min(config.height - 1,
                     static_cast<std::size_t>(random.unit() * static_cast<double>(config.height)));
        const HabitatCell& cell = habitat.cell(x, z);
        if (cell.water) {
            continue;
        }
        Vec3 position = habitat.cell_center(x, z);
        position.x += (random.unit() - 0.5) * config.cell_size * 0.8;
        position.z += (random.unit() - 0.5) * config.cell_size * 0.8;
        const double fit = habitat_fitness(cell, definition);
        if (fit > best_fit) {
            best_fit = fit;
            best = position;
        }
        if (fit >= 0.55 && attempt >= 8) {
            return position;
        }
    }
    return best;
}

Vec3 grouped_land_position(const HabitatGrid& habitat,
                           ScenarioRandom& random,
                           const SpeciesDefinition& definition,
                           Vec3 anchor) {
    const double radius =
        std::max(habitat.config().cell_size,
                 std::min(definition.social_radius * 0.45, definition.home_range_radius * 0.4));
    for (int attempt = 0; attempt < 80; ++attempt) {
        const double angle = random.unit() * 2.0 * std::numbers::pi;
        const double distance = std::sqrt(random.unit()) * radius;
        const Vec3 candidate{
            anchor.x + std::cos(angle) * distance,
            anchor.y,
            anchor.z + std::sin(angle) * distance,
        };
        const HabitatCell* cell = habitat.cell_at(candidate);
        if (cell != nullptr && habitat_fitness(*cell, definition) >= 0.28) {
            return candidate;
        }
    }
    return anchor;
}

double initial_age(const SpeciesDefinition& definition, ScenarioRandom& random) {
    if (definition.kind == EntityKind::plant) {
        const double minimum = std::min(definition.maturity_hours, definition.lifespan_hours * 0.1);
        const double span = std::max(0.0, definition.lifespan_hours * 0.35 - minimum);
        return minimum + random.unit() * span;
    }
    const double minimum = definition.maturity_hours * 0.65;
    const double maximum = std::min(definition.lifespan_hours * 0.45,
                                    std::max(minimum, definition.maturity_hours * 3.0));
    return minimum + random.unit() * std::max(0.0, maximum - minimum);
}

using SeededPositions = std::unordered_map<SpeciesId, std::vector<Vec3>>;

std::optional<Vec3> food_linked_anchor(const HabitatGrid& habitat,
                                       ScenarioRandom& random,
                                       const SpeciesDefinition& definition,
                                       const SeededPositions& seeded_positions) {
    std::size_t available = 0;
    for (const SpeciesId food_id : definition.food_species) {
        if (const auto it = seeded_positions.find(food_id); it != seeded_positions.end()) {
            available += it->second.size();
        }
    }
    if (available == 0) {
        return std::nullopt;
    }

    std::size_t choice = std::min(
        available - 1,
        static_cast<std::size_t>(random.unit() * static_cast<double>(available)));
    for (const SpeciesId food_id : definition.food_species) {
        const auto it = seeded_positions.find(food_id);
        if (it == seeded_positions.end()) {
            continue;
        }
        if (choice < it->second.size()) {
            return grouped_land_position(habitat, random, definition, it->second[choice]);
        }
        choice -= it->second.size();
    }
    return std::nullopt;
}

} // namespace

std::size_t ScenarioSeedResult::total_seeded() const noexcept {
    return std::accumulate(populations.begin(), populations.end(), std::size_t{0},
                           [](std::size_t total, const SeededPopulation& population) {
                               return total + population.seeded;
                           });
}

bool ScenarioSeedResult::complete() const noexcept {
    return std::ranges::all_of(populations, [](const SeededPopulation& population) {
        return population.seeded == population.requested;
    });
}

ScenarioSeedResult seed_temperate_island(World& world, const IslandScenarioConfig& config) {
    // Seed resources first so consumers can anchor their first groups near already
    // placed food. This keeps the initial world locally coherent without coupling
    // the scenario builder to World internals.
    const std::array<std::pair<SpeciesId, std::size_t>, 29> requested{{
        {species::grass, config.grass},
        {species::clover, config.clover},
        {species::wildflower, config.wildflower},
        {species::oak, config.oak},
        {species::birch, config.birch},
        {species::pine, config.pine},
        {species::willow, config.willow},
        {species::berry_bush, config.berry_bush},
        {species::fern, config.fern},
        {species::reeds, config.reeds},
        {species::mushroom, config.mushroom},
        {species::rabbit, config.rabbit},
        {species::deer, config.deer},
        {species::mouse, config.mouse},
        {species::hare, config.hare},
        {species::boar, config.boar},
        {species::vole, config.vole},
        {species::bee, config.bee},
        {species::butterfly, config.butterfly},
        {species::beetle, config.beetle},
        {species::ant, config.ant},
        {species::bumblebee, config.bumblebee},
        {species::moth, config.moth},
        {species::robin, config.robin},
        {species::hedgehog, config.hedgehog},
        {species::frog, config.frog},
        {species::wolf, config.wolf},
        {species::fox, config.fox},
        {species::owl, config.owl},
    }};

    ScenarioRandom random(config.seed);
    ScenarioSeedResult result;
    result.populations.reserve(requested.size());
    SeededPositions seeded_positions;

    for (const auto& [species_id, count] : requested) {
        SeededPopulation population{species_id, count, 0};
        const SpeciesDefinition* definition = world.species().find(species_id);
        if (definition == nullptr) {
            result.populations.push_back(population);
            continue;
        }
        Vec3 group_anchor{};
        const std::size_t group_size = std::max<std::size_t>(1, definition->initial_group_size);
        for (std::size_t i = 0; i < count; ++i) {
            const bool starts_group = i % group_size == 0;
            if (starts_group) {
                const std::optional<Vec3> linked =
                    food_linked_anchor(world.habitat(), random, *definition, seeded_positions);
                group_anchor = linked.value_or(
                    random_land_position(world.habitat(), random, *definition));
            }
            const Vec3 position =
                starts_group || group_size == 1
                    ? group_anchor
                    : grouped_land_position(world.habitat(), random, *definition, group_anchor);
            const double energy =
                is_animal(definition->kind)
                    ? definition->max_energy * (0.65 + random.unit() * 0.3)
                    : -1.0;
            if (world.enqueue_organism(species_id, position, energy,
                                       initial_age(*definition, random)) != 0) {
                ++population.seeded;
                seeded_positions[species_id].push_back(position);
            }
        }
        result.populations.push_back(population);
    }
    world.flush_commands();
    return result;
}

} // namespace sim
