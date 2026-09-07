extends SceneTree

## Headless regression check for the player-scale visualizer.

const Actions = preload("res://scripts/input_actions.gd")
const Minimap = preload("res://scripts/world_minimap.gd")

const EXPECTED_TICK_HZ := 20.0
const EXPECTED_ECOLOGY_STEP := 1.0 / 1200.0
const EXPECTED_RENDER_RADIUS := 450.0


func _initialize() -> void:
	if not _check_ui_contracts():
		quit(1)
		return

	var required_classes_ok := (
		ClassDB.class_exists("SimWorld")
		and ClassDB.class_exists("SimSnapshot")
		and ClassDB.class_exists("SimEntityState")
		and ClassDB.class_exists("SimHabitatGrid")
	)
	print("SIM_CHECK classes_ok=", required_classes_ok)
	if not required_classes_ok:
		push_error("Simulation GDExtension classes are not registered.")
		quit(1)
		return

	var world: Node = ClassDB.instantiate("SimWorld") as Node
	if world == null:
		push_error("Could not instantiate SimWorld.")
		quit(1)
		return

	world.set("island_mode", false)
	world.set("demo_agent_count", 3)
	world.call("reset_world")
	var bridge_snapshot: Object = world.call("get_sim_snapshot")
	var bridge_entities: Array = bridge_snapshot.get("entities") if bridge_snapshot != null else []
	var bridge_ok := bridge_snapshot != null and bridge_entities.size() == 3
	print("SIM_CHECK core_bridge_ok=", bridge_ok)
	if not bridge_ok:
		world.free()
		quit(1)
		return

	world.set("island_mode", true)
	world.set("tick_hz", EXPECTED_TICK_HZ)
	world.call("reset_world")

	var timing_ok := (
		is_equal_approx(float(world.call("get_tick_hz")), EXPECTED_TICK_HZ)
		and is_equal_approx(
			float(world.call("get_ecology_hours_per_tick")),
			EXPECTED_ECOLOGY_STEP
		)
	)
	print(
		"SIM_CHECK ecology_timing_ok=",
		timing_ok,
		" tick_hz=",
		world.call("get_tick_hz"),
		" ecology_step=",
		world.call("get_ecology_hours_per_tick")
	)
	if not timing_ok:
		world.free()
		quit(1)
		return

	var habitat: Object = world.call("get_habitat_grid")
	var habitat_width := int(habitat.get("width")) if habitat != null else 0
	var habitat_height := int(habitat.get("height")) if habitat != null else 0
	var habitat_cell_size := float(habitat.get("cell_size")) if habitat != null else 0.0
	var habitat_origin: Vector3 = habitat.get("origin") if habitat != null else Vector3.ZERO
	var surfaces: PackedByteArray = habitat.get("surface") if habitat != null else PackedByteArray()
	var habitat_ok := (
		habitat != null
		and habitat_width >= 80
		and habitat_height == habitat_width
		and surfaces.size() == habitat_width * habitat_height
		and int(world.call("get_entity_count")) > 100
	)
	print(
		"SIM_CHECK habitat_ok=",
		habitat_ok,
		" map=",
		habitat_width,
		"x",
		habitat_height,
		" entities=",
		world.call("get_entity_count")
	)
	if not habitat_ok:
		world.free()
		quit(1)
		return

	var bounds := Rect2(
		Vector2(habitat_origin.x, habitat_origin.z),
		Vector2(
			float(habitat_width) * habitat_cell_size,
			float(habitat_height) * habitat_cell_size
		)
	)
	var overview: Dictionary = world.call("get_world_overview", 48)
	var spawn := Minimap.inhabited_spawn(overview)
	var spawn_ok := _overview_point_is_populated_land(overview, spawn)
	print("SIM_CHECK inhabited_spawn_ok=", spawn_ok, " spawn=", spawn)
	if not spawn_ok:
		world.free()
		quit(1)
		return

	world.set("render_center", Vector3(spawn.x, 0.0, spawn.z))
	world.set("render_radius", EXPECTED_RENDER_RADIUS)
	world.call("refresh_render_interest")

	var render_bridge_ok := (
		world.has_method("get_current_render_snapshot")
		and world.has_method("get_render_alpha")
		and world.has_method("get_render_generation")
		and world.has_method("get_simulation_lod_stats")
	)
	var current_render_snapshot: Object = (
		world.call("get_current_render_snapshot")
		if render_bridge_ok
		else null
	)
	var render_alpha := (
		float(world.call("get_render_alpha"))
		if render_bridge_ok
		else -1.0
	)
	var render_generation := (
		int(world.call("get_render_generation"))
		if render_bridge_ok
		else -1
	)
	var lod_stats: Dictionary = (
		world.call("get_simulation_lod_stats")
		if render_bridge_ok
		else {}
	)
	var lod_total := int(lod_stats.get("total_regions", 0))
	var lod_sum := (
		int(lod_stats.get("individual_regions", 0))
		+ int(lod_stats.get("cohort_regions", 0))
		+ int(lod_stats.get("aggregate_regions", 0))
	)
	render_bridge_ok = (
		render_bridge_ok
		and current_render_snapshot != null
		and render_alpha >= 0.0
		and render_alpha <= 1.0
		and render_generation >= 0
		and lod_total > 0
		and lod_sum == lod_total
	)
	print(
		"SIM_CHECK render_bridge_ok=",
		render_bridge_ok,
		" alpha=",
		render_alpha,
		" generation=",
		render_generation,
		" lod_regions=",
		lod_total
	)
	if not render_bridge_ok:
		world.free()
		quit(1)
		return

	var viewport := SubViewport.new()
	viewport.name = "PlayerScaleCheckViewport"
	viewport.own_world_3d = true
	viewport.size = Vector2i(128, 128)
	root.add_child(viewport)

	var view := Node3D.new()
	view.name = "CheckWorldView"
	view.set_script(load("res://scripts/world_view.gd"))
	viewport.add_child(view)
	view.call("bind_sim", world)

	var camera := Camera3D.new()
	camera.name = "CheckCamera"
	viewport.add_child(camera)
	var sun := DirectionalLight3D.new()
	sun.name = "CheckSun"
	viewport.add_child(sun)
	var avatar := Node3D.new()
	avatar.name = "CheckAvatar"
	viewport.add_child(avatar)

	var camera_controller := Node.new()
	camera_controller.name = "CheckCameraController"
	camera_controller.set_script(load("res://scripts/camera_controller.gd"))
	camera_controller.set("camera_path", NodePath("../CheckCamera"))
	camera_controller.set("sun_path", NodePath("../CheckSun"))
	camera_controller.set("avatar_path", NodePath("../CheckAvatar"))
	camera_controller.set("world_view_path", NodePath("../CheckWorldView"))
	viewport.add_child(camera_controller)
	camera_controller.call("bind_sim", world, bounds, spawn)

	var anchor: Vector3 = camera_controller.call("anchor_position")
	var focus := anchor + Vector3.UP * 1.35
	var third_person_distance := camera.position.distance_to(focus)
	var camera_ok := (
		camera.projection == Camera3D.PROJECTION_PERSPECTIVE
		and is_equal_approx(float(camera_controller.call("render_radius")), EXPECTED_RENDER_RADIUS)
		and is_equal_approx(float(camera_controller.call("camera_distance")), 7.0)
		and is_equal_approx(float(world.get("render_radius")), EXPECTED_RENDER_RADIUS)
		and third_person_distance >= 5.0
		and third_person_distance <= 7.5
		and avatar.position.distance_to(anchor) < 0.01
	)
	print(
		"SIM_CHECK third_person_camera_ok=",
		camera_ok,
		" camera_distance=",
		third_person_distance,
		" render_radius=",
		world.get("render_radius")
	)
	if not camera_ok:
		viewport.free()
		world.free()
		quit(1)
		return

	var render_grid: Vector2i = view.call("render_grid_size")
	var terrain_vertices := int(view.call("terrain_vertex_count"))
	var local_terrain_ok := (
		render_grid.x > 0
		and render_grid.y > 0
		and render_grid.x <= 18
		and render_grid.y <= 18
		and terrain_vertices > 0
		and terrain_vertices < 24000
		and not view.has_method("uses_terrain3d")
	)
	print(
		"SIM_CHECK local_terrain_ok=",
		local_terrain_ok,
		" grid=",
		render_grid,
		" vertices=",
		terrain_vertices
	)
	if not local_terrain_ok:
		viewport.free()
		world.free()
		quit(1)
		return

	var minimap := Control.new()
	minimap.name = "CheckMinimap"
	minimap.custom_minimum_size = Vector2(260.0, 260.0)
	minimap.size = Vector2(260.0, 260.0)
	minimap.set_script(load("res://scripts/world_minimap.gd"))
	viewport.add_child(minimap)
	minimap.call("bind_sim", world, camera_controller)
	var map_center: Vector3 = minimap.call(
		"world_position_from_local",
		Vector2(130.0, 130.0)
	)
	var expected_center := Vector3(bounds.get_center().x, 0.0, bounds.get_center().y)
	var minimap_ok := (
		bool(minimap.call("has_map_texture"))
		and Vector2(map_center.x, map_center.z).distance_to(
			Vector2(expected_center.x, expected_center.z)
		) <= habitat_cell_size * 2.0
	)
	print("SIM_CHECK minimap_ok=", minimap_ok, " mapped_center=", map_center)
	if not minimap_ok:
		viewport.free()
		world.free()
		quit(1)
		return

	var full_snap: Object = world.call("get_sim_snapshot")
	var deer_id := _catalog_id(world, "deer")
	var deer_pos := _first_species_position(full_snap, deer_id)
	var teleport_ok := false
	var terrain_y := float(view.call("presentation_height", spawn))
	var presented: Vector3 = view.call("_presentation_position", spawn, 0.0)
	var grounding_ok := (
		terrain_y > -1.0
		and is_equal_approx(presented.y, terrain_y)
	)
	if deer_pos != Vector3.INF:
		camera_controller.call("teleport_to", deer_pos)
		var teleported: Vector3 = camera_controller.call("anchor_position")
		teleport_ok = Vector2(teleported.x, teleported.z).distance_to(
			Vector2(deer_pos.x, deer_pos.z)
		) < 0.01
	print(
		"SIM_CHECK teleport_ok=",
		teleport_ok,
		" grounding_ok=",
		grounding_ok
	)
	if not teleport_ok or not grounding_ok:
		viewport.free()
		world.free()
		quit(1)
		return

	viewport.free()
	world.free()
	quit(0)


func _check_ui_contracts() -> bool:
	var missing_actions := PackedStringArray()
	var empty_actions := PackedStringArray()
	for action in Actions.required_actions():
		if not InputMap.has_action(action):
			missing_actions.append(action)
		elif InputMap.action_get_events(action).is_empty():
			empty_actions.append(action)

	var keyboard_mouse_ok := (
		_action_has_key(Actions.CAMERA_MOVE_FORWARD)
		and _action_has_key(Actions.CAMERA_MOVE_UP)
		and _action_has_mouse(Actions.CAMERA_LOOK_CAPTURE)
		and _action_has_key(Actions.CAMERA_LOOK_RELEASE)
		and _action_has_mouse(Actions.CAMERA_SPEED_INCREASE)
		and _action_has_key(Actions.CAMERA_SPEED_BOOST)
	)
	var gamepad_ok := (
		_action_has_joy(Actions.CAMERA_MOVE_LEFT)
		and _action_has_joy(Actions.CAMERA_MOVE_FORWARD)
		and _action_has_joy(Actions.CAMERA_MOVE_UP)
		and _action_has_joy(Actions.CAMERA_LOOK_LEFT)
		and _action_has_joy(Actions.CAMERA_SPEED_BOOST)
		and _action_has_joy(Actions.SIM_TOGGLE_PAUSE)
	)

	var theme_ok := load("res://ui/sim_theme.tres") is Theme

	var previous_locale := TranslationServer.get_locale()
	TranslationServer.set_locale("ru")
	var translation_ok := (
		str(TranslationServer.translate("UI_VIEW_THIRD_PERSON_GOD"))
		== "3-Е ЛИЦО · РЕЖИМ БОГА"
	)
	TranslationServer.set_locale(previous_locale)

	var presentation_files_ok := (
		FileAccess.file_exists("res://scripts/world_minimap.gd")
		and not FileAccess.file_exists("res://scripts/world_map_view.gd")
		and not FileAccess.file_exists("res://scripts/world_overview.gd")
	)

	var ok := (
		missing_actions.is_empty()
		and empty_actions.is_empty()
		and keyboard_mouse_ok
		and gamepad_ok
		and theme_ok
		and translation_ok
		and presentation_files_ok
	)
	print(
		"SIM_CHECK ui_contracts_ok=",
		ok,
		" missing_actions=",
		missing_actions,
		" empty_actions=",
		empty_actions,
		" keyboard_mouse=",
		keyboard_mouse_ok,
		" gamepad=",
		gamepad_ok,
		" theme=",
		theme_ok,
		" translation=",
		translation_ok,
		" presentation_files=",
		presentation_files_ok
	)
	return ok


func _overview_point_is_populated_land(data: Dictionary, point: Vector3) -> bool:
	var width := int(data.get("width", 0))
	var height := int(data.get("height", 0))
	var cell_size := float(data.get("cell_size", 0.0))
	var origin: Vector3 = data.get("origin", Vector3.ZERO)
	var surface: PackedByteArray = data.get("surface", PackedByteArray())
	if width <= 0 or height <= 0 or cell_size <= 0.0:
		return false
	var x := clampi(int(floor((point.x - origin.x) / cell_size)), 0, width - 1)
	var z := clampi(int(floor((point.z - origin.z) / cell_size)), 0, height - 1)
	var i := z * width + x
	if i < 0 or i >= surface.size() or int(surface[i]) != 0:
		return false
	for key in ["plants", "herbivores", "omnivores", "carnivores", "insects"]:
		var counts: PackedInt32Array = data.get(key, PackedInt32Array())
		if i < counts.size() and int(counts[i]) > 0:
			return true
	return false


func _action_has_key(action: StringName) -> bool:
	for event in InputMap.action_get_events(action):
		if event is InputEventKey:
			return true
	return false


func _action_has_mouse(action: StringName) -> bool:
	for event in InputMap.action_get_events(action):
		if event is InputEventMouseButton:
			return true
	return false


func _action_has_joy(action: StringName) -> bool:
	for event in InputMap.action_get_events(action):
		if event is InputEventJoypadMotion or event is InputEventJoypadButton:
			return true
	return false


func _catalog_id(world: Node, key: String) -> int:
	for entry in world.call("get_species_catalog"):
		if typeof(entry) == TYPE_DICTIONARY and str(entry.get("key")) == key:
			return int(entry.get("id", 0))
	return 0


func _first_species_position(snap: Object, species_id: int) -> Vector3:
	if snap == null or species_id == 0:
		return Vector3.INF
	for entity in snap.get("entities"):
		if entity != null and int(entity.get("species_id")) == species_id:
			return entity.get("position") as Vector3
	return Vector3.INF
