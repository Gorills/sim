extends Node3D

## Player-scale presentation only. Canonical rules stay in sim_core.
## A small local ArrayMesh is rebuilt around render interest; no global terrain
## renderer or Terrain3D region streaming participates in the runtime view.

const SURFACE_LAND := 0
const SURFACE_OCEAN := 1
const SURFACE_FRESH := 2
const KIND_PLANT := 1
const KIND_HERBIVORE := 2
const KIND_CARNIVORE := 3
const KIND_INSECT := 4
const KIND_OMNIVORE := 5
const INTENT_RESTING := 1

const PALETTE := {
	"grass": Color("#668f3d"),
	"clover": Color("#4f8a3b"),
	"oak": Color("#3f713b"),
	"birch": Color("#709b51"),
	"pine": Color("#315f45"),
	"berry_bush": Color("#456f37"),
	"fern": Color("#477a45"),
	"reeds": Color("#71834a"),
	"mushroom": Color("#a66f4e"),
	"rabbit": Color("#b9a78b"),
	"deer": Color("#9a6945"),
	"mouse": Color("#82766b"),
	"hare": Color("#b9aa91"),
	"boar": Color("#59463b"),
	"wolf": Color("#687078"),
	"fox": Color("#b96132"),
	"bee": Color("#d9a928"),
	"butterfly": Color("#b982a0"),
	"beetle": Color("#443b32"),
	"ant": Color("#573832"),
}

var _sim: Node
var _mesh_terrain: MeshInstance3D
var _terrain_material: StandardMaterial3D
var _fresh_water: MeshInstance3D
var _batches: Dictionary = {}
var _catalog: Dictionary = {}
var _color_by_species: Dictionary = {}
var _shape_by_species: Dictionary = {}
var _flying_by_species: Dictionary = {}
var _mesh_cache: Dictionary = {}
var _material_cache: Dictionary = {}
const ORGANISM_MESH_DIR := "res://assets/organisms/"
const RELIEF := 420.0
const ORGANISM_REFRESH_SEC := 0.05
const TERRAIN_APPEARANCE_REFRESH_SEC := 5.0
const TERRAIN_SUBDIVISIONS := 4
const SAND := Color("#8f8058")
const SOIL := Color("#66543b")
const TRUNK := Color("#59442f")
const BIRCH_BARK := Color("#c1bca5")
const DARK_LEAF := Color("#294e35")

var _last_tick: int = -1
var _terrain_signature: String = ""
var _habitat: Object
var _terrain_clock: float = 999.0
var _organism_clock: float = 999.0
var _last_interest_center := Vector3.INF
var _terrain_vertex_count := 0
var _terrain_heights := PackedFloat32Array()
var _terrain_colors := PackedColorArray()


func bind_sim(sim: Node) -> void:
	_ensure_visual_nodes()
	_sim = sim
	_refresh_catalog()
	_terrain_signature = ""
	_terrain_clock = 999.0
	_organism_clock = 999.0
	_last_interest_center = _sim.get("render_center") as Vector3
	_refresh_terrain(true)


func species_color(species_id: int, fallback_id: int = 0) -> Color:
	if _color_by_species.has(species_id):
		return _color_by_species[species_id]
	var hue := fposmod(float(fallback_id if fallback_id != 0 else species_id) * 0.1618, 1.0)
	return Color.from_hsv(hue, 0.55, 0.95)


func legend_lines() -> PackedStringArray:
	var lines: PackedStringArray = PackedStringArray()
	var keys: Array = _catalog.keys()
	keys.sort()
	for species_id in keys:
		var info: Dictionary = _catalog[species_id]
		var color: Color = species_color(int(species_id))
		lines.append("%s %s" % [color.to_html(false), str(info.get("display_name", info.get("key", "?")))])
	return lines


func set_detail_active(active: bool) -> void:
	visible = active
	set_process(active)
	if active and _sim != null:
		refresh_interest_now()


func refresh_interest_now() -> void:
	if _sim == null:
		return
	_last_interest_center = _sim.get("render_center") as Vector3
	_refresh_terrain(true)
	_terrain_clock = 0.0


func presentation_height(world_position: Vector3) -> float:
	return _sampled_terrain_height(world_position)


func terrain_vertex_count() -> int:
	return _terrain_vertex_count


func render_grid_size() -> Vector2i:
	if _habitat == null:
		return Vector2i.ZERO
	return Vector2i(int(_habitat.get("width")), int(_habitat.get("height")))


func _ready() -> void:
	_ensure_visual_nodes()


func _ensure_visual_nodes() -> void:
	if _mesh_terrain != null:
		return
	_terrain_material = StandardMaterial3D.new()
	_terrain_material.vertex_color_use_as_albedo = true
	_terrain_material.roughness = 0.98
	_terrain_material.specular_mode = BaseMaterial3D.SPECULAR_DISABLED
	_mesh_terrain = MeshInstance3D.new()
	_mesh_terrain.name = "IslandTerrainMesh"
	_mesh_terrain.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_ON
	add_child(_mesh_terrain)
	_fresh_water = MeshInstance3D.new()
	_fresh_water.name = "FreshWater"
	_fresh_water.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	var water_material := StandardMaterial3D.new()
	water_material.albedo_color = Color("#2d6670")
	water_material.roughness = 0.28
	water_material.metallic = 0.02
	_fresh_water.material_override = water_material
	add_child(_fresh_water)
	_mesh_terrain.visible = true


func _process(delta: float) -> void:
	if _sim == null:
		return
	if _catalog.is_empty():
		_refresh_catalog()

	var interest_center: Vector3 = _sim.get("render_center") as Vector3
	var interest_changed := (
		_last_interest_center == Vector3.INF
		or interest_center.distance_squared_to(_last_interest_center) > 1.0
	)
	_terrain_clock += delta
	if interest_changed or _habitat == null or not _has_terrain_visual():
		_last_interest_center = interest_center
		_refresh_terrain(true)
		_terrain_clock = 0.0
	elif _terrain_clock >= TERRAIN_APPEARANCE_REFRESH_SEC:
		_refresh_terrain(false)
		_terrain_clock = 0.0

	_organism_clock += delta
	if _organism_clock < ORGANISM_REFRESH_SEC:
		return
	_organism_clock = 0.0

	var snap: Object = _sim.call("get_render_snapshot")
	if snap == null:
		return
	var tick := int(snap.get("tick"))
	if tick < _last_tick:
		_terrain_signature = ""
		_refresh_catalog()
		_refresh_terrain(true)
		_last_interest_center = _sim.get("render_center") as Vector3
		_terrain_clock = 0.0
	_last_tick = tick
	_update_organisms(snap.get("entities") as Array)


func _refresh_catalog() -> void:
	_catalog.clear()
	_color_by_species.clear()
	_shape_by_species.clear()
	_flying_by_species.clear()
	if _sim == null or not _sim.has_method("get_species_catalog"):
		return
	var entries: Array = _sim.call("get_species_catalog")
	for entry in entries:
		if typeof(entry) != TYPE_DICTIONARY:
			continue
		var species_id := int(entry.get("id", 0))
		_catalog[species_id] = entry
		var key := str(entry.get("key", ""))
		if PALETTE.has(key):
			_color_by_species[species_id] = PALETTE[key]
		else:
			_color_by_species[species_id] = Color.from_hsv(fposmod(float(species_id) * 0.1618, 1.0), 0.55, 0.95)
		_shape_by_species[species_id] = _shape_from_entry(entry)
		var tags: PackedStringArray = PackedStringArray(entry.get("tags", []))
		_flying_by_species[species_id] = "flying" in tags


func _shape_from_entry(entry: Dictionary) -> String:
	var tags: PackedStringArray = PackedStringArray(entry.get("tags", []))
	if "tree" in tags:
		return "tree"
	if "shrub" in tags:
		return "shrub"
	if "fungus" in tags:
		return "mushroom"
	var kind := int(entry.get("kind", 0))
	match kind:
		KIND_PLANT:
			return "ground"
		KIND_HERBIVORE:
			return "herbivore"
		KIND_CARNIVORE:
			return "carnivore"
		KIND_INSECT:
			return "insect"
		KIND_OMNIVORE:
			return "omnivore"
	return "generic"


func _mesh_for_species(species_id: int) -> Mesh:
	if _mesh_cache.has(species_id):
		return _mesh_cache[species_id]
	var entry: Dictionary = _catalog.get(species_id, {})
	var key := str(entry.get("key", ""))
	var imported := _load_organism_mesh(key)
	if imported != null:
		_mesh_cache[species_id] = imported
		return imported
	var shape := str(_shape_by_species.get(species_id, "generic"))
	var color := species_color(species_id)
	var mesh: Mesh
	match shape:
		"tree":
			mesh = _make_tree_mesh(key, color)
		"shrub":
			mesh = _make_shrub_mesh(color)
		"mushroom":
			mesh = _make_mushroom_mesh(color)
		"ground":
			mesh = _make_ground_plant_mesh(key, color)
		"herbivore":
			mesh = _make_animal_mesh(key, color, false)
		"carnivore":
			mesh = _make_animal_mesh(key, color, true)
		"omnivore":
			mesh = _make_animal_mesh(key, color, false)
		"insect":
			mesh = _make_insect_mesh(key, color)
		_:
			mesh = _make_animal_mesh(key, color, false)
	_mesh_cache[species_id] = mesh
	return mesh


func _make_tree_mesh(key: String, leaf_color: Color) -> Mesh:
	var parts: Array = []
	var trunk_color := BIRCH_BARK if key == "birch" else TRUNK
	parts.append(_part(_cylinder(0.13, 0.19, 1.15), Vector3(0.0, 0.58, 0.0), Vector3.ZERO, trunk_color))
	if key == "pine":
		parts.append(_part(_cone(0.72, 1.05), Vector3(0.0, 1.12, 0.0), Vector3.ZERO, leaf_color.darkened(0.12)))
		parts.append(_part(_cone(0.58, 0.90), Vector3(0.0, 1.58, 0.0), Vector3.ZERO, leaf_color))
		parts.append(_part(_cone(0.40, 0.72), Vector3(0.0, 1.98, 0.0), Vector3.ZERO, leaf_color.lightened(0.05)))
	else:
		parts.append(_part(_sphere(0.58, 0.76), Vector3(-0.24, 1.42, 0.0), Vector3.ZERO, leaf_color.darkened(0.08)))
		parts.append(_part(_sphere(0.62, 0.82), Vector3(0.25, 1.48, 0.05), Vector3.ZERO, leaf_color))
		parts.append(_part(_sphere(0.52, 0.72), Vector3(0.0, 1.82, -0.08), Vector3.ZERO, leaf_color.lightened(0.05)))
	return _compound_mesh(parts)


func _make_shrub_mesh(color: Color) -> Mesh:
	var parts: Array = [
		_part(_sphere(0.34, 0.48), Vector3(-0.22, 0.25, 0.03), Vector3.ZERO, color.darkened(0.12)),
		_part(_sphere(0.38, 0.54), Vector3(0.18, 0.28, -0.08), Vector3.ZERO, color),
		_part(_sphere(0.28, 0.42), Vector3(0.02, 0.43, 0.16), Vector3.ZERO, color.lightened(0.08)),
	]
	return _compound_mesh(parts)


func _make_mushroom_mesh(color: Color) -> Mesh:
	return _compound_mesh([
		_part(_cylinder(0.07, 0.09, 0.34), Vector3(0.0, 0.17, 0.0), Vector3.ZERO, Color("#d7c7a7")),
		_part(_sphere(0.25, 0.20), Vector3(0.0, 0.38, 0.0), Vector3.ZERO, color),
	])


func _make_ground_plant_mesh(key: String, color: Color) -> Mesh:
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var blade_count := 7 if key == "grass" or key == "reeds" else 5
	for blade_index in range(blade_count):
		var angle := TAU * float(blade_index) / float(blade_count)
		var radius := 0.08 + 0.04 * float(blade_index % 2)
		var center := Vector3(cos(angle) * radius, 0.0, sin(angle) * radius)
		var side := Vector3(cos(angle + PI * 0.5), 0.0, sin(angle + PI * 0.5)) * 0.055
		var height := 0.34 + 0.08 * float(blade_index % 3)
		var blade_color := color.lightened(0.04 * float(blade_index % 3))
		st.set_color(blade_color)
		st.add_vertex(center - side)
		st.add_vertex(center + side)
		st.add_vertex(center + Vector3(0.0, height, 0.0))
		st.add_vertex(center + side)
		st.add_vertex(center - side)
		st.add_vertex(center + Vector3(0.0, height, 0.0))
	st.generate_normals()
	var mesh := st.commit()
	mesh.surface_set_material(0, _material_for(Color.WHITE, true))
	return mesh


func _make_animal_mesh(key: String, color: Color, pointed: bool) -> Mesh:
	var parts: Array = []
	var body_length := 0.80
	var body_height := 0.34
	if key == "deer":
		body_length = 0.95
		body_height = 0.40
	elif key == "boar":
		body_length = 0.88
		body_height = 0.42
	var body := _capsule(body_height * 0.52, body_length)
	parts.append(_part(body, Vector3(0.0, 0.48, 0.0), Vector3(0.0, 0.0, PI * 0.5), color))
	var head_radius := body_height * 0.42
	parts.append(_part(_sphere(head_radius, head_radius * 1.75), Vector3(body_length * 0.48, 0.56, 0.0), Vector3.ZERO, color.lightened(0.04)))
	for x_sign in [-1.0, 1.0]:
		for z_sign in [-1.0, 1.0]:
			var leg_x: float = x_sign * body_length * 0.28
			var leg_z: float = z_sign * body_height * 0.30
			parts.append(_part(_cylinder(0.035, 0.045, 0.34), Vector3(leg_x, 0.22, leg_z), Vector3.ZERO, color.darkened(0.20)))
	if key == "rabbit" or key == "hare":
		parts.append(_part(_cone(0.075, 0.34), Vector3(body_length * 0.48, 0.86, -0.08), Vector3.ZERO, color))
		parts.append(_part(_cone(0.075, 0.34), Vector3(body_length * 0.48, 0.86, 0.08), Vector3.ZERO, color))
	elif pointed or key == "fox" or key == "wolf":
		parts.append(_part(_cone(0.10, 0.25), Vector3(body_length * 0.47, 0.78, -0.10), Vector3.ZERO, color.darkened(0.06)))
		parts.append(_part(_cone(0.10, 0.25), Vector3(body_length * 0.47, 0.78, 0.10), Vector3.ZERO, color.darkened(0.06)))
	var tail_color := color.lightened(0.08) if key == "fox" else color
	parts.append(_part(_cone(0.09, 0.40), Vector3(-body_length * 0.55, 0.53, 0.0), Vector3(0.0, 0.0, -PI * 0.5), tail_color))
	return _compound_mesh(parts)


func _make_insect_mesh(key: String, color: Color) -> Mesh:
	var parts: Array = [
		_part(_sphere(0.10, 0.20), Vector3.ZERO, Vector3(0.0, 0.0, PI * 0.5), color),
		_part(_sphere(0.06, 0.10), Vector3(0.12, 0.0, 0.0), Vector3.ZERO, color.darkened(0.28)),
	]
	if key == "bee" or key == "butterfly":
		var wing := Color(0.72, 0.82, 0.86, 0.82) if key == "bee" else color.lightened(0.32)
		parts.append(_part(_sphere(0.13, 0.05), Vector3(0.0, 0.06, -0.11), Vector3(PI * 0.5, 0.0, 0.0), wing))
		parts.append(_part(_sphere(0.13, 0.05), Vector3(0.0, 0.06, 0.11), Vector3(PI * 0.5, 0.0, 0.0), wing))
	return _compound_mesh(parts)


func _load_organism_mesh(key: String) -> Mesh:
	if key.is_empty():
		return null
	var path := "%s%s.glb" % [ORGANISM_MESH_DIR, key]
	if not ResourceLoader.exists(path) and not FileAccess.file_exists(path):
		return null
	var resource := load(path)
	if resource is Mesh:
		return resource
	if resource is PackedScene:
		var root: Node = (resource as PackedScene).instantiate()
		var baked := _bake_scene_mesh(root)
		root.free()
		return baked
	return null


func _bake_scene_mesh(node: Node) -> Mesh:
	var parts: Array = []
	_collect_mesh_parts(node, Transform3D.IDENTITY, parts)
	if parts.is_empty():
		return null
	var result := ArrayMesh.new()
	for part in parts:
		_append_transformed_mesh(result, part["instance"] as MeshInstance3D, part["transform"] as Transform3D)
	return result


func _collect_mesh_parts(node: Node, parent_transform: Transform3D, out: Array) -> void:
	var transform := parent_transform
	if node is Node3D:
		transform = parent_transform * (node as Node3D).transform
	if node is MeshInstance3D and (node as MeshInstance3D).mesh != null:
		out.append({"instance": node, "transform": transform})
	for child in node.get_children():
		_collect_mesh_parts(child, transform, out)


func _append_transformed_mesh(result: ArrayMesh, mesh_instance: MeshInstance3D, transform: Transform3D) -> void:
	var mesh := mesh_instance.mesh
	var basis := transform.basis
	for surface_index in range(mesh.get_surface_count()):
		var arrays := mesh.surface_get_arrays(surface_index)
		if arrays.is_empty():
			continue
		var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
		for vertex_index in range(vertices.size()):
			vertices[vertex_index] = transform * vertices[vertex_index]
		arrays[Mesh.ARRAY_VERTEX] = vertices
		if arrays[Mesh.ARRAY_NORMAL] != null:
			var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
			for normal_index in range(normals.size()):
				normals[normal_index] = (basis * normals[normal_index]).normalized()
			arrays[Mesh.ARRAY_NORMAL] = normals
		result.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
		var material: Material = mesh_instance.get_active_material(surface_index)
		if material != null:
			result.surface_set_material(result.get_surface_count() - 1, material)


func _part(mesh: Mesh, position: Vector3, rotation: Vector3, color: Color) -> Dictionary:
	return {"mesh": mesh, "position": position, "rotation": rotation, "color": color}


func _compound_mesh(parts: Array) -> ArrayMesh:
	var result := ArrayMesh.new()
	for part in parts:
		var primitive: Mesh = part["mesh"]
		var basis := Basis.from_euler(part["rotation"] as Vector3)
		var transform := Transform3D(basis, part["position"] as Vector3)
		for surface_index in range(primitive.get_surface_count()):
			var arrays := primitive.surface_get_arrays(surface_index)
			var vertices: PackedVector3Array = arrays[Mesh.ARRAY_VERTEX]
			for vertex_index in range(vertices.size()):
				vertices[vertex_index] = transform * vertices[vertex_index]
			arrays[Mesh.ARRAY_VERTEX] = vertices
			var normals: PackedVector3Array = arrays[Mesh.ARRAY_NORMAL]
			for normal_index in range(normals.size()):
				normals[normal_index] = (basis * normals[normal_index]).normalized()
			arrays[Mesh.ARRAY_NORMAL] = normals
			result.add_surface_from_arrays(Mesh.PRIMITIVE_TRIANGLES, arrays)
			result.surface_set_material(result.get_surface_count() - 1, _material_for(part["color"] as Color))
	return result


func _material_for(color: Color, vertex_color: bool = false) -> StandardMaterial3D:
	var cache_key := "%s:%s" % [color.to_html(), str(vertex_color)]
	if _material_cache.has(cache_key):
		return _material_cache[cache_key]
	var material := StandardMaterial3D.new()
	material.albedo_color = color
	material.vertex_color_use_as_albedo = vertex_color
	material.roughness = 0.88
	if color.a < 1.0:
		material.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
		material.albedo_color.a = color.a
	_material_cache[cache_key] = material
	return material


func _cylinder(top_radius: float, bottom_radius: float, height: float) -> CylinderMesh:
	var mesh := CylinderMesh.new()
	mesh.top_radius = top_radius
	mesh.bottom_radius = bottom_radius
	mesh.height = height
	mesh.radial_segments = 7
	mesh.rings = 1
	return mesh


func _cone(radius: float, height: float) -> CylinderMesh:
	return _cylinder(0.0, radius, height)


func _sphere(radius: float, height: float) -> SphereMesh:
	var mesh := SphereMesh.new()
	mesh.radius = radius
	mesh.height = height
	mesh.radial_segments = 8
	mesh.rings = 4
	return mesh


func _capsule(radius: float, height: float) -> CapsuleMesh:
	var mesh := CapsuleMesh.new()
	mesh.radius = radius
	mesh.height = maxf(height, radius * 2.05)
	mesh.radial_segments = 8
	mesh.rings = 3
	return mesh


func _has_terrain_visual() -> bool:
	return _mesh_terrain != null and _mesh_terrain.mesh != null


func _refresh_terrain(force: bool) -> void:
	if _sim == null or not _sim.has_method("get_render_habitat_grid"):
		return
	var habitat: Object = _sim.call("get_render_habitat_grid")
	if habitat == null:
		return
	var width := int(habitat.get("width"))
	var height := int(habitat.get("height"))
	if width <= 0 or height <= 0:
		return

	var region_origin: Vector3 = habitat.get("origin")
	var signature := "%s:%s:%s:%.2f:%.2f" % [
		width,
		height,
		str(habitat.get("cell_size")),
		region_origin.x,
		region_origin.z,
	]
	var geometry_changed := signature != _terrain_signature or not _has_terrain_visual()
	if not force and not geometry_changed and _terrain_clock < TERRAIN_APPEARANCE_REFRESH_SEC:
		return

	_habitat = habitat
	_terrain_signature = signature
	_build_terrain_mesh(habitat)


func _build_terrain_mesh(habitat: Object) -> void:
	var width := int(habitat.get("width"))
	var height := int(habitat.get("height"))
	var cell_size := float(habitat.get("cell_size"))
	var origin: Vector3 = habitat.get("origin")
	var elevation: PackedFloat32Array = habitat.get("elevation")
	var moisture: PackedFloat32Array = habitat.get("moisture")
	var canopy: PackedFloat32Array = habitat.get("canopy")
	var organic: PackedFloat32Array = habitat.get("organic")
	var pollination: PackedFloat32Array = habitat.get("pollination")
	var surface: PackedByteArray = habitat.get("surface")
	if elevation.size() != width * height or surface.size() != elevation.size():
		return

	var base_vertex_count := (width + 1) * (height + 1)
	_terrain_heights.resize(base_vertex_count)
	_terrain_colors.resize(base_vertex_count)
	for vertex_z in range(height + 1):
		for vertex_x in range(width + 1):
			var sample := _terrain_vertex_sample(
				vertex_x,
				vertex_z,
				width,
				height,
				elevation,
				moisture,
				canopy,
				organic,
				pollination,
				surface
			)
			var vertex_index := vertex_z * (width + 1) + vertex_x
			_terrain_heights[vertex_index] = sample.w
			_terrain_colors[vertex_index] = Color(sample.x, sample.y, sample.z)

	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var out_index := 0
	for z in range(height):
		for x in range(width):
			if int(surface[z * width + x]) != SURFACE_LAND:
				continue
			for sub_z in range(TERRAIN_SUBDIVISIONS):
				for sub_x in range(TERRAIN_SUBDIVISIONS):
					var u0 := float(sub_x) / float(TERRAIN_SUBDIVISIONS)
					var u1 := float(sub_x + 1) / float(TERRAIN_SUBDIVISIONS)
					var v0 := float(sub_z) / float(TERRAIN_SUBDIVISIONS)
					var v1 := float(sub_z + 1) / float(TERRAIN_SUBDIVISIONS)
					var samples: Array[Vector2] = [
						Vector2(u0, v0),
						Vector2(u1, v0),
						Vector2(u1, v1),
						Vector2(u0, v1),
					]
					for uv: Vector2 in samples:
						var local_x: float = float(x) + uv.x
						var local_z: float = float(z) + uv.y
						st.set_color(_interpolated_base_color(local_x, local_z, width, height))
						st.add_vertex(Vector3(
							origin.x + local_x * cell_size,
							_interpolated_base_height(local_x, local_z, width, height),
							origin.z + local_z * cell_size
						))
					st.add_index(out_index)
					st.add_index(out_index + 1)
					st.add_index(out_index + 2)
					st.add_index(out_index)
					st.add_index(out_index + 2)
					st.add_index(out_index + 3)
					out_index += 4

	if out_index == 0:
		_mesh_terrain.mesh = null
		_terrain_vertex_count = 0
	else:
		st.generate_normals()
		_mesh_terrain.mesh = st.commit()
		_mesh_terrain.material_override = _terrain_material
		_terrain_vertex_count = out_index
	_build_fresh_water_mesh(habitat)


func _interpolated_base_height(
	local_x: float,
	local_z: float,
	width: int,
	height: int
) -> float:
	var x0 := clampi(int(floor(local_x)), 0, width - 1)
	var z0 := clampi(int(floor(local_z)), 0, height - 1)
	var x1 := x0 + 1
	var z1 := z0 + 1
	var tx := clampf(local_x - float(x0), 0.0, 1.0)
	var tz := clampf(local_z - float(z0), 0.0, 1.0)
	var stride := width + 1
	var top := lerpf(_terrain_heights[z0 * stride + x0], _terrain_heights[z0 * stride + x1], tx)
	var bottom := lerpf(_terrain_heights[z1 * stride + x0], _terrain_heights[z1 * stride + x1], tx)
	return lerpf(top, bottom, tz)


func _interpolated_base_color(
	local_x: float,
	local_z: float,
	width: int,
	height: int
) -> Color:
	var x0 := clampi(int(floor(local_x)), 0, width - 1)
	var z0 := clampi(int(floor(local_z)), 0, height - 1)
	var x1 := x0 + 1
	var z1 := z0 + 1
	var tx := clampf(local_x - float(x0), 0.0, 1.0)
	var tz := clampf(local_z - float(z0), 0.0, 1.0)
	var stride := width + 1
	var top := _terrain_colors[z0 * stride + x0].lerp(
		_terrain_colors[z0 * stride + x1],
		tx
	)
	var bottom := _terrain_colors[z1 * stride + x0].lerp(
		_terrain_colors[z1 * stride + x1],
		tx
	)
	return top.lerp(bottom, tz)


func _build_fresh_water_mesh(habitat: Object) -> void:
	var width := int(habitat.get("width"))
	var height := int(habitat.get("height"))
	var cell_size := float(habitat.get("cell_size"))
	var origin: Vector3 = habitat.get("origin")
	var surface: PackedByteArray = habitat.get("surface")
	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var has_fresh := false
	var vertex_index := 0
	for z in range(height):
		for x in range(width):
			if int(surface[z * width + x]) != SURFACE_FRESH:
				continue
			has_fresh = true
			var x0 := origin.x + float(x) * cell_size
			var z0 := origin.z + float(z) * cell_size
			var x1 := x0 + cell_size
			var z1 := z0 + cell_size
			var water_y := _interpolated_base_height(
				float(x) + 0.5,
				float(z) + 0.5,
				width,
				height
			) + 0.18
			for point in [
				Vector3(x0, water_y, z0),
				Vector3(x0, water_y, z1),
				Vector3(x1, water_y, z1),
				Vector3(x1, water_y, z0),
			]:
				st.add_vertex(point)
			st.add_index(vertex_index)
			st.add_index(vertex_index + 3)
			st.add_index(vertex_index + 2)
			st.add_index(vertex_index)
			st.add_index(vertex_index + 2)
			st.add_index(vertex_index + 1)
			vertex_index += 4
	if not has_fresh:
		_fresh_water.mesh = null
		return
	st.generate_normals()
	_fresh_water.mesh = st.commit()


func _terrain_vertex_sample(
	vertex_x: int,
	vertex_z: int,
	width: int,
	height: int,
	elevation: PackedFloat32Array,
	moisture: PackedFloat32Array,
	canopy: PackedFloat32Array,
	organic: PackedFloat32Array,
	pollination: PackedFloat32Array,
	surface: PackedByteArray
) -> Vector4:
	var land_count := 0
	var fresh_count := 0
	var ocean_count := 0
	for offset_z in [-1, 0]:
		for offset_x in [-1, 0]:
			var cell_x: int = vertex_x + offset_x
			var cell_z: int = vertex_z + offset_z
			if cell_x < 0 or cell_z < 0 or cell_x >= width or cell_z >= height:
				ocean_count += 1
				continue
			var index: int = cell_z * width + cell_x
			var cell_surface := int(surface[index])
			if cell_surface == SURFACE_OCEAN:
				ocean_count += 1
			elif cell_surface == SURFACE_FRESH:
				fresh_count += 1
			else:
				land_count += 1
	if land_count > 0:
		var height_sum := 0.0
		var color_sum := Vector3.ZERO
		var weight_sum := 0.0
		for sample_z in range(vertex_z - 2, vertex_z + 2):
			for sample_x in range(vertex_x - 2, vertex_x + 2):
				if sample_x < 0 or sample_z < 0 or sample_x >= width or sample_z >= height:
					continue
				var sample_index := sample_z * width + sample_x
				if int(surface[sample_index]) != SURFACE_LAND:
					continue
				var distance := Vector2(
					float(sample_x) + 0.5 - float(vertex_x),
					float(sample_z) + 0.5 - float(vertex_z)
				).length()
				var weight := 1.0 / (0.65 + distance)
				height_sum += clampf(elevation[sample_index], 0.0, 0.82) * weight
				var cell_color := _cell_color(
					SURFACE_LAND,
					moisture[sample_index] if sample_index < moisture.size() else 0.5,
					canopy[sample_index] if sample_index < canopy.size() else 0.0,
					elevation[sample_index],
					organic[sample_index] if sample_index < organic.size() else 0.0,
					pollination[sample_index] if sample_index < pollination.size() else 0.0
				)
				color_sum += Vector3(cell_color.r, cell_color.g, cell_color.b) * weight
				weight_sum += weight
		var averaged_elevation := height_sum / maxf(weight_sum, 0.001)
		var terrain_height := smoothstep(0.0, 0.82, averaged_elevation) * RELIEF * 0.66
		var color_vector := color_sum / maxf(weight_sum, 0.001)
		var land_color := Color(color_vector.x, color_vector.y, color_vector.z)
		if ocean_count > 0:
			var coast_strength := clampf(float(ocean_count) * 0.20, 0.0, 0.60)
			land_color = land_color.lerp(SAND, coast_strength)
			terrain_height = lerpf(terrain_height, 0.04, coast_strength * 0.82)
		if fresh_count > 0:
			var bank_strength := clampf(float(fresh_count) * 0.18, 0.0, 0.54)
			land_color = land_color.lerp(SOIL.lightened(0.12), bank_strength * 0.42)
			terrain_height = lerpf(terrain_height, 0.04, bank_strength)
		var variation := sin(float(vertex_x) * 1.73 + float(vertex_z) * 2.37) * 0.025
		land_color = land_color.lightened(maxf(0.0, variation)).darkened(maxf(0.0, -variation))
		return Vector4(land_color.r, land_color.g, land_color.b, terrain_height)
	if fresh_count > 0:
		var fresh_color := Color("#2d6670")
		return Vector4(fresh_color.r, fresh_color.g, fresh_color.b, 0.025)
	var deep_color := Color("#17384b")
	return Vector4(deep_color.r, deep_color.g, deep_color.b, -0.34)


func _presentation_position(sim_position: Vector3, altitude: float = 0.0) -> Vector3:
	return Vector3(sim_position.x, _sampled_terrain_height(sim_position) + altitude, sim_position.z)


func _altitude_for_species(species_id: int, intent: int, sim_y: float) -> float:
	if bool(_flying_by_species.get(species_id, false)):
		return 0.12 if intent == INTENT_RESTING else sim_y
	return 0.0


func _sampled_terrain_height(sim_position: Vector3) -> float:
	if _habitat == null:
		return 0.0
	var width := int(_habitat.get("width"))
	var height := int(_habitat.get("height"))
	var cell_size := float(_habitat.get("cell_size"))
	var origin: Vector3 = _habitat.get("origin")
	if width <= 0 or height <= 0 or cell_size <= 0.0:
		return 0.0
	if _terrain_heights.size() != (width + 1) * (height + 1):
		return 0.0
	var local_x := clampf((sim_position.x - origin.x) / cell_size, 0.0, float(width))
	var local_z := clampf((sim_position.z - origin.z) / cell_size, 0.0, float(height))
	return _interpolated_base_height(local_x, local_z, width, height)


func _cell_color(
	surface: int,
	moisture: float,
	canopy: float,
	elevation: float,
	organic: float,
	pollination: float
) -> Color:
	if surface == SURFACE_OCEAN:
		return Color("#17384b")
	if surface == SURFACE_FRESH:
		return Color("#2d6670")
	var dry := Color("#766640")
	var meadow := Color("#486735")
	var wet := Color("#31523a")
	var moisture_value := clampf(moisture, 0.0, 1.0)
	var land := dry.lerp(meadow, smoothstep(0.08, 0.48, moisture_value))
	land = land.lerp(wet, smoothstep(0.46, 1.0, moisture_value))
	land = land.darkened(clampf(canopy, 0.0, 1.0) * 0.18)
	land = land.lerp(SOIL.darkened(0.08), clampf(organic, 0.0, 1.0) * 0.18)
	land = land.lerp(Color("#b7a34a"), clampf(pollination, 0.0, 1.0) * 0.16)
	if elevation > 0.60:
		land = land.lerp(SOIL.lightened(0.18), smoothstep(0.60, 0.90, elevation) * 0.42)
	return land


func _update_organisms(entities: Array) -> void:
	var grouped: Dictionary = {}
	for entity in entities:
		if entity == null:
			continue
		var species_id := int(entity.get("species_id"))
		if not grouped.has(species_id):
			grouped[species_id] = []
		grouped[species_id].append(entity)

	for species_id in grouped.keys():
		var group: Array = grouped[species_id]
		var batch: MultiMeshInstance3D = _batches.get(species_id) as MultiMeshInstance3D
		if batch == null:
			batch = _make_batch(int(species_id))
			_batches[species_id] = batch
		var multimesh := batch.multimesh
		_ensure_multimesh_capacity(multimesh, group.size())
		multimesh.visible_instance_count = group.size()
		for index in range(group.size()):
			var entity: Object = group[index]
			var sim_position: Vector3 = entity.get("position") as Vector3
			var intent := int(entity.get("intent"))
			var altitude := _altitude_for_species(int(species_id), intent, sim_position.y)
			var position: Vector3 = _presentation_position(sim_position, altitude)
			var shape := str(_shape_by_species.get(int(species_id), "generic"))
			var scale := _scale_for_entity(int(species_id), float(entity.get("biomass")))
			var entity_id := int(entity.get("id"))
			var yaw := _entity_yaw(entity, entity_id)
			var size_variation := lerpf(0.90, 1.10, _hash_unit(entity_id))
			var pose_scale := Vector3.ONE
			if intent == INTENT_RESTING and shape != "tree" and shape != "shrub" and shape != "ground" and shape != "mushroom":
				pose_scale.y = 0.68
			var basis := Basis(Vector3.UP, yaw) * Basis.from_scale(
				pose_scale * scale * size_variation
			)
			var transform := Transform3D(basis, position + Vector3.UP * 0.015)
			multimesh.set_instance_transform(index, transform)

	for species_id in _batches.keys():
		if grouped.has(species_id):
			continue
		var empty_batch: MultiMeshInstance3D = _batches[species_id]
		empty_batch.multimesh.visible_instance_count = 0


func _ensure_multimesh_capacity(multimesh: MultiMesh, required: int) -> void:
	if required <= multimesh.instance_count:
		return
	var capacity := maxi(16, multimesh.instance_count)
	while capacity < required:
		capacity *= 2
	multimesh.instance_count = capacity


func _make_batch(species_id: int) -> MultiMeshInstance3D:
	var multimesh := MultiMesh.new()
	multimesh.transform_format = MultiMesh.TRANSFORM_3D
	multimesh.use_colors = false
	multimesh.mesh = _mesh_for_species(species_id)
	multimesh.visible_instance_count = 0
	var instance := MultiMeshInstance3D.new()
	instance.multimesh = multimesh
	# Thousands of dynamic organism shadow casters are disproportionately
	# expensive in an aerial spectator view. Terrain keeps directional shadows.
	instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	add_child(instance)
	return instance


func _entity_yaw(entity: Object, entity_id: int) -> float:
	var shape := str(_shape_by_species.get(int(entity.get("species_id")), "generic"))
	if shape == "tree" or shape == "shrub" or shape == "ground" or shape == "mushroom":
		return _hash_unit(entity_id + 71) * TAU
	var velocity: Vector3 = entity.get("velocity")
	if Vector2(velocity.x, velocity.z).length_squared() > 0.0001:
		return atan2(-velocity.z, velocity.x)
	return _hash_unit(entity_id + 193) * TAU


func _hash_unit(value: int) -> float:
	return fposmod(sin(float(value) * 12.9898) * 43758.5453, 1.0)


func _scale_for_entity(species_id: int, biomass: float) -> float:
	var shape := str(_shape_by_species.get(species_id, "generic"))
	match shape:
		"ground":
			return clampf(sqrt(maxf(0.05, biomass)) * 0.34, 0.32, 0.90)
		"tree":
			return clampf(0.52 + sqrt(maxf(0.0, biomass)) * 0.040, 0.58, 1.55)
		"shrub":
			return clampf(0.45 + sqrt(maxf(0.0, biomass)) * 0.08, 0.4, 1.1)
		"mushroom":
			return clampf(0.35 + biomass * 0.08, 0.3, 0.8)
		"herbivore":
			return clampf(0.42 + sqrt(maxf(0.05, biomass)) * 0.07, 0.36, 1.25)
		"omnivore":
			return clampf(0.55 + sqrt(maxf(0.05, biomass)) * 0.05, 0.5, 1.2)
		"carnivore":
			return clampf(0.5 + sqrt(maxf(0.05, biomass)) * 0.06, 0.45, 1.15)
		"insect":
			return 0.42
	return 1.0
