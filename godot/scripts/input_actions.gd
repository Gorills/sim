class_name SimInputActions
extends RefCounted

## Semantic input contract shared by the visualizer. Physical bindings live in
## project.godot; camera/presentation code depends only on these action names.

const CAMERA_MOVE_LEFT: StringName = &"camera_move_left"
const CAMERA_MOVE_RIGHT: StringName = &"camera_move_right"
const CAMERA_MOVE_FORWARD: StringName = &"camera_move_forward"
const CAMERA_MOVE_BACK: StringName = &"camera_move_back"
const CAMERA_MOVE_UP: StringName = &"camera_move_up"
const CAMERA_MOVE_DOWN: StringName = &"camera_move_down"

const CAMERA_LOOK_LEFT: StringName = &"camera_look_left"
const CAMERA_LOOK_RIGHT: StringName = &"camera_look_right"
const CAMERA_LOOK_UP: StringName = &"camera_look_up"
const CAMERA_LOOK_DOWN: StringName = &"camera_look_down"
const CAMERA_LOOK_DRAG: StringName = &"camera_look_drag"

const CAMERA_SPEED_INCREASE: StringName = &"camera_speed_increase"
const CAMERA_SPEED_DECREASE: StringName = &"camera_speed_decrease"
const CAMERA_SPEED_BOOST: StringName = &"camera_speed_boost"

const SIM_TOGGLE_PAUSE: StringName = &"sim_toggle_pause"
const SIM_RESET: StringName = &"sim_reset"
const SIM_SPEED_1: StringName = &"sim_speed_1"
const SIM_SPEED_2: StringName = &"sim_speed_2"
const SIM_SPEED_3: StringName = &"sim_speed_3"


static func required_actions() -> PackedStringArray:
	return PackedStringArray([
		CAMERA_MOVE_LEFT,
		CAMERA_MOVE_RIGHT,
		CAMERA_MOVE_FORWARD,
		CAMERA_MOVE_BACK,
		CAMERA_MOVE_UP,
		CAMERA_MOVE_DOWN,
		CAMERA_LOOK_LEFT,
		CAMERA_LOOK_RIGHT,
		CAMERA_LOOK_UP,
		CAMERA_LOOK_DOWN,
		CAMERA_LOOK_DRAG,
		CAMERA_SPEED_INCREASE,
		CAMERA_SPEED_DECREASE,
		CAMERA_SPEED_BOOST,
		SIM_TOGGLE_PAUSE,
		SIM_RESET,
		SIM_SPEED_1,
		SIM_SPEED_2,
		SIM_SPEED_3,
	])
