# ADR 0004: Terrain3D как визуализатор рельефа

- Статус: принято
- Дата: 2026-09-07

## Контекст

Остров уже рисуется из `World::habitat_snapshot()`: GDScript проецирует
экологический `elevation` в метры сцены и красит клетки по влажности, канопи,
органике и опылению. Нужен более устойчивый клипмап для текущего острова и
будущих больших карт, без передачи владения геометрией Godot.

## Решение

1. Источник геометрии остаётся `HabitatGrid::generate_island()` в `sim_core`.
2. `sim_godot` по-прежнему отдаёт packed arrays. Ни `sim_core`, ни `sim_godot`
   не зависят от Terrain3D.
3. GDScript проецирует скаляр высоты в метры (как раньше) и заливает heightmap
   и colormap в Terrain3D. Организмы сажаются по той же CPU-проекции, а не по
   физике Godot.
4. Аддон ставится скриптом `scripts/fetch_terrain3d.sh` (пин `v1.0.2-stable`)
   в `godot/addons/terrain_3d` и не коммитится. Editor-плагин (`plugin.cfg`)
   не включается: в Godot 4.7 headless `--import` падает на доках кистей.
   Официальный zip собран против godot-cpp 4.4 и на Godot 4.5+ печатает
   `instance_reset_physics_interpolation() is deprecated`.
   `scripts/build_terrain3d.sh` / `make terrain3d-rebuild` пересобирает linux
   debug `.so` против godot-cpp `godot-4.5-stable` и убирает вызов.
   Перед scons применяется `scripts/patches/terrain3d-1.0.2-godot-cpp-45.patch`
   (локальный `is_power_of_2` переименовывается, иначе clash с godot-cpp 4.5).
5. Если класс `Terrain3D` не загрузился, визуализатор оставляет ArrayMesh.

## Последствия

- Headless-проверка требует загруженный аддон (`make godot-check` качает его).
- Кисти и шум Terrain3D не являются генератором мира.
- Collision Terrain3D выключен: движение остаётся в `sim_core`.

## Отклонённые варианты

- Линковать Terrain3D в `sim_godot`: смешивает два GDExtension и тянет Godot-UI
  в адаптер симуляции.
- Авторство heightmap в редакторе Terrain3D: ломает headless и детерминизм.
- Заменить проекцию высоты на `Terrain3DData.get_height()` как единственный
  источник посадки: clipmap/LOD и headless могут разъехаться с клеткой.
