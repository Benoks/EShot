#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
overlay="${repo_root}/src/capture/CaptureOverlay.cpp"

# A native Wayland surface cannot span multiple outputs reliably. The capture
# window is deliberately pinned to the output under the cursor, so this branch
# must only ever create a snapshot for that one output. Full desktop stitching
# belongs to the separate XWayland overlay path above it.
native_wayland_block="$(awk '
  /const bool isWayland = QGuiApplication::platformName\(\)\.contains\(QStringLiteral\("wayland"\)/ { in_block = 1 }
  in_block { print }
  in_block && /^    QRect logicalRect;/ { exit }
' "${overlay}")"

[[ "${native_wayland_block}" == *'m_screenSnapshot = LinuxPortalScreenshot::grabScreen(screen, this);'* ]] || {
  echo 'native Wayland capture no longer uses the cursor screen snapshot' >&2
  exit 1
}

[[ "${native_wayland_block}" != *'m_virtualDesktopRect = virtualLogical;'* ]] || {
  echo 'native Wayland capture must not replace its single-screen geometry with a virtual desktop' >&2
  exit 1
}

[[ "${native_wayland_block}" != *'QPixmap stitched('* ]] || {
  echo 'native Wayland capture must not stitch a multi-monitor snapshot into a single-screen overlay' >&2
  exit 1
}

printf 'native Wayland overlay tests passed\n'
