CMAKE ?= cmake
CTEST ?= ctest
GODOT ?= godot

CORE_PRESET ?= debug
FULL_PRESET ?= debug-godot
SIM_ARGS ?= --ticks 1440 --ecology-hours 0.1666666667 --report-every 240 --seed 42

.DEFAULT_GOAL := help

.PHONY: help build sim-build sim test godot-check godot-run fetch-terrain3d terrain3d-rebuild

help:
	@echo "make build       Full build: core, tests, CLI, godot-cpp, and GDExtension"
	@echo "make sim         Build and run only the headless C++ simulation"
	@echo "make test        Build and run sim_core tests"
	@echo "make godot-check Build and smoke-test the GDExtension in headless Godot"
	@echo "make godot-run   Build and run the Godot visualizer"
	@echo "make fetch-terrain3d  Download pinned Terrain3D into godot/addons"
	@echo "make terrain3d-rebuild  Rebuild Terrain3D against godot-cpp 4.5 (silences Godot 4.5+ deprecation)"
	@echo
	@echo "Overrides: CORE_PRESET=release FULL_PRESET=debug-godot"
	@echo "           SIM_ARGS='--ticks 8760 --ecology-hours 1 --seed 7' GODOT=godot4"

build:
	$(CMAKE) --preset $(FULL_PRESET)
	$(CMAKE) --build --preset $(FULL_PRESET)

sim-build:
	$(CMAKE) --preset $(CORE_PRESET)
	$(CMAKE) --build --preset $(CORE_PRESET) --target sim_cli

sim: sim-build
	./build/$(CORE_PRESET)/sim_cli/sim_cli $(SIM_ARGS)

test:
	$(CMAKE) --preset $(CORE_PRESET)
	$(CMAKE) --build --preset $(CORE_PRESET) --target sim_core_tests
	$(CTEST) --preset $(CORE_PRESET)

fetch-terrain3d:
	./scripts/fetch_terrain3d.sh

terrain3d-rebuild: fetch-terrain3d
	./scripts/build_terrain3d.sh

godot-check: build fetch-terrain3d
	@command -v "$(GODOT)" >/dev/null 2>&1 || { echo "Godot executable not found: $(GODOT)"; exit 127; }
	$(GODOT) --headless --path godot --import
	$(GODOT) --headless --path godot -s res://scripts/check_extension.gd
	$(GODOT) --headless --path godot --quit-after 5

godot-run: build fetch-terrain3d
	@command -v "$(GODOT)" >/dev/null 2>&1 || { echo "Godot executable not found: $(GODOT)"; exit 127; }
	$(GODOT) --path godot
