#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tag="${1:-v1.0.2-stable}"
cpp_tag="${GODOT_CPP_TAG:-godot-4.5-stable}"
src="${root}/build/terrain3d-src"
dest="${root}/godot/addons/terrain_3d"
stamp="${dest}/.sim_terrain3d_godot_cpp"

"${root}/scripts/fetch_terrain3d.sh" "${tag}"

if [[ -f "${stamp}" ]] && [[ "$(<"${stamp}")" == "${cpp_tag}" ]] \
    && [[ -f "${dest}/bin/libterrain.linux.debug.x86_64.so" ]]; then
    echo "Terrain3D linux debug library already built against ${cpp_tag}"
    exit 0
fi

mkdir -p "${root}/build"
if [[ -d "${src}/.git" ]]; then
    git -C "${src}" fetch --tags --depth 1 origin "refs/tags/${tag}:refs/tags/${tag}"
    git -C "${src}" checkout --detach "${tag}"
else
    git clone --depth 1 --branch "${tag}" https://github.com/TokisanGames/Terrain3D.git "${src}"
fi

if [[ -d "${src}/godot-cpp/.git" ]]; then
    git -C "${src}/godot-cpp" fetch --tags --depth 1 origin "refs/tags/${cpp_tag}:refs/tags/${cpp_tag}"
    git -C "${src}/godot-cpp" checkout --detach "${cpp_tag}"
else
    rm -rf "${src}/godot-cpp"
    git clone --depth 1 --branch "${cpp_tag}" https://github.com/godotengine/godot-cpp.git "${src}/godot-cpp"
fi
if [[ -f "${src}/godot-cpp/.gitmodules" ]]; then
    git -C "${src}/godot-cpp" submodule update --init --depth 1
fi

patch_file="${root}/scripts/patches/terrain3d-1.0.2-godot-cpp-45.patch"
if [[ -f "${patch_file}" ]]; then
    if git -C "${src}" apply --check "${patch_file}" >/dev/null 2>&1; then
        git -C "${src}" apply "${patch_file}"
    fi
fi

venv="${root}/build/terrain3d-venv"
if [[ -x "${venv}/bin/scons" ]]; then
    scons_bin="${venv}/bin/scons"
elif command -v scons >/dev/null; then
    scons_bin="$(command -v scons)"
else
    python3 -m venv "${venv}"
    "${venv}/bin/pip" install --upgrade pip
    "${venv}/bin/pip" install 'scons>=4'
    scons_bin="${venv}/bin/scons"
fi
if [[ ! -x "${scons_bin}" ]]; then
    echo "scons is not available; install it to rebuild Terrain3D against ${cpp_tag}" >&2
    exit 1
fi

(
    cd "${src}"
    "${scons_bin}" target=template_debug
)

shopt -s nullglob
built=("${src}/project/addons/terrain_3d/bin/"libterrain.linux.debug.*)
if [[ ${#built[@]} -eq 0 ]]; then
    echo "Terrain3D build did not produce libterrain.linux.debug.*" >&2
    exit 1
fi
mkdir -p "${dest}/bin"
cp -a "${built[@]}" "${dest}/bin/"
printf '%s\n' "${cpp_tag}" >"${stamp}"
echo "Terrain3D linux debug library rebuilt against ${cpp_tag}"
