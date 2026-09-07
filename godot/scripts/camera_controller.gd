extends Node

## Free-fly spectator camera. The camera only moves presentation interest:
## simulation rules remain independent from the viewer.

const Actions = preload("res://scripts/input_actions.gd")

const DEFAULT_ALTITUDE := 650.0
const MIN_ALTITUDE := 24.0
const MAX_ALTITUDE := 2600.0
const DEFAULT_PITCH := deg_to_rad(-38.0)
const MIN_PITCH := deg_to_rad(-88.0)
const MAX_PITCH := deg_to_rad(82.0)
const DEFAULT_SPEED := 420.0
const MIN_SPEED := 80.0
const MAX_SPEED := 1600.0
const SPEED_STEP := 1.25
const BOOST_FACTOR := 3.5
const MOVE_RESPONSE := 9.0
const LOOK_SPEED := 1.75
const MOUSE_LOOK_SENSITIVITY := 0.0038
const RENDER_RADIUS := 1200.0
const STREAM_SHIFT_THRESHOLD := 550.0
const CAMERA_FAR := 4200.0
const SHADOW_DISTANCE := 1800.0

@export var camera_path: NodePath
@export var sun_path: NodePath

@onready var _camera: Camera3D = get_node(camera_path) as Camera3D
@onready var _sun: DirectionalLight3D = get_node(sun_path) as DirectionalLight3D

var _sim: Node
var _world_bounds := Rect2(Vector2(-9600.0, -9600.0), Vector2(19200.0, 19200.0))
var _position := Vector3.ZERO
var _velocity := Vector3.ZERO
var _yaw := 0.0
var _pitch := DEFAULT_PITCH
var _fly_speed := DEFAULT_SPEED
var _last_stream_center := Vector3.INF


func bind_sim(sim: Node, world_bounds: Rect2) -> void:
	_sim = sim
	set_world_bounds(world_bounds)
	reset_view()


func set_world_bounds(world_bounds: Rect2) -> void:
	if world_bounds.size.x <= 0.0 or world_bounds.size.y <= 0.0:
		return
	_world_bounds = world_bounds


func reset_view() -> void:
	var center := _world_bounds.get_center()
	_position = Vector3(center.x, DEFAULT_ALTITUDE, center.y)
	_velocity = Vector3.ZERO
	_yaw = 0.0
	_pitch = DEFAULT_PITCH
	_fly_speed = DEFAULT_SPEED
	_last_stream_center = Vector3.INF
	_apply_camera()
	_sync_render_interest(true)


func render_radius() -> float:
	return RENDER_RADIUS


func fly_speed() -> float:
	return _fly_speed


func _exit_tree() -> void:
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _process(delta: float) -> void:
	if _sim == null or _camera == null:
		return

	var desired_velocity := Vector3.ZERO
	if _camera_input_allowed():
		desired_velocity = _movement_velocity()

	var response := 1.0 - exp(-MOVE_RESPONSE * delta)
	_velocity = _velocity.lerp(desired_velocity, response)
	_position += _velocity * delta
	_position = _clamp_position(_position)

	_apply_gamepad_look(delta)
	_apply_camera()
	_sync_render_interest(false)


func _unhandled_input(event: InputEvent) -> void:
	if _sim == null:
		return

	if event.is_action_pressed(Actions.CAMERA_LOOK_DRAG):
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
		get_viewport().set_input_as_handled()
		return
	if event.is_action_released(Actions.CAMERA_LOOK_DRAG):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
		get_viewport().set_input_as_handled()
		return

	if event.is_action_pressed(Actions.CAMERA_SPEED_INCREASE):
		_fly_speed = clampf(_fly_speed * SPEED_STEP, MIN_SPEED, MAX_SPEED)
		get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed(Actions.CAMERA_SPEED_DECREASE):
		_fly_speed = clampf(_fly_speed / SPEED_STEP, MIN_SPEED, MAX_SPEED)
		get_viewport().set_input_as_handled()
		return

	if event is InputEventMouseMotion and Input.is_action_pressed(Actions.CAMERA_LOOK_DRAG):
		var motion := event as InputEventMouseMotion
		_yaw -= motion.relative.x * MOUSE_LOOK_SENSITIVITY
		_pitch = clampf(
			_pitch - motion.relative.y * MOUSE_LOOK_SENSITIVITY,
			MIN_PITCH,
			MAX_PITCH
		)
		get_viewport().set_input_as_handled()


func _movement_velocity() -> Vector3:
	var move := Input.get_vector(
		Actions.CAMERA_MOVE_LEFT,
		Actions.CAMERA_MOVE_RIGHT,
		Actions.CAMERA_MOVE_FORWARD,
		Actions.CAMERA_MOVE_BACK
	)
	var forward := _look_direction()
	forward.y = 0.0
	if forward.length_squared() < 0.0001:
		forward = Vector3.FORWARD
	else:
		forward = forward.normalized()
	var right := forward.cross(Vector3.UP).normalized()
	var vertical := (
		Input.get_action_strength(Actions.CAMERA_MOVE_UP)
		- Input.get_action_strength(Actions.CAMERA_MOVE_DOWN)
	)

	var direction := right * move.x + forward * -move.y + Vector3.UP * vertical
	if direction.length_squared() > 1.0:
		direction = direction.normalized()

	var speed := _fly_speed
	if Input.is_action_pressed(Actions.CAMERA_SPEED_BOOST):
		speed *= BOOST_FACTOR
	return direction * speed


func _apply_gamepad_look(delta: float) -> void:
	if not _camera_input_allowed():
		return
	var look := Input.get_vector(
		Actions.CAMERA_LOOK_LEFT,
		Actions.CAMERA_LOOK_RIGHT,
		Actions.CAMERA_LOOK_UP,
		Actions.CAMERA_LOOK_DOWN
	)
	if look.length_squared() <= 0.0001:
		return
	_yaw -= look.x * LOOK_SPEED * delta
	_pitch = clampf(
		_pitch - look.y * LOOK_SPEED * delta,
		MIN_PITCH,
		MAX_PITCH
	)


func _apply_camera() -> void:
	_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	_camera.near = 0.5
	_camera.far = CAMERA_FAR
	_camera.fov = 55.0
	_camera.position = _position
	_camera.look_at(_position + _look_direction(), Vector3.UP)
	if _sun != null:
		_sun.directional_shadow_max_distance = SHADOW_DISTANCE


func _look_direction() -> Vector3:
	var horizontal := cos(_pitch)
	return Vector3(
		sin(_yaw) * horizontal,
		sin(_pitch),
		-cos(_yaw) * horizontal
	).normalized()


func _sync_render_interest(force: bool) -> void:
	if _sim == null:
		return

	var center := Vector3(_position.x, 0.0, _position.z)
	var moved_far_enough := (
		_last_stream_center == Vector3.INF
		or center.distance_squared_to(_last_stream_center)
			>= STREAM_SHIFT_THRESHOLD * STREAM_SHIFT_THRESHOLD
	)
	if not force and not moved_far_enough:
		return

	_sim.set("render_center", center)
	_sim.set("render_radius", RENDER_RADIUS)
	_last_stream_center = center
	if _sim.has_method("refresh_render_interest"):
		_sim.call("refresh_render_interest")


func _clamp_position(position: Vector3) -> Vector3:
	position.x = clampf(position.x, _world_bounds.position.x, _world_bounds.end.x)
	position.z = clampf(position.z, _world_bounds.position.y, _world_bounds.end.y)
	position.y = clampf(position.y, MIN_ALTITUDE, MAX_ALTITUDE)
	return position


func _camera_input_allowed() -> bool:
	return get_viewport().gui_get_focus_owner() == null
