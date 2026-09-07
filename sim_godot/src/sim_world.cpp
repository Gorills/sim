#include "sim_world.hpp"

#include "convert.hpp"
#include "sim_entity_state.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <utility>
#include <vector>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

using namespace godot;

namespace {

// Third-person 1x is an observation pace, not a 15-minute-per-second timelapse.
// One simulated minute per real second keeps meter-scale animal locomotion readable.
constexpr double kEcologyHoursPerRealSecond = 1.0 / 60.0;

PackedFloat32Array to_packed_floats(const std::vector<double>& values) {
    PackedFloat32Array out;
    out.resize(static_cast<int64_t>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        out[static_cast<int64_t>(i)] = static_cast<float>(values[i]);
    }
    return out;
}

PackedByteArray to_packed_bytes(const std::vector<std::uint8_t>& values) {
    PackedByteArray out;
    out.resize(static_cast<int64_t>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        out[static_cast<int64_t>(i)] = values[i];
    }
    return out;
}

PackedInt32Array to_packed_ints(const std::vector<std::uint32_t>& values) {
    PackedInt32Array out;
    out.resize(static_cast<int64_t>(values.size()));
    for (std::size_t i = 0; i < values.size(); ++i) {
        out[static_cast<int64_t>(i)] = static_cast<std::int32_t>(values[i]);
    }
    return out;
}

} // namespace

SimWorld::SimWorld() {
    config_.ecology_hours_per_tick = config_.tick_dt * kEcologyHoursPerRealSecond;
}

SimWorld::~SimWorld() {
    if (runtime_) {
        runtime_->stop();
    }
}

void SimWorld::_bind_methods() {
    ClassDB::bind_method(D_METHOD("get_tick_hz"), &SimWorld::get_tick_hz);
    ClassDB::bind_method(D_METHOD("set_tick_hz", "hz"), &SimWorld::set_tick_hz);
    ClassDB::bind_method(D_METHOD("get_ecology_hours_per_tick"),
                         &SimWorld::get_ecology_hours_per_tick);
    ClassDB::bind_method(D_METHOD("is_paused"), &SimWorld::is_paused);
    ClassDB::bind_method(D_METHOD("set_paused", "paused"), &SimWorld::set_paused);
    ClassDB::bind_method(D_METHOD("get_demo_agent_count"), &SimWorld::get_demo_agent_count);
    ClassDB::bind_method(D_METHOD("set_demo_agent_count", "count"),
                         &SimWorld::set_demo_agent_count);
    ClassDB::bind_method(D_METHOD("is_island_mode"), &SimWorld::is_island_mode);
    ClassDB::bind_method(D_METHOD("set_island_mode", "enabled"), &SimWorld::set_island_mode);
    ClassDB::bind_method(D_METHOD("get_speed_scale"), &SimWorld::get_speed_scale);
    ClassDB::bind_method(D_METHOD("set_speed_scale", "speed_scale"), &SimWorld::set_speed_scale);
    ClassDB::bind_method(D_METHOD("get_tick_index"), &SimWorld::get_tick_index);
    ClassDB::bind_method(D_METHOD("get_entity_count"), &SimWorld::get_entity_count);
    ClassDB::bind_method(D_METHOD("get_render_center"), &SimWorld::get_render_center);
    ClassDB::bind_method(D_METHOD("set_render_center", "center"), &SimWorld::set_render_center);
    ClassDB::bind_method(D_METHOD("get_render_radius"), &SimWorld::get_render_radius);
    ClassDB::bind_method(D_METHOD("set_render_radius", "radius"), &SimWorld::set_render_radius);
    ClassDB::bind_method(D_METHOD("refresh_render_interest"), &SimWorld::refresh_render_interest);
    ClassDB::bind_method(D_METHOD("get_render_generation"), &SimWorld::get_render_generation);
    ClassDB::bind_method(D_METHOD("get_render_snapshot"), &SimWorld::get_render_snapshot);
    ClassDB::bind_method(D_METHOD("get_current_render_snapshot"),
                         &SimWorld::get_current_render_snapshot);
    ClassDB::bind_method(D_METHOD("get_render_alpha"), &SimWorld::get_render_alpha);
    ClassDB::bind_method(D_METHOD("get_sim_snapshot"), &SimWorld::get_sim_snapshot);
    ClassDB::bind_method(D_METHOD("get_habitat_grid"), &SimWorld::get_habitat_grid);
    ClassDB::bind_method(D_METHOD("get_render_habitat_grid"), &SimWorld::get_render_habitat_grid);
    ClassDB::bind_method(D_METHOD("get_world_overview", "resolution"), &SimWorld::get_world_overview);
    ClassDB::bind_method(D_METHOD("get_species_catalog"), &SimWorld::get_species_catalog);
    ClassDB::bind_method(D_METHOD("get_ecosystem_stats"), &SimWorld::get_ecosystem_stats);
    ClassDB::bind_method(D_METHOD("get_simulation_lod_stats"),
                         &SimWorld::get_simulation_lod_stats);
    ClassDB::bind_method(D_METHOD("spawn_agent", "position", "velocity"), &SimWorld::spawn_agent);
    ClassDB::bind_method(D_METHOD("despawn", "id"), &SimWorld::despawn);
    ClassDB::bind_method(D_METHOD("reset_world"), &SimWorld::reset_world);

    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "tick_hz"), "set_tick_hz", "get_tick_hz");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "paused"), "set_paused", "is_paused");
    ADD_PROPERTY(PropertyInfo(Variant::INT, "demo_agent_count"), "set_demo_agent_count",
                 "get_demo_agent_count");
    ADD_PROPERTY(PropertyInfo(Variant::BOOL, "island_mode"), "set_island_mode", "is_island_mode");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "speed_scale"), "set_speed_scale", "get_speed_scale");
    ADD_PROPERTY(PropertyInfo(Variant::VECTOR3, "render_center"), "set_render_center",
                 "get_render_center");
    ADD_PROPERTY(PropertyInfo(Variant::FLOAT, "render_radius"), "set_render_radius",
                 "get_render_radius");

    ADD_SIGNAL(MethodInfo("ticked", PropertyInfo(Variant::INT, "tick")));
}

void SimWorld::_ready() {
    set_process(true);
    set_process_priority(-100);
    ensure_runtime();
}

void SimWorld::_process(double delta) {
    static_cast<void>(delta);
    const int64_t tick = get_tick_index();
    if (tick != last_emitted_tick_) {
        last_emitted_tick_ = tick;
        emit_signal("ticked", tick);
    }
}

void SimWorld::set_tick_hz(double hz) {
    if (!std::isfinite(hz)) {
        return;
    }
    const double clamped = std::clamp(hz, 1.0, 1000.0);
    config_.tick_dt = 1.0 / clamped;
    config_.ecology_hours_per_tick = config_.tick_dt * kEcologyHoursPerRealSecond;
    if (runtime_) {
        runtime_->request_timing(config_.tick_dt, config_.ecology_hours_per_tick);
    }
}

double SimWorld::get_tick_hz() const {
    return 1.0 / config_.tick_dt;
}

double SimWorld::get_ecology_hours_per_tick() const {
    return config_.ecology_hours_per_tick;
}

void SimWorld::set_paused(bool paused) {
    paused_ = paused;
    if (runtime_) {
        runtime_->request_paused(paused);
    }
}

bool SimWorld::is_paused() const {
    return paused_;
}

void SimWorld::set_demo_agent_count(int32_t count) {
    demo_agent_count_ = std::max(0, count);
}

int32_t SimWorld::get_demo_agent_count() const {
    return demo_agent_count_;
}

void SimWorld::set_island_mode(bool enabled) {
    island_mode_ = enabled;
}

bool SimWorld::is_island_mode() const {
    return island_mode_;
}

void SimWorld::set_speed_scale(double speed_scale) {
    if (!std::isfinite(speed_scale)) {
        return;
    }
    speed_scale_ = std::clamp(speed_scale, 0.0, 64.0);
    if (runtime_) {
        runtime_->request_speed_scale(speed_scale_);
    }
}

double SimWorld::get_speed_scale() const {
    return speed_scale_;
}

std::shared_ptr<const sim::RuntimeFrame> SimWorld::published_frame() const {
    return runtime_ ? runtime_->frame() : std::shared_ptr<const sim::RuntimeFrame>{};
}

int64_t SimWorld::get_tick_index() const {
    const auto frame = published_frame();
    return frame ? static_cast<int64_t>(frame->tick) : 0;
}

int64_t SimWorld::get_entity_count() const {
    const auto frame = published_frame();
    return frame ? static_cast<int64_t>(frame->entity_count) : 0;
}

void SimWorld::set_render_center(godot::Vector3 center) {
    const sim::Vec3 value = sim_godot::from_godot(center);
    if (!std::isfinite(value.x) || !std::isfinite(value.z)) {
        return;
    }
    render_center_ = value;
}

godot::Vector3 SimWorld::get_render_center() const {
    return sim_godot::to_godot(render_center_);
}

void SimWorld::set_render_radius(double radius) {
    if (!std::isfinite(radius)) {
        return;
    }
    render_radius_ = std::clamp(radius, 150.0, 5'000.0);
}

double SimWorld::get_render_radius() const {
    return render_radius_;
}

void SimWorld::refresh_render_interest() {
    if (runtime_) {
        runtime_->request_render_interest(render_center_, render_radius_, true);
    }
}

int64_t SimWorld::get_render_generation() const {
    const auto frame = published_frame();
    return frame ? static_cast<int64_t>(frame->render_generation) : 0;
}

godot::Ref<SimSnapshot> SimWorld::get_render_snapshot() const {
    const auto frame = published_frame();
    if (!frame || !frame->current_render) {
        return make_snapshot({}, 1.0);
    }
    const double alpha = get_render_alpha();
    if (!frame->previous_render) {
        return make_snapshot(*frame->current_render, alpha);
    }
    return make_snapshot(sim::interpolate(*frame->previous_render, *frame->current_render, alpha),
                         alpha);
}

godot::Ref<SimSnapshot> SimWorld::get_current_render_snapshot() const {
    const auto frame = published_frame();
    return frame && frame->current_render ? make_snapshot(*frame->current_render, 1.0)
                                          : make_snapshot({}, 1.0);
}

double SimWorld::get_render_alpha() const {
    return runtime_ ? runtime_->render_alpha() : 1.0;
}

godot::Ref<SimSnapshot> SimWorld::get_sim_snapshot() const {
    const auto frame = published_frame();
    return frame && frame->full_snapshot ? make_snapshot(*frame->full_snapshot, 1.0)
                                         : make_snapshot({}, 1.0);
}

godot::Ref<SimHabitatGrid> SimWorld::make_habitat(const sim::HabitatSnapshot& habitat) const {
    godot::Ref<SimHabitatGrid> out;
    out.instantiate();
    out->set_width(static_cast<int32_t>(habitat.width));
    out->set_height(static_cast<int32_t>(habitat.height));
    out->set_cell_size(habitat.cell_size);
    out->set_origin(sim_godot::to_godot(habitat.origin));
    out->set_elevation(to_packed_floats(habitat.elevation));
    out->set_moisture(to_packed_floats(habitat.moisture));
    out->set_nutrients(to_packed_floats(habitat.nutrients));
    out->set_temperature(to_packed_floats(habitat.temperature));
    out->set_canopy(to_packed_floats(habitat.canopy));
    out->set_light(to_packed_floats(habitat.light));
    out->set_organic(to_packed_floats(habitat.organic));
    out->set_pollination(to_packed_floats(habitat.pollination));
    out->set_surface(to_packed_bytes(habitat.surface));
    return out;
}

godot::Ref<SimHabitatGrid> SimWorld::get_habitat_grid() const {
    const auto frame = published_frame();
    return frame && frame->full_habitat ? make_habitat(*frame->full_habitat)
                                        : make_habitat({});
}

godot::Ref<SimHabitatGrid> SimWorld::get_render_habitat_grid() const {
    const auto frame = published_frame();
    return frame && frame->render_habitat ? make_habitat(*frame->render_habitat)
                                          : make_habitat({});
}

godot::Dictionary SimWorld::get_world_overview(int32_t resolution) const {
    Dictionary out;
    if (!runtime_) {
        return out;
    }

    runtime_->request_overview_resolution(
        static_cast<std::size_t>(std::clamp(resolution, 16, 256)));
    const auto frame = published_frame();
    if (!frame || !frame->overview) {
        return out;
    }

    const sim::OverviewSnapshot& overview = *frame->overview;
    out["width"] = static_cast<int64_t>(overview.width);
    out["height"] = static_cast<int64_t>(overview.height);
    out["cell_size"] = overview.cell_size;
    out["origin"] = sim_godot::to_godot(overview.origin);
    out["surface"] = to_packed_bytes(overview.surface);
    out["plants"] = to_packed_ints(overview.plants);
    out["herbivores"] = to_packed_ints(overview.herbivores);
    out["omnivores"] = to_packed_ints(overview.omnivores);
    out["carnivores"] = to_packed_ints(overview.carnivores);
    out["insects"] = to_packed_ints(overview.insects);
    out["render_center"] = sim_godot::to_godot(render_center_);
    out["render_radius"] = render_radius_;
    return out;
}

godot::Array SimWorld::get_species_catalog() const {
    Array out;
    for (const sim::SpeciesDefinition& definition : species_catalog_) {
        Dictionary entry;
        entry["id"] = static_cast<int64_t>(definition.id);
        entry["key"] = String(definition.key.c_str());
        entry["display_name"] = String(definition.display_name.c_str());
        entry["kind"] = static_cast<int32_t>(definition.kind);
        entry["activity_peak_hour"] = definition.activity_peak_hour;
        entry["active_hours_per_day"] = definition.active_hours_per_day;
        entry["initial_group_size"] = static_cast<int64_t>(definition.initial_group_size);
        PackedStringArray tags;
        for (const std::string& tag : definition.tags) {
            tags.push_back(String(tag.c_str()));
        }
        entry["tags"] = tags;
        out.push_back(entry);
    }
    return out;
}

const sim::SpeciesDefinition* SimWorld::find_species(sim::SpeciesId id) const {
    for (const sim::SpeciesDefinition& definition : species_catalog_) {
        if (definition.id == id) {
            return &definition;
        }
    }
    return nullptr;
}

godot::Dictionary SimWorld::get_ecosystem_stats() const {
    Dictionary out;
    const auto frame = published_frame();
    if (!frame || !frame->stats) {
        return out;
    }

    const sim::EcosystemStats& stats = *frame->stats;
    out["tick"] = static_cast<int64_t>(frame->tick);
    out["entity_count"] = static_cast<int64_t>(frame->entity_count);
    out["hour_of_day"] = frame->hour_of_day;
    out["paused"] = paused_;
    out["simulated_hours"] = stats.simulated_hours;
    out["year_phase"] = stats.year_phase;
    out["mean_moisture"] = stats.mean_moisture;
    out["mean_temperature"] = stats.mean_temperature;
    out["mean_canopy"] = stats.mean_canopy;
    out["mean_light"] = stats.mean_light;
    out["mean_organic"] = stats.mean_organic;
    out["mean_pollination"] = stats.mean_pollination;
    out["max_canopy"] = stats.max_canopy;
    out["max_organic"] = stats.max_organic;
    out["max_pollination"] = stats.max_pollination;
    out["mean_insect_activity"] = stats.mean_insect_activity;
    out["plants"] = static_cast<int64_t>(stats.plants);
    out["herbivores"] = static_cast<int64_t>(stats.herbivores);
    out["omnivores"] = static_cast<int64_t>(stats.omnivores);
    out["carnivores"] = static_cast<int64_t>(stats.carnivores);
    out["insects"] = static_cast<int64_t>(stats.insects);
    out["decomposers"] = static_cast<int64_t>(stats.decomposers);
    out["resting"] = static_cast<int64_t>(stats.resting);
    out["roaming"] = static_cast<int64_t>(stats.roaming);
    out["socializing"] = static_cast<int64_t>(stats.socializing);
    out["foraging"] = static_cast<int64_t>(stats.foraging);
    out["feeding"] = static_cast<int64_t>(stats.feeding);
    out["drinking"] = static_cast<int64_t>(stats.drinking);
    out["fleeing"] = static_cast<int64_t>(stats.fleeing);

    Dictionary populations;
    for (const sim::SpeciesPopulation& population : stats.populations) {
        const sim::SpeciesDefinition* definition = find_species(population.species_id);
        if (definition == nullptr) {
            continue;
        }
        populations[String(definition->key.c_str())] = static_cast<int64_t>(population.count);
    }
    out["populations"] = populations;
    return out;
}

godot::Dictionary SimWorld::get_simulation_lod_stats() const {
    Dictionary out;
    const auto frame = published_frame();
    if (!frame) {
        return out;
    }
    const sim::SimulationLodSummary& lod = frame->lod_summary;
    out["total_regions"] = static_cast<int64_t>(lod.total_regions);
    out["individual_regions"] = static_cast<int64_t>(lod.individual_regions);
    out["cohort_regions"] = static_cast<int64_t>(lod.cohort_regions);
    out["aggregate_regions"] = static_cast<int64_t>(lod.aggregate_regions);
    out["due_regions"] = static_cast<int64_t>(lod.due_regions);
    return out;
}

int64_t SimWorld::spawn_agent(godot::Vector3 position, godot::Vector3 velocity) {
    ensure_runtime();
    if (!runtime_) {
        return 0;
    }
    const sim::EntityId id =
        runtime_->enqueue_spawn(sim_godot::from_godot(position), sim_godot::from_godot(velocity));
    return static_cast<int64_t>(id);
}

bool SimWorld::despawn(int64_t id) {
    if (id <= 0 || !runtime_) {
        return false;
    }
    runtime_->request_despawn(static_cast<sim::EntityId>(id));
    return true;
}

void SimWorld::reset_world() {
    if (runtime_) {
        runtime_->stop();
        runtime_.reset();
    }

    std::unique_ptr<sim::World> world = create_seeded_world();
    species_catalog_ = world->species().all();
    world->set_paused(paused_);

    sim::RuntimeOptions options;
    options.speed_scale = speed_scale_;
    options.render_center = render_center_;
    options.render_radius = render_radius_;
    options.overview_resolution = 48;

    runtime_ = std::make_unique<sim::SimulationRuntime>(std::move(world), options);
    runtime_->start();
    last_emitted_tick_ = -1;
}

void SimWorld::ensure_runtime() {
    if (!runtime_) {
        reset_world();
    }
}

std::unique_ptr<sim::World> SimWorld::create_seeded_world() {
    auto world = std::make_unique<sim::World>(config_);
    seed_initial_world(*world);
    return world;
}

void SimWorld::seed_initial_world(sim::World& world) {
    if (island_mode_) {
        sim::IslandScenarioConfig scenario;
        scenario.seed = config_.seed;
        static_cast<void>(sim::seed_temperate_island(world, scenario));
        return;
    }
    spawn_demo_agents(world);
    world.flush_commands();
}

void SimWorld::spawn_demo_agents(sim::World& world) {
    if (demo_agent_count_ <= 0) {
        return;
    }
    const double n = static_cast<double>(demo_agent_count_);
    for (int32_t i = 0; i < demo_agent_count_; ++i) {
        const double angle = (2.0 * std::numbers::pi * static_cast<double>(i)) / n;
        const sim::Vec3 position{std::cos(angle) * 4.0, 0.4, std::sin(angle) * 4.0};
        const sim::Vec3 velocity{-std::sin(angle) * 2.0, 0.0, std::cos(angle) * 2.0};
        static_cast<void>(world.enqueue_spawn(position, velocity));
    }
}

godot::Ref<SimSnapshot> SimWorld::make_snapshot(const sim::Snapshot& snapshot, double alpha) const {
    godot::Ref<SimSnapshot> out;
    out.instantiate();
    out->set_tick(static_cast<int64_t>(snapshot.tick));
    out->set_alpha(alpha);
    out->set_tick_dt(snapshot.tick_dt);
    out->set_simulated_hours(snapshot.simulated_hours);
    out->set_mean_moisture(snapshot.mean_moisture);
    out->set_mean_temperature(snapshot.mean_temperature);
    out->set_mean_canopy(snapshot.mean_canopy);
    out->set_mean_light(snapshot.mean_light);
    out->set_mean_organic(snapshot.mean_organic);
    out->set_mean_pollination(snapshot.mean_pollination);
    out->set_year_phase(snapshot.year_phase);
    out->set_hour_of_day(snapshot.hour_of_day);
    out->set_paused(snapshot.paused);

    godot::TypedArray<SimEntityState> entities;
    for (const sim::EntityState& src : snapshot.entities) {
        godot::Ref<SimEntityState> entity;
        entity.instantiate();
        entity->set_id(static_cast<int64_t>(src.id));
        entity->set_position(sim_godot::to_godot(src.position));
        entity->set_velocity(sim_godot::to_godot(src.velocity));
        entity->set_species_id(static_cast<int64_t>(src.species_id));
        entity->set_kind(static_cast<int32_t>(src.kind));
        entity->set_intent(static_cast<int32_t>(src.intent));
        entity->set_target_position(sim_godot::to_godot(src.target_position));
        entity->set_target_id(static_cast<int64_t>(src.target_id));
        entity->set_biomass(src.biomass);
        entity->set_energy(src.energy);
        entity->set_hydration(src.hydration);
        entities.push_back(entity);
    }
    out->set_entities(entities);
    return out;
}
