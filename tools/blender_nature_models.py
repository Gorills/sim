"""Build temperate-island organism meshes in Blender 5 for Godot MultiMesh.

Organisms are modeled Z-up, animals/insects face +X, origin at ground
(except flying insects, whose origin is the thorax).

Catalog-backed GLBs from tools/blender_catalog_organisms.py are kept unless
SIM_PROCEDURAL_ORGANISMS=1.
"""

from __future__ import annotations

import math
import os
import random
from typing import Iterable, Sequence

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXPORT_DIR = os.path.join(ROOT, "godot", "assets", "organisms")
BLEND_PATH = os.path.join(ROOT, "tools", "blender", "nature_kit.blend")

SPECIES = [
    "oak",
    "birch",
    "pine",
    "grass",
    "clover",
    "berry_bush",
    "fern",
    "reeds",
    "mushroom",
    "rabbit",
    "hare",
    "mouse",
    "deer",
    "boar",
    "wolf",
    "fox",
    "bee",
    "butterfly",
    "beetle",
    "ant",
]

LAYOUT = {
    "oak": (-5.4, 0.0, 0.0),
    "birch": (0.0, 0.0, 0.0),
    "pine": (5.4, 0.0, 0.0),
    "grass": (-7.5, 4.2, 0.0),
    "clover": (-4.5, 4.2, 0.0),
    "berry_bush": (-1.5, 4.2, 0.0),
    "fern": (1.5, 4.2, 0.0),
    "reeds": (4.5, 4.2, 0.0),
    "mushroom": (7.5, 4.2, 0.0),
    "rabbit": (-6.6, 8.4, 0.0),
    "hare": (-4.4, 8.4, 0.0),
    "mouse": (-2.2, 8.4, 0.0),
    "deer": (0.2, 8.4, 0.0),
    "boar": (2.6, 8.4, 0.0),
    "wolf": (5.0, 8.4, 0.0),
    "fox": (7.4, 8.4, 0.0),
    "bee": (-4.8, 11.6, 0.0),
    "butterfly": (-1.6, 11.6, 0.0),
    "beetle": (1.6, 11.6, 0.0),
    "ant": (4.8, 11.6, 0.0),
}

INSECT_SHOW_SCALE = 7.0


def unit_hash(*values: float) -> float:
    acc = 0.0
    for index, value in enumerate(values):
        acc += math.sin(value * 12.9898 + index * 78.233) * 43758.5453
    return acc - math.floor(acc)


def rotate_around(direction: Vector, angle: float, twist: float) -> Vector:
    axis = direction.normalized()
    if abs(axis.z) < 0.92:
        helper = Vector((0.0, 0.0, 1.0))
    else:
        helper = Vector((1.0, 0.0, 0.0))
    side = axis.cross(helper).normalized()
    up = axis.cross(side).normalized()
    swung = (axis * math.cos(angle) + up * math.sin(angle)).normalized()
    quat = Quaternion(axis, twist)
    return (quat @ swung).normalized()


def z_align(direction: Vector) -> Matrix:
    vector = Vector(direction)
    if vector.length_squared < 1e-12:
        vector = Vector((0.0, 0.0, 1.0))
    return vector.normalized().to_track_quat("Z", "Y").to_matrix().to_4x4()


class Builder:
    def __init__(self) -> None:
        self.bm = bmesh.new()
        self.materials: list[bpy.types.Material] = []

    def slot(self, material: bpy.types.Material) -> int:
        if material not in self.materials:
            self.materials.append(material)
        return self.materials.index(material)

    def _assign(self, verts: Sequence[bmesh.types.BMVert], material: bpy.types.Material) -> list[bmesh.types.BMVert]:
        index = self.slot(material)
        seen: set[int] = set()
        for vert in verts:
            for face in vert.link_faces:
                if face.index in seen:
                    continue
                seen.add(face.index)
                face.material_index = index
                face.smooth = True
        return list(verts)

    def sphere(
        self,
        center: Vector,
        radii: Vector | float,
        material: bpy.types.Material,
        u: int = 16,
        v: int = 10,
        ico: bool = False,
    ) -> list[bmesh.types.BMVert]:
        if isinstance(radii, (int, float)):
            radii = Vector((float(radii), float(radii), float(radii)))
        matrix = Matrix.Translation(center) @ Matrix.Diagonal((radii.x, radii.y, radii.z, 1.0))
        if ico:
            result = bmesh.ops.create_icosphere(
                self.bm, subdivisions=2, radius=1.0, matrix=matrix, calc_uvs=True
            )
        else:
            result = bmesh.ops.create_uvsphere(
                self.bm, u_segments=u, v_segments=v, radius=1.0, matrix=matrix, calc_uvs=True
            )
        return self._assign(result["verts"], material)

    def limb(
        self,
        start: Vector,
        end: Vector,
        radius_start: float,
        radius_end: float,
        material: bpy.types.Material,
        segments: int = 8,
        caps: bool = True,
    ) -> list[bmesh.types.BMVert]:
        delta = end - start
        length = delta.length
        if length < 1e-6:
            return []
        matrix = Matrix.Translation((start + end) * 0.5) @ z_align(delta)
        result = bmesh.ops.create_cone(
            self.bm,
            cap_ends=caps,
            cap_tris=False,
            segments=max(6, segments),
            radius1=radius_start,
            radius2=radius_end,
            depth=length,
            matrix=matrix,
            calc_uvs=True,
        )
        return self._assign(result["verts"], material)

    def cone(
        self,
        base: Vector,
        tip: Vector,
        radius: float,
        material: bpy.types.Material,
        segments: int = 8,
    ) -> list[bmesh.types.BMVert]:
        return self.limb(base, tip, radius, 0.0008, material, segments=segments)

    def disk(
        self,
        center: Vector,
        normal: Vector,
        radius: float,
        thickness: float,
        material: bpy.types.Material,
        segments: int = 12,
    ) -> list[bmesh.types.BMVert]:
        matrix = Matrix.Translation(center) @ z_align(normal)
        result = bmesh.ops.create_cone(
            self.bm,
            cap_ends=True,
            cap_tris=False,
            segments=segments,
            radius1=radius,
            radius2=radius,
            depth=max(0.001, thickness),
            matrix=matrix,
            calc_uvs=True,
        )
        return self._assign(result["verts"], material)

    def leaf(
        self,
        origin: Vector,
        toward: Vector,
        length: float,
        width: float,
        material: bpy.types.Material,
        cup: float = 0.012,
    ) -> None:
        direction = toward.normalized()
        if abs(direction.z) < 0.94:
            side = direction.cross(Vector((0.0, 0.0, 1.0))).normalized()
        else:
            side = direction.cross(Vector((1.0, 0.0, 0.0))).normalized()
        lift = direction.cross(side).normalized()
        if lift.z < 0.0:
            lift = -lift
            side = -side
        verts = [
            self.bm.verts.new(origin),
            self.bm.verts.new(origin + direction * length * 0.38 + side * width * 0.52 + lift * cup),
            self.bm.verts.new(origin + direction * length + lift * cup * 0.25),
            self.bm.verts.new(origin + direction * length * 0.38 - side * width * 0.52 + lift * cup),
        ]
        face = self.bm.faces.new(verts)
        index = self.slot(material)
        face.material_index = index
        face.smooth = True

    def heart_leaf(
        self,
        origin: Vector,
        toward: Vector,
        length: float,
        width: float,
        material: bpy.types.Material,
    ) -> None:
        direction = toward.normalized()
        if abs(direction.z) < 0.94:
            side = direction.cross(Vector((0.0, 0.0, 1.0))).normalized()
        else:
            side = direction.cross(Vector((1.0, 0.0, 0.0))).normalized()
        lift = Vector((0.0, 0.0, 1.0))
        notch = origin + direction * length * 0.92
        verts = [
            self.bm.verts.new(origin),
            self.bm.verts.new(origin + direction * length * 0.42 + side * width * 0.58 + lift * 0.01),
            self.bm.verts.new(origin + direction * length + side * width * 0.18 + lift * 0.008),
            self.bm.verts.new(notch + lift * 0.004),
            self.bm.verts.new(origin + direction * length - side * width * 0.18 + lift * 0.008),
            self.bm.verts.new(origin + direction * length * 0.42 - side * width * 0.58 + lift * 0.01),
        ]
        face = self.bm.faces.new(verts)
        index = self.slot(material)
        face.material_index = index
        face.smooth = True

    def blade(
        self,
        root: Vector,
        height: float,
        width: float,
        bend: Vector,
        material: bpy.types.Material,
        steps: int = 4,
    ) -> None:
        if abs(bend.x) + abs(bend.y) < 1e-6:
            side = Vector((1.0, 0.0, 0.0))
        else:
            side = Vector((-bend.y, bend.x, 0.0)).normalized()
        left: list[bmesh.types.BMVert] = []
        right: list[bmesh.types.BMVert] = []
        for step in range(steps + 1):
            t = step / steps
            taper = max(0.08, 1.0 - t)
            point = root + Vector((0.0, 0.0, height * t)) + bend * (t * t)
            left.append(self.bm.verts.new(point - side * width * 0.5 * taper))
            right.append(self.bm.verts.new(point + side * width * 0.5 * taper))
        index = self.slot(material)
        for step in range(steps):
            quad = [left[step], right[step], right[step + 1], left[step + 1]]
            face = self.bm.faces.new(quad)
            face.material_index = index
            face.smooth = True

    def bark_jitter(self, verts: Iterable[bmesh.types.BMVert], amount: float) -> None:
        self.bm.normal_update()
        for vert in verts:
            noise = (unit_hash(vert.co.x * 9.1, vert.co.y * 8.3, vert.co.z * 7.7) - 0.5) * 2.0
            vert.co += vert.normal * noise * amount


def make_material(
    name: str,
    color: Sequence[float],
    roughness: float = 0.72,
    metallic: float = 0.0,
    alpha: float = 1.0,
    transmission: float = 0.0,
    emission: float = 0.0,
) -> bpy.types.Material:
    material = bpy.data.materials.get(name)
    if material is None:
        material = bpy.data.materials.new(name)
    material.use_nodes = True
    principled = next(node for node in material.node_tree.nodes if node.type == "BSDF_PRINCIPLED")
    principled.inputs["Base Color"].default_value = (color[0], color[1], color[2], 1.0)
    principled.inputs["Roughness"].default_value = roughness
    principled.inputs["Metallic"].default_value = metallic
    principled.inputs["Alpha"].default_value = alpha
    principled.inputs["Transmission Weight"].default_value = transmission
    principled.inputs["Emission Color"].default_value = (color[0], color[1], color[2], 1.0)
    principled.inputs["Emission Strength"].default_value = emission
    material.diffuse_color = (color[0], color[1], color[2], alpha)
    material.use_backface_culling = False
    if alpha < 0.999:
        material.blend_method = "BLEND"
        if hasattr(material, "surface_render_method"):
            material.surface_render_method = "BLENDED"
    else:
        material.blend_method = "OPAQUE"
        if hasattr(material, "surface_render_method"):
            material.surface_render_method = "DITHERED"
    return material


def palette() -> dict[str, bpy.types.Material]:
    return {
        "bark": make_material("org_bark", (0.345, 0.255, 0.175), 0.88),
        "bark_dark": make_material("org_bark_dark", (0.22, 0.15, 0.10), 0.92),
        "birch": make_material("org_birch", (0.82, 0.80, 0.72), 0.62),
        "birch_mark": make_material("org_birch_mark", (0.18, 0.16, 0.14), 0.85),
        "oak_leaf": make_material("org_oak_leaf", (0.25, 0.44, 0.23), 0.58),
        "oak_leaf_dark": make_material("org_oak_leaf_dark", (0.18, 0.32, 0.17), 0.62),
        "birch_leaf": make_material("org_birch_leaf", (0.42, 0.60, 0.30), 0.55),
        "pine_needle": make_material("org_pine_needle", (0.16, 0.34, 0.24), 0.64),
        "pine_needle_lit": make_material("org_pine_needle_lit", (0.22, 0.42, 0.28), 0.6),
        "wood_thin": make_material("org_wood_thin", (0.40, 0.28, 0.16), 0.84),
        "grass": make_material("org_grass", (0.40, 0.56, 0.24), 0.55),
        "grass_dark": make_material("org_grass_dark", (0.30, 0.46, 0.18), 0.6),
        "clover": make_material("org_clover", (0.28, 0.52, 0.22), 0.5),
        "clover_flower": make_material("org_clover_flower", (0.92, 0.78, 0.86), 0.42),
        "bush": make_material("org_bush", (0.24, 0.42, 0.20), 0.58),
        "berry": make_material("org_berry", (0.62, 0.12, 0.16), 0.32),
        "fern": make_material("org_fern", (0.26, 0.46, 0.26), 0.52),
        "reed": make_material("org_reed", (0.42, 0.50, 0.26), 0.6),
        "reed_head": make_material("org_reed_head", (0.55, 0.46, 0.28), 0.7),
        "stem": make_material("org_stem", (0.78, 0.72, 0.52), 0.55),
        "cap": make_material("org_cap", (0.62, 0.38, 0.24), 0.48),
        "cap_spot": make_material("org_cap_spot", (0.93, 0.88, 0.78), 0.4),
        "gills": make_material("org_gills", (0.86, 0.74, 0.55), 0.7),
        "fur_rabbit": make_material("org_fur_rabbit", (0.72, 0.64, 0.52), 0.78),
        "fur_hare": make_material("org_fur_hare", (0.70, 0.64, 0.54), 0.8),
        "fur_mouse": make_material("org_fur_mouse", (0.48, 0.44, 0.40), 0.82),
        "fur_deer": make_material("org_fur_deer", (0.55, 0.38, 0.24), 0.76),
        "fur_deer_light": make_material("org_fur_deer_light", (0.78, 0.68, 0.52), 0.7),
        "fur_boar": make_material("org_fur_boar", (0.32, 0.25, 0.20), 0.88),
        "fur_wolf": make_material("org_fur_wolf", (0.38, 0.42, 0.45), 0.8),
        "fur_wolf_light": make_material("org_fur_wolf_light", (0.62, 0.64, 0.66), 0.74),
        "fur_fox": make_material("org_fur_fox", (0.72, 0.36, 0.16), 0.7),
        "fur_fox_white": make_material("org_fur_fox_white", (0.90, 0.88, 0.82), 0.62),
        "nose": make_material("org_nose", (0.12, 0.10, 0.10), 0.45),
        "eye": make_material("org_eye", (0.05, 0.05, 0.05), 0.22, metallic=0.15),
        "antler": make_material("org_antler", (0.55, 0.46, 0.32), 0.55),
        "tusk": make_material("org_tusk", (0.90, 0.86, 0.74), 0.35),
        "paw": make_material("org_paw", (0.16, 0.12, 0.10), 0.7),
        "bee_gold": make_material("org_bee_gold", (0.84, 0.64, 0.12), 0.42),
        "bee_black": make_material("org_bee_black", (0.08, 0.07, 0.06), 0.5),
        "wing": make_material("org_wing", (0.78, 0.86, 0.90), 0.18, alpha=0.38, transmission=0.55),
        "wing_pink": make_material("org_wing_pink", (0.78, 0.42, 0.55), 0.32, alpha=0.82),
        "wing_dark": make_material("org_wing_dark", (0.22, 0.12, 0.18), 0.4, alpha=0.9),
        "beetle": make_material("org_beetle", (0.18, 0.28, 0.16), 0.28, metallic=0.35),
        "beetle_shell": make_material("org_beetle_shell", (0.12, 0.16, 0.10), 0.22, metallic=0.45),
        "ant": make_material("org_ant", (0.32, 0.16, 0.12), 0.55),
        "ground": make_material("org_ground", (0.36, 0.42, 0.28), 0.95),
    }


def to_object(builder: Builder, name: str, collection: bpy.types.Collection) -> bpy.types.Object:
    mesh = bpy.data.meshes.new(name)
    builder.bm.normal_update()
    builder.bm.to_mesh(mesh)
    builder.bm.free()
    mesh.update()
    for polygon in mesh.polygons:
        polygon.use_smooth = True
    for material in builder.materials:
        mesh.materials.append(material)
    obj = bpy.data.objects.new(name, mesh)
    collection.objects.link(obj)
    return obj


def apply_subsurf(obj: bpy.types.Object, levels: int = 1) -> None:
    modifier = obj.modifiers.new("subsurf", "SUBSURF")
    modifier.levels = levels
    modifier.render_levels = levels
    modifier.quality = 3
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = obj.evaluated_get(depsgraph)
    baked = bpy.data.meshes.new_from_object(evaluated, preserve_all_data_layers=True, depsgraph=depsgraph)
    baked.name = obj.name
    old = obj.data
    obj.modifiers.clear()
    obj.data = baked
    if old.users == 0:
        bpy.data.meshes.remove(old)


def add_leaf_cluster(
    builder: Builder,
    tip: Vector,
    direction: Vector,
    rng: random.Random,
    count: int,
    length: float,
    width: float,
    materials: Sequence[bpy.types.Material],
) -> None:
    for _ in range(count):
        leaf_dir = rotate_around(direction, rng.uniform(0.25, 1.05), rng.uniform(0.0, math.tau))
        if leaf_dir.z < 0.12:
            leaf_dir = Vector((leaf_dir.x, leaf_dir.y, 0.22)).normalized()
        material = materials[0] if rng.random() < 0.65 else materials[min(1, len(materials) - 1)]
        offset = tip + leaf_dir * rng.uniform(0.01, 0.05)
        builder.leaf(offset, leaf_dir, length * rng.uniform(0.82, 1.12), width * rng.uniform(0.8, 1.15), material)


def foliage_clump(
    builder: Builder,
    center: Vector,
    size: float,
    materials: Sequence[bpy.types.Material],
    rng: random.Random,
    leaves: int = 10,
) -> None:
    builder.sphere(
        center,
        Vector((size * 0.72, size * 0.72, size * 0.58)),
        materials[0],
        u=10,
        v=7,
        ico=True,
    )
    add_leaf_cluster(builder, center, Vector((0.0, 0.0, 1.0)), rng, leaves, size * 0.85, size * 0.38, materials)


def build_oak(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(11)
    wood: list[bmesh.types.BMVert] = []
    wood.extend(builder.limb(Vector((0.0, 0.0, 0.0)), Vector((0.04, -0.03, 0.48)), 0.17, 0.14, mats["bark"], segments=12))
    wood.extend(builder.limb(Vector((0.04, -0.03, 0.48)), Vector((0.02, 0.02, 1.12)), 0.14, 0.08, mats["bark"], segments=12))
    tips: list[tuple[Vector, Vector]] = []
    for index in range(8):
        yaw = index / 8 * math.tau + 0.18
        height = 0.62 + (index % 4) * 0.14
        start = Vector((math.cos(yaw) * 0.04, math.sin(yaw) * 0.04, height))
        direction = Vector((math.cos(yaw), math.sin(yaw), 0.62 + 0.08 * (index % 3))).normalized()
        end = start + direction * rng.uniform(0.58, 0.92)
        wood.extend(builder.limb(start, end, 0.055, 0.022, mats["bark"], segments=8))
        for fork in range(2):
            child = rotate_around(direction, rng.uniform(0.35, 0.7), fork * 2.15 + index)
            tip = end + child * rng.uniform(0.22, 0.38)
            wood.extend(builder.limb(end, tip, 0.022, 0.011, mats["bark"], segments=6))
            tips.append((tip, child))
        tips.append((end, direction))
    builder.bark_jitter(wood, 0.014)
    leaves = (mats["oak_leaf"], mats["oak_leaf_dark"])
    for tip, direction in tips:
        foliage_clump(builder, tip + direction * 0.04, rng.uniform(0.22, 0.32), leaves, rng, leaves=8)
        if rng.random() < 0.35:
            builder.sphere(tip + Vector((0.0, 0.0, -0.03)), Vector((0.016, 0.016, 0.024)), mats["bark_dark"], u=6, v=4)
    return builder


def build_birch(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(23)
    wood: list[bmesh.types.BMVert] = []
    wood.extend(builder.limb(Vector((0.0, 0.0, 0.0)), Vector((-0.02, 0.03, 0.85)), 0.08, 0.055, mats["birch"], segments=12))
    wood.extend(builder.limb(Vector((-0.02, 0.03, 0.85)), Vector((-0.05, 0.01, 2.35)), 0.055, 0.022, mats["birch"], segments=12))
    tips: list[tuple[Vector, Vector]] = []
    for index in range(7):
        yaw = index / 7 * math.tau + 0.4
        height = 0.95 + index * 0.18
        start = Vector((math.cos(yaw) * 0.02, math.sin(yaw) * 0.02, height))
        direction = Vector((math.cos(yaw) * 0.7, math.sin(yaw) * 0.7, 0.85)).normalized()
        end = start + direction * rng.uniform(0.35, 0.55)
        wood.extend(builder.limb(start, end, 0.018, 0.008, mats["birch"], segments=6))
        hang = (direction + Vector((0.0, 0.0, -0.55))).normalized()
        tip = end + hang * 0.18
        wood.extend(builder.limb(end, tip, 0.008, 0.004, mats["birch"], segments=5))
        tips.append((tip, hang))
        tips.append((end, direction))
    builder.bark_jitter(wood, 0.003)
    for height in (0.28, 0.62, 0.98, 1.35, 1.72, 2.05):
        mark = Vector((rng.uniform(-0.02, 0.02), rng.uniform(-0.02, 0.02), height))
        builder.sphere(mark, Vector((0.05, 0.016, 0.014)), mats["birch_mark"], u=6, v=4)
    leaves = (mats["birch_leaf"], mats["oak_leaf"])
    for tip, direction in tips:
        foliage_clump(builder, tip, rng.uniform(0.14, 0.2), leaves, rng, leaves=7)
    return builder


def build_pine(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(41)
    wood = builder.limb(Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 2.65)), 0.12, 0.03, mats["bark_dark"], segments=12)
    builder.bark_jitter(wood, 0.008)
    layers = (
        (0.42, 0.98, 0.78, mats["pine_needle"]),
        (0.88, 0.82, 0.72, mats["pine_needle_lit"]),
        (1.28, 0.66, 0.64, mats["pine_needle"]),
        (1.64, 0.50, 0.56, mats["pine_needle_lit"]),
        (1.96, 0.34, 0.48, mats["pine_needle"]),
        (2.24, 0.20, 0.42, mats["pine_needle_lit"]),
    )
    for height, radius, depth, material in layers:
        builder.limb(
            Vector((0.0, 0.0, height)),
            Vector((0.0, 0.0, height + depth)),
            radius,
            0.04,
            material,
            segments=12,
        )
        for spoke in range(5):
            yaw = spoke / 5 * math.tau + height
            start = Vector((0.0, 0.0, height + 0.12))
            end = start + Vector((math.cos(yaw) * radius * 0.72, math.sin(yaw) * radius * 0.72, -0.08))
            builder.limb(start, end, 0.02, 0.01, mats["bark_dark"], segments=5)
            if rng.random() < 0.4:
                builder.sphere(end + Vector((0.0, 0.0, -0.04)), Vector((0.03, 0.03, 0.05)), mats["bark"], u=6, v=4)
    builder.cone(Vector((0.0, 0.0, 2.52)), Vector((0.0, 0.0, 2.95)), 0.14, mats["pine_needle"], segments=10)
    return builder


def build_grass(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(7)
    for index in range(22):
        angle = index / 16 * math.tau + rng.uniform(-0.12, 0.12)
        radius = 0.04 + 0.10 * (index % 5) / 5.0
        root = Vector((math.cos(angle) * radius, math.sin(angle) * radius, 0.0))
        height = 0.28 + rng.uniform(0.0, 0.18)
        bend = Vector((math.cos(angle), math.sin(angle), 0.0)) * rng.uniform(0.04, 0.12)
        material = mats["grass"] if index % 3 else mats["grass_dark"]
        builder.blade(root, height, rng.uniform(0.012, 0.022), bend, material, steps=4)
    return builder


def build_clover(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(13)
    for clump in range(5):
        base = Vector((rng.uniform(-0.12, 0.12), rng.uniform(-0.12, 0.12), 0.0))
        builder.limb(base, base + Vector((0.0, 0.0, 0.07)), 0.006, 0.004, mats["wood_thin"], segments=5)
        for leaflet in range(3):
            angle = leaflet / 3 * math.tau + rng.uniform(-0.08, 0.08)
            toward = Vector((math.cos(angle), math.sin(angle), 0.35)).normalized()
            builder.heart_leaf(base + Vector((0.0, 0.0, 0.07)), toward, 0.09, 0.08, mats["clover"])
    for _ in range(4):
        pos = Vector((rng.uniform(-0.1, 0.1), rng.uniform(-0.1, 0.1), rng.uniform(0.10, 0.16)))
        builder.sphere(pos, 0.018, mats["clover_flower"], u=8, v=6, ico=True)
    return builder


def build_berry_bush(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(17)
    tips: list[tuple[Vector, Vector]] = []
    for stem in range(6):
        angle = stem / 6 * math.tau
        start = Vector((math.cos(angle) * 0.04, math.sin(angle) * 0.04, 0.0))
        bend = Vector((math.cos(angle + 0.4), math.sin(angle + 0.4), 1.4)).normalized()
        end = start + bend * rng.uniform(0.42, 0.62)
        builder.limb(start, end, 0.018, 0.008, mats["wood_thin"], segments=6)
        for fork in range(2):
            child = rotate_around(bend, rng.uniform(0.35, 0.7), fork * 2.1)
            tip = end + child * rng.uniform(0.16, 0.26)
            builder.limb(end, tip, 0.008, 0.004, mats["wood_thin"], segments=5)
            tips.append((tip, child))
    for tip, direction in tips:
        foliage_clump(builder, tip, 0.11, (mats["bush"], mats["oak_leaf"]), rng, leaves=5)
        if rng.random() < 0.8:
            builder.sphere(tip + Vector((rng.uniform(-0.03, 0.03), rng.uniform(-0.03, 0.03), -0.02)), 0.018, mats["berry"], u=7, v=5)
    return builder


def build_fern(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(19)
    for frond in range(6):
        yaw = frond / 6 * math.tau + rng.uniform(-0.1, 0.1)
        length = rng.uniform(0.38, 0.52)
        points = []
        for step in range(8):
            t = step / 7
            points.append(
                Vector(
                    (
                        math.cos(yaw) * 0.08 * t + math.cos(yaw) * 0.32 * t,
                        math.sin(yaw) * 0.08 * t + math.sin(yaw) * 0.32 * t,
                        0.04 + 0.22 * math.sin(t * math.pi * 0.72),
                    )
                )
            )
        for step in range(len(points) - 1):
            builder.limb(points[step], points[step + 1], 0.01 * (1.0 - step / 8), 0.007, mats["wood_thin"], segments=5)
            side = Vector((-math.sin(yaw), math.cos(yaw), 0.0))
            leaflet_len = 0.11 * (1.0 - step / 9)
            for sign in (-1.0, 1.0):
                toward = (side * sign + Vector((0.0, 0.0, 0.18)) + (points[step + 1] - points[step]).normalized() * 0.2).normalized()
                builder.leaf(points[step], toward, leaflet_len, leaflet_len * 0.38, mats["fern"], cup=0.006)
    return builder


def build_reeds(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    rng = random.Random(29)
    for index in range(9):
        angle = index / 9 * math.tau
        root = Vector((math.cos(angle) * 0.07, math.sin(angle) * 0.07, 0.0))
        lean = Vector((math.cos(angle + 0.4), math.sin(angle + 0.4), 0.0)) * rng.uniform(0.04, 0.12)
        top = root + Vector((0.0, 0.0, rng.uniform(0.72, 1.05))) + lean
        builder.limb(root, top, 0.012, 0.007, mats["reed"], segments=6)
        builder.sphere(top + Vector((0.0, 0.0, 0.05)), Vector((0.018, 0.018, 0.07)), mats["reed_head"], u=6, v=5)
    return builder


def build_mushroom(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    builder.limb(Vector((0.0, 0.0, 0.0)), Vector((0.0, 0.0, 0.22)), 0.035, 0.045, mats["stem"], segments=10)
    builder.sphere(Vector((0.0, 0.0, 0.28)), Vector((0.16, 0.16, 0.09)), mats["cap"], u=14, v=8)
    builder.disk(Vector((0.0, 0.0, 0.255)), Vector((0.0, 0.0, -1.0)), 0.145, 0.012, mats["gills"], segments=16)
    rng = random.Random(3)
    for _ in range(7):
        angle = rng.uniform(0.0, math.tau)
        radial = rng.uniform(0.03, 0.11)
        pos = Vector((math.cos(angle) * radial, math.sin(angle) * radial, 0.34 - radial * 0.35))
        builder.sphere(pos, 0.018, mats["cap_spot"], u=6, v=4)
    builder.limb(Vector((0.12, 0.08, 0.0)), Vector((0.12, 0.08, 0.12)), 0.016, 0.02, mats["stem"], segments=7)
    builder.sphere(Vector((0.12, 0.08, 0.15)), Vector((0.07, 0.07, 0.045)), mats["cap"], u=10, v=6)
    return builder


def add_eyes(builder: Builder, center: Vector, toward: Vector, spread: float, radius: float, mats: dict[str, bpy.types.Material]) -> None:
    direction = toward.normalized()
    if abs(direction.z) < 0.9:
        up = Vector((0.0, 0.0, 1.0))
    else:
        up = Vector((0.0, 1.0, 0.0))
    side = direction.cross(up).normalized()
    lift = up * radius * 0.35
    builder.sphere(center + side * spread + lift, radius, mats["eye"], u=8, v=6)
    builder.sphere(center - side * spread + lift, radius, mats["eye"], u=8, v=6)


def add_quad_legs(
    builder: Builder,
    body_center: Vector,
    body_len: float,
    body_width: float,
    hip_z: float,
    foot_z: float,
    radius: float,
    mats: dict[str, bpy.types.Material],
    hind_scale: float = 1.0,
) -> None:
    for x_sign, y_sign in ((1.0, 1.0), (1.0, -1.0), (-1.0, 1.0), (-1.0, -1.0)):
        is_hind = x_sign < 0.0
        hip = body_center + Vector((x_sign * body_len * 0.32, y_sign * body_width * 0.55, hip_z - body_center.z))
        hip.z = hip_z
        knee = Vector((hip.x + (-0.04 if is_hind else 0.02), hip.y * 0.92, (hip_z + foot_z) * 0.45))
        foot = Vector((hip.x + (-0.03 if is_hind else 0.04), hip.y * 0.85, foot_z))
        rad = radius * (hind_scale if is_hind else 1.0)
        builder.limb(hip, knee, rad, rad * 0.82, mats["leg"], segments=8)
        builder.limb(knee, foot, rad * 0.82, rad * 0.62, mats["leg"], segments=7)
        builder.sphere(foot + Vector((0.012, 0.0, 0.012)), rad * 0.85, mats.get("paw", mats["leg"]), u=8, v=6)


def build_rabbit(mats: dict[str, bpy.types.Material], hare: bool = False) -> Builder:
    builder = Builder()
    fur = mats["fur_hare"] if hare else mats["fur_rabbit"]
    scale = 1.18 if hare else 1.0
    body = Vector((0.0, 0.0, 0.20 * scale))
    builder.sphere(body, Vector((0.16 * scale, 0.11 * scale, 0.12 * scale)), fur, u=12, v=8)
    head = Vector((0.16 * scale, 0.0, 0.27 * scale))
    builder.sphere(head, Vector((0.08 * scale, 0.07 * scale, 0.07 * scale)), fur, u=10, v=7)
    builder.sphere(head + Vector((0.07 * scale, 0.0, -0.01)), Vector((0.03, 0.025, 0.022)), mats["nose"], u=6, v=4)
    add_eyes(builder, head + Vector((0.04 * scale, 0.0, 0.02 * scale)), Vector((1.0, 0.0, 0.2)), 0.035 * scale, 0.012, mats)
    ear_h = 0.22 * scale if hare else 0.16 * scale
    for sign in (-1.0, 1.0):
        base = head + Vector((-0.01, sign * 0.03 * scale, 0.05 * scale))
        tip = base + Vector((-0.01 if hare else 0.0, sign * 0.01, ear_h))
        builder.limb(base, tip, 0.018 * scale, 0.01, fur, segments=6)
        builder.limb(base + Vector((0.0, sign * 0.002, 0.01)), tip - Vector((0.0, 0.0, 0.02)), 0.01, 0.006, mats["fur_fox_white"], segments=5)
    builder.sphere(body + Vector((-0.15 * scale, 0.0, 0.02)), 0.045 * scale, mats["fur_fox_white"], u=8, v=6)
    leg_mat = dict(mats)
    leg_mat["leg"] = fur
    add_quad_legs(builder, body, 0.28 * scale, 0.10 * scale, 0.16 * scale, 0.0, 0.028 * scale, leg_mat, hind_scale=1.35)
    return builder


def build_mouse(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    fur = mats["fur_mouse"]
    body = Vector((0.0, 0.0, 0.07))
    builder.sphere(body, Vector((0.09, 0.05, 0.045)), fur, u=10, v=7)
    head = Vector((0.08, 0.0, 0.085))
    builder.sphere(head, Vector((0.04, 0.032, 0.03)), fur, u=8, v=6)
    builder.sphere(head + Vector((0.035, 0.0, -0.004)), 0.012, mats["nose"], u=6, v=4)
    add_eyes(builder, head + Vector((0.015, 0.0, 0.01)), Vector((1.0, 0.0, 0.2)), 0.016, 0.007, mats)
    for sign in (-1.0, 1.0):
        builder.disk(head + Vector((-0.005, sign * 0.03, 0.02)), Vector((0.2, sign, 0.6)), 0.028, 0.006, fur, segments=10)
    builder.limb(body + Vector((-0.08, 0.0, 0.0)), body + Vector((-0.28, 0.02, 0.02)), 0.008, 0.003, fur, segments=6)
    leg_mat = dict(mats)
    leg_mat["leg"] = fur
    add_quad_legs(builder, body, 0.14, 0.05, 0.05, 0.0, 0.012, leg_mat, hind_scale=1.2)
    return builder


def build_deer(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    fur = mats["fur_deer"]
    body = Vector((0.0, 0.0, 0.72))
    builder.sphere(body, Vector((0.42, 0.16, 0.18)), fur, u=14, v=9)
    builder.sphere(body + Vector((0.0, 0.0, -0.04)), Vector((0.32, 0.14, 0.12)), mats["fur_deer_light"], u=10, v=7)
    neck_start = body + Vector((0.32, 0.0, 0.06))
    head = Vector((0.62, 0.0, 0.92))
    builder.limb(neck_start, head, 0.07, 0.05, fur, segments=8)
    builder.sphere(head, Vector((0.09, 0.07, 0.07)), fur, u=10, v=7)
    builder.sphere(head + Vector((0.09, 0.0, -0.02)), Vector((0.07, 0.04, 0.035)), fur, u=8, v=6)
    builder.sphere(head + Vector((0.15, 0.0, -0.02)), 0.016, mats["nose"], u=6, v=4)
    add_eyes(builder, head + Vector((0.04, 0.0, 0.02)), Vector((1.0, 0.0, 0.15)), 0.04, 0.012, mats)
    for sign in (-1.0, 1.0):
        ear_base = head + Vector((-0.02, sign * 0.04, 0.05))
        builder.limb(ear_base, ear_base + Vector((-0.02, sign * 0.03, 0.08)), 0.018, 0.008, fur, segments=5)
        antler_base = head + Vector((-0.01, sign * 0.03, 0.07))
        t1 = antler_base + Vector((0.01, sign * 0.02, 0.16))
        builder.limb(antler_base, t1, 0.012, 0.007, mats["antler"], segments=5)
        builder.limb(t1 * 0.4 + antler_base * 0.6, t1 + Vector((0.05, sign * 0.04, 0.04)), 0.007, 0.004, mats["antler"], segments=4)
    builder.limb(body + Vector((-0.38, 0.0, 0.02)), body + Vector((-0.48, 0.0, 0.08)), 0.03, 0.01, mats["fur_deer_light"], segments=5)
    leg_mat = dict(mats)
    leg_mat["leg"] = mats["fur_deer_light"]
    add_quad_legs(builder, body, 0.72, 0.16, 0.62, 0.0, 0.042, leg_mat, hind_scale=1.05)
    return builder


def build_boar(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    fur = mats["fur_boar"]
    body = Vector((0.0, 0.0, 0.42))
    builder.sphere(body, Vector((0.42, 0.22, 0.22)), fur, u=14, v=9)
    head = body + Vector((0.38, 0.0, 0.04))
    builder.sphere(head, Vector((0.16, 0.12, 0.12)), fur, u=10, v=7)
    snout = head + Vector((0.16, 0.0, -0.02))
    builder.limb(head + Vector((0.08, 0.0, -0.01)), snout, 0.06, 0.045, fur, segments=8)
    builder.disk(snout + Vector((0.02, 0.0, 0.0)), Vector((1.0, 0.0, 0.0)), 0.045, 0.02, mats["nose"], segments=10)
    add_eyes(builder, head + Vector((0.04, 0.0, 0.04)), Vector((1.0, 0.0, 0.1)), 0.06, 0.012, mats)
    for sign in (-1.0, 1.0):
        builder.limb(head + Vector((0.02, sign * 0.08, 0.08)), head + Vector((0.0, sign * 0.11, 0.12)), 0.02, 0.01, fur, segments=5)
        tusk_base = snout + Vector((-0.02, sign * 0.03, -0.01))
        builder.cone(tusk_base, tusk_base + Vector((0.04, sign * 0.02, 0.05)), 0.012, mats["tusk"], segments=6)
    builder.limb(body + Vector((-0.38, 0.0, 0.04)), body + Vector((-0.48, 0.0, 0.02)), 0.03, 0.012, fur, segments=5)
    leg_mat = dict(mats)
    leg_mat["leg"] = fur
    add_quad_legs(builder, body, 0.7, 0.2, 0.32, 0.0, 0.055, leg_mat, hind_scale=1.1)
    return builder


def build_canid(mats: dict[str, bpy.types.Material], fox: bool = False) -> Builder:
    builder = Builder()
    fur = mats["fur_fox"] if fox else mats["fur_wolf"]
    belly = mats["fur_fox_white"] if fox else mats["fur_wolf_light"]
    scale = 0.82 if fox else 1.0
    body = Vector((0.0, 0.0, 0.42 * scale))
    builder.sphere(body, Vector((0.34 * scale, 0.13 * scale, 0.14 * scale)), fur, u=13, v=8)
    builder.sphere(body + Vector((0.0, 0.0, -0.03 * scale)), Vector((0.26 * scale, 0.11 * scale, 0.09 * scale)), belly, u=10, v=6)
    neck = body + Vector((0.26 * scale, 0.0, 0.04 * scale))
    head = Vector((0.46 * scale, 0.0, 0.52 * scale))
    builder.limb(neck, head, 0.07 * scale, 0.05 * scale, fur, segments=8)
    builder.sphere(head, Vector((0.08 * scale, 0.06 * scale, 0.06 * scale)), fur, u=10, v=7)
    builder.sphere(head + Vector((0.09 * scale, 0.0, -0.01 * scale)), Vector((0.07 * scale, 0.035 * scale, 0.03 * scale)), fur, u=8, v=6)
    builder.sphere(head + Vector((0.15 * scale, 0.0, -0.01 * scale)), 0.014 * scale, mats["nose"], u=6, v=4)
    add_eyes(builder, head + Vector((0.03 * scale, 0.0, 0.02 * scale)), Vector((1.0, 0.0, 0.12)), 0.032 * scale, 0.01, mats)
    for sign in (-1.0, 1.0):
        base = head + Vector((-0.01 * scale, sign * 0.03 * scale, 0.05 * scale))
        builder.cone(base, base + Vector((-0.01, sign * 0.02 * scale, 0.09 * scale)), 0.022 * scale, fur, segments=6)
        builder.cone(base + Vector((0.0, 0.0, 0.01)), base + Vector((0.0, sign * 0.012, 0.06 * scale)), 0.01, belly, segments=5)
    tail_start = body + Vector((-0.32 * scale, 0.0, 0.04 * scale))
    tail_end = tail_start + Vector((-0.42 * scale, 0.04, 0.12 * scale if fox else 0.02))
    builder.limb(tail_start, tail_end, 0.05 * scale, 0.028 * scale if fox else 0.02 * scale, fur, segments=8)
    if fox:
        builder.sphere(tail_end, 0.04, mats["fur_fox_white"], u=8, v=6)
    leg_mat = dict(mats)
    leg_mat["leg"] = mats["paw"] if fox else fur
    add_quad_legs(builder, body, 0.58 * scale, 0.13 * scale, 0.34 * scale, 0.0, 0.032 * scale, leg_mat, hind_scale=1.08)
    return builder


def add_insect_legs(
    builder: Builder,
    thorax: Vector,
    length: float,
    spread: float,
    material: bpy.types.Material,
    count: int = 3,
) -> None:
    for side in (-1.0, 1.0):
        for index in range(count):
            x = (index - (count - 1) / 2) * 0.035
            hip = thorax + Vector((x, side * 0.02, -0.01))
            mid = hip + Vector((0.01 * side, side * spread * 0.55, -length * 0.45))
            foot = hip + Vector((0.02 * side, side * spread, -length))
            builder.limb(hip, mid, 0.006, 0.004, material, segments=4, caps=True)
            builder.limb(mid, foot, 0.004, 0.003, material, segments=4, caps=True)


def build_bee(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    thorax = Vector((0.0, 0.0, 0.0))
    builder.sphere(thorax, Vector((0.04, 0.035, 0.032)), mats["bee_black"], u=10, v=7)
    builder.sphere(thorax + Vector((0.055, 0.0, 0.0)), Vector((0.028, 0.024, 0.024)), mats["bee_black"], u=8, v=6)
    builder.sphere(thorax + Vector((-0.06, 0.0, 0.0)), Vector((0.05, 0.036, 0.032)), mats["bee_gold"], u=10, v=7)
    builder.sphere(thorax + Vector((-0.09, 0.0, 0.0)), Vector((0.028, 0.032, 0.028)), mats["bee_black"], u=8, v=6)
    builder.sphere(thorax + Vector((-0.12, 0.0, 0.0)), Vector((0.022, 0.026, 0.022)), mats["bee_gold"], u=8, v=6)
    add_eyes(builder, thorax + Vector((0.07, 0.0, 0.01)), Vector((1.0, 0.0, 0.2)), 0.016, 0.01, mats)
    for sign in (-1.0, 1.0):
        builder.disk(
            thorax + Vector((0.0, sign * 0.05, 0.03)),
            Vector((0.1, sign, 0.8)),
            0.055,
            0.004,
            mats["wing"],
            segments=12,
        )
    add_insect_legs(builder, thorax, 0.05, 0.045, mats["bee_black"])
    return builder


def build_butterfly(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    body = Vector((0.0, 0.0, 0.0))
    builder.sphere(body, Vector((0.07, 0.014, 0.014)), mats["bee_black"], u=10, v=6)
    builder.sphere(body + Vector((0.06, 0.0, 0.0)), Vector((0.016, 0.014, 0.014)), mats["bee_black"], u=8, v=6)
    add_eyes(builder, body + Vector((0.07, 0.0, 0.006)), Vector((1.0, 0.0, 0.2)), 0.01, 0.006, mats)
    for sign in (-1.0, 1.0):
        builder.disk(body + Vector((0.01, sign * 0.08, 0.01)), Vector((0.05, sign * 0.15, 1.0)), 0.09, 0.004, mats["wing_pink"], segments=14)
        builder.disk(body + Vector((-0.04, sign * 0.055, 0.008)), Vector((0.0, sign * 0.2, 1.0)), 0.055, 0.004, mats["wing_dark"], segments=12)
        builder.sphere(body + Vector((0.04, sign * 0.07, 0.016)), 0.016, mats["fur_fox_white"], u=6, v=4)
        builder.limb(
            body + Vector((0.07, sign * 0.004, 0.01)),
            body + Vector((0.12, sign * 0.03, 0.05)),
            0.003,
            0.002,
            mats["bee_black"],
            segments=4,
        )
    return builder


def build_beetle(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    body = Vector((0.0, 0.0, 0.04))
    builder.sphere(body, Vector((0.08, 0.05, 0.035)), mats["beetle_shell"], u=12, v=8)
    builder.sphere(body + Vector((0.07, 0.0, 0.01)), Vector((0.03, 0.028, 0.022)), mats["beetle"], u=8, v=6)
    add_eyes(builder, body + Vector((0.085, 0.0, 0.016)), Vector((1.0, 0.0, 0.2)), 0.014, 0.007, mats)
    add_insect_legs(builder, body, 0.035, 0.05, mats["bee_black"])
    return builder


def build_ant(mats: dict[str, bpy.types.Material]) -> Builder:
    builder = Builder()
    thorax = Vector((0.0, 0.0, 0.035))
    builder.sphere(thorax, Vector((0.03, 0.022, 0.018)), mats["ant"], u=8, v=6)
    builder.sphere(thorax + Vector((0.055, 0.0, 0.01)), Vector((0.028, 0.022, 0.02)), mats["ant"], u=8, v=6)
    builder.sphere(thorax + Vector((-0.07, 0.0, 0.012)), Vector((0.045, 0.03, 0.028)), mats["ant"], u=10, v=7)
    add_eyes(builder, thorax + Vector((0.07, 0.0, 0.016)), Vector((1.0, 0.0, 0.2)), 0.012, 0.006, mats)
    for sign in (-1.0, 1.0):
        builder.limb(
            thorax + Vector((0.07, sign * 0.006, 0.02)),
            thorax + Vector((0.11, sign * 0.03, 0.05)),
            0.004,
            0.002,
            mats["ant"],
            segments=4,
        )
    add_insect_legs(builder, thorax, 0.03, 0.04, mats["ant"])
    return builder


BUILDERS = {
    "oak": build_oak,
    "birch": build_birch,
    "pine": build_pine,
    "grass": build_grass,
    "clover": build_clover,
    "berry_bush": build_berry_bush,
    "fern": build_fern,
    "reeds": build_reeds,
    "mushroom": build_mushroom,
    "rabbit": lambda mats: build_rabbit(mats, False),
    "hare": lambda mats: build_rabbit(mats, True),
    "mouse": build_mouse,
    "deer": build_deer,
    "boar": build_boar,
    "wolf": lambda mats: build_canid(mats, False),
    "fox": lambda mats: build_canid(mats, True),
    "bee": build_bee,
    "butterfly": build_butterfly,
    "beetle": build_beetle,
    "ant": build_ant,
}


def ensure_object_mode() -> None:
    if bpy.context.view_layer.objects.active and bpy.context.view_layer.objects.active.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def wipe_generated() -> None:
    ensure_object_mode()
    keep = {"Camera", "Light"}
    for obj in list(bpy.data.objects):
        if obj.name in keep:
            continue
        if obj.name.startswith("org_") or obj.name in {"ShowcaseGround", "FillLight", "RimLight", "KeyLight"}:
            bpy.data.objects.remove(obj, do_unlink=True)
    cube = bpy.data.objects.get("Cube")
    if cube is not None:
        bpy.data.objects.remove(cube, do_unlink=True)
    for mesh in list(bpy.data.meshes):
        if mesh.users == 0:
            bpy.data.meshes.remove(mesh)
    for material in list(bpy.data.materials):
        if material.name.startswith("org_") and material.users == 0:
            bpy.data.materials.remove(material)
    for collection in list(bpy.data.collections):
        if collection.name in {"Organisms", "Showcase"} and collection != bpy.context.scene.collection:
            bpy.data.collections.remove(collection)


def ensure_collection(name: str) -> bpy.types.Collection:
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(collection)
    return collection


def setup_stage() -> None:
    scene = bpy.context.scene
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    world = scene.world
    if world is None:
        world = bpy.data.worlds.new("World")
        scene.world = world
    world.use_nodes = True
    background = world.node_tree.nodes.get("Background")
    if background is not None:
        background.inputs[0].default_value = (0.46, 0.62, 0.78, 1.0)
        background.inputs[1].default_value = 0.55

    light = bpy.data.objects.get("Light")
    if light is not None and light.type == "LIGHT":
        light.data.type = "SUN"
        light.data.energy = 4.2
        light.data.color = (1.0, 0.94, 0.84)
        light.data.angle = 0.18
        light.rotation_euler = (math.radians(48.0), math.radians(15.0), math.radians(28.0))

    fill_data = bpy.data.lights.new("FillLightData", "AREA")
    fill_data.energy = 120.0
    fill_data.size = 6.0
    fill_data.color = (0.72, 0.82, 1.0)
    fill = bpy.data.objects.new("FillLight", fill_data)
    fill.location = (-7.0, -8.0, 5.5)
    fill.rotation_euler = (math.radians(65.0), 0.0, math.radians(-35.0))
    bpy.context.scene.collection.objects.link(fill)

    ground_mesh = bpy.data.meshes.new("ShowcaseGround")
    ground_builder = bmesh.new()
    bmesh.ops.create_grid(ground_builder, x_segments=8, y_segments=8, size=12.0, calc_uvs=True)
    bmesh.ops.translate(ground_builder, verts=ground_builder.verts, vec=(0.0, 5.0, -0.01))
    ground_builder.to_mesh(ground_mesh)
    ground_builder.free()
    ground = bpy.data.objects.new("ShowcaseGround", ground_mesh)
    ground_mesh.materials.append(make_material("org_ground", (0.33, 0.40, 0.26), 0.96))
    bpy.context.scene.collection.objects.link(ground)

    camera = bpy.data.objects.get("Camera")
    if camera is None:
        camera_data = bpy.data.cameras.new("Camera")
        camera = bpy.data.objects.new("Camera", camera_data)
        bpy.context.scene.collection.objects.link(camera)
        bpy.context.scene.camera = camera
    camera.location = (14.8, -16.4, 8.2)
    target = Vector((0.4, 4.6, 1.15))
    camera.rotation_euler = (target - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera.data.lens = 45.0


def export_organism(obj: bpy.types.Object, path: str) -> dict:
    location = obj.location.copy()
    rotation = obj.rotation_euler.copy()
    scale = obj.scale.copy()
    hide = obj.hide_viewport
    obj.location = (0.0, 0.0, 0.0)
    obj.rotation_euler = (0.0, 0.0, 0.0)
    obj.scale = (1.0, 1.0, 1.0)
    obj.hide_viewport = False
    obj.hide_render = False
    obj.hide_set(False)
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.gltf(
        filepath=path,
        export_format="GLB",
        use_selection=True,
        export_apply=True,
        export_yup=True,
        export_materials="EXPORT",
        export_cameras=False,
        export_lights=False,
        export_animations=False,
        export_skins=False,
        export_extras=False,
        check_existing=False,
    )
    obj.location = location
    obj.rotation_euler = rotation
    obj.scale = scale
    obj.hide_viewport = hide
    return {"name": obj.name, "path": path, "tris": len(obj.data.polygons) if obj.type == "MESH" else 0}


def set_viewport_shading() -> None:
    for window in bpy.context.window_manager.windows:
        for area in window.screen.areas:
            if area.type != "VIEW_3D":
                continue
            for space in area.spaces:
                if space.type != "VIEW_3D":
                    continue
                space.shading.type = "MATERIAL"
                space.shading.use_scene_lights = True
                space.shading.use_scene_world = True
                space.overlay.show_overlays = False


def main() -> dict:
    wipe_generated()
    mats = palette()
    organisms = ensure_collection("Organisms")
    setup_stage()
    created = []
    errors = []
    for key in SPECIES:
        try:
            builder = BUILDERS[key](mats)
            obj = to_object(builder, f"org_{key}", organisms)
            obj.location = LAYOUT[key]
            if key in {"bee", "butterfly", "beetle", "ant"}:
                obj.scale = (INSECT_SHOW_SCALE, INSECT_SHOW_SCALE, INSECT_SHOW_SCALE)
            if key in {"rabbit", "hare", "mouse", "deer", "boar", "wolf", "fox", "bee", "butterfly", "beetle", "ant", "mushroom"}:
                apply_subsurf(obj, 1)
            created.append({"key": key, "object": obj.name, "faces": len(obj.data.polygons)})
        except Exception as exc:  # noqa: BLE001
            errors.append({"key": key, "error": str(exc)})

    os.makedirs(EXPORT_DIR, exist_ok=True)
    os.makedirs(os.path.dirname(BLEND_PATH), exist_ok=True)
    catalog_backed = {
        "oak",
        "birch",
        "pine",
        "grass",
        "reeds",
        "deer",
        "boar",
        "wolf",
        "fox",
        "mouse",
        "butterfly",
        "ant",
    }
    keep_catalog = os.environ.get("SIM_PROCEDURAL_ORGANISMS") != "1"
    exported = []
    for key in SPECIES:
        obj = bpy.data.objects.get(f"org_{key}")
        if obj is None:
            continue
        glb_path = os.path.join(EXPORT_DIR, f"{key}.glb")
        if keep_catalog and key in catalog_backed and os.path.isfile(glb_path):
            continue
        exported.append(export_organism(obj, glb_path))

    bpy.ops.wm.save_as_mainfile(filepath=BLEND_PATH)
    set_viewport_shading()
    bpy.context.view_layer.update()
    return {
        "ok": not errors,
        "created": created,
        "exported": exported,
        "errors": errors,
        "blend": BLEND_PATH,
        "export_dir": EXPORT_DIR,
    }


result = main()
