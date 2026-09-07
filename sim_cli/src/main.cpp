#include "sim/sim.hpp"

#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <string>
#include <string_view>

namespace {

struct Options {
    int ticks = 1'440;
    int agents = 8;
    int report_every = 240;
    double hz = 60.0;
    double ecology_hours = 1.0 / 6.0;
    std::uint64_t seed = 42;
    std::string scenario = "island";
};

void print_usage(std::string_view argv0) {
    std::cerr
        << "Usage: " << argv0
        << " [--scenario island|agents] [--ticks N] [--ecology-hours N]\n"
           "       [--seed N] [--report-every N] [--agents N] [--hz N]\n";
}

bool parse_int(std::string_view text, int& out) {
    try {
        std::size_t idx = 0;
        const int value = std::stoi(std::string(text), &idx);
        if (idx != text.size() || value < 0) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_double(std::string_view text, double& out) {
    try {
        std::size_t idx = 0;
        const double value = std::stod(std::string(text), &idx);
        if (idx != text.size() || !std::isfinite(value) || value <= 0.0) {
            return false;
        }
        out = value;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_u64(std::string_view text, std::uint64_t& out) {
    try {
        std::size_t idx = 0;
        const auto value = std::stoull(std::string(text), &idx);
        if (idx != text.size()) {
            return false;
        }
        out = static_cast<std::uint64_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_args(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg = argv[static_cast<std::size_t>(i)];
        if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        }
        if (i + 1 >= argc) {
            return false;
        }
        const std::string_view value = argv[static_cast<std::size_t>(i + 1)];
        if (arg == "--scenario") {
            if (value != "island" && value != "agents") {
                return false;
            }
            options.scenario = value;
        } else if (arg == "--ticks") {
            if (!parse_int(value, options.ticks)) {
                return false;
            }
        } else if (arg == "--agents") {
            if (!parse_int(value, options.agents)) {
                return false;
            }
        } else if (arg == "--hz") {
            if (!parse_double(value, options.hz)) {
                return false;
            }
        } else if (arg == "--ecology-hours") {
            if (!parse_double(value, options.ecology_hours)) {
                return false;
            }
        } else if (arg == "--seed") {
            if (!parse_u64(value, options.seed)) {
                return false;
            }
        } else if (arg == "--report-every") {
            if (!parse_int(value, options.report_every)) {
                return false;
            }
        } else {
            return false;
        }
        ++i;
    }
    return true;
}

void spawn_demo_agents(sim::World& world, int count) {
    const double n = count > 0 ? static_cast<double>(count) : 1.0;
    for (int i = 0; i < count; ++i) {
        const double angle = (2.0 * std::numbers::pi * static_cast<double>(i)) / n;
        const sim::Vec3 position{std::cos(angle) * 4.0, 0.4, std::sin(angle) * 4.0};
        const sim::Vec3 velocity{-std::sin(angle) * 2.0, 0.0, std::cos(angle) * 2.0};
        static_cast<void>(world.enqueue_spawn(position, velocity));
    }
}

const char* season_name(double year_phase) {
    if (year_phase < 0.125 || year_phase >= 0.875) {
        return "winter";
    }
    if (year_phase < 0.375) {
        return "spring";
    }
    if (year_phase < 0.625) {
        return "summer";
    }
    return "autumn";
}

void print_ecosystem_report(const sim::World& world) {
    const sim::EcosystemStats stats = world.ecosystem_stats();
    std::cout << std::fixed << std::setprecision(2) << "tick=" << world.tick_index()
              << " day=" << stats.simulated_hours / 24.0
              << " hour=" << world.hour_of_day()
              << " season=" << season_name(stats.year_phase)
              << " moisture=" << stats.mean_moisture
              << " temp=" << stats.mean_temperature << "C"
              << " canopy=" << std::setprecision(3) << stats.mean_canopy
              << "(max " << stats.max_canopy << ")"
              << " light=" << stats.mean_light
              << " organic=" << stats.mean_organic
              << "(max " << stats.max_organic << ")"
              << " pollination=" << stats.mean_pollination
              << "(max " << stats.max_pollination << ")"
              << std::setprecision(2)
              << " plants=" << stats.plants << " herbivores=" << stats.herbivores
              << " omnivores=" << stats.omnivores << " carnivores=" << stats.carnivores
              << " insects=" << stats.insects << " decomposers=" << stats.decomposers
              << " insect_activity=" << stats.mean_insect_activity
              << " behavior(rest=" << stats.resting << ",roam=" << stats.roaming
              << ",social=" << stats.socializing << ",forage=" << stats.foraging
              << ",feed=" << stats.feeding << ",drink=" << stats.drinking
              << ",flee=" << stats.fleeing << ')';
    for (const sim::SpeciesPopulation& population : stats.populations) {
        if (const sim::SpeciesDefinition* definition = world.species().find(population.species_id);
            definition != nullptr) {
            std::cout << ' ' << definition->key << '=' << population.count;
            if (sim::is_animal(definition->kind) && population.count != 0) {
                std::cout << "(e" << population.mean_energy_fraction << ",h"
                          << population.mean_hydration << ')';
            }
            const auto dead = population.deaths_age + population.deaths_starvation +
                              population.deaths_dehydration + population.deaths_biomass_loss;
            if (dead != 0) {
                std::cout << "[dead age=" << population.deaths_age
                          << ",food=" << population.deaths_starvation
                          << ",water=" << population.deaths_dehydration
                          << ",biomass=" << population.deaths_biomass_loss << ']';
            }
        }
    }
    std::cout << " deaths(age=" << stats.deaths_age << ",food=" << stats.deaths_starvation
              << ",water=" << stats.deaths_dehydration
              << ",biomass=" << stats.deaths_biomass_loss << ")\n";
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parse_args(argc, argv, options)) {
        print_usage(argv[0]);
        return 2;
    }

    sim::WorldConfig config;
    config.tick_dt = 1.0 / options.hz;
    config.ecology_hours_per_tick = options.ecology_hours;
    config.seed = options.seed;
    sim::World world(config);

    if (options.scenario == "island") {
        sim::IslandScenarioConfig island;
        island.seed = options.seed;
        const sim::ScenarioSeedResult seeded = sim::seed_temperate_island(world, island);
        if (!seeded.complete()) {
            std::cerr << "Failed to seed the complete island population\n";
            return 1;
        }
        const auto& habitat = world.habitat().config();
        std::cout << "scenario=island seed=" << options.seed
                  << " entities=" << seeded.total_seeded()
                  << " map=" << habitat.width << "x" << habitat.height << '\n';
        print_ecosystem_report(world);
    } else {
        spawn_demo_agents(world, options.agents);
        world.flush_commands();
    }

    for (int i = 0; i < options.ticks; ++i) {
        world.tick();
        if (options.scenario == "island" && options.report_every > 0 &&
            (i + 1) % options.report_every == 0) {
            print_ecosystem_report(world);
        }
    }

    if (options.scenario == "island") {
        if (options.report_every <= 0 || options.ticks % options.report_every != 0) {
            print_ecosystem_report(world);
        }
        return 0;
    }

    const sim::Snapshot snap = world.snapshot();
    std::cout << "tick=" << snap.tick << " entities=" << snap.entities.size()
              << " dt=" << snap.tick_dt << '\n';
    for (const sim::EntityState& entity : snap.entities) {
        std::cout << "  id=" << entity.id << " pos=(" << entity.position.x << ", "
                  << entity.position.y << ", " << entity.position.z << ") vel=("
                  << entity.velocity.x << ", " << entity.velocity.y << ", " << entity.velocity.z
                  << ")\n";
    }
    return 0;
}
