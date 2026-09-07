#include "sim/runtime.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace sim {

namespace {

double wall_seconds(std::chrono::steady_clock::time_point time) {
    return std::chrono::duration<double>(time.time_since_epoch()).count();
}

double sane_speed(double value) {
    if (!std::isfinite(value)) {
        return 1.0;
    }
    return std::clamp(value, 0.0, 64.0);
}

double sane_tick_dt(double value) {
    return std::isfinite(value) && value > 0.0 ? value : 1.0 / 60.0;
}

double sane_ecology_step(double value) {
    return std::isfinite(value) && value > 0.0 ? value : 1.0 / 60.0;
}

} // namespace

SimulationRuntime::SimulationRuntime(std::unique_ptr<World> world, RuntimeOptions options)
    : world_(std::move(world)),
      lod_grid_(world_ ? world_->config().bounds_min : Vec3{},
                world_ ? world_->config().bounds_max : Vec3{1.0, 1.0, 1.0},
                options.lod),
      paused_(world_ ? world_->paused() : false),
      speed_scale_(sane_speed(options.speed_scale)),
      tick_dt_(world_ ? world_->tick_dt() : 1.0 / 60.0),
      ecology_hours_per_tick_(world_ ? world_->ecology_hours_per_tick() : 1.0 / 60.0),
      render_center_(options.render_center),
      render_radius_(std::max(1.0, options.render_radius)),
      overview_resolution_(std::max<std::size_t>(1, options.overview_resolution)) {
    publish_initial();
}

SimulationRuntime::~SimulationRuntime() {
    stop();
}

void SimulationRuntime::start() {
    if (!world_ || running_.exchange(true)) {
        return;
    }
    stop_requested_.store(false);
    worker_ = std::thread(&SimulationRuntime::worker_loop, this);
}

void SimulationRuntime::stop() {
    if (!running_.exchange(false)) {
        if (worker_.joinable()) {
            stop_requested_.store(true);
            command_cv_.notify_all();
            worker_.join();
        }
        return;
    }
    stop_requested_.store(true);
    command_cv_.notify_all();
    if (worker_.joinable()) {
        worker_.join();
    }
}

void SimulationRuntime::push(Command command) {
    {
        std::lock_guard lock(command_mutex_);
        commands_.push_back(std::move(command));
    }
    command_cv_.notify_one();
}

void SimulationRuntime::request_paused(bool paused) {
    paused_.store(paused);
    Command command;
    command.kind = CommandKind::set_paused;
    command.bool_value = paused;
    push(std::move(command));
}

void SimulationRuntime::request_speed_scale(double speed_scale) {
    speed_scale_.store(sane_speed(speed_scale));
    command_cv_.notify_one();
}

void SimulationRuntime::request_timing(double tick_dt, double ecology_hours_per_tick) {
    tick_dt = sane_tick_dt(tick_dt);
    ecology_hours_per_tick = sane_ecology_step(ecology_hours_per_tick);
    tick_dt_.store(tick_dt);
    ecology_hours_per_tick_.store(ecology_hours_per_tick);

    Command command;
    command.kind = CommandKind::set_timing;
    command.first = tick_dt;
    command.second = ecology_hours_per_tick;
    push(std::move(command));
}

void SimulationRuntime::request_render_interest(Vec3 center,
                                                double radius,
                                                bool refresh_habitat) {
    Command command;
    command.kind = CommandKind::render_interest;
    command.vector_value = center;
    command.first = std::max(1.0, radius);
    command.bool_value = refresh_habitat;
    push(std::move(command));
}

void SimulationRuntime::request_overview_resolution(std::size_t resolution) {
    Command command;
    command.kind = CommandKind::overview_resolution;
    command.size_value = std::max<std::size_t>(1, resolution);
    push(std::move(command));
}

EntityId SimulationRuntime::enqueue_spawn(Vec3 position, Vec3 velocity) {
    if (!running()) {
        return 0;
    }
    auto result = std::make_shared<std::promise<EntityId>>();
    std::future<EntityId> future = result->get_future();

    Command command;
    command.kind = CommandKind::spawn;
    command.vector_value = position;
    command.vector_value2 = velocity;
    command.spawn_result = result;
    push(std::move(command));
    return future.get();
}

void SimulationRuntime::request_despawn(EntityId id) {
    if (id == 0) {
        return;
    }
    Command command;
    command.kind = CommandKind::despawn;
    command.id = id;
    push(std::move(command));
}

std::shared_ptr<const RuntimeFrame> SimulationRuntime::frame() const {
    std::lock_guard lock(published_mutex_);
    return published_;
}

double SimulationRuntime::render_alpha() const noexcept {
    if (paused_.load()) {
        return 1.0;
    }
    const double speed = speed_scale_.load();
    if (speed <= 0.0) {
        return 1.0;
    }
    const double interval = tick_dt_.load() / speed;
    if (!(interval > 0.0) || !std::isfinite(interval)) {
        return 1.0;
    }
    const double now = wall_seconds(Clock::now());
    const double elapsed = std::max(0.0, now - last_tick_wall_seconds_.load());
    return std::clamp(elapsed / interval, 0.0, 1.0);
}

void SimulationRuntime::publish_initial() {
    if (!world_) {
        std::lock_guard lock(published_mutex_);
        published_ = std::make_shared<RuntimeFrame>();
        return;
    }

    const auto now = Clock::now();
    auto render = std::make_shared<Snapshot>(world_->snapshot(render_center_, render_radius_));
    working_.previous_render = render;
    working_.current_render = render;
    working_.full_snapshot = std::make_shared<Snapshot>(world_->snapshot());
    working_.full_habitat = std::make_shared<HabitatSnapshot>(world_->habitat_snapshot());
    working_.render_habitat =
        std::make_shared<HabitatSnapshot>(world_->habitat_snapshot(render_center_, render_radius_ * 1.15));
    working_.overview =
        std::make_shared<OverviewSnapshot>(world_->overview_snapshot(overview_resolution_));
    working_.stats = std::make_shared<EcosystemStats>(world_->ecosystem_stats());
    working_.tick = world_->tick_index();
    working_.entity_count = world_->entity_count();
    working_.hour_of_day = world_->hour_of_day();
    working_.paused = world_->paused();
    working_.render_center = render_center_;
    working_.render_radius = render_radius_;
    working_.lod_summary = lod_grid_.summary(render_center_, working_.tick);

    last_stats_refresh_ = now;
    last_full_snapshot_refresh_ = now;
    last_full_habitat_refresh_ = now;
    last_overview_refresh_ = now;
    last_tick_wall_seconds_.store(wall_seconds(now));

    std::lock_guard lock(published_mutex_);
    published_ = std::make_shared<RuntimeFrame>(working_);
}

bool SimulationRuntime::process_commands() {
    std::deque<Command> pending;
    {
        std::lock_guard lock(command_mutex_);
        pending.swap(commands_);
    }
    if (pending.empty()) {
        return false;
    }

    bool publication_dirty = false;
    bool queued_world_command = false;
    for (Command& command : pending) {
        switch (command.kind) {
        case CommandKind::set_paused:
            world_->set_paused(command.bool_value);
            publication_dirty = true;
            break;
        case CommandKind::set_timing:
            world_->set_tick_dt(command.first);
            world_->set_ecology_hours_per_tick(command.second);
            publication_dirty = true;
            break;
        case CommandKind::render_interest:
            render_center_ = command.vector_value;
            render_radius_ = command.first;
            render_habitat_dirty_ = render_habitat_dirty_ || command.bool_value;
            refresh_render_interest();
            publication_dirty = true;
            break;
        case CommandKind::overview_resolution:
            if (overview_resolution_ != command.size_value) {
                overview_resolution_ = command.size_value;
                overview_dirty_ = true;
            }
            refresh_overview();
            publication_dirty = true;
            break;
        case CommandKind::spawn: {
            const EntityId id =
                world_->enqueue_spawn(command.vector_value, command.vector_value2);
            if (command.spawn_result) {
                command.spawn_result->set_value(id);
            }
            queued_world_command = true;
            break;
        }
        case CommandKind::despawn:
            world_->enqueue_despawn(command.id);
            queued_world_command = true;
            break;
        }
    }

    if (queued_world_command && world_->paused()) {
        world_->flush_commands();
        refresh_render_interest();
        working_.full_snapshot = std::make_shared<Snapshot>(world_->snapshot());
        working_.stats = std::make_shared<EcosystemStats>(world_->ecosystem_stats());
        publication_dirty = true;
    }

    if (publication_dirty) {
        publish(Clock::now(), false);
    }
    return true;
}

void SimulationRuntime::refresh_render_interest() {
    working_.previous_render = working_.current_render;
    working_.current_render =
        std::make_shared<Snapshot>(world_->snapshot(render_center_, render_radius_));
    if (!working_.previous_render) {
        working_.previous_render = working_.current_render;
    }
    if (render_habitat_dirty_ || !working_.render_habitat) {
        working_.render_habitat = std::make_shared<HabitatSnapshot>(
            world_->habitat_snapshot(render_center_, render_radius_ * 1.15));
        render_habitat_dirty_ = false;
    }
    working_.render_center = render_center_;
    working_.render_radius = render_radius_;
}

void SimulationRuntime::refresh_overview() {
    if (overview_dirty_ || !working_.overview) {
        working_.overview =
            std::make_shared<OverviewSnapshot>(world_->overview_snapshot(overview_resolution_));
        overview_dirty_ = false;
        last_overview_refresh_ = Clock::now();
    }
}

void SimulationRuntime::refresh_full_telemetry(Clock::time_point now, bool force) {
    using namespace std::chrono_literals;
    if (force || now - last_stats_refresh_ >= 500ms) {
        working_.stats = std::make_shared<EcosystemStats>(world_->ecosystem_stats());
        last_stats_refresh_ = now;
    }
    if (force || now - last_full_snapshot_refresh_ >= 1s) {
        working_.full_snapshot = std::make_shared<Snapshot>(world_->snapshot());
        last_full_snapshot_refresh_ = now;
    }
    if (force || now - last_full_habitat_refresh_ >= 2s) {
        working_.full_habitat = std::make_shared<HabitatSnapshot>(world_->habitat_snapshot());
        last_full_habitat_refresh_ = now;
    }
    if (force || overview_dirty_ || now - last_overview_refresh_ >= 2s) {
        working_.overview =
            std::make_shared<OverviewSnapshot>(world_->overview_snapshot(overview_resolution_));
        overview_dirty_ = false;
        last_overview_refresh_ = now;
    }
}

void SimulationRuntime::publish(Clock::time_point now, bool tick_advanced) {
    working_.tick = world_->tick_index();
    working_.entity_count = world_->entity_count();
    working_.hour_of_day = world_->hour_of_day();
    working_.paused = world_->paused();
    working_.render_center = render_center_;
    working_.render_radius = render_radius_;
    working_.lod_summary = lod_grid_.summary(render_center_, working_.tick);
    if (tick_advanced) {
        last_tick_wall_seconds_.store(wall_seconds(now));
    }

    auto frame = std::make_shared<RuntimeFrame>(working_);
    std::lock_guard lock(published_mutex_);
    published_ = std::move(frame);
}

void SimulationRuntime::worker_loop() {
    auto next_tick = Clock::now();
    double last_interval = -1.0;

    while (!stop_requested_.load()) {
        process_commands();
        if (stop_requested_.load()) {
            break;
        }

        const double speed = speed_scale_.load();
        const bool paused = paused_.load();
        if (paused || speed <= 0.0) {
            next_tick = Clock::now();
            std::unique_lock lock(command_mutex_);
            command_cv_.wait(lock, [this] {
                return stop_requested_.load() || !commands_.empty() ||
                       (!paused_.load() && speed_scale_.load() > 0.0);
            });
            continue;
        }

        const double interval_seconds = sane_tick_dt(tick_dt_.load()) / speed;
        const auto interval = std::chrono::duration_cast<Clock::duration>(
            std::chrono::duration<double>(interval_seconds));
        if (last_interval < 0.0 || std::abs(last_interval - interval_seconds) > 1.0e-9) {
            next_tick = Clock::now() + interval;
            last_interval = interval_seconds;
        }

        const auto now = Clock::now();
        if (now < next_tick) {
            std::unique_lock lock(command_mutex_);
            command_cv_.wait_until(lock, next_tick, [this] {
                return stop_requested_.load() || !commands_.empty() ||
                       paused_.load() || speed_scale_.load() <= 0.0;
            });
            continue;
        }

        working_.previous_render = working_.current_render;
        world_->tick();
        working_.current_render =
            std::make_shared<Snapshot>(world_->snapshot(render_center_, render_radius_));
        if (!working_.previous_render) {
            working_.previous_render = working_.current_render;
        }

        const auto published_at = Clock::now();
        refresh_full_telemetry(published_at, false);
        publish(published_at, true);
        next_tick += interval;
    }
}

} // namespace sim
