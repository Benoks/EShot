#!/usr/bin/env bash
set -euo pipefail

missing=0

check_cmd() {
  local name="$1"
  local cmd="$2"
  if command -v "${cmd}" >/dev/null 2>&1; then
    printf '[ok] %s: %s\n' "${name}" "$(command -v "${cmd}")"
  else
    printf '[missing] %s: %s\n' "${name}" "${cmd}"
    missing=1
  fi
}

check_gst() {
  local plugin="$1"
  if gst-inspect-1.0 "${plugin}" >/dev/null 2>&1; then
    printf '[ok] GStreamer plugin: %s\n' "${plugin}"
  else
    printf '[missing] GStreamer plugin: %s\n' "${plugin}"
    missing=1
  fi
}

check_qt_wayland() {
  local plugin_dir=""
  if command -v qtpaths6 >/dev/null 2>&1; then
    plugin_dir="$(qtpaths6 --plugin-dir 2>/dev/null || true)"
  elif command -v qtpaths >/dev/null 2>&1; then
    plugin_dir="$(qtpaths --plugin-dir 2>/dev/null || true)"
  fi

  if [[ -n "${plugin_dir}" && -d "${plugin_dir}/platforms" ]] &&
     compgen -G "${plugin_dir}/platforms/libqwayland*.so" >/dev/null; then
    printf '[ok] Qt Wayland platform plugin: %s/platforms\n' "${plugin_dir}"
  else
    printf '[warn] Qt Wayland platform plugin not found; install qt6-wayland for native Wayland sessions\n'
  fi
}

check_cmd "CMake" cmake
check_cmd "Ninja" ninja
check_cmd "Download client" curl
check_cmd "FFmpeg" ffmpeg
check_cmd "Tesseract" tesseract
check_cmd "GStreamer launcher" gst-launch-1.0
check_cmd "GStreamer inspector" gst-inspect-1.0
check_cmd "Desktop file validator" desktop-file-validate
check_cmd "AppStream validator" appstreamcli
check_qt_wayland

check_gst pipewiresrc
check_gst pulsesrc
check_gst x264enc
check_gst h264parse
check_gst mp4mux
if gst-inspect-1.0 fdkaacenc >/dev/null 2>&1 \
   || gst-inspect-1.0 avenc_aac >/dev/null 2>&1 \
   || gst-inspect-1.0 faac >/dev/null 2>&1 \
   || gst-inspect-1.0 voaacenc >/dev/null 2>&1; then
    printf '[ok] GStreamer AAC encoder\n'
else
    printf '[missing] GStreamer AAC encoder\n'
    missing=1
fi

portal_xml=""
if command -v qdbus6 >/dev/null 2>&1; then
  portal_xml="$(qdbus6 org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop 2>/dev/null || true)"
elif command -v qdbus >/dev/null 2>&1; then
  portal_xml="$(qdbus org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop 2>/dev/null || true)"
elif command -v busctl >/dev/null 2>&1; then
  portal_xml="$(busctl --user introspect org.freedesktop.portal.Desktop /org/freedesktop/portal/desktop 2>/dev/null || true)"
else
  printf '[warn] qdbus6/qdbus/busctl not found; portal bus check skipped\n'
fi

if [[ -n "${portal_xml}" ]]; then
  printf '[ok] xdg-desktop-portal session bus service\n'
  for iface in Screenshot ScreenCast GlobalShortcuts; do
    if grep -q "org.freedesktop.portal.${iface}" <<<"${portal_xml}"; then
      printf '[ok] portal interface: %s\n' "${iface}"
    else
      printf '[warn] portal interface not visible: %s\n' "${iface}"
    fi
  done
else
  printf '[warn] xdg-desktop-portal service was not reachable\n'
fi

printf 'Qt platform session: %s\n' "${XDG_SESSION_TYPE:-unknown}"

exit "${missing}"
