# Visualization architecture

The Godot visualizer is now built around the same scale intended for a future
player-controlled third-person character. God mode changes movement freedom, not
camera scale.

## Camera contract

`camera_controller.gd` controls a visible spectator avatar. The camera stays
about 7 m behind the avatar with a 68° FOV. It is always perspective and never
switches to a top-down or whole-island projection.

God mode keeps:

- horizontal movement through WASD / left stick;
- vertical flight through Q/E or LB/RB;
- RMB / right-stick camera control;
- wheel-adjustable movement speed;
- Shift / L3 boost.

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

Terrain rebuilds occur when render interest moves and otherwise at a low
appearance refresh rate. The clean-checkout CI no longer downloads Terrain3D,
which verifies that the player view has no hidden runtime dependency on it.

## Organism presentation

Nearby organisms come from `get_render_snapshot()` and remain grouped in
MultiMeshes by species. MultiMesh capacity grows geometrically and is retained;
normal population changes alter only `visible_instance_count`.

Organism transforms are presented at 20 Hz. The simulation snapshot is
interpolated by the GDExtension, so render FPS does not require running ecology
at render FPS.

## Simulation/render decoupling

The island contains roughly nine thousand initial organisms. Running full
ecology at 60 Hz needlessly performs habitat, canopy, plant, spatial-index, and
animal updates sixty times per second.

The visualizer now runs ecology at 20 Hz. `SimWorld::set_tick_hz()` scales
`ecology_hours_per_tick` with the tick interval, preserving the previous
simulated-hours-per-real-second rate at 1x speed. In other words, the optimization
reduces update frequency without slowing ecological time.

Simulation speed controls 1x / 4x / 16x remain unchanged.

## Debug minimap

`world_minimap.gd` is a separate 2D debug/navigation layer.

It uses a 48×48 aggregated `get_world_overview()` refresh every two seconds.
The whole world is never instantiated as 3D geometry. Population and surface
information are baked into a tiny ImageTexture, while the current camera marker
redraws independently.

A left click maps the minimap pixel to world X/Z and calls
`camera_controller.teleport_to()`. Teleport immediately moves render interest,
rebuilds the local terrain around the destination, places the avatar above that
surface, and preserves the third-person camera scale.

## Input contract

Physical bindings live only in `godot/project.godot`. Runtime code uses
semantic actions from `godot/scripts/input_actions.gd`.

| Intent | Keyboard/mouse | Gamepad |
| --- | --- | --- |
| Move | WASD | left stick / D-pad |
| Fly down/up | Q / E | LB / RB |
| Camera | RMB drag | right stick |
| Flight speed | mouse wheel | — |
| Boost | Shift | L3 |
| Pause | Space | Start |
| Simulation speed | 1 / 2 / 3 | — |
| Teleport | click minimap | — |

## Regression contract

`godot/scripts/check_extension.gd` verifies:

- semantic KBM/gamepad bindings and localization;
- 20 Hz simulation timing with the preserved ecology-time rate;
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
