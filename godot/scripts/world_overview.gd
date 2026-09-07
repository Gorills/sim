extends Control

## Cheap whole-world minimap. The C++ bridge returns aggregated cells only;
## no full entity list or 3D instances are created for off-screen simulation.

const Palette = preload("res://scripts/visualization_palette.gd")

const REFRESH_SEC := 0.75
const NORMAL_SIZE := Vector2(360.0, 360.0)
const MARGIN := 16.0

var _sim: Node
var _data: Dictionary = {}
var _refresh_clock := 999.0
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
	var layout_changed := _layout()
	if _sim == null:
		if layout_changed:
			queue_redraw()
		return

	_refresh_clock += delta
	if _refresh_clock >= REFRESH_SEC:
		_refresh()
		_refresh_clock = 0.0
		queue_redraw()
	elif layout_changed:
		queue_redraw()


func _refresh() -> void:
	if _sim == null or not _sim.has_method("get_world_overview"):
		return
	_data = _sim.call("get_world_overview", 96)


func _layout() -> bool:
	var previous_size := size
	var previous_position := position
	var viewport_size := get_viewport_rect().size
	if _expanded:
		var side := minf(viewport_size.x, viewport_size.y) * 0.84
		size = Vector2(side, side)
		position = (viewport_size - size) * 0.5
	else:
		size = NORMAL_SIZE
		position = Vector2(
			maxf(MARGIN, viewport_size.x - size.x - MARGIN),
			maxf(332.0, viewport_size.y - size.y - MARGIN)
		)
	return previous_size != size or previous_position != position


func _draw() -> void:
	draw_rect(Rect2(Vector2.ZERO, size), Palette.OVERVIEW_BACKGROUND, true)
	draw_rect(Rect2(Vector2.ZERO, size), Palette.OVERVIEW_BORDER, false, 1.0)
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
			var color := Palette.overview_cell_color(
				int(surface[i]),
				int(plants[i]) if i < plants.size() else 0,
				int(herbivores[i]) if i < herbivores.size() else 0,
				int(omnivores[i]) if i < omnivores.size() else 0,
				int(carnivores[i]) if i < carnivores.size() else 0,
				int(insects[i]) if i < insects.size() else 0
			)
			draw_rect(
				Rect2(
					map_rect.position + Vector2(float(x), float(z)) * pixel,
					pixel + Vector2(0.5, 0.5)
				),
				color,
				true
			)

	var render_center: Vector3 = _data.get("render_center", Vector3.ZERO)
	var render_radius := float(_data.get("render_radius", 0.0))
	var world_size := Vector2(float(width) * cell_size, float(height) * cell_size)
	if world_size.x > 0.0 and world_size.y > 0.0 and render_radius > 0.0:
		var min_world := Vector2(
			render_center.x - render_radius - origin.x,
			render_center.z - render_radius - origin.z
		)
		var max_world := Vector2(
			render_center.x + render_radius - origin.x,
			render_center.z + render_radius - origin.z
		)
		var min_px := map_rect.position + Vector2(
			clampf(min_world.x / world_size.x, 0.0, 1.0) * map_rect.size.x,
			clampf(min_world.y / world_size.y, 0.0, 1.0) * map_rect.size.y
		)
		var max_px := map_rect.position + Vector2(
			clampf(max_world.x / world_size.x, 0.0, 1.0) * map_rect.size.x,
			clampf(max_world.y / world_size.y, 0.0, 1.0) * map_rect.size.y
		)
		draw_rect(Rect2(min_px, max_px - min_px), Palette.INTEREST_OUTLINE, false, 2.0)

	draw_string(
		get_theme_default_font(),
		Vector2(10.0, 17.0),
		tr("UI_WORLD_OVERVIEW_TITLE"),
		HORIZONTAL_ALIGNMENT_LEFT,
		-1.0,
		12,
		Palette.OVERVIEW_TEXT
	)
