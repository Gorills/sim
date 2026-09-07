class_name SimInputActions
extends RefCounted

## Semantic input contract shared by the visualizer. Bindings live in project.godot;
## gameplay/presentation code depends only on these action names.

const CAMERA_MOVE_LEFT: StringName = &"camera_move_left"
const CAMERA_MOVE_RIGHT: StringName = &"camera_move_right"
const CAMERA_MOVE_FORWARD: StringName = &"camera_move_forward"
const CAMERA_MOVE_BACK: StringName = &"camera_move_back"
const CAMERA_ORBIT_LEFT: StringName = &"camera_orbit_left"
const CAMERA_ORBIT_RIGHT: StringName = &"camera_orbit_right"
const CAMERA_ORBIT_UP: StringName = &"camera_orbit_up"
const CAMERA_ORBIT_DOWN: StringName = &"camera_orbit_down"
const CAMERA_ZOOM_IN: StringName = &"camera_zoom_in"
const CAMERA_ZOOM_OUT: StringName = &"camera_zoom_out"
const CAMERA_ORBIT_DRAG: StringName = &"camera_orbit_drag"
const CAMERA_PAN_DRAG: StringName = &"camera_pan_drag"
const VIEW_TOGGLE_OVERVIEW: StringName = &"view_toggle_overview"
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
		CAMERA_ORBIT_LEFT,
		CAMERA_ORBIT_RIGHT,
		CAMERA_ORBIT_UP,
		CAMERA_ORBIT_DOWN,
		CAMERA_ZOOM_IN,
		CAMERA_ZOOM_OUT,
		CAMERA_ORBIT_DRAG,
		CAMERA_PAN_DRAG,
		VIEW_TOGGLE_OVERVIEW,
		SIM_TOGGLE_PAUSE,
		SIM_RESET,
		SIM_SPEED_1,
		SIM_SPEED_2,
		SIM_SPEED_3,
	])
