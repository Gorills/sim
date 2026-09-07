extends Node3D

## Wires the C++ SimWorld node into the scene after the GDExtension loads.
## Space pauses; R resets; 1/2/3 set speed. Presentation only.

@onready var world_view: Node3D = $WorldView
@onready var hud: Label = %Hud
@onready var legend: RichTextLabel = %Legend
@onready var camera: Camera3D = $Camera3D
@onready var sun: DirectionalLight3D = $DirectionalLight3D
@onready var world_environment: WorldEnvironment = $WorldEnvironment
@onready var ocean: MeshInstance3D = $Ocean
@onready var overview: Control = %WorldOverview

var _sim: Node
var _speed_scale: float = 1.0
var _camera_distance: float = 1200.0
var _camera_target := Vector3.ZERO
var _camera_yaw: float = 0.0
var _camera_pitch: float = deg_to_rad(55.0)
var _island_extent: float = 19200.0


func _ready() -> void:
	if not ClassDB.class_exists("SimWorld"):
		printerr("SIM: GDExtension not loaded. Build sim_godot and restart Godot.")
		hud.text = "GDExtension not loaded.\nBuild sim_godot (cmake --preset debug-godot)\nand restart the Godot editor."
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
	if overview != null and overview.has_method("bind_sim"):
		overview.call("bind_sim", _sim)
	_refresh_world_extent()
	_fit_camera()
	_rebuild_legend()
	print("SIM: entities=", _sim.call("get_entity_count"), " tick=", _sim.call("get_tick_index"))


func _unhandled_input(event: InputEvent) -> void:
	if _sim == null:
		return
	if event is InputEventMouseButton and event.pressed:
		if event.button_index == MOUSE_BUTTON_WHEEL_UP:
			_camera_distance = clampf(_camera_distance * 0.86, 250.0, 4000.0)
			_fit_camera()
		elif event.button_index == MOUSE_BUTTON_WHEEL_DOWN:
			_camera_distance = clampf(_camera_distance * 1.16, 250.0, 4000.0)
			_fit_camera()
	if event is InputEventMouseMotion:
		var motion := event as InputEventMouseMotion
		if motion.button_mask & MOUSE_BUTTON_MASK_RIGHT:
			_camera_yaw -= motion.relative.x * 0.006
			_camera_pitch = clampf(
				_camera_pitch + motion.relative.y * 0.004,
				deg_to_rad(18.0),
				deg_to_rad(78.0)
			)
			_fit_camera()
		elif motion.button_mask & MOUSE_BUTTON_MASK_MIDDLE:
			var right := camera.global_transform.basis.x
			right.y = 0.0
			right = right.normalized()
			var forward := -camera.global_transform.basis.z
			forward.y = 0.0
			forward = forward.normalized()
			var pan_scale := _camera_distance * 0.0018
			_camera_target += (-right * motion.relative.x + forward * motion.relative.y) * pan_scale
			var half_extent := _island_extent * 0.48
			_camera_target.x = clampf(_camera_target.x, -half_extent, half_extent)
			_camera_target.z = clampf(_camera_target.z, -half_extent, half_extent)
			_fit_camera()
	if event is InputEventKey and event.pressed and not event.echo:
		match event.keycode:
			KEY_SPACE:
				_sim.set("paused", not bool(_sim.get("paused")))
			KEY_R:
				_sim.call("reset_world")
				_refresh_world_extent()
				_rebuild_legend()
				_fit_camera()
			KEY_1:
				_set_speed(1.0)
			KEY_2:
				_set_speed(4.0)
			KEY_3:
				_set_speed(16.0)
			KEY_M:
				if overview != null and overview.has_method("toggle_expanded"):
					overview.call("toggle_expanded")


func _process(_delta: float) -> void:
	if _sim == null:
		return
	var stats: Dictionary = _sim.call("get_ecosystem_stats") if _sim.has_method("get_ecosystem_stats") else {}
	var paused := bool(stats.get("paused", false))
	var days := float(stats.get("simulated_hours", 0.0)) / 24.0
	var season := _season_name(float(stats.get("year_phase", 0.0)))
	var hour := float(stats.get("hour_of_day", 0.0))
	_update_daylight(hour)
	var populations: Dictionary = stats.get("populations", {})
	hud.text = "day %.2f   %s %s   %.0fx   entities %s   %s\nclimate  moisture %.2f   %.1f C   canopy %.3f (max %.2f)   light %.2f   organic %.3f (max %.2f)   pollen %.3f (max %.2f)\nguilds  plants %s   herbivores %s   omnivores %s   carnivores %s   insects %s (activity %.2f)   decomposers %s\nbehavior  rest %s   roam %s   group %s   forage %s   feed %s   drink %s   flee %s\n%s\nSpace pause   R reset   1/2/3 speed   M overview   wheel zoom   RMB orbit   MMB pan" % [
		days,
		season,
		_clock_text(hour),
		_speed_scale,
		str(stats.get("entity_count", 0)),
		"paused" if paused else "running",
		float(stats.get("mean_moisture", 0.0)),
		float(stats.get("mean_temperature", 0.0)),
		float(stats.get("mean_canopy", 0.0)),
		float(stats.get("max_canopy", 0.0)),
		float(stats.get("mean_light", 0.0)),
		float(stats.get("mean_organic", 0.0)),
		float(stats.get("max_organic", 0.0)),
		float(stats.get("mean_pollination", 0.0)),
		float(stats.get("max_pollination", 0.0)),
		str(stats.get("plants", 0)),
		str(stats.get("herbivores", 0)),
		str(stats.get("omnivores", 0)),
		str(stats.get("carnivores", 0)),
		str(stats.get("insects", 0)),
		float(stats.get("mean_insect_activity", 0.0)),
		str(stats.get("decomposers", 0)),
		str(stats.get("resting", 0)),
		str(stats.get("roaming", 0)),
		str(stats.get("socializing", 0)),
		str(stats.get("foraging", 0)),
		str(stats.get("feeding", 0)),
		str(stats.get("drinking", 0)),
		str(stats.get("fleeing", 0)),
		_population_line(populations),
	]


func _set_speed(scale: float) -> void:
	_speed_scale = scale
	_sim.set("speed_scale", scale)


func _refresh_world_extent() -> void:
	if _sim == null or not _sim.has_method("get_world_overview"):
		return
	var data: Dictionary = _sim.call("get_world_overview", 16)
	var width := int(data.get("width", 0))
	var height := int(data.get("height", 0))
	var cell_size := float(data.get("cell_size", 0.0))
	if width > 0 and height > 0 and cell_size > 0.0:
		_island_extent = maxf(float(width), float(height)) * cell_size
	var ocean_scale := (_island_extent * 1.35) / 96.0
	ocean.scale = Vector3(ocean_scale, 1.0, ocean_scale)


func _sync_render_interest() -> void:
	if _sim == null:
		return
	var radius := clampf(_camera_distance * 1.85, 800.0, 5000.0)
	_sim.set("render_center", _camera_target)
	_sim.set("render_radius", radius)


func _fit_camera() -> void:
	if _sim == null:
		return
	var distance := _camera_distance
	var horizontal := cos(_camera_pitch) * distance
	camera.position = _camera_target + Vector3(
		sin(_camera_yaw) * horizontal,
		sin(_camera_pitch) * distance,
		cos(_camera_yaw) * horizontal
	)
	camera.look_at(_camera_target + Vector3.UP * 20.0)
	camera.fov = 48.0
	camera.far = 12000.0
	sun.directional_shadow_max_distance = clampf(_camera_distance * 2.5, 1200.0, 6000.0)
	_sync_render_interest()


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
	var bb := "[b]Species[/b]\n"
	for line in lines:
		var parts := line.split(" ", false, 1)
		if parts.size() < 2:
			continue
		bb += "[color=#%s]■[/color] %s\n" % [parts[0], parts[1]]
	legend.text = bb


func _season_name(phase: float) -> String:
	if phase < 0.125 or phase >= 0.875:
		return "winter"
	if phase < 0.375:
		return "spring"
	if phase < 0.625:
		return "summer"
	return "autumn"


func _population_line(populations: Dictionary) -> String:
	if populations.is_empty():
		return "populations  —"
	var keys: Array = populations.keys()
	keys.sort()
	var parts: PackedStringArray = PackedStringArray()
	for key in keys:
		parts.append("%s=%s" % [str(key), str(populations[key])])
	return " ".join(parts)
