#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
runner="${repo_root}/scripts/linux/run-linux.sh"
builder="${repo_root}/scripts/linux/build-linux.sh"

for script in "${runner}" "${builder}"; do
  if [[ ! -x "${script}" ]]; then
    echo "${script} must be executable for the documented local workflow" >&2
    exit 1
  fi
done

grep -F 'source "${repo_root}/scripts/linux/runtime-common.sh"' "${runner}" >/dev/null || {
  echo 'local Linux runner must use the shared Wayland overlay policy' >&2
  exit 1
}
grep -F 'if [[ "$(eshot_xwayland_overlay_enabled)" == "1" ]]; then' "${runner}" >/dev/null || {
  echo 'local Linux runner must enable the XWayland overlay when supported' >&2
  exit 1
}
grep -F "export QT_QPA_PLATFORM='xcb;wayland'" "${runner}" >/dev/null
grep -F 'export ESHOT_WAYLAND_XWAYLAND_OVERLAY=1' "${runner}" >/dev/null

printf 'local runner overlay contract tests passed\n'
