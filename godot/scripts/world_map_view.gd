extends Node3D

## Coarse whole-world renderer used only in map-camera mode. It consumes the
## aggregated overview contract and never creates detailed organism instances
## or asks for the full habitat grid.

const Palette = preload("res://scripts/visualization_palette.gd")

const RESOLUTION := 64
const REFRESH_SEC := 2.0

var _sim: Node
var _mesh_instance: MeshInstance3D
var _material: StandardMaterial3D
var _refresh_clock := 999.0


func _ready() -> void:
	_material = StandardMaterial3D.new()
	_material.vertex_color_use_as_albedo = true
	_material.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
	_material.cull_mode = BaseMaterial3D.CULL_DISABLED

	_mesh_instance = MeshInstance3D.new()
	_mesh_instance.name = "CoarseWorldMap"
	_mesh_instance.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
	_mesh_instance.material_override = _material
	add_child(_mesh_instance)

	visible = false
	set_process(false)


func bind_sim(sim: Node) -> void:
	_sim = sim
	_refresh_clock = 999.0


func set_active(active: bool) -> void:
	visible = active
	set_process(active)
	if active:
		_refresh_clock = 999.0
		_refresh()


func refresh_now() -> void:
	if _sim != null:
		_refresh()


func has_map_mesh() -> bool:
	return _mesh_instance != null and _mesh_instance.mesh != null


func _process(delta: float) -> void:
	if _sim == null:
		return
	_refresh_clock += delta
	if _refresh_clock >= REFRESH_SEC:
		_refresh()
		_refresh_clock = 0.0


func _refresh() -> void:
	if _sim == null or not _sim.has_method("get_world_overview"):
		return
	var data: Dictionary = _sim.call("get_world_overview", RESOLUTION)
	_build_mesh(data)


func _build_mesh(data: Dictionary) -> void:
	var width := int(data.get("width", 0))
	var height := int(data.get("height", 0))
	var cell_size := float(data.get("cell_size", 0.0))
	var origin: Vector3 = data.get("origin", Vector3.ZERO)
	var surface: PackedByteArray = data.get("surface", PackedByteArray())
	var plants: PackedInt32Array = data.get("plants", PackedInt32Array())
	var herbivores: PackedInt32Array = data.get("herbivores", PackedInt32Array())
	var omnivores: PackedInt32Array = data.get("omnivores", PackedInt32Array())
	var carnivores: PackedInt32Array = data.get("carnivores", PackedInt32Array())
	var insects: PackedInt32Array = data.get("insects", PackedInt32Array())
	if width <= 0 or height <= 0 or cell_size <= 0.0 or surface.size() != width * height:
		_mesh_instance.mesh = null
		return

	var st := SurfaceTool.new()
	st.begin(Mesh.PRIMITIVE_TRIANGLES)
	var vertex_index := 0
	for z in range(height):
		for x in range(width):
			var i := z * width + x
			var color := Palette.overview_cell_color(
				int(surface[i]),
				int(plants[i]) if i < plants.size() else 0,
				int(herbivores[i]) if i < herbivores.size() else 0,
				int(omnivores[i]) if i < omnivores.size() else 0,
				int(carnivores[i]) if i < carnivores.size() else 0,
				int(insects[i]) if i < insects.size() else 0
			)
			var x0 := origin.x + float(x) * cell_size
			var z0 := origin.z + float(z) * cell_size
			var x1 := x0 + cell_size
			var z1 := z0 + cell_size
			var y := -10.0 if int(surface[i]) == Palette.SURFACE_OCEAN else 0.0
			if int(surface[i]) == Palette.SURFACE_FRESH:
				y = 1.0

			for point in [
				Vector3(x0, y, z0),
				Vector3(x1, y, z0),
				Vector3(x1, y, z1),
				Vector3(x0, y, z1),
			]:
				st.set_color(color)
				st.add_vertex(point)
			st.add_index(vertex_index)
			st.add_index(vertex_index + 1)
			st.add_index(vertex_index + 2)
			st.add_index(vertex_index)
			st.add_index(vertex_index + 2)
			st.add_index(vertex_index + 3)
			vertex_index += 4

	_mesh_instance.mesh = st.commit()
