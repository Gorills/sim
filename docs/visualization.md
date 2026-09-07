# Visualization architecture

The Godot visualizer deliberately separates simulation data, input intent, camera
behavior, and presentation. Keep these contracts narrow when extending it.

## Input contract

Physical bindings live only in `godot/project.godot` under InputMap. Runtime code
uses semantic names from `godot/scripts/input_actions.gd`; it must not check
`KEY_*`, mouse button constants, or joypad axes directly.

| Intent | Actions | Default keyboard/mouse | Default gamepad |
| --- | --- | --- | --- |
| Move/pan | `camera_move_*` | WASD | left stick / D-pad |
| Orbit | `camera_orbit_*`, `camera_orbit_drag` | RMB drag, Q/E yaw | right stick |
| Zoom | `camera_zoom_in/out` | wheel | LB/RB |
| World map | `view_toggle_overview` | M | Y |
| Pause | `sim_toggle_pause` | Space | Start |
| Reset | `sim_reset` | R | — |
| Speed | `sim_speed_1/2/3` | 1/2/3 | — |

Movement and orbit axes are read with `Input.get_vector()`, which gives one
2D intent vector with a circular deadzone suitable for keyboard, D-pad, and
analog sticks. Camera code does not know which physical device produced it.

Source:
- Godot Input: https://docs.godotengine.org/en/4.5/classes/class_input.html
- Godot InputMap: https://docs.godotengine.org/en/4.5/classes/class_inputmap.html

## Camera modes

### Local detail

`camera_controller.gd` owns target, distance, yaw, pitch, smoothing, bounds,
and render-interest synchronization. The camera is perspective. Movement speed
scales with camera distance; RMB orbit captures the pointer while dragging.

The render center is snapped to 300 m steps. This avoids rebuilding the habitat
window for tiny camera movements while keeping navigation continuous.

### Whole-world map

The whole-world camera is orthographic. It pans and zooms independently, and
exiting the map makes the selected map position the new local camera target.

The map never increases the detailed render radius to the island size.
`world_map_view.gd` consumes only `SimWorld.get_world_overview()`, a 64×64
aggregated representation. Detailed terrain, organisms, ocean, fog, and the
local minimap are disabled while this mode is active.

Source:
- Godot Camera3D projection: https://docs.godotengine.org/en/4.5/classes/class_camera3d.html

## Rendering tiers

There are two presentation tiers and they must remain separate:

1. **Detailed interest window** — `get_render_snapshot()` +
   `get_render_habitat_grid()`; used by `world_view.gd`.
2. **Coarse whole-world overview** — `get_world_overview()`; used by
   `world_map_view.gd` and `world_overview.gd`.

Terrain3D uses 64-vertex regions. With the island's 75 m vertex spacing, one
region spans 4.8 km. `world_view.gd` activates only the regions intersecting
the current habitat window and deactivates regions that leave it. Do not return
to Terrain3D's default 256-vertex region here: at 75 m spacing it spans 19.2 km,
the full island width.

Source:
- Terrain3D data/region API:
  https://terrain3d.readthedocs.io/en/stable/api/class_terrain3ddata.html
- Terrain3D API:
  https://terrain3d.readthedocs.io/en/stable/api/class_terrain3d.html

## Localization

UI source strings live in versioned gettext catalogs under `godot/i18n/`.
Code and scenes use stable message IDs such as `HUD_SIMULATION` and
`UI_VIEW_MAP`, never English prose as lookup keys. `project.godot` loads the
`en.po` and `ru.po` catalogs directly and explicitly falls back to English.

PO is used instead of generated CSV `*.translation` binaries so a clean
checkout has no localization bootstrap dependency, works cleanly in CI, and can
grow into gettext contexts and plural forms without changing the runtime
contract.

When adding interface text:

1. add one stable ID to every locale catalog;
2. keep placeholders semantically equivalent between locales;
3. use `tr("ID")` for dynamic text or the ID as the Control text for
   auto-translated static text.

Source:
- Godot internationalization:
  https://docs.godotengine.org/en/4.5/tutorials/i18n/internationalizing_games.html
- Godot gettext localization:
  https://docs.godotengine.org/en/4.5/tutorials/i18n/localization_using_gettext.html

## UI design system

`godot/ui/sim_theme.tres` is the project-wide Theme configured in
`project.godot`. Shared typography, panel surfaces, borders, spacing, button
states, and focus treatment belong there. Scene nodes should not add
`theme_override_*` values for reusable visual rules.

Simulation-specific map colors live separately in
`visualization_palette.gd`; they are data-visualization semantics, not widget
theme tokens.

Source:
- Godot themes:
  https://docs.godotengine.org/en/4.5/tutorials/ui/gui_using_theme_editor.html

## Regression contract

`godot/scripts/check_extension.gd` verifies:

- all semantic actions exist and have bindings;
- keyboard/mouse and gamepad paths are represented;
- the project Theme loads;
- the Russian translation imports and resolves;
- Terrain3D uses 64-vertex regions;
- a 1.2 km interest window stays bounded before and after a large move;
- the coarse world map builds from aggregated overview data.

CI fetches the pinned Terrain3D release, imports the project, runs this contract
check, and then smoke-runs the main scene.
