#pragma once

#include "sim/simulation_lod.hpp"
#include "sim/world.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <future>
#include <memory>
#include <mutex>
#include <thread>

namespace sim {

struct RuntimeOptions {
    double speed_scale = 1.0;
    Vec3 render_center{};
    double render_radius = 450.0;
    std::size_t overview_resolution = 48;
    SimulationLodConfig lod{};
};

struct RuntimeFrame {
    std::shared_ptr<const Snapshot> previous_render{};
    std::shared_ptr<const Snapshot> current_render{};
    std::shared_ptr<const Snapshot> full_snapshot{};
    std::shared_ptr<const HabitatSnapshot> full_habitat{};
    std::shared_ptr<const HabitatSnapshot> render_habitat{};
    std::shared_ptr<const OverviewSnapshot> overview{};
    std::shared_ptr<const EcosystemStats> stats{};

    std::uint64_t tick = 0;
    std::size_t entity_count = 0;
    double hour_of_day = 0.0;
    bool paused = false;
    Vec3 render_center{};
    double render_radius = 0.0;
    std::uint64_t render_generation = 0;
    SimulationLodSummary lod_summary{};
};

class SimulationRuntime {
public:
    SimulationRuntime(std::unique_ptr<World> world, RuntimeOptions options = {});
    ~SimulationRuntime();

    SimulationRuntime(const SimulationRuntime&) = delete;
    SimulationRuntime& operator=(const SimulationRuntime&) = delete;

    void start();
    void stop();
    [[nodiscard]] bool running() const noexcept { return running_.load(); }

    void request_paused(bool paused);
    [[nodiscard]] bool requested_paused() const noexcept { return paused_.load(); }

    void request_speed_scale(double speed_scale);
    [[nodiscard]] double speed_scale() const noexcept { return speed_scale_.load(); }

    void request_timing(double tick_dt, double ecology_hours_per_tick);
    [[nodiscard]] double tick_dt() const noexcept { return tick_dt_.load(); }
    [[nodiscard]] double ecology_hours_per_tick() const noexcept {
        return ecology_hours_per_tick_.load();
    }

    void request_render_interest(Vec3 center, double radius, bool refresh_habitat);
    void request_overview_resolution(std::size_t resolution);

    [[nodiscard]] EntityId enqueue_spawn(Vec3 position, Vec3 velocity);
    void request_despawn(EntityId id);

    [[nodiscard]] std::shared_ptr<const RuntimeFrame> frame() const;
    [[nodiscard]] double render_alpha() const noexcept;

private:
    enum class CommandKind : std::uint8_t {
        set_paused,
        set_timing,
        render_interest,
        overview_resolution,
        spawn,
        despawn,
    };

    struct Command {
        CommandKind kind = CommandKind::set_paused;
        bool bool_value = false;
        double first = 0.0;
        double second = 0.0;
        std::size_t size_value = 0;
        Vec3 vector_value{};
        Vec3 vector_value2{};
        EntityId id = 0;
        std::shared_ptr<std::promise<EntityId>> spawn_result{};
    };

    using Clock = std::chrono::steady_clock;

    void push(Command command);
    void worker_loop();
    bool process_commands();
    void refresh_render_interest();
    void refresh_overview();
    void refresh_full_telemetry(Clock::time_point now, bool force);
    void publish(Clock::time_point now, bool tick_advanced);
    void publish_initial();

    std::unique_ptr<World> world_{};
    SimulationLodGrid lod_grid_;

    mutable std::mutex published_mutex_{};
    std::shared_ptr<const RuntimeFrame> published_{};
    RuntimeFrame working_{};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<bool> paused_{false};
    std::atomic<double> speed_scale_{1.0};
    std::atomic<double> tick_dt_{1.0 / 60.0};
    std::atomic<double> ecology_hours_per_tick_{1.0 / 60.0};
    std::atomic<double> last_tick_wall_seconds_{0.0};

    std::thread worker_{};
    std::mutex command_mutex_{};
    std::condition_variable command_cv_{};
    std::deque<Command> commands_{};

    Vec3 render_center_{};
    double render_radius_ = 450.0;
    std::size_t overview_resolution_ = 48;
    bool render_habitat_dirty_ = false;
    std::uint64_t render_generation_ = 0;
    bool overview_dirty_ = false;

    Clock::time_point last_stats_refresh_{};
    Clock::time_point last_full_snapshot_refresh_{};
    Clock::time_point last_full_habitat_refresh_{};
    Clock::time_point last_overview_refresh_{};
};

} // namespace sim
