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
for launcher in "${runner}" "${repo_root}/scripts/linux/eshot-launcher" \
                "${repo_root}/packaging/linux/AppRun"; do
  grep -xF 'eshot_configure_overlay_backend' "${launcher}" >/dev/null || {
    echo "${launcher} must use the shared per-desktop overlay policy" >&2
    exit 1
  }
done

printf 'local runner overlay contract tests passed\n'
