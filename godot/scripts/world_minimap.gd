extends Control

signal teleport_requested(world_position: Vector3)

## Debug/navigation minimap. Whole-world aggregation is intentionally low-rate;
## camera marker redraws independently so navigation stays responsive.

const RESOLUTION := 48
const MAP_REFRESH_SEC := 2.0
const MARKER_REFRESH_SEC := 0.10

const COLOR_OCEAN := Color("#17384b")
const COLOR_FRESH := Color("#2d6670")
const COLOR_LAND := Color("#47663a")
const COLOR_POPULATED := Color("#8aa05b")
const COLOR_PREDATOR := Color("#b55a4a")
const COLOR_CAMERA := Color("#f4df7a")
const COLOR_BORDER := Color("#d7ddd6")

var _sim: Node
var _camera_controller: Node
var _data: Dictionary = {}
var _texture: ImageTexture
var _map_clock := 999.0
var _marker_clock := 999.0


func bind_sim(sim: Node, camera_controller: Node) -> void:
	_sim = sim
	_camera_controller = camera_controller
	refresh_now()


func refresh_now() -> void:
	if _sim == null or not _sim.has_method("get_world_overview"):
		return
	_data = _sim.call("get_world_overview", RESOLUTION)
	_rebuild_texture()
	_map_clock = 0.0
	queue_redraw()


func has_map_texture() -> bool:
	return _texture != null and not _data.is_empty()


func world_position_from_local(local_position: Vector2) -> Vector3:
	if _data.is_empty():
		return Vector3.ZERO
	var map_rect := _map_rect()
	var uv := Vector2(
		clampf((local_position.x - map_rect.position.x) / maxf(map_rect.size.x, 1.0), 0.0, 1.0),
		clampf((local_position.y - map_rect.position.y) / maxf(map_rect.size.y, 1.0), 0.0, 1.0)
	)
	var width := int(_data.get("width", 0))
	var height := int(_data.get("height", 0))
	var cell_size := float(_data.get("cell_size", 0.0))
	var origin: Vector3 = _data.get("origin", Vector3.ZERO)
	return Vector3(
		origin.x + uv.x * float(width) * cell_size,
		0.0,
		origin.z + uv.y * float(height) * cell_size
	)


static func inhabited_spawn(data: Dictionary) -> Vector3:
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
		return Vector3(origin.x, 0.0, origin.z)

	var best_index := -1
	var best_score := -1.0
	var first_land := -1
	for i in range(width * height):
		if int(surface[i]) != 0:
			continue
		if first_land < 0:
			first_land = i
		var score := (
			float(herbivores[i] if i < herbivores.size() else 0) * 4.0
			+ float(omnivores[i] if i < omnivores.size() else 0) * 5.0
			+ float(carnivores[i] if i < carnivores.size() else 0) * 6.0
			+ float(insects[i] if i < insects.size() else 0) * 1.5
			+ minf(float(plants[i] if i < plants.size() else 0), 30.0) * 0.15
		)
		if score > best_score:
			best_score = score
			best_index = i

	if best_index < 0:
		best_index = first_land
	if best_index < 0:
		return Vector3(origin.x, 0.0, origin.z)

	var x := best_index % width
	var z := best_index / width
	return Vector3(
		origin.x + (float(x) + 0.5) * cell_size,
		0.0,
		origin.z + (float(z) + 0.5) * cell_size
	)


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_STOP
	set_process(true)


func _process(delta: float) -> void:
	if _sim == null:
		return
	_map_clock += delta
	_marker_clock += delta
	if _map_clock >= MAP_REFRESH_SEC:
		refresh_now()
	elif _marker_clock >= MARKER_REFRESH_SEC:
		_marker_clock = 0.0
		queue_redraw()


func _gui_input(event: InputEvent) -> void:
	if event is InputEventMouseButton:
		var button := event as InputEventMouseButton
		if button.button_index == MOUSE_BUTTON_LEFT and button.pressed:
			var world_position := world_position_from_local(button.position)
			teleport_requested.emit(world_position)
			accept_event()


func _rebuild_texture() -> void:
	var width := int(_data.get("width", 0))
	var height := int(_data.get("height", 0))
	var surface: PackedByteArray = _data.get("surface", PackedByteArray())
	var plants: PackedInt32Array = _data.get("plants", PackedInt32Array())
	var herbivores: PackedInt32Array = _data.get("herbivores", PackedInt32Array())
	var omnivores: PackedInt32Array = _data.get("omnivores", PackedInt32Array())
	var carnivores: PackedInt32Array = _data.get("carnivores", PackedInt32Array())
	var insects: PackedInt32Array = _data.get("insects", PackedInt32Array())
	if width <= 0 or height <= 0 or surface.size() != width * height:
		_texture = null
		return

	var image := Image.create(width, height, false, Image.FORMAT_RGBA8)
	for z in range(height):
		for x in range(width):
			var i := z * width + x
			var color := COLOR_OCEAN
			if int(surface[i]) == 2:
				color = COLOR_FRESH
			elif int(surface[i]) == 0:
				var population := (
					int(plants[i] if i < plants.size() else 0)
					+ int(herbivores[i] if i < herbivores.size() else 0)
					+ int(omnivores[i] if i < omnivores.size() else 0)
					+ int(insects[i] if i < insects.size() else 0)
				)
				color = COLOR_LAND.lerp(COLOR_POPULATED, clampf(float(population) / 28.0, 0.0, 1.0))
				if i < carnivores.size() and int(carnivores[i]) > 0:
					color = color.lerp(COLOR_PREDATOR, 0.58)
			image.set_pixel(x, z, color)
	_texture = ImageTexture.create_from_image(image)


func _draw() -> void:
	var map_rect := _map_rect()
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.03, 0.04, 0.04, 0.92), true)
	if _texture != null:
		draw_texture_rect(_texture, map_rect, false)
	draw_rect(map_rect, COLOR_BORDER, false, 1.0)

	if _camera_controller == null or _data.is_empty():
		return
	var anchor: Vector3 = _camera_controller.call("anchor_position")
	var origin: Vector3 = _data.get("origin", Vector3.ZERO)
	var width := int(_data.get("width", 0))
	var height := int(_data.get("height", 0))
	var cell_size := float(_data.get("cell_size", 0.0))
	var world_size := Vector2(float(width) * cell_size, float(height) * cell_size)
	if world_size.x <= 0.0 or world_size.y <= 0.0:
		return
	var uv := Vector2(
		clampf((anchor.x - origin.x) / world_size.x, 0.0, 1.0),
		clampf((anchor.z - origin.z) / world_size.y, 0.0, 1.0)
	)
	var point := map_rect.position + uv * map_rect.size
	draw_circle(point, 4.0, COLOR_CAMERA)
	draw_circle(point, 7.0, COLOR_CAMERA, false, 1.0)


func _map_rect() -> Rect2:
	return Rect2(Vector2(6.0, 6.0), size - Vector2(12.0, 12.0))
