extends SceneTree

## Headless smoke check that the GDExtension and visualization contracts work.

const Actions = preload("res://scripts/input_actions.gd")

var _world: Node
var _view: Node3D
var _checked_view := false


func _initialize() -> void:
	if not _check_ui_contracts():
		quit(1)
		return

	var has_world := ClassDB.class_exists("SimWorld")
	var has_snap := ClassDB.class_exists("SimSnapshot")
	var has_entity := ClassDB.class_exists("SimEntityState")
	var has_habitat := ClassDB.class_exists("SimHabitatGrid")
	print("SIM_CHECK SimWorld=", has_world, " SimSnapshot=", has_snap, " SimEntityState=", has_entity, " SimHabitatGrid=", has_habitat)
	if not has_world or not has_snap or not has_entity or not has_habitat:
		push_error("One or more simulation classes are not registered. Build sim_godot and copy it to godot/bin.")
		quit(1)
		return
	var world: Node = ClassDB.instantiate("SimWorld") as Node
	print("SIM_CHECK instantiate_ok=", world != null)
	if world == null:
		quit(1)
		return

	world.set("island_mode", false)
	world.set("demo_agent_count", 3)
	world.call("reset_world")
	var snapshot: Object = world.call("get_sim_snapshot")
	var entity_count := int(world.call("get_entity_count"))
	var snapshot_entities: Array = snapshot.get("entities") if snapshot != null else []
	var bridge_ok := snapshot != null and entity_count == 3 and snapshot_entities.size() == 3
	print("SIM_CHECK core_bridge_ok=", bridge_ok, " entities=", entity_count)
	if not bridge_ok:
		push_error("SimWorld was registered, but the sim_core snapshot bridge failed.")
		world.free()
		quit(1)
		return

	world.set("island_mode", true)
	world.call("reset_world")
	var habitat: Object = world.call("get_habitat_grid")
	var island_entities := int(world.call("get_entity_count"))
	var width := int(habitat.get("width")) if habitat != null else 0
	var height := int(habitat.get("height")) if habitat != null else 0
	var elevations: PackedFloat32Array = habitat.get("elevation") if habitat != null else PackedFloat32Array()
	var surfaces: PackedByteArray = habitat.get("surface") if habitat != null else PackedByteArray()
	var has_ocean := false
	var has_land := false
	var has_fresh := false
	for value in surfaces:
		has_ocean = has_ocean or int(value) == 1
		has_land = has_land or int(value) == 0
		has_fresh = has_fresh or int(value) == 2
	var habitat_ok := (
		habitat != null
		and width >= 80
		and height == width
		and elevations.size() == width * height
		and surfaces.size() == elevations.size()
		and has_ocean
		and has_land
		and has_fresh
		and island_entities > 100
	)
	print("SIM_CHECK habitat_ok=", habitat_ok, " map=", width, "x", height, " island_entities=", island_entities)
	if not habitat_ok:
		push_error("Island habitat snapshot is missing or too small for the visualizer.")
		world.free()
		quit(1)
		return

	var organism_keys := PackedStringArray([
		"oak", "birch", "pine", "grass", "clover", "berry_bush", "fern", "reeds", "mushroom",
		"rabbit", "hare", "mouse", "deer", "boar", "wolf", "fox",
		"bee", "butterfly", "beetle", "ant",
	])
	var missing_meshes: PackedStringArray = PackedStringArray()
	for key in organism_keys:
		var mesh_path := "res://assets/organisms/%s.glb" % key
		var resource := load(mesh_path)
		if resource == null:
			missing_meshes.append(key)
	var meshes_ok := missing_meshes.is_empty()
	print("SIM_CHECK organism_meshes_ok=", meshes_ok, " count=", organism_keys.size())
	if not meshes_ok:
		push_error("Missing organism GLB meshes: %s" % ", ".join(missing_meshes))
		world.free()
		quit(1)
		return

	var oak_scene: PackedScene = load("res://assets/organisms/oak.glb") as PackedScene
	var oak_root: Node = oak_scene.instantiate() if oak_scene != null else null
	var oak_has_mesh := _scene_has_mesh(oak_root)
	print("SIM_CHECK oak_mesh_ok=", oak_has_mesh)
	if oak_root != null:
		oak_root.free()
	if not oak_has_mesh:
		push_error("oak.glb imported but has no MeshInstance3D.")
		world.free()
		quit(1)
		return

	_world = world
	_view = Node3D.new()
	_view.set_script(load("res://scripts/world_view.gd"))


func _process(_delta: float) -> bool:
	if _checked_view or _world == null or _view == null:
		return false
	_checked_view = true
	var viewport := SubViewport.new()
	viewport.name = "TerrainCheckViewport"
	viewport.own_world_3d = true
	viewport.size = Vector2i(64, 64)
	root.add_child(viewport)
	var camera := Camera3D.new()
	camera.name = "CheckCamera"
	viewport.add_child(camera)
	viewport.add_child(_view)
	var deer_id := _catalog_id(_world, "deer")
	var bee_id := _catalog_id(_world, "bee")
	var full_snap: Object = _world.call("get_sim_snapshot")
	var deer_pos := _first_species_position(full_snap, deer_id)
	if deer_pos != Vector3.INF:
		_world.set("render_center", deer_pos)
		_world.set("render_radius", 1200.0)
		_world.call("refresh_render_interest")
	_view.call("bind_sim", _world)
	var has_terrain3d := ClassDB.class_exists("Terrain3D")
	var uses_terrain3d := bool(_view.call("uses_terrain3d"))
	print(
		"SIM_CHECK Terrain3D=",
		has_terrain3d,
		" uses_terrain3d=",
		uses_terrain3d,
		" view_tree=",
		_view.is_inside_tree(),
		" debug=",
		_view.call("terrain3d_debug")
	)
	if not has_terrain3d or not uses_terrain3d:
		push_error("Terrain3D visualizer is not active. Run scripts/fetch_terrain3d.sh and reimport the Godot project.")
		_view.free()
		_world.free()
		quit(1)
		return true

	var region_size := int(_view.call("terrain3d_region_size"))
	var region_count := int(_view.call("terrain3d_region_count"))
	var terrain_streaming_ok := region_size == 64 and region_count >= 1 and region_count <= 4
	print(
		"SIM_CHECK terrain_streaming_ok=",
		terrain_streaming_ok,
		" region_size=",
		region_size,
		" active_regions=",
		region_count
	)
	if not terrain_streaming_ok:
		push_error("Terrain3D must use 64-vertex regions and keep the 1.2 km interest window bounded.")
		_view.free()
		_world.free()
		quit(1)
		return true

	var snap: Object = _world.call("get_render_snapshot")
	var local_deer_pos := _first_species_position(snap, deer_id)
	if local_deer_pos != Vector3.INF:
		deer_pos = local_deer_pos
	var terrain_y: float = float(_view.call("_sampled_terrain_height", deer_pos))
	var presented: Vector3 = _view.call("_presentation_position", deer_pos, 0.0)
	var deer_altitude: float = float(_view.call("_altitude_for_species", deer_id, 1, deer_pos.y))
	var bee_rest: float = float(_view.call("_altitude_for_species", bee_id, 1, 1.05))
	var bee_fly: float = float(_view.call("_altitude_for_species", bee_id, 2, 1.05))
	var grounded_ok := (
		deer_id != 0
		and bee_id != 0
		and deer_pos != Vector3.INF
		and terrain_y > 0.2
		and is_equal_approx(presented.y, terrain_y)
		and not is_equal_approx(presented.y, terrain_y + deer_pos.y)
		and is_equal_approx(deer_altitude, 0.0)
		and is_equal_approx(bee_rest, 0.12)
		and is_equal_approx(bee_fly, 1.05)
	)
	print(
		"SIM_CHECK animals_grounded_ok=",
		grounded_ok,
		" terrain_y=",
		terrain_y,
		" presented_y=",
		presented.y,
		" deer_altitude=",
		deer_altitude,
		" bee_rest=",
		bee_rest,
		" bee_fly=",
		bee_fly
	)
	if not grounded_ok:
		push_error("Grounded organisms are not sitting on the island surface.")
		_view.free()
		_world.free()
		quit(1)
		return true

	_world.set("render_center", Vector3(-6000.0, 0.0, -6000.0))
	_world.set("render_radius", 1200.0)
	_world.call("refresh_render_interest")
	_view.call("_refresh_terrain", true)
	var moved_region_count := int(_view.call("terrain3d_region_count"))
	var unload_ok := moved_region_count >= 1 and moved_region_count <= 4
	print(
		"SIM_CHECK terrain_unload_ok=",
		unload_ok,
		" active_regions_after_move=",
		moved_region_count
	)
	if not unload_ok:
		push_error("Terrain3D active regions accumulated after moving the render-interest window.")
		_view.free()
		_world.free()
		quit(1)
		return true

	_world.set("render_center", deer_pos)
	_world.set("render_radius", 1200.0)
	_world.call("refresh_render_interest")
	_view.call("_refresh_terrain", true)
	var returned_region_count := int(_view.call("terrain3d_region_count"))
	var reactivate_ok := returned_region_count >= 1 and returned_region_count <= 4
	print(
		"SIM_CHECK terrain_reactivate_ok=",
		reactivate_ok,
		" active_regions_after_return=",
		returned_region_count
	)
	if not reactivate_ok:
		push_error("Terrain3D failed to reactivate a previously removed interest region.")
		_view.free()
		_world.free()
		quit(1)
		return true

	var map_view := Node3D.new()
	map_view.set_script(load("res://scripts/world_map_view.gd"))
	viewport.add_child(map_view)
	map_view.call("bind_sim", _world)
	map_view.call("set_active", true)
	var map_ok := bool(map_view.call("has_map_mesh"))
	print("SIM_CHECK coarse_world_map_ok=", map_ok)
	map_view.free()
	_view.free()
	if not map_ok:
		push_error("Coarse whole-world map failed to build from overview data.")
		_world.free()
		quit(1)
		return true

	_world.free()
	quit(0)
	return true


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
		and _action_has_key(Actions.CAMERA_MOVE_BACK)
		and _action_has_mouse(Actions.CAMERA_ORBIT_DRAG)
		and _action_has_mouse(Actions.CAMERA_ZOOM_IN)
	)
	var gamepad_ok := (
		_action_has_joy(Actions.CAMERA_MOVE_LEFT)
		and _action_has_joy(Actions.CAMERA_MOVE_FORWARD)
		and _action_has_joy(Actions.CAMERA_ORBIT_LEFT)
		and _action_has_joy(Actions.CAMERA_ORBIT_UP)
		and _action_has_joy(Actions.CAMERA_ZOOM_IN)
		and _action_has_joy(Actions.VIEW_TOGGLE_OVERVIEW)
		and _action_has_joy(Actions.SIM_TOGGLE_PAUSE)
	)

	var theme := load("res://ui/sim_theme.tres") as Theme
	var theme_ok := theme != null

	var previous_locale := TranslationServer.get_locale()
	TranslationServer.set_locale("ru")
	var translation_ok := str(TranslationServer.translate("UI_VIEW_MAP")) == "КАРТА МИРА"
	TranslationServer.set_locale(previous_locale)

	var ok := (
		missing_actions.is_empty()
		and empty_actions.is_empty()
		and keyboard_mouse_ok
		and gamepad_ok
		and theme_ok
		and translation_ok
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
		translation_ok
	)
	if not ok:
		push_error("Visualization input, theme, or localization contract is incomplete.")
	return ok


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


func _scene_has_mesh(node: Node) -> bool:
	if node == null:
		return false
	if node is MeshInstance3D and (node as MeshInstance3D).mesh != null:
		return true
	for child in node.get_children():
		if _scene_has_mesh(child):
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
			var position: Vector3 = entity.get("position")
			return position
	return Vector3.INF
