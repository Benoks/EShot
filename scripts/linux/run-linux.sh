#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${repo_root}/scripts/linux/runtime-common.sh"
app="${repo_root}/dist-linux/bin/EShot"

# Exercise exactly the same per-desktop backend policy as packaged builds.
eshot_configure_overlay_backend

if [[ ! -x "${app}" ]]; then
  "${repo_root}/scripts/linux/build-linux.sh"
fi

exec "${app}" "$@"
