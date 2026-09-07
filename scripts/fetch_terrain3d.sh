#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tag="${1:-v1.0.2-stable}"
dest="${root}/godot/addons/terrain_3d"
stamp="${dest}/.sim_terrain3d_version"
zip_name="Terrain3D_${tag}.zip"
url="https://github.com/TokisanGames/Terrain3D/releases/download/${tag}/${zip_name}"

if [[ -f "${stamp}" ]] && [[ "$(<"${stamp}")" == "${tag}" ]] && [[ -f "${dest}/terrain.gdextension" || -f "${dest}/bin/terrain.gdextension" ]]; then
    rm -f "${dest}/plugin.cfg"
    echo "Terrain3D is at ${dest} (${tag})"
    exit 0
fi

tmp="$(mktemp -d "${TMPDIR:-/tmp}/sim-terrain3d.XXXXXX")"
cleanup() {
    rm -rf "${tmp}"
}
trap cleanup EXIT

zip_path="${tmp}/${zip_name}"
echo "Downloading ${url}"
curl -fL --retry 3 --retry-delay 2 -o "${zip_path}" "${url}"
unzip -q "${zip_path}" -d "${tmp}/extracted"

addon_src=""
if [[ -d "${tmp}/extracted/addons/terrain_3d" ]]; then
    addon_src="${tmp}/extracted/addons/terrain_3d"
else
    addon_src="$(find "${tmp}/extracted" -type d -name terrain_3d | head -n 1 || true)"
fi
if [[ -z "${addon_src}" ]]; then
    echo "Terrain3D zip did not contain addons/terrain_3d" >&2
    exit 1
fi

mkdir -p "${root}/godot/addons"
rm -rf "${dest}"
cp -a "${addon_src}" "${dest}"
printf '%s\n' "${tag}" >"${stamp}"
# Editor plugin docks crash Godot 4.7 headless import; runtime GDExtension is enough.
rm -f "${dest}/plugin.cfg"
echo "Terrain3D is at ${dest} (${tag})"
