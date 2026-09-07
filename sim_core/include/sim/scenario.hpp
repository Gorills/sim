#pragma once

#include "sim/species.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sim {

class World;

struct IslandScenarioConfig {
    std::uint64_t seed = 42;
    std::size_t grass = 220;
    std::size_t clover = 90;
    std::size_t oak = 14;
    std::size_t birch = 12;
    std::size_t pine = 12;
    std::size_t berry_bush = 40;
    std::size_t fern = 48;
    std::size_t reeds = 28;
    std::size_t mushroom = 36;
    std::size_t rabbit = 22;
    std::size_t deer = 7;
    std::size_t mouse = 28;
    std::size_t hare = 16;
    std::size_t boar = 6;
    std::size_t wolf = 2;
    std::size_t fox = 4;
    std::size_t bee = 24;
    std::size_t butterfly = 18;
    std::size_t beetle = 22;
    std::size_t ant = 24;
};

struct SeededPopulation {
    SpeciesId species_id = 0;
    std::size_t requested = 0;
    std::size_t seeded = 0;
};

struct ScenarioSeedResult {
    std::vector<SeededPopulation> populations{};

    [[nodiscard]] std::size_t total_seeded() const noexcept;
    [[nodiscard]] bool complete() const noexcept;
};

[[nodiscard]] ScenarioSeedResult seed_temperate_island(
    World& world,
    const IslandScenarioConfig& config = {});

} // namespace sim
