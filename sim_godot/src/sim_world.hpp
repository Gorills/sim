#pragma once

#include "sim/sim.hpp"
#include "sim_habitat_grid.hpp"
#include "sim_snapshot.hpp"

#include <cstdint>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/vector3.hpp>
#include <memory>
#include <vector>

class SimWorld : public godot::Node {
    GDCLASS(SimWorld, godot::Node)

public:
    SimWorld();
    ~SimWorld() override;

    void _ready() override;
    void _process(double delta) override;

    void set_tick_hz(double hz);
    [[nodiscard]] double get_tick_hz() const;
    [[nodiscard]] double get_ecology_hours_per_tick() const;

    void set_paused(bool paused);
    [[nodiscard]] bool is_paused() const;

    void set_demo_agent_count(int32_t count);
    [[nodiscard]] int32_t get_demo_agent_count() const;

    void set_island_mode(bool enabled);
    [[nodiscard]] bool is_island_mode() const;

    void set_speed_scale(double speed_scale);
    [[nodiscard]] double get_speed_scale() const;

    [[nodiscard]] int64_t get_tick_index() const;
    [[nodiscard]] int64_t get_entity_count() const;

    void set_render_center(godot::Vector3 center);
    [[nodiscard]] godot::Vector3 get_render_center() const;
    void set_render_radius(double radius);
    [[nodiscard]] double get_render_radius() const;
    void refresh_render_interest();
    [[nodiscard]] int64_t get_render_generation() const;

    [[nodiscard]] godot::Ref<SimSnapshot> get_render_snapshot() const;
    [[nodiscard]] godot::Ref<SimSnapshot> get_current_render_snapshot() const;
    [[nodiscard]] double get_render_alpha() const;
    [[nodiscard]] godot::Ref<SimSnapshot> get_sim_snapshot() const;
    [[nodiscard]] godot::Ref<SimHabitatGrid> get_habitat_grid() const;
    [[nodiscard]] godot::Ref<SimHabitatGrid> get_render_habitat_grid() const;
    [[nodiscard]] godot::Dictionary get_world_overview(int32_t resolution) const;
    [[nodiscard]] godot::Array get_species_catalog() const;
    [[nodiscard]] godot::Dictionary get_ecosystem_stats() const;
    [[nodiscard]] godot::Dictionary get_simulation_lod_stats() const;

    int64_t spawn_agent(godot::Vector3 position, godot::Vector3 velocity);
    bool despawn(int64_t id);
    void reset_world();

protected:
    static void _bind_methods();

private:
    void ensure_runtime();
    [[nodiscard]] std::unique_ptr<sim::World> create_seeded_world();
    void seed_initial_world(sim::World& world);
    void spawn_demo_agents(sim::World& world);
    [[nodiscard]] std::shared_ptr<const sim::RuntimeFrame> published_frame() const;
    [[nodiscard]] const sim::SpeciesDefinition* find_species(sim::SpeciesId id) const;
    [[nodiscard]] godot::Ref<SimSnapshot> make_snapshot(const sim::Snapshot& snapshot,
                                                        double alpha) const;
    [[nodiscard]] godot::Ref<SimHabitatGrid> make_habitat(
        const sim::HabitatSnapshot& habitat) const;

    sim::WorldConfig config_{};
    std::unique_ptr<sim::SimulationRuntime> runtime_{};
    std::vector<sim::SpeciesDefinition> species_catalog_{};
    int32_t demo_agent_count_ = 12;
    bool island_mode_ = true;
    bool paused_ = false;
    double speed_scale_ = 1.0;
    sim::Vec3 render_center_{};
    double render_radius_ = 450.0;
    int64_t last_emitted_tick_ = -1;
};
