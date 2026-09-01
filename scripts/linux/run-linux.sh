#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${repo_root}/scripts/linux/runtime-common.sh"
app="${repo_root}/dist-linux/bin/EShot"

# Match the packaged launcher's complete XWayland canvas on KDE and GNOME
# Wayland, so local testing exercises the same multi-monitor selection path.
if [[ "$(eshot_xwayland_overlay_enabled)" == "1" ]]; then
  export QT_QPA_PLATFORM='xcb;wayland'
  export ESHOT_WAYLAND_XWAYLAND_OVERLAY=1
fi

if [[ ! -x "${app}" ]]; then
  "${repo_root}/scripts/linux/build-linux.sh"
fi

exec "${app}" "$@"
