#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${repo_root}/build-linux"
dist_dir="${repo_root}/dist-linux"

cmake -S "${repo_root}" -B "${build_dir}" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "${build_dir}" --parallel
cmake --install "${build_dir}" --prefix "${dist_dir}"

echo
echo "Built EShot Linux:"
echo "  ${dist_dir}/bin/EShot"
