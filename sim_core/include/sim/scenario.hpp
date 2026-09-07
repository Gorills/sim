#pragma once

#include "sim/species.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sim {

class World;

struct IslandScenarioConfig {
    std::uint64_t seed = 42;
    std::size_t grass = 1'200;
    std::size_t clover = 450;
    std::size_t oak = 90;
    std::size_t birch = 70;
    std::size_t pine = 90;
    std::size_t berry_bush = 180;
    std::size_t fern = 220;
    std::size_t reeds = 120;
    std::size_t mushroom = 160;
    std::size_t rabbit = 240;
    std::size_t deer = 60;
    std::size_t mouse = 300;
    std::size_t hare = 120;
    std::size_t boar = 30;
    std::size_t wolf = 4;
    std::size_t fox = 10;
    std::size_t bee = 120;
    std::size_t butterfly = 90;
    std::size_t beetle = 120;
    std::size_t ant = 160;
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
