extends Node

## Player-scale third-person spectator. God mode moves the avatar anchor freely,
## while camera distance/FOV stay at the future player-view scale.

const Actions = preload("res://scripts/input_actions.gd")

const CAMERA_DISTANCE := 7.0
const CAMERA_FOCUS_HEIGHT := 1.35
const DEFAULT_PITCH := deg_to_rad(-10.0)
const MIN_PITCH := deg_to_rad(-45.0)
const MAX_PITCH := deg_to_rad(22.0)
const DEFAULT_SPEED := 6.0
const MIN_SPEED := 2.0
const MAX_SPEED := 80.0
const SPEED_STEP := 1.30
const BOOST_FACTOR := 2.0
const MOVE_RESPONSE := 11.0
const AVATAR_TURN_RESPONSE := 12.0
const LOOK_SPEED := 1.85
const MOUSE_LOOK_SENSITIVITY := 0.0025
const CAMERA_GROUND_CLEARANCE := 0.45
const CAMERA_COLLISION_SAMPLES := 8
const GROUND_CLEARANCE := 0.9
const MAX_WORLD_Y := 520.0

const RENDER_RADIUS := 450.0
const STREAM_SHIFT_THRESHOLD := 120.0
const CAMERA_FAR := 650.0
const SHADOW_DISTANCE := 350.0

@export var camera_path: NodePath
@export var sun_path: NodePath
@export var avatar_path: NodePath
@export var world_view_path: NodePath

var _camera: Camera3D
var _sun: DirectionalLight3D
var _avatar: Node3D
var _world_view: Node

var _sim: Node
var _world_bounds := Rect2(Vector2(-9600.0, -9600.0), Vector2(19200.0, 19200.0))
var _anchor_position := Vector3.ZERO
var _velocity := Vector3.ZERO
var _yaw := 0.0
var _avatar_yaw := 0.0
var _pitch := DEFAULT_PITCH
var _fly_speed := DEFAULT_SPEED
var _last_stream_center := Vector3.INF


func bind_sim(sim: Node, world_bounds: Rect2, spawn_position: Vector3) -> void:
	_resolve_nodes()
	if _camera == null or _avatar == null or _world_view == null:
		push_error("CameraController paths are not resolved.")
		return
	_sim = sim
	set_world_bounds(world_bounds)
	_velocity = Vector3.ZERO
	_yaw = 0.0
	_avatar_yaw = 0.0
	_pitch = DEFAULT_PITCH
	_fly_speed = DEFAULT_SPEED
	_anchor_position = _clamp_horizontal(spawn_position)
	_last_stream_center = Vector3.INF
	_sync_render_interest(true)
	_anchor_position.y = _ground_height(_anchor_position) + GROUND_CLEARANCE
	if DisplayServer.get_name() != "headless":
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
	_apply_camera()


func _resolve_nodes() -> void:
	_camera = get_node_or_null(camera_path) as Camera3D
	_sun = get_node_or_null(sun_path) as DirectionalLight3D
	_avatar = get_node_or_null(avatar_path) as Node3D
	_world_view = get_node_or_null(world_view_path)


func set_world_bounds(world_bounds: Rect2) -> void:
	if world_bounds.size.x <= 0.0 or world_bounds.size.y <= 0.0:
		return
	_world_bounds = world_bounds


func teleport_to(world_position: Vector3) -> void:
	if _sim == null:
		return
	_velocity = Vector3.ZERO
	_anchor_position = _clamp_horizontal(world_position)
	_sync_render_interest(true)
	_refresh_world_view()
	_anchor_position.y = _ground_height(_anchor_position) + GROUND_CLEARANCE
	_apply_camera()


func anchor_position() -> Vector3:
	return _anchor_position


func render_radius() -> float:
	return RENDER_RADIUS


func camera_distance() -> float:
	return CAMERA_DISTANCE


func fly_speed() -> float:
	return _fly_speed


func _exit_tree() -> void:
	if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE


func _process(delta: float) -> void:
	if _sim == null or _camera == null:
		return

	if _camera_input_allowed():
		_apply_gamepad_look(delta)

	var desired_velocity := Vector3.ZERO
	if _camera_input_allowed():
		desired_velocity = _movement_velocity()

	var response := 1.0 - exp(-MOVE_RESPONSE * delta)
	_velocity = _velocity.lerp(desired_velocity, response)
	_anchor_position += _velocity * delta
	_anchor_position = _clamp_horizontal(_anchor_position)
	_anchor_position.y = clampf(
		_anchor_position.y,
		_ground_height(_anchor_position) + GROUND_CLEARANCE,
		MAX_WORLD_Y
	)

	_update_avatar_facing(delta)
	_sync_render_interest(false)
	_apply_camera()


func _unhandled_input(event: InputEvent) -> void:
	if _sim == null:
		return

	if event.is_action_pressed(Actions.CAMERA_LOOK_RELEASE):
		if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
			get_viewport().set_input_as_handled()
		return
	if event.is_action_pressed(Actions.CAMERA_LOOK_CAPTURE):
		if Input.mouse_mode != Input.MOUSE_MODE_CAPTURED:
			Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
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

	if (
		event is InputEventMouseMotion
		and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED
		and _camera_input_allowed()
	):
		var motion := event as InputEventMouseMotion
		_yaw -= motion.screen_relative.x * MOUSE_LOOK_SENSITIVITY
		_pitch = clampf(
			_pitch - motion.screen_relative.y * MOUSE_LOOK_SENSITIVITY,
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
	var forward := Vector3(sin(_yaw), 0.0, -cos(_yaw))
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
	var focus := _anchor_position + Vector3.UP * CAMERA_FOCUS_HEIGHT
	var direction := _look_direction()
	var desired_camera_position := focus - direction * CAMERA_DISTANCE
	var camera_position := _terrain_safe_camera_position(focus, desired_camera_position)

	_camera.projection = Camera3D.PROJECTION_PERSPECTIVE
	_camera.near = 0.10
	_camera.far = CAMERA_FAR
	_camera.fov = 68.0
	_camera.look_at_from_position(camera_position, focus, Vector3.UP)

	if _avatar != null:
		_avatar.position = _anchor_position
		_avatar.rotation.y = _avatar_yaw
	if _sun != null:
		_sun.directional_shadow_max_distance = SHADOW_DISTANCE


func _update_avatar_facing(delta: float) -> void:
	var horizontal_velocity := Vector2(_velocity.x, _velocity.z)
	if horizontal_velocity.length_squared() <= 0.01:
		return
	var target_yaw := atan2(_velocity.x, -_velocity.z)
	var response := 1.0 - exp(-AVATAR_TURN_RESPONSE * delta)
	_avatar_yaw = lerp_angle(_avatar_yaw, target_yaw, response)


func _terrain_safe_camera_position(focus: Vector3, desired: Vector3) -> Vector3:
	var safe_position := focus
	for sample_index in range(1, CAMERA_COLLISION_SAMPLES + 1):
		var t := float(sample_index) / float(CAMERA_COLLISION_SAMPLES)
		var sample := focus.lerp(desired, t)
		var ground_y := _ground_height(sample) + CAMERA_GROUND_CLEARANCE
		if sample.y < ground_y:
			var safe_t := maxf(
				0.0,
				float(sample_index - 1) / float(CAMERA_COLLISION_SAMPLES) - 0.03
			)
			return focus.lerp(desired, safe_t)
		safe_position = sample
	return safe_position


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
	var center := Vector3(_anchor_position.x, 0.0, _anchor_position.z)
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
	_refresh_world_view()


func _refresh_world_view() -> void:
	if _world_view != null and _world_view.has_method("refresh_interest_now"):
		_world_view.call("refresh_interest_now")


func _ground_height(world_position: Vector3) -> float:
	if _world_view != null and _world_view.has_method("presentation_height"):
		return float(_world_view.call("presentation_height", world_position))
	return 0.0


func _clamp_horizontal(position: Vector3) -> Vector3:
	position.x = clampf(position.x, _world_bounds.position.x, _world_bounds.end.x)
	position.z = clampf(position.z, _world_bounds.position.y, _world_bounds.end.y)
	return position


func _camera_input_allowed() -> bool:
	return get_viewport().gui_get_focus_owner() == null
