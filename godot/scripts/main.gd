extends Node3D

## Scene orchestration only. Physical bindings live in project.godot and camera
## behavior lives in camera_controller.gd.

const Actions = preload("res://scripts/input_actions.gd")

@onready var world_view: Node3D = $WorldView
@onready var world_map_view: Node3D = $WorldMapView
@onready var camera_controller: Node = $CameraController
@onready var hud: Label = %Hud
@onready var legend: RichTextLabel = %Legend
@onready var controls_hint: Label = %ControlsHint
@onready var view_badge: Label = %ViewBadge
@onready var sun: DirectionalLight3D = $DirectionalLight3D
@onready var world_environment: WorldEnvironment = $WorldEnvironment
@onready var ocean: MeshInstance3D = $Ocean
@onready var overview: Control = %WorldOverview

var _sim: Node
var _speed_scale := 1.0
var _world_bounds := Rect2(Vector2(-9600.0, -9600.0), Vector2(19200.0, 19200.0))


func _ready() -> void:
	if not ClassDB.class_exists("SimWorld"):
		printerr("SIM: GDExtension not loaded. Build sim_godot and restart Godot.")
		hud.text = tr("UI_EXTENSION_MISSING")
		return

	print("SIM: SimWorld GDExtension loaded")
	_sim = ClassDB.instantiate("SimWorld") as Node
	_sim.name = "SimWorld"
	_sim.set("island_mode", true)
	_sim.set("tick_hz", 60.0)
	_sim.set("speed_scale", _speed_scale)
	add_child(_sim)
	move_child(_sim, 0)

	if world_view.has_method("bind_sim"):
		world_view.call("bind_sim", _sim)
	if world_map_view.has_method("bind_sim"):
		world_map_view.call("bind_sim", _sim)
	if overview != null and overview.has_method("bind_sim"):
		overview.call("bind_sim", _sim)

	_refresh_world_bounds()
	camera_controller.connect("view_mode_changed", _on_view_mode_changed)
	camera_controller.call("bind_sim", _sim, _world_bounds)
	_rebuild_legend()
	print("SIM: entities=", _sim.call("get_entity_count"), " tick=", _sim.call("get_tick_index"))


func _unhandled_input(event: InputEvent) -> void:
	if _sim == null or not _press_once(event):
		return

	if event.is_action_pressed(Actions.SIM_TOGGLE_PAUSE):
		_sim.set("paused", not bool(_sim.get("paused")))
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed(Actions.SIM_RESET):
		_sim.call("reset_world")
		_refresh_world_bounds()
		camera_controller.call("set_world_bounds", _world_bounds)
		camera_controller.call("reset_view")
		if world_map_view.has_method("refresh_now"):
			world_map_view.call("refresh_now")
		_rebuild_legend()
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed(Actions.SIM_SPEED_1):
		_set_speed(1.0)
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed(Actions.SIM_SPEED_2):
		_set_speed(4.0)
		get_viewport().set_input_as_handled()
	elif event.is_action_pressed(Actions.SIM_SPEED_3):
		_set_speed(16.0)
		get_viewport().set_input_as_handled()


func _process(_delta: float) -> void:
	if _sim == null:
		return

	var stats: Dictionary = (
		_sim.call("get_ecosystem_stats")
		if _sim.has_method("get_ecosystem_stats")
		else {}
	)
	var paused := bool(stats.get("paused", false))
	var days := float(stats.get("simulated_hours", 0.0)) / 24.0
	var season := _season_name(float(stats.get("year_phase", 0.0)))
	var hour := float(stats.get("hour_of_day", 0.0))
	_update_daylight(hour)

	hud.text = "\n".join([
		tr("HUD_SIMULATION") % [
			days,
			season,
			_clock_text(hour),
			_speed_scale,
			str(stats.get("entity_count", 0)),
			tr("HUD_PAUSED") if paused else tr("HUD_RUNNING"),
		],
		tr("HUD_CLIMATE") % [
			float(stats.get("mean_moisture", 0.0)),
			float(stats.get("mean_temperature", 0.0)),
			float(stats.get("mean_canopy", 0.0)),
			float(stats.get("max_canopy", 0.0)),
			float(stats.get("mean_light", 0.0)),
			float(stats.get("mean_organic", 0.0)),
			float(stats.get("max_organic", 0.0)),
			float(stats.get("mean_pollination", 0.0)),
			float(stats.get("max_pollination", 0.0)),
		],
		tr("HUD_GUILDS") % [
			str(stats.get("plants", 0)),
			str(stats.get("herbivores", 0)),
			str(stats.get("omnivores", 0)),
			str(stats.get("carnivores", 0)),
			str(stats.get("insects", 0)),
			float(stats.get("mean_insect_activity", 0.0)),
			str(stats.get("decomposers", 0)),
		],
		tr("HUD_BEHAVIOR") % [
			str(stats.get("resting", 0)),
			str(stats.get("roaming", 0)),
			str(stats.get("socializing", 0)),
			str(stats.get("foraging", 0)),
			str(stats.get("feeding", 0)),
			str(stats.get("drinking", 0)),
			str(stats.get("fleeing", 0)),
		],
		_population_line(stats.get("populations", {})),
	])


func _press_once(event: InputEvent) -> bool:
	if event is InputEventKey:
		var key := event as InputEventKey
		return key.pressed and not key.echo
	if event is InputEventJoypadButton:
		return (event as InputEventJoypadButton).pressed
	return false


func _set_speed(scale: float) -> void:
	_speed_scale = scale
	_sim.set("speed_scale", scale)


func _refresh_world_bounds() -> void:
	if _sim == null or not _sim.has_method("get_world_overview"):
		return

	var data: Dictionary = _sim.call("get_world_overview", 16)
	var width := int(data.get("width", 0))
	var height := int(data.get("height", 0))
	var cell_size := float(data.get("cell_size", 0.0))
	var origin: Vector3 = data.get("origin", Vector3.ZERO)
	if width <= 0 or height <= 0 or cell_size <= 0.0:
		return

	var world_size := Vector2(float(width) * cell_size, float(height) * cell_size)
	_world_bounds = Rect2(Vector2(origin.x, origin.z), world_size)
	var ocean_scale := maxf(world_size.x, world_size.y) * 1.35 / 96.0
	ocean.scale = Vector3(ocean_scale, 1.0, ocean_scale)


func _on_view_mode_changed(overview_enabled: bool) -> void:
	if world_view.has_method("set_detail_active"):
		world_view.call("set_detail_active", not overview_enabled)
	if world_map_view.has_method("set_active"):
		world_map_view.call("set_active", overview_enabled)

	ocean.visible = not overview_enabled
	if overview != null:
		overview.visible = not overview_enabled
	if legend != null and legend.get_parent() != null:
		legend.get_parent().visible = not overview_enabled

	view_badge.text = tr("UI_VIEW_MAP") if overview_enabled else tr("UI_VIEW_LOCAL")
	controls_hint.text = (
		tr("HUD_CONTROLS_MAP")
		if overview_enabled
		else tr("HUD_CONTROLS_LOCAL")
	)


func _update_daylight(hour: float) -> void:
	var solar_height := sin((hour - 6.0) / 24.0 * TAU)
	var daylight := smoothstep(-0.14, 0.35, solar_height)
	sun.rotation = Vector3(
		deg_to_rad(-12.0) - daylight * deg_to_rad(55.0),
		deg_to_rad(-35.0) + hour / 24.0 * TAU,
		0.0
	)
	sun.light_energy = lerpf(0.06, 1.05, daylight)
	sun.light_color = Color("#7890b5").lerp(Color("#ffe8c2"), daylight)
	if world_environment.environment != null:
		world_environment.environment.ambient_light_energy = lerpf(0.11, 0.48, daylight)
		world_environment.environment.ambient_light_color = Color("#253552").lerp(
			Color("#b8c7bf"),
			daylight
		)


func _clock_text(hour: float) -> String:
	var total_minutes := int(floor(fposmod(hour, 24.0) * 60.0))
	return "%02d:%02d" % [total_minutes / 60, total_minutes % 60]


func _rebuild_legend() -> void:
	if legend == null or world_view == null or not world_view.has_method("legend_lines"):
		return

	var lines: PackedStringArray = world_view.call("legend_lines")
	var bb := "[b]%s[/b]\n" % tr("UI_LEGEND_TITLE")
	for line in lines:
		var parts := line.split(" ", false, 1)
		if parts.size() < 2:
			continue
		bb += "[color=#%s]■[/color] %s\n" % [parts[0], parts[1]]
	legend.text = bb


func _season_name(phase: float) -> String:
	if phase < 0.125 or phase >= 0.875:
		return tr("SEASON_WINTER")
	if phase < 0.375:
		return tr("SEASON_SPRING")
	if phase < 0.625:
		return tr("SEASON_SUMMER")
	return tr("SEASON_AUTUMN")


func _population_line(populations: Dictionary) -> String:
	if populations.is_empty():
		return tr("HUD_POPULATIONS_EMPTY")

	var keys: Array = populations.keys()
	keys.sort()
	var parts := PackedStringArray()
	for key in keys:
		parts.append("%s=%s" % [str(key), str(populations[key])])
	return tr("HUD_POPULATIONS") % " ".join(parts)
