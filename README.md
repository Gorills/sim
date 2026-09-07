# Sim

Детерминированная симуляция живого острова на C++ и визуализация в Godot 4.5.
Ядро не знает про движок: Godot подключается только как GDExtension-визуализатор.

В текущем вертикальном срезе работают сезонный климат, канопи/свет, органика и
опыление, рост растений, пресная вода, метаболизм, устойчивые цели поведения,
суточная активность, отдых, стада/колонии, охота и бегство, размножение и пищевая
сеть умеренного острова 256×256 по 75 м на клетку (19,2 км по стороне) из 29 видов
(трава/цветы/деревья/кустарники, травоядные/всеядные/хищники, птицы, опылители и
редуценты). Стартовый сценарий содержит 9 017 организмов: растения образуют пятна и
рощи, а группы потребителей по возможности размещаются рядом с уже посеянной кормовой
базой. Виды описаны данными, сценарий воспроизводится по seed. 3D-визуализатор получает только
interest-region вокруг камеры и пакетно рисует локальные организмы/рельеф. Весь мир доступен
через отдельный агрегированный overview без передачи полного списка сущностей в Godot.

Подробности границ: [docs/architecture.md](docs/architecture.md). Решения:
[ядро без Godot](docs/adr/0001-godot-free-core-and-gdextension.md),
[data-driven экология](docs/adr/0002-data-driven-ecology-and-spatial-fields.md) и
[слои биома и пакетный вид](docs/adr/0003-biome-layers-and-batched-island-view.md) и
[Terrain3D как визуализатор рельефа](docs/adr/0004-terrain3d-presentation.md).

```
sim_core/     C++ мир, команды, фиксированный тик, снимок
sim_cli/      headless-прогон
sim_godot/    GDExtension: SimWorld, SimSnapshot, SimEntityState, SimHabitatGrid
godot/        проект визуализатора (сцены и GDScript)
third_party/  godot-cpp (не в git, ставится скриптом или FetchContent)
```

Terrain3D качается отдельно и не хранится в git:

```bash
./scripts/fetch_terrain3d.sh   # v1.0.2-stable → godot/addons/terrain_3d
make terrain3d-rebuild         # optional: rebuild linux .so vs godot-cpp 4.5
```

## Требования

- CMake 3.22+, компилятор с C++20
- Godot **4.5+** только для визуализации (проверено на 4.7.2)
- git + сеть — чтобы скачать godot-cpp

## Быстрый запуск через Make

```bash
make build        # полная сборка: ядро, тесты, CLI, godot-cpp и GDExtension
make sim          # headless-прогон островной экосистемы с телеметрией
make test         # тесты sim_core
make godot-check  # полная сборка и headless-проверка связи Godot ↔ C++
make godot-run    # полная сборка и запуск визуализатора
```

Параметры можно переопределить без изменения Makefile:

```bash
make sim SIM_ARGS="--ticks 8760 --ecology-hours 1 --seed 7 --report-every 720"
make sim SIM_ARGS="--scenario agents --ticks 600 --agents 32 --hz 120"
make godot-check GODOT=godot4
```

Контрольный 180-дневный прогон стандартного острова (не входит в ASan-suite):

```bash
cmake --preset release && cmake --build --preset release
./build/release/sim_cli/sim_cli --ticks 4320 --ecology-hours 1 --report-every 1440 --seed 42
```

Это проверка конкретного сценария seed=42, не универсальный баланс. ASan-тесты ловят раннее исчезновение ключевых видов на компактном острове (~40 суток).

## Сборка ядра (без Godot)

```bash
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
./build/debug/sim_cli/sim_cli --ticks 120 --agents 8
```

Пресет `debug` включает ASan/UBSan. Его `.so` нельзя загружать в обычный редактор Godot.

## Сборка GDExtension

```bash
cmake --preset debug-godot
cmake --build --preset debug-godot
```

Либо вручную:

```bash
./scripts/fetch_godot_cpp.sh godot-4.5-stable
cmake -S . -B build/debug-godot -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug-godot
```

Библиотека копируется в `godot/bin/`. Откройте каталог `godot/` в Godot 4.5+ и запустите сцену `scenes/main.tscn`. Первый запуск редактора (или `godot --headless --path godot --import`) регистрирует `.gdextension`.

```bash
godot --headless --path godot --import
godot --headless --path godot -s res://scripts/check_extension.gd
```

- Space — пауза
- R — сброс мира
- 1 / 2 / 3 — скорость 1× / 4× / 16×
- колесо мыши — масштаб обзора
- правая кнопка мыши — поворот камеры
- средняя кнопка мыши — перемещение камеры
- M — развернуть/свернуть агрегированную карту всего мира

Если библиотеки нет, проект всё равно открывается: HUD подскажет собрать расширение.

## Контракт для нового кода

- Правила мира — только в `sim_core`.
- Новые поля сущности: сначала снимок, потом обёртка в `sim_godot`, потом GDScript.
- GDScript не интегрирует физику и не хранит каноническое состояние.
- Тик симуляции не равен FPS: только `World::tick` / `Stepper`.
