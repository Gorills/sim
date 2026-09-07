# third_party

Vendor **godot-cpp** here, pinned to the same Godot minor as `godot/bin/sim.gdextension` (`compatibility_minimum = 4.5`).

```bash
./scripts/fetch_godot_cpp.sh godot-4.5-stable
```

CMake uses `third_party/godot-cpp` when present. Alternatively configure with `-DSIM_FETCH_GODOT_CPP=ON` (or the `debug-godot` preset) to download the same tag into the build tree.

The Godot visualizer also needs **Terrain3D** (MIT, pinned `v1.0.2-stable`) as a presentation-only clipmap. Fetch it with `./scripts/fetch_terrain3d.sh` or `make fetch-terrain3d`; `make godot-check` / `make godot-run` do this automatically. The official zip is built against godot-cpp 4.4; `make terrain3d-rebuild` recompiles the linux debug library against `godot-4.5-stable` (with `scripts/patches/terrain3d-1.0.2-godot-cpp-45.patch`) so Godot 4.5+ does not print the physics-interpolation deprecation. Do not put simulation domain code in this directory.
