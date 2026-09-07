#!/usr/bin/env bash
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
tag="${1:-godot-4.5-stable}"
dest="${root}/third_party/godot-cpp"

if [[ -d "${dest}/.git" ]]; then
    git -C "${dest}" fetch --tags --depth 1 origin "refs/tags/${tag}:refs/tags/${tag}"
    git -C "${dest}" checkout --detach "${tag}"
else
    mkdir -p "${root}/third_party"
    git clone --depth 1 --branch "${tag}" https://github.com/godotengine/godot-cpp.git "${dest}"
fi

if [[ -f "${dest}/.gitmodules" ]]; then
    git -C "${dest}" submodule update --init --depth 1
fi

echo "godot-cpp is at ${dest} (${tag})"
