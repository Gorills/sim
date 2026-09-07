class_name SimVisualizationPalette
extends RefCounted

const SURFACE_LAND := 0
const SURFACE_OCEAN := 1
const SURFACE_FRESH := 2

const OVERVIEW_BACKGROUND := Color(0.015, 0.025, 0.035, 0.88)
const OVERVIEW_BORDER := Color(0.55, 0.68, 0.72, 0.8)
const OVERVIEW_TEXT := Color(0.92, 0.94, 0.95)
const INTEREST_OUTLINE := Color(0.96, 0.96, 0.90, 0.95)

const OCEAN := Color("#153344")
const FRESH_WATER := Color("#286270")
const LAND := Color("#314b31")
const VEGETATION := Color("#52763a")
const HERBIVORE := Color("#c0aa77")
const INSECT := Color("#c6ad45")
const OMNIVORE := Color("#bf743e")
const CARNIVORE := Color("#bd4f48")


static func overview_cell_color(
	surface_kind: int,
	plant_count: int,
	herbivore_count: int,
	omnivore_count: int,
	carnivore_count: int,
	insect_count: int
) -> Color:
	if surface_kind == SURFACE_OCEAN:
		return OCEAN
	if surface_kind == SURFACE_FRESH:
		return FRESH_WATER

	var color := LAND
	var vegetation := clampf(log(1.0 + float(plant_count)) / 4.0, 0.0, 1.0)
	color = color.lerp(VEGETATION, vegetation * 0.45)

	if herbivore_count > 0:
		var strength := clampf(0.30 + log(1.0 + float(herbivore_count)) * 0.16, 0.0, 0.78)
		color = color.lerp(HERBIVORE, strength)
	if insect_count > 0:
		var strength := clampf(log(1.0 + float(insect_count)) * 0.10, 0.0, 0.32)
		color = color.lerp(INSECT, strength)
	if omnivore_count > 0:
		var strength := clampf(0.42 + log(1.0 + float(omnivore_count)) * 0.14, 0.0, 0.82)
		color = color.lerp(OMNIVORE, strength)
	if carnivore_count > 0:
		var strength := clampf(0.55 + log(1.0 + float(carnivore_count)) * 0.16, 0.0, 0.92)
		color = color.lerp(CARNIVORE, strength)
	return color
