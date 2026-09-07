#pragma once

#include "sim/species.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace sim {

class World;

struct IslandScenarioConfig {
    std::uint64_t seed = 42;
    std::size_t grass = 2'200;
    std::size_t clover = 800;
    std::size_t oak = 140;
    std::size_t birch = 120;
    std::size_t pine = 140;
    std::size_t berry_bush = 350;
    std::size_t fern = 450;
    std::size_t reeds = 260;
    std::size_t mushroom = 300;
    std::size_t wildflower = 900;
    std::size_t willow = 80;
    std::size_t rabbit = 360;
    std::size_t deer = 80;
    std::size_t mouse = 500;
    std::size_t hare = 180;
    std::size_t boar = 45;
    std::size_t vole = 420;
    std::size_t bee = 220;
    std::size_t butterfly = 160;
    std::size_t beetle = 260;
    std::size_t ant = 360;
    std::size_t bumblebee = 180;
    std::size_t moth = 180;
    std::size_t robin = 120;
    std::size_t hedgehog = 70;
    std::size_t frog = 100;
    std::size_t wolf = 6;
    std::size_t fox = 18;
    std::size_t owl = 18;
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
