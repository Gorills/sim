extends Control

## Cheap whole-world overview. The C++ bridge returns aggregated cells only;
## no full entity list or 3D instances are created for off-screen simulation.

const SURFACE_LAND := 0
const SURFACE_OCEAN := 1
const SURFACE_FRESH := 2
const REFRESH_SEC := 0.75
const NORMAL_SIZE := Vector2(360.0, 360.0)
const MARGIN := 16.0

var _sim: Node
var _data: Dictionary = {}
var _refresh_clock: float = 999.0
var _expanded := false


func bind_sim(sim: Node) -> void:
	_sim = sim
	_refresh_clock = 999.0
	_refresh()
	queue_redraw()


func toggle_expanded() -> void:
	_expanded = not _expanded
	_layout()
	queue_redraw()


func _ready() -> void:
	mouse_filter = Control.MOUSE_FILTER_IGNORE
	set_process(true)
	_layout()


func _process(delta: float) -> void:
	_layout()
	if _sim == null:
		return
	_refresh_clock += delta
	if _refresh_clock >= REFRESH_SEC:
		_refresh()
		_refresh_clock = 0.0
	queue_redraw()


func _refresh() -> void:
	if _sim == null or not _sim.has_method("get_world_overview"):
		return
	_data = _sim.call("get_world_overview", 96)


func _layout() -> void:
	var viewport_size := get_viewport_rect().size
	if _expanded:
		var side := minf(viewport_size.x, viewport_size.y) * 0.84
		size = Vector2(side, side)
		position = (viewport_size - size) * 0.5
	else:
		size = NORMAL_SIZE
		position = Vector2(
			maxf(MARGIN, viewport_size.x - size.x - MARGIN),
			maxf(220.0, viewport_size.y - size.y - MARGIN)
		)


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.015, 0.025, 0.035, 0.88), true)
	draw_rect(Rect2(Vector2.ZERO, size), Color(0.55, 0.68, 0.72, 0.8), false, 1.0)
	if _data.is_empty():
		return

	var width := int(_data.get("width", 0))
	var height := int(_data.get("height", 0))
	var cell_size := float(_data.get("cell_size", 1.0))
	var origin: Vector3 = _data.get("origin", Vector3.ZERO)
	var surface: PackedByteArray = _data.get("surface", PackedByteArray())
	var plants: PackedInt32Array = _data.get("plants", PackedInt32Array())
	var herbivores: PackedInt32Array = _data.get("herbivores", PackedInt32Array())
	var omnivores: PackedInt32Array = _data.get("omnivores", PackedInt32Array())
	var carnivores: PackedInt32Array = _data.get("carnivores", PackedInt32Array())
	var insects: PackedInt32Array = _data.get("insects", PackedInt32Array())
	if width <= 0 or height <= 0 or surface.size() != width * height:
		return

	var title_height := 24.0
	var map_rect := Rect2(Vector2(8.0, title_height), size - Vector2(16.0, title_height + 8.0))
	var pixel := Vector2(map_rect.size.x / float(width), map_rect.size.y / float(height))
	for z in range(height):
		for x in range(width):
			var i := z * width + x
			var color := _cell_color(
				int(surface[i]),
				int(plants[i]) if i < plants.size() else 0,
				int(herbivores[i]) if i < herbivores.size() else 0,
				int(omnivores[i]) if i < omnivores.size() else 0,
				int(carnivores[i]) if i < carnivores.size() else 0,
				int(insects[i]) if i < insects.size() else 0
			)
			draw_rect(
				Rect2(map_rect.position + Vector2(float(x), float(z)) * pixel, pixel + Vector2(0.5, 0.5)),
				color,
				true
			)

	var render_center: Vector3 = _data.get("render_center", Vector3.ZERO)
	var render_radius := float(_data.get("render_radius", 0.0))
	var world_size := Vector2(float(width) * cell_size, float(height) * cell_size)
	if world_size.x > 0.0 and world_size.y > 0.0 and render_radius > 0.0:
		var min_world := Vector2(render_center.x - render_radius - origin.x, render_center.z - render_radius - origin.z)
		var max_world := Vector2(render_center.x + render_radius - origin.x, render_center.z + render_radius - origin.z)
		var min_px := map_rect.position + Vector2(
			clampf(min_world.x / world_size.x, 0.0, 1.0) * map_rect.size.x,
			clampf(min_world.y / world_size.y, 0.0, 1.0) * map_rect.size.y
		)
		var max_px := map_rect.position + Vector2(
			clampf(max_world.x / world_size.x, 0.0, 1.0) * map_rect.size.x,
			clampf(max_world.y / world_size.y, 0.0, 1.0) * map_rect.size.y
		)
		draw_rect(Rect2(min_px, max_px - min_px), Color(0.96, 0.96, 0.90, 0.95), false, 2.0)

	draw_string(
		get_theme_default_font(),
		Vector2(10.0, 17.0),
		"world overview   red predators / tan herbivores / orange omnivores",
		HORIZONTAL_ALIGNMENT_LEFT,
		-1.0,
		12,
		Color(0.92, 0.94, 0.95)
	)


func _cell_color(surface_kind: int, plant_count: int, herbivore_count: int, omnivore_count: int, carnivore_count: int, insect_count: int) -> Color:
	if surface_kind == SURFACE_OCEAN:
		return Color("#153344")
	if surface_kind == SURFACE_FRESH:
		return Color("#286270")

	var color := Color("#314b31")
	var vegetation := clampf(log(1.0 + float(plant_count)) / 4.0, 0.0, 1.0)
	color = color.lerp(Color("#52763a"), vegetation * 0.45)

	if herbivore_count > 0:
		var strength := clampf(0.30 + log(1.0 + float(herbivore_count)) * 0.16, 0.0, 0.78)
		color = color.lerp(Color("#c0aa77"), strength)
	if insect_count > 0:
		var strength := clampf(log(1.0 + float(insect_count)) * 0.10, 0.0, 0.32)
		color = color.lerp(Color("#c6ad45"), strength)
	if omnivore_count > 0:
		var strength := clampf(0.42 + log(1.0 + float(omnivore_count)) * 0.14, 0.0, 0.82)
		color = color.lerp(Color("#bf743e"), strength)
	if carnivore_count > 0:
		var strength := clampf(0.55 + log(1.0 + float(carnivore_count)) * 0.16, 0.0, 0.92)
		color = color.lerp(Color("#bd4f48"), strength)
	return color
