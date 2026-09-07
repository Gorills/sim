"""Import RPG/3D catalog meshes and export Godot organism GLBs.

Catalog-backed species replace procedural meshes in godot/assets/organisms.
Gaps (clover, berry_bush, fern, mushroom, rabbit, hare, bee, beetle) stay procedural.

Usage:
  blender --background --python tools/blender_catalog_organisms.py
  SIM_MODEL_CATALOG=/path/to/models blender --background --python tools/blender_catalog_organisms.py

Convention matches tools/blender_nature_models.py: Z-up, animals face +X,
origin at ground (thorax for flying insects), export_yup GLB.
"""

from __future__ import annotations

import json
import math
import os
import shutil
import subprocess
import zipfile
from dataclasses import dataclass
from typing import Iterable

import bmesh
import bpy
from mathutils import Matrix, Vector

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
EXPORT_DIR = os.path.join(ROOT, "godot", "assets", "organisms")
DEFAULT_CATALOG = os.path.join(os.path.expanduser("~"), "Рабочий стол", "RPG", "3D", "models")
CACHE_DIR = os.environ.get("SIM_CATALOG_CACHE", "/tmp/sim-catalog-cache")


@dataclass(frozen=True)
class Spec:
    key: str
    kind: str
    target_height: float
    ground: bool = True
    face_plus_x: bool = False
    max_faces: int | None = None


SPECS: tuple[Spec, ...] = (
    Spec("oak", "plant", 2.05),
    Spec("birch", "plant", 2.40),
    Spec("pine", "plant", 2.90),
    Spec("grass", "plant", 0.42),
    Spec("reeds", "plant", 1.00, max_faces=4000),
    Spec("fox", "animal", 0.55, face_plus_x=True),
    Spec("deer", "animal", 1.12, face_plus_x=True),
    Spec("boar", "animal", 0.72, face_plus_x=True),
    Spec("wolf", "animal", 0.68, face_plus_x=True),
    Spec("mouse", "animal", 0.11, face_plus_x=True),
    Spec("butterfly", "fly", 0.12, ground=False, face_plus_x=True),
    Spec("ant", "insect", 0.07, face_plus_x=True, max_faces=2500),
)

CATALOG_BACKED_KEYS = tuple(spec.key for spec in SPECS)


def catalog_dir() -> str:
    return os.environ.get("SIM_MODEL_CATALOG", DEFAULT_CATALOG)


def ensure_object_mode() -> None:
    active = bpy.context.view_layer.objects.active
    if active is not None and active.mode != "OBJECT":
        bpy.ops.object.mode_set(mode="OBJECT")


def reset_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def select_only(objects: Iterable[bpy.types.Object]) -> bpy.types.Object:
    ensure_object_mode()
    bpy.ops.object.select_all(action="DESELECT")
    items = [obj for obj in objects if obj is not None]
    if not items:
        raise RuntimeError("nothing to select")
    for obj in items:
        obj.hide_set(False)
        obj.hide_viewport = False
        obj.hide_select = False
        obj.select_set(True)
    bpy.context.view_layer.objects.active = items[0]
    return items[0]


def append_objects(blend_path: str, names: Iterable[str]) -> list[bpy.types.Object]:
    wanted = list(names)
    with bpy.data.libraries.load(blend_path, link=False) as (source, dest):
        available = set(source.objects)
        dest.objects = [name for name in wanted if name in available]
        missing = [name for name in wanted if name not in available]
    if missing:
        raise RuntimeError(f"{os.path.basename(blend_path)} missing {missing}")
    imported: list[bpy.types.Object] = []
    scene = bpy.context.scene.collection
    for obj in dest.objects:
        if obj is None:
            continue
        if obj.name not in scene.objects:
            scene.objects.link(obj)
        imported.append(obj)
    return imported


def import_fbx(path: str) -> list[bpy.types.Object]:
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=path, use_anim=False, ignore_leaf_bones=True)
    return [obj for obj in bpy.data.objects if obj not in before]


def extract_zip(archive: str, dest: str) -> str:
    os.makedirs(dest, exist_ok=True)
    marker = os.path.join(dest, ".extracted")
    if not os.path.isfile(marker):
        with zipfile.ZipFile(archive) as zipped:
            zipped.extractall(dest)
        with open(marker, "w", encoding="utf-8") as handle:
            handle.write(archive)
    return dest


def find_named(root: str, filename: str) -> str:
    for dirpath, _dirs, files in os.walk(root):
        if filename in files:
            return os.path.join(dirpath, filename)
    raise FileNotFoundError(f"{filename} under {root}")


def extract_rar(archive: str, dest: str) -> str:
    os.makedirs(dest, exist_ok=True)
    marker = os.path.join(dest, ".extracted")
    if not os.path.isfile(marker):
        unar = shutil.which("unar")
        if unar is None:
            raise RuntimeError(f"unar is required to extract {archive}")
        subprocess.run([unar, "-f", "-o", dest, archive], check=True)
        with open(marker, "w", encoding="utf-8") as handle:
            handle.write(archive)
    return dest


def bake_join(objects: Iterable[bpy.types.Object], name: str) -> bpy.types.Object:
    depsgraph = bpy.context.evaluated_depsgraph_get()
    baked: list[bpy.types.Object] = []
    scene = bpy.context.scene.collection
    for obj in objects:
        if obj.type not in {"MESH", "CURVE"}:
            continue
        evaluated = obj.evaluated_get(depsgraph)
        mesh = bpy.data.meshes.new_from_object(
            evaluated, preserve_all_data_layers=True, depsgraph=depsgraph
        )
        if mesh is None or len(mesh.vertices) == 0:
            if mesh is not None:
                bpy.data.meshes.remove(mesh)
            continue
        mesh.name = f"{name}_{obj.name}_bake"
        clone = bpy.data.objects.new(mesh.name, mesh)
        clone.matrix_world = obj.matrix_world.copy()
        scene.objects.link(clone)
        baked.append(clone)
    if not baked:
        raise RuntimeError(f"{name}: no evaluable mesh")
    if len(baked) == 1:
        baked[0].name = name
        return baked[0]
    active = select_only(baked)
    bpy.ops.object.join()
    active = bpy.context.view_layer.objects.active
    active.name = name
    return active


def collapse_duplicate_materials(obj: bpy.types.Object) -> None:
    mesh = obj.data
    if obj.type != "MESH" or not mesh.materials:
        return
    unique: dict[str, bpy.types.Material] = {}
    remap: list[int] = []
    compacted: list[bpy.types.Material] = []
    for material in mesh.materials:
        key = material.name if material is not None else ""
        if key not in unique:
            unique[key] = material
            compacted.append(material)
        remap.append(compacted.index(unique[key]))
    for polygon in mesh.polygons:
        polygon.material_index = remap[polygon.material_index]
    mesh.materials.clear()
    for material in compacted:
        mesh.materials.append(material)


def enable_leaf_alpha(obj: bpy.types.Object) -> None:
    if obj.type != "MESH":
        return
    for material in obj.data.materials:
        if material is None:
            continue
        material.use_backface_culling = False
        has_alpha = False
        if material.use_nodes:
            for node in material.node_tree.nodes:
                if node.type == "BSDF_PRINCIPLED" and node.inputs["Alpha"].is_linked:
                    has_alpha = True
                if node.type == "TEX_IMAGE" and node.image is not None and node.image.channels == 4:
                    has_alpha = True
        if has_alpha:
            material.blend_method = "HASHED"
            if hasattr(material, "surface_render_method"):
                material.surface_render_method = "DITHERED"


def apply_world(obj: bpy.types.Object) -> None:
    mesh = obj.data
    mesh.transform(obj.matrix_world)
    mesh.update()
    obj.matrix_world = Matrix.Identity(4)
    obj.location = (0.0, 0.0, 0.0)
    obj.rotation_mode = "XYZ"
    obj.rotation_euler = (0.0, 0.0, 0.0)
    obj.scale = (1.0, 1.0, 1.0)


def rotate_mesh(obj: bpy.types.Object, axis: str, radians: float) -> None:
    obj.data.transform(Matrix.Rotation(radians, 4, axis))
    obj.data.update()


def mesh_aabb(obj: bpy.types.Object) -> tuple[Vector, Vector]:
    xs = [vert.co.x for vert in obj.data.vertices]
    ys = [vert.co.y for vert in obj.data.vertices]
    zs = [vert.co.z for vert in obj.data.vertices]
    return Vector((min(xs), min(ys), min(zs))), Vector((max(xs), max(ys), max(zs)))


def translate_mesh(obj: bpy.types.Object, offset: Vector) -> None:
    obj.data.transform(Matrix.Translation(offset))
    obj.data.update()


def scale_mesh(obj: bpy.types.Object, factor: float) -> None:
    obj.data.transform(Matrix.Diagonal((factor, factor, factor, 1.0)))
    obj.data.update()


def triangulate(obj: bpy.types.Object) -> None:
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.triangulate(bm, faces=bm.faces[:])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def downscale_images(max_dim: int = 1024) -> None:
    for image in bpy.data.images:
        width, height = image.size
        if width <= 0 or height <= 0 or max(width, height) <= max_dim:
            continue
        if width >= height:
            image.scale(max_dim, max(1, int(height * max_dim / width)))
        else:
            image.scale(max(1, int(width * max_dim / height)), max_dim)
        try:
            image.pack()
        except Exception:
            pass


def decimate(obj: bpy.types.Object, max_faces: int) -> None:
    faces = len(obj.data.polygons)
    if faces <= max_faces:
        return
    modifier = obj.modifiers.new("catalog_decimate", "DECIMATE")
    modifier.ratio = max(0.02, max_faces / float(faces))
    select_only([obj])
    bpy.ops.object.modifier_apply(modifier=modifier.name)


def normalize(obj: bpy.types.Object, spec: Spec) -> None:
    apply_world(obj)
    mins, maxs = mesh_aabb(obj)
    size = maxs - mins
    if spec.kind == "plant":
        if size.y > size.z * 1.15 and size.y >= size.x:
            rotate_mesh(obj, "X", math.radians(90.0))
        elif size.x > size.z * 1.15 and size.x > size.y:
            rotate_mesh(obj, "Y", math.radians(-90.0))
    if spec.face_plus_x:
        mins, maxs = mesh_aabb(obj)
        size = maxs - mins
        if size.y > size.x:
            rotate_mesh(obj, "Z", math.radians(-90.0))
    mins, maxs = mesh_aabb(obj)
    if spec.ground:
        translate_mesh(obj, Vector((-(mins.x + maxs.x) * 0.5, -(mins.y + maxs.y) * 0.5, -mins.z)))
    else:
        center = (mins + maxs) * 0.5
        translate_mesh(obj, -center)
    mins, maxs = mesh_aabb(obj)
    height = max(maxs.z - mins.z, 1e-6)
    scale_mesh(obj, spec.target_height / height)
    if spec.max_faces:
        decimate(obj, spec.max_faces)
    triangulate(obj)
    if spec.ground:
        mins, maxs = mesh_aabb(obj)
        translate_mesh(obj, Vector((-(mins.x + maxs.x) * 0.5, -(mins.y + maxs.y) * 0.5, -mins.z)))
    collapse_duplicate_materials(obj)
    enable_leaf_alpha(obj)
    downscale_images()


def export_glb(obj: bpy.types.Object, path: str) -> dict:
    os.makedirs(os.path.dirname(path), exist_ok=True)
    select_only([obj])
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
    mins, maxs = mesh_aabb(obj)
    size = maxs - mins
    return {
        "name": obj.name,
        "path": path,
        "tris": len(obj.data.polygons) if obj.type == "MESH" else 0,
        "size": [round(size.x, 4), round(size.y, 4), round(size.z, 4)],
    }


def remap_images(search_dirs: Iterable[str]) -> None:
    folders = [os.path.abspath(path) for path in search_dirs]
    for image in bpy.data.images:
        basename = os.path.basename(str(image.filepath).replace("\\", "/"))
        if not basename:
            basename = os.path.basename(image.name)
        for folder in folders:
            candidate = os.path.join(folder, basename)
            if not os.path.isfile(candidate):
                continue
            image.filepath = candidate
            image.source = "FILE"
            try:
                image.reload()
                image.pack()
            except Exception:
                pass
            break


def assign_image(obj: bpy.types.Object, image_path: str, alpha_path: str | None = None) -> None:
    if not os.path.isfile(image_path):
        return
    image = bpy.data.images.load(image_path, check_existing=True)
    image.pack()
    material = bpy.data.materials.new(f"{obj.name}_albedo")
    material.use_nodes = True
    material.use_backface_culling = False
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    principled = next(node for node in nodes if node.type == "BSDF_PRINCIPLED")
    texture = nodes.new("ShaderNodeTexImage")
    texture.image = image
    links.new(texture.outputs["Color"], principled.inputs["Base Color"])
    alpha_image = image
    if alpha_path and os.path.isfile(alpha_path):
        alpha_image = bpy.data.images.load(alpha_path, check_existing=True)
        alpha_image.pack()
        alpha_node = nodes.new("ShaderNodeTexImage")
        alpha_node.image = alpha_image
        links.new(alpha_node.outputs["Color"], principled.inputs["Alpha"])
        material.blend_method = "HASHED"
    elif image.channels == 4:
        links.new(texture.outputs["Alpha"], principled.inputs["Alpha"])
        material.blend_method = "HASHED"
    obj.data.materials.clear()
    obj.data.materials.append(material)


def mesh_objects(objects: Iterable[bpy.types.Object]) -> list[bpy.types.Object]:
    return [obj for obj in objects if obj.type in {"MESH", "CURVE"}]


def build_nature(catalog: str) -> dict[str, bpy.types.Object]:
    blend = os.path.join(catalog, "Nature+collection.blend")
    names = [
        "Bark.016",
        "Leaves.016",
        "Bark",
        "Leaves.001",
        "U3DMesh.007",
        "Armature.035",
        "Grass_B_Low_Small",
    ]
    imported = {obj.name: obj for obj in append_objects(blend, names)}
    groups = {
        "oak": ["Bark.016", "Leaves.016"],
        "birch": ["Bark", "Leaves.001"],
        "pine": ["U3DMesh.007"],
        "grass": ["Grass_B_Low_Small"],
    }
    built: dict[str, bpy.types.Object] = {}
    for key, object_names in groups.items():
        sources = [imported[name] for name in object_names if name in imported]
        built[key] = bake_join(sources, f"org_{key}")
    return built


def build_reeds(catalog: str) -> bpy.types.Object:
    archive = os.path.join(catalog, "Phormium_BLEND.zip")
    dest = extract_zip(archive, os.path.join(CACHE_DIR, "phormium"))
    blend = os.path.join(dest, "Phormium_BLEND", "phormium_tenax.blend")
    imported = append_objects(blend, ["Phormium_tenax_A001"])
    return bake_join(imported, "org_reeds")


def build_animal(catalog: str, key: str, rel_fbx: str, texture_name: str) -> bpy.types.Object:
    archive = os.path.join(catalog, "Free.rar")
    dest = extract_rar(archive, os.path.join(CACHE_DIR, "free"))
    fbx = os.path.join(dest, "Free", "Models", "Fbx", "Singlemodel", rel_fbx)
    texture = os.path.join(dest, "Free", "Texture", texture_name)
    imported = import_fbx(fbx)
    remap_images([os.path.dirname(texture)])
    obj = bake_join(mesh_objects(imported), f"org_{key}")
    has_image = False
    for material in obj.data.materials:
        if material is None or not getattr(material, "use_nodes", False):
            continue
        for node in material.node_tree.nodes:
            if node.type == "TEX_IMAGE" and node.image is not None:
                has_image = True
    if not has_image:
        assign_image(obj, texture)
    return obj


def build_mouse(catalog: str) -> bpy.types.Object:
    archive = os.path.join(catalog, "rat.rar")
    dest = extract_rar(archive, os.path.join(CACHE_DIR, "rat"))
    fbx = find_named(dest, "Rat.fbx")
    texture = find_named(dest, "rat.png")
    imported = import_fbx(fbx)
    obj = bake_join(mesh_objects(imported), "org_mouse")
    assign_image(obj, texture)
    return obj


def build_butterfly(catalog: str) -> bpy.types.Object:
    archive = os.path.join(catalog, "Animated+Butterfly+Pack+By+Travis+Davids.zip")
    dest = extract_zip(archive, os.path.join(CACHE_DIR, "butterfly"))
    root = os.path.join(dest, "Animated Butterfly Pack By Travis Davids")
    fbx = os.path.join(root, "Idle Animations (90 Frames)", "Butterfly_Idle_1.fbx")
    color = os.path.join(
        root,
        "Textures And Butterfly Body",
        "Blender 2.80 Diffuse, & Opacity Map",
        "blender_2.80_PNG_COLOR_OPACITY_Morpho_didius_Male_Dos_MHNT.png",
    )
    imported = import_fbx(fbx)
    obj = bake_join(mesh_objects(imported), "org_butterfly")
    assign_image(obj, color)
    return obj


def build_ant(catalog: str) -> bpy.types.Object:
    blend = os.path.join(catalog, "ArgentineAnt.blend")
    with bpy.data.libraries.load(blend, link=False) as (source, dest):
        dest.objects = list(source.objects)
    scene = bpy.context.scene.collection
    imported: list[bpy.types.Object] = []
    for obj in dest.objects:
        if obj is None:
            continue
        if obj.name not in scene.objects:
            scene.objects.link(obj)
        imported.append(obj)
    return bake_join(mesh_objects(imported), "org_ant")


def export_key(obj: bpy.types.Object, spec: Spec) -> dict:
    normalize(obj, spec)
    return export_glb(obj, os.path.join(EXPORT_DIR, f"{spec.key}.glb"))


def run() -> dict:
    catalog = catalog_dir()
    if not os.path.isdir(catalog):
        return {"ok": False, "error": f"catalog missing: {catalog}"}
    os.makedirs(EXPORT_DIR, exist_ok=True)
    os.makedirs(CACHE_DIR, exist_ok=True)
    specs = {spec.key: spec for spec in SPECS}
    exported: list[dict] = []
    errors: list[dict] = []

    def ship(key: str, factory) -> None:
        try:
            reset_scene()
            exported.append(export_key(factory(), specs[key]))
        except Exception as exc:  # noqa: BLE001
            errors.append({"key": key, "error": str(exc)})

    for key in ("oak", "birch", "pine", "grass"):
        ship(key, lambda key=key: build_nature(catalog)[key])
    ship("reeds", lambda: build_reeds(catalog))
    ship("fox", lambda: build_animal(catalog, "fox", "01foxFinal.fbx", "01foxtexture.png"))
    ship("deer", lambda: build_animal(catalog, "deer", "05deerFinal.fbx", "05deertexture.png"))
    ship("boar", lambda: build_animal(catalog, "boar", "04bizonFinal.fbx", "04bizontexture.png"))
    ship("wolf", lambda: build_animal(catalog, "wolf", "25dogFinal.fbx", "25dogtexture.png"))
    ship("mouse", lambda: build_mouse(catalog))
    ship("butterfly", lambda: build_butterfly(catalog))
    ship("ant", lambda: build_ant(catalog))

    return {
        "ok": not errors and len(exported) == len(SPECS),
        "catalog": catalog,
        "export_dir": EXPORT_DIR,
        "exported": exported,
        "errors": errors,
        "skipped_procedural": [
            "clover",
            "berry_bush",
            "fern",
            "mushroom",
            "rabbit",
            "hare",
            "bee",
            "beetle",
        ],
    }


result = run()
print("CATALOG_EXPORT " + json.dumps(result, ensure_ascii=True))
