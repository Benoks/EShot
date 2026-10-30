#!/usr/bin/env bash
# Checks that an EShot AppImage bundles the expected Qt version and the
# runtime pieces both display backends need (xcb with xcb-cursor, Wayland).
set -euo pipefail

if [[ "$#" -lt 1 || "$#" -gt 2 ]]; then
  printf 'Usage: %s <AppImage> [expected Qt version]\n' "${0##*/}" >&2
  exit 2
fi

appimage="$(realpath "$1")"
expected_qt="${2:-}"
[[ -x "${appimage}" ]] || {
  printf 'AppImage is missing or not executable: %s\n' "${appimage}" >&2
  exit 1
}

work_dir="$(mktemp -d)"
trap 'rm -rf "${work_dir}"' EXIT
(
  cd "${work_dir}"
  "${appimage}" --appimage-extract >/dev/null
)
appdir="${work_dir}/squashfs-root"

failures=0
require() {
  local description="$1"
  shift
  local match
  match="$(compgen -G "$1" || true)"
  if [[ -z "${match}" ]]; then
    printf 'Missing %s (%s)\n' "${description}" "${1#"${appdir}/"}" >&2
    failures=$((failures + 1))
  fi
}

require 'Qt Core library' "${appdir}/usr/lib/libQt6Core.so.6*"
require 'xcb platform plugin' "${appdir}/usr/plugins/platforms/libqxcb.so"
require 'Wayland platform plugin' "${appdir}/usr/plugins/platforms/libqwayland*.so"
require 'Wayland shell integration' "${appdir}/usr/plugins/wayland-shell-integration/*.so"
require 'SVG image plugin' "${appdir}/usr/plugins/imageformats/libqsvg.so"
require 'Qt Wayland client library' "${appdir}/usr/lib/libQt6WaylandClient.so.6*"

# Qt >= 6.5 links the xcb plugin against xcb-cursor, which many distributions
# do not install by default. Older Qt (distro 6.2) does not link it.
if ldd "${appdir}/usr/plugins/platforms/libqxcb.so" 2>/dev/null | grep -q 'libxcb-cursor'; then
  require 'bundled xcb-cursor library' "${appdir}/usr/lib/libxcb-cursor.so.0*"
fi

if [[ -n "${expected_qt}" ]]; then
  # linuxdeploy copies only the soname (libQt6Core.so.6); read the version
  # from Qt's build string instead ("Qt 6.8.2 (x86_64-little_endian...").
  bundled_qt="$(grep -aoE 'Qt 6\.[0-9]+\.[0-9]+ \(' "${appdir}/usr/lib/libQt6Core.so.6" 2>/dev/null \
    | head -n1 | sed -E 's/^Qt ([0-9.]+) \($/\1/' || true)"
  if [[ "${bundled_qt}" != "${expected_qt}" ]]; then
    printf 'AppImage bundles Qt %s, expected %s\n' "${bundled_qt:-unknown}" "${expected_qt}" >&2
    failures=$((failures + 1))
  fi
fi

if (( failures > 0 )); then
  exit 1
fi
printf 'AppImage Qt runtime verified%s.\n' "${expected_qt:+ (Qt ${expected_qt})}"
