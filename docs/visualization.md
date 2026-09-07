# Visualization architecture

The Godot visualizer deliberately separates simulation state, input intent,
camera behavior, and presentation. The current viewer is a bounded 3D spectator
camera; there is no whole-world render mode.

## Input contract

Physical bindings live only in `godot/project.godot` under InputMap. Runtime
code uses semantic names from `godot/scripts/input_actions.gd`; it must not
check keyboard, mouse, or joypad constants directly.

| Intent | Actions | Keyboard/mouse | Gamepad |
| --- | --- | --- | --- |
| Horizontal flight | `camera_move_left/right/forward/back` | WASD | left stick / D-pad |
| Vertical flight | `camera_move_up/down` | E / Q | RB / LB |
| Look | `camera_look_*`, `camera_look_drag` | RMB drag | right stick |
| Camera speed | `camera_speed_increase/decrease` | wheel | — |
| Fast flight | `camera_speed_boost` | Shift | L3 |
| Pause | `sim_toggle_pause` | Space | Start |
| Reset | `sim_reset` | R | — |
| Simulation speed | `sim_speed_1/2/3` | 1 / 2 / 3 | — |

Movement and look axes use `Input.get_vector()`, providing one normalized 2D
intent with a circular deadzone for keyboard, D-pad, and analog sticks. Captured
mouse look uses relative mouse motion.

Sources:
- Godot Input: https://docs.godotengine.org/en/4.5/classes/class_input.html
- Godot InputMap: https://docs.godotengine.org/en/4.5/classes/class_inputmap.html

## Spectator camera

`camera_controller.gd` owns position, velocity smoothing, yaw, pitch, bounds,
flight speed, and render-interest synchronization.

The camera is always perspective. There is deliberately no orthographic or
whole-island mode. The spectator may fly anywhere inside the island bounds, but
only the area around the current camera position is presented in detail.

The render-interest radius is fixed at 1.2 km. Camera altitude and camera speed
never increase that radius. Spectator altitude is capped at 1.1 km and the
streaming center sits 350 m ahead of the camera's horizontal look direction, so
the normal downward sightline remains inside loaded terrain instead of exposing
only the ocean/background. The render center only changes after the desired
center has moved 550 m, providing hysteresis and avoiding rebuilds from small
movement or look changes.

This is the first camera contract for the future third-person viewer: movement
and look intents are already device-independent, while spectator-only vertical
flight and speed controls can later be disabled when a controlled actor is
introduced.

Source:
- Godot Camera3D: https://docs.godotengine.org/en/4.5/classes/class_camera3d.html

## Rendering contract

There is one presentation tier:

**Detailed interest window** — `get_render_snapshot()` plus
`get_render_habitat_grid()`, consumed by `world_view.gd`.

Do not add a renderer that consumes `get_world_overview()` every frame or on a
timer. `get_world_overview(16)` is currently used only once at startup/reset to
obtain island bounds metadata; it is not a presentation path.

The removed `world_map_view.gd`, `world_overview.gd`, and
`visualization_palette.gd` files are intentionally absent and CI checks that
they stay absent.

Terrain3D uses 64-vertex regions. At the island's 75 m vertex spacing, one
region spans 4.8 km. `world_view.gd` activates only regions intersecting the
current habitat window and deactivates regions that leave it. Returning to a
previously visited region reactivates the retained Terrain3D region rather than
creating an unbounded active set.

Sources:
- Terrain3D data/region API:
  https://terrain3d.readthedocs.io/en/stable/api/class_terrain3ddata.html
- Terrain3D API:
  https://terrain3d.readthedocs.io/en/stable/api/class_terrain3d.html

## Presentation performance

Presentation frequency is intentionally lower than simulation frequency.

- Simulation remains at 60 Hz.
- Organism transforms are presented at 20 Hz.
- HUD ecosystem statistics are refreshed at 2 Hz. This matters because
  `World::ecosystem_stats()` scans the full entity population.
- Terrain appearance data refreshes every 4 seconds unless the interest window
  moves; an interest move refreshes terrain immediately.
- MultiMesh capacity grows geometrically and is retained. Visible instance count
  changes without reallocating the backing buffer for normal population
  fluctuations.
- Organism MultiMeshes do not cast directional shadows in the aerial spectator
  view. Terrain shadows remain enabled with a bounded shadow distance.
- 3D MSAA is disabled for this viewer while performance is the priority.

Godot automatically performs view-frustum culling, but a single MultiMesh is
culled as one object rather than per instance. The local interest window keeps
each species MultiMesh bounded to the nearby population instead of relying on a
global MultiMesh.

Sources:
- Godot 3D performance:
  https://docs.godotengine.org/en/4.5/tutorials/performance/optimizing_3d_performance.html
- Godot MultiMesh optimization:
  https://docs.godotengine.org/en/4.5/tutorials/performance/using_multimesh.html

## Localization

UI strings live in versioned gettext catalogs under `godot/i18n/`. Code and
scenes use stable message IDs such as `HUD_SIMULATION` and `UI_VIEW_GOD`,
never English prose as lookup keys. `project.godot` loads `en.po` and
`ru.po` directly and explicitly falls back to English.

When adding interface text:

1. add one stable ID to every locale catalog;
2. keep placeholders semantically equivalent between locales;
3. use `tr("ID")` for dynamic text or the ID as Control text for
   auto-translated static text.

Sources:
- Godot internationalization:
  https://docs.godotengine.org/en/4.5/tutorials/i18n/internationalizing_games.html
- Godot gettext localization:
  https://docs.godotengine.org/en/4.5/tutorials/i18n/localization_using_gettext.html

## UI design system

`godot/ui/sim_theme.tres` is the project-wide Theme configured in
`project.godot`. Shared typography, panel surfaces, borders, spacing, button
states, and focus treatment belong there. Scene nodes should not add reusable
visual rules through local `theme_override_*` values.

Source:
- Godot themes:
  https://docs.godotengine.org/en/4.5/tutorials/ui/gui_using_theme_editor.html

## Regression contract

`godot/scripts/check_extension.gd` verifies:

- all semantic spectator actions exist and have bindings;
- keyboard/mouse and gamepad paths are represented;
- the project Theme loads;
- the Russian translation resolves;
- the old overview renderers remain removed;
- the spectator camera stays perspective with a fixed 1.2 km render radius;
- Terrain3D uses 64-vertex regions;
- active terrain regions stay bounded after a large move and reactivate
  correctly on return;
- organism grounding still matches the local terrain.

CI fetches the pinned Terrain3D release, imports the project, runs this contract
check, then smoke-runs the main scene.
