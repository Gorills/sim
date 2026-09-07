extends Node

signal view_mode_changed(overview_enabled: bool)

const Actions = preload("res://scripts/input_actions.gd")

const LOCAL_MIN_DISTANCE := 250.0
const LOCAL_MAX_DISTANCE := 3200.0
const LOCAL_DEFAULT_DISTANCE := 1200.0
const LOCAL_MIN_PITCH := deg_to_rad(22.0)
const LOCAL_MAX_PITCH := deg_to_rad(78.0)
const LOCAL_DEFAULT_PITCH := deg_to_rad(55.0)
const LOCAL_MOVE_MIN_SPEED := 180.0
const LOCAL_MOVE_DISTANCE_FACTOR := 0.72
const LOCAL_ORBIT_SPEED := 1.65
const MOUSE_ORBIT_SENSITIVITY := 0.0055
const MOUSE_PAN_DISTANCE_FACTOR := 0.0018
const ZOOM_IN_FACTOR := 0.84
const ZOOM_OUT_FACTOR := 1.19
const STREAM_CENTER_STEP := 300.0
const STREAM_MIN_RADIUS := 900.0
const STREAM_MAX_RADIUS := 3800.0
const MAP_PADDING := 1.08
const MAP_MIN_FRACTION := 0.12
const SMOOTH_RATE := 12.0

@export var camera_path: NodePath
@export var sun_path: NodePath

@onready var _camera: Camera3D = get_node(camera_path) as Camera3D
@onready var _sun: DirectionalLight3D = get_node(sun_path) as DirectionalLight3D

var _sim: Node
var _world_bounds := Rect2(Vector2(-9600.0, -9600.0), Vector2(19200.0, 19200.0))

var _overview_mode := false
var _target := Vector3.ZERO
var _desired_target := Vector3.ZERO
var _distance := LOCAL_DEFAULT_DISTANCE
var _desired_distance := LOCAL_DEFAULT_DISTANCE
var _yaw := 0.0
var _desired_yaw := 0.0
var _pitch := LOCAL_DEFAULT_PITCH
var _desired_pitch := LOCAL_DEFAULT_PITCH

var _overview_target := Vector3.ZERO
var _overview_size := 19200.0
var _desired_overview_size := 19200.0

var _last_stream_center := Vector3.INF
var _last_stream_radius := -1.0


func bind_sim(sim: Node, world_bounds: Rect2) -> void:
	_sim = sim
	set_world_bounds(world_bounds)
	reset_view()


func set_world_bounds(world_bounds: Rect2) -> void:
	if world_bounds.size.x <= 0.0 or world_bounds.size.y <= 0.0:
		return
	_world_bounds = world_bounds


func reset_view() -> void:
	var center_2d := _world_bounds.get_center()
	var center := Vector3(center_2d.x, 0.0, center_2d.y)
	_overview_mode = false
	_target = center
	_desired_target = center
	_distance = LOCAL_DEFAULT_DISTANCE
	_desired_distance = LOCAL_DEFAULT_DISTANCE
	_yaw = 0.0
	_desired_yaw = 0.0
	_pitch = LOCAL_DEFAULT_PITCH
	_desired_pitch = LOCAL_DEFAULT_PITCH
	_overview_target = center
	_overview_size = _overview_fit_size()
	_desired_overview_size = _overview_size
	_last_stream_center = Vector3.INF
	_last_stream_radius = -1.0
	_apply_camera()
	_sync_render_interest(true)
	view_mode_changed.emit(false)


func is_overview_mode() -> bool:
	return _overview_mode


func toggle_overview() -> void:
	if _sim == null:
		return
	if _overview_mode:
		_overview_mode = false
		_target = _overview_target
		_desired_target = _overview_target
		_sync_render_interest(true)
	else:
		_overview_mode = true
		_overview_target = _desired_target
		_overview_size = _overview_fit_size()
		_desired_overview_size = _overview_size
		_sync_render_interest(true)
	view_mode_changed.emit(_overview_mode)


func _process(delta: float) -> void:
	if _sim == null or _camera == null:
		return
	if _camera_input_allowed():
		_apply_continuous_input(delta)
	_smooth_state(delta)
	_apply_camera()
	_sync_render_interest(false)


func _unhandled_input(event: InputEvent) -> void:
	if _sim == null:
		return

	if event.is_action_pressed(Actions.VIEW_TOGGLE_OVERVIEW) and not _is_echo(event):
		toggle_overview()
		get_viewport().set_input_as_handled()
		return

	if event.is_action_pressed(Actions.CAMERA_ZOOM_IN):
		_zoom(ZOOM_IN_FACTOR)
		get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed(Actions.CAMERA_ZOOM_OUT):
		_zoom(ZOOM_OUT_FACTOR)
		get_viewport().set_input_as_handled()
		return

	if event is InputEventMouseMotion:
		var motion := event as InputEventMouseMotion
		if Input.is_action_pressed(Actions.CAMERA_ORBIT_DRAG) and not _overview_mode:
			_desired_yaw -= motion.relative.x * MOUSE_ORBIT_SENSITIVITY
			_desired_pitch = clampf(
				_desired_pitch - motion.relative.y * MOUSE_ORBIT_SENSITIVITY,
				LOCAL_MIN_PITCH,
				LOCAL_MAX_PITCH
			)
			get_viewport().set_input_as_handled()
		elif Input.is_action_pressed(Actions.CAMERA_PAN_DRAG):
			_pan_mouse(motion.relative)
			get_viewport().set_input_as_handled()


func _apply_continuous_input(delta: float) -> void:
	var move := Input.get_vector(
		Actions.CAMERA_MOVE_LEFT,
		Actions.CAMERA_MOVE_RIGHT,
		Actions.CAMERA_MOVE_FORWARD,
		Actions.CAMERA_MOVE_BACK
	)
	if move.length_squared() > 0.0001:
		if _overview_mode:
			var map_speed := _desired_overview_size * 0.72
			_overview_target += Vector3(move.x, 0.0, move.y) * map_speed * delta
			_overview_target = _clamp_target(_overview_target, true)
		else:
			var right := _camera.global_transform.basis.x
			right.y = 0.0
			right = right.normalized()
			var forward := -_camera.global_transform.basis.z
			forward.y = 0.0
			forward = forward.normalized()
			var speed := maxf(LOCAL_MOVE_MIN_SPEED, _desired_distance * LOCAL_MOVE_DISTANCE_FACTOR)
			_desired_target += (right * move.x + forward * -move.y) * speed * delta
			_desired_target = _clamp_target(_desired_target, false)

	if not _overview_mode:
		var orbit := Input.get_vector(
			Actions.CAMERA_ORBIT_LEFT,
			Actions.CAMERA_ORBIT_RIGHT,
			Actions.CAMERA_ORBIT_UP,
			Actions.CAMERA_ORBIT_DOWN
		)
		if orbit.length_squared() > 0.0001:
			_desired_yaw -= orbit.x * LOCAL_ORBIT_SPEED * delta
			_desired_pitch = clampf(
				_desired_pitch - orbit.y * LOCAL_ORBIT_SPEED * delta,
				LOCAL_MIN_PITCH,
				LOCAL_MAX_PITCH
			)


func _pan_mouse(relative: Vector2) -> void:
	if _overview_mode:
		var viewport_height := maxf(get_viewport().get_visible_rect().size.y, 1.0)
		var units_per_pixel := _desired_overview_size / viewport_height
		_overview_target += Vector3(-relative.x, 0.0, -relative.y) * units_per_pixel
		_overview_target = _clamp_target(_overview_target, true)
		return

	var right := _camera.global_transform.basis.x
	right.y = 0.0
	right = right.normalized()
	var forward := -_camera.global_transform.basis.z
	forward.y = 0.0
	forward = forward.normalized()
	var pan_scale := _desired_distance * MOUSE_PAN_DISTANCE_FACTOR
	_desired_target += (-right * relative.x + forward * relative.y) * pan_scale
	_desired_target = _clamp_target(_desired_target, false)


func _zoom(factor: float) -> void:
	if _overview_mode:
		var min_size := maxf(
			minf(_world_bounds.size.x, _world_bounds.size.y) * MAP_MIN_FRACTION,
			800.0
		)
		var max_size := _overview_fit_size() * 1.15
		_desired_overview_size = clampf(_desired_overview_size * factor, min_size, max_size)
		_overview_target = _clamp_target(_overview_target, true)
	else:
		_desired_distance = clampf(
			_desired_distance * factor,
			LOCAL_MIN_DISTANCE,
			LOCAL_MAX_DISTANCE
		)


func _smooth_state(delta: float) -> void:
	var weight := 1.0 - exp(-SMOOTH_RATE * delta)
	if _overview_mode:
		_overview_size = lerpf(_overview_size, _desired_overview_size, weight)
	else:
		_target = _target.lerp(_desired_target, weight)
		_distance = lerpf(_distance, _desired_distance, weight)
		_yaw = lerp_angle(_yaw, _desired_yaw, weight)
		_pitch = lerpf(_pitch, _desired_pitch, weight)


func _apply_camera() -> void:
	if _overview_mode:
		_camera.projection = Camera3D.PROJECTION_ORTHOGONAL
		_camera.keep_aspect = Camera3D.KEEP_HEIGHT
		_camera.size = _overview_size
		_camera.near = 10.0
		_camera.far = 30000.0
		var height := maxf(3000.0, maxf(_world_bounds.size.x, _world_bounds.size.y) * 0.35)
		_camera.position = _overview_target + Vector3.UP * height
		_camera.look_at(_overview_target, Vector3.FORWARD)
		if _sun != null:
			_sun.directional_shadow_max_distance = 0.0
		return

	_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	_camera.near = 0.5
	_camera.fov = 48.0
	_camera.far = clampf(_distance * 3.4, 3200.0, 10000.0)
	var horizontal := cos(_pitch) * _distance
	_camera.position = _target + Vector3(
		sin(_yaw) * horizontal,
		sin(_pitch) * _distance,
		cos(_yaw) * horizontal
	)
	_camera.look_at(_target + Vector3.UP * 20.0)
	if _sun != null:
		_sun.directional_shadow_max_distance = clampf(_distance * 2.4, 1200.0, 5500.0)


func _sync_render_interest(force: bool) -> void:
	if _sim == null:
		return
	if _overview_mode and not force:
		return

	var stream_center := _overview_target if _overview_mode else _stream_center(_target)
	var radius := 150.0 if _overview_mode else _local_render_radius()
	if (
		not force
		and stream_center.distance_squared_to(_last_stream_center) < 1.0
		and absf(radius - _last_stream_radius) < 1.0
	):
		return

	_sim.set("render_center", stream_center)
	_sim.set("render_radius", radius)
	_last_stream_center = stream_center
	_last_stream_radius = radius
	if force and _sim.has_method("refresh_render_interest"):
		_sim.call("refresh_render_interest")


func _local_render_radius() -> float:
	return clampf(
		_desired_distance * 1.35 + 550.0,
		STREAM_MIN_RADIUS,
		STREAM_MAX_RADIUS
	)


func _stream_center(center: Vector3) -> Vector3:
	return Vector3(
		roundf(center.x / STREAM_CENTER_STEP) * STREAM_CENTER_STEP,
		0.0,
		roundf(center.z / STREAM_CENTER_STEP) * STREAM_CENTER_STEP
	)


func _overview_fit_size() -> float:
	var viewport := get_viewport().get_visible_rect().size
	var aspect := viewport.x / maxf(viewport.y, 1.0)
	var vertical_size := _world_bounds.size.y * MAP_PADDING
	if aspect < 1.0:
		vertical_size = maxf(vertical_size, _world_bounds.size.x * MAP_PADDING / aspect)
	return vertical_size


func _clamp_target(target: Vector3, account_for_map_view: bool) -> Vector3:
	var min_x := _world_bounds.position.x
	var min_z := _world_bounds.position.y
	var max_x := _world_bounds.end.x
	var max_z := _world_bounds.end.y
	if account_for_map_view:
		var viewport := get_viewport().get_visible_rect().size
		var aspect := viewport.x / maxf(viewport.y, 1.0)
		var half_z := minf(_desired_overview_size * 0.5, _world_bounds.size.y * 0.5)
		var half_x := minf(_desired_overview_size * aspect * 0.5, _world_bounds.size.x * 0.5)
		if half_x * 2.0 >= _world_bounds.size.x:
			target.x = _world_bounds.get_center().x
		else:
			target.x = clampf(target.x, min_x + half_x, max_x - half_x)
		if half_z * 2.0 >= _world_bounds.size.y:
			target.z = _world_bounds.get_center().y
		else:
			target.z = clampf(target.z, min_z + half_z, max_z - half_z)
	else:
		target.x = clampf(target.x, min_x, max_x)
		target.z = clampf(target.z, min_z, max_z)
	target.y = 0.0
	return target


func _camera_input_allowed() -> bool:
	return get_viewport().gui_get_focus_owner() == null


func _is_echo(event: InputEvent) -> bool:
	return event is InputEventKey and (event as InputEventKey).echo
