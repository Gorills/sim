# Visualization architecture

The Godot visualizer is now built around the same scale intended for a future
player-controlled third-person character. God mode changes movement freedom, not
camera scale.

## Camera contract

`camera_controller.gd` controls a visible spectator avatar. The camera stays
about 7 m behind the avatar with a 68° FOV. It is always perspective and never
switches to a top-down or whole-island projection.

God mode keeps:

- camera-relative horizontal movement through WASD / left stick;
- vertical flight through Q/E or LB/RB;
- captured-mouse orbit look and right-stick camera control;
- Escape to release the cursor and RMB to recapture it;
- wheel-adjustable movement speed;
- Shift / L3 boost.

The visible avatar turns toward movement independently from camera yaw. Camera
placement samples the presentation terrain between the avatar and the desired
7 m orbit position, preventing the camera from dropping through hills. The
current local ArrayMesh has no physics collider, so a SpringArm3D would not
provide collision yet; a future physical player should use CharacterBody3D plus
SpringArm3D once terrain collision exists.

The avatar starts on land in an aggregated overview cell containing organisms.
The spawn selector weights animals more heavily than vegetation so startup lands
in an inhabited area rather than an arbitrary island center.

The detailed render radius is fixed at 450 m and moves with the avatar after a
120 m hysteresis threshold. Camera FOV, distance, altitude, and movement speed do
not increase this radius.

## Local terrain

The runtime view no longer uses Terrain3D. `world_view.gd` requests only
`get_render_habitat_grid()` and builds a local ArrayMesh.

The ecology habitat is intentionally coarse (75 m cells), so each local cell is
subdivided four times for presentation. At a 450 m render radius this remains a
small mesh while avoiding the visibly huge triangles that were acceptable only
from an aerial camera.

Terrain rebuilds occur only after the simulation worker publishes a completed
render-interest generation or the view is reset. Camera movement never waits for
snapshot or habitat extraction: the old ready region remains visible until the
new region is published. There is no timer-driven full SurfaceTool/normal
regeneration in steady state. The clean-checkout CI no longer downloads
Terrain3D, which verifies that the player view has no hidden runtime dependency
on it.

## Organism presentation

Nearby organisms remain grouped in MultiMeshes by species. MultiMesh capacity
grows geometrically and is retained; normal population changes alter only
`visible_instance_count`.

Ecology still advances at 20 Hz, but organism transforms are presented every
render frame. On a new simulation tick, `world_view.gd` reads
`get_current_render_snapshot()` once and caches the previous/current
Transform3D for each visible entity. Between ticks it reads only the cheap
`get_render_alpha()` value and interpolates those cached transforms. This keeps
the expensive GDExtension snapshot/object conversion at simulation rate while
removing the former 20 FPS presentation cap.

## Simulation/render decoupling

The island contains roughly nine thousand initial organisms. `sim_core` now
owns the mutable `World` inside `SimulationRuntime` on a dedicated worker
thread. Godot does not call `World::tick()`, habitat scans, ecosystem-stat
scans, or overview aggregation from its frame loop. It consumes immutable
published `RuntimeFrame` data instead.

The visualizer requests 20 Hz simulation timing. At 1x, one real second advances
one simulated minute. Simulation speed controls 1x / 4x / 16x remain unchanged;
faster modes change worker simulation cadence without making the Godot frame
loop execute catch-up ticks.

The core also owns a fixed simulation-region grid and a phased LOD scheduler.
The default 600 m regions are classified as individual, cohort, or aggregate
around the simulation observer. Organisms in individual regions run every
ecology tick, cohort regions every 4 ticks, and aggregate regions every 20
ticks. Lower-frequency regions are phase-shifted so their work is distributed
instead of forming one synchronized spike.

This is real temporal simulation LOD: every organism stores the last ecology
tick applied to it. A deferred organism receives the full accumulated simulated
interval on its next scheduled update. Promotion to a higher-detail region also
catches up the elapsed interval immediately, so moving the observer cannot erase
world time. `get_simulation_lod_stats()` exposes actual updated/deferred entity
counts and maximum catch-up interval, not only region classification.

The representation is still individual at this stage. Cohort/aggregate
population state and conservation-preserving materialization/dematerialization
are the next simulation-core layer. Temporal LOD reduces update work now without
pretending that individual storage already has whole-world asymptotics.

## Debug minimap

`world_minimap.gd` is a separate 2D debug/navigation layer.

It uses a 48×48 aggregated `get_world_overview()` refresh every two seconds.
The whole world is never instantiated as 3D geometry. Population and surface
information are baked into a tiny ImageTexture, while the current camera marker
redraws independently.

A left click maps the minimap pixel to world X/Z and calls
`camera_controller.teleport_to()`. Teleport moves the avatar immediately and
queues new render interest. The previous ready terrain remains present while the
simulation worker prepares the destination snapshot/habitat; `world_view.gd`
switches only when the completed render generation is published.

## Input contract

Physical bindings live only in `godot/project.godot`. Runtime code uses
semantic actions from `godot/scripts/input_actions.gd`.

| Intent | Keyboard/mouse | Gamepad |
| --- | --- | --- |
| Move | WASD | left stick / D-pad |
| Fly down/up | Q / E | LB / RB |
| Camera orbit | captured mouse | right stick |
| Release / recapture cursor | Esc / RMB | — |
| Flight speed | mouse wheel | — |
| Boost | Shift | L3 |
| Pause | Space | Start |
| Simulation speed | 1 / 2 / 3 | — |
| Teleport | click minimap | — |

## Regression contract

`godot/scripts/check_extension.gd` verifies:

- semantic KBM/gamepad bindings and localization;
- immutable render snapshot/alpha/generation bridge used by the async runtime;
- simulation LOD region accounting (individual + cohort + aggregate = total);
- simulation LOD work telemetry for updated/deferred organisms and catch-up time;
- 20 Hz simulation timing with the one-simulated-minute-per-real-second 1x rate;
- an inhabited land spawn from the aggregated overview;
- perspective third-person camera distance near 7 m;
- a fixed 450 m render radius;
- local terrain grid and vertex budgets;
- absence of Terrain3D from the runtime view;
- minimap texture generation and pixel-to-world mapping;
- teleport to another inhabited location;
- local organism grounding.

CI performs release tests, sanitizer tests, builds the GDExtension, imports a
clean Godot project without Terrain3D, runs the visualization contract, and
smoke-runs the main scene.
