#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
source "${repo_root}/scripts/linux/runtime-common.sh"

failures=0

assert_eq() {
  local expected="$1"
  local actual="$2"
  local label="$3"
  if [[ "${expected}" != "${actual}" ]]; then
    printf 'FAIL %s: expected <%s>, got <%s>\n' "${label}" "${expected}" "${actual}" >&2
    failures=$((failures + 1))
  fi
}

assert_contains() {
  local needle="$1"
  local haystack="$2"
  local label="$3"
  if [[ " ${haystack} " != *" ${needle} "* ]]; then
    printf 'FAIL %s: <%s> not found in <%s>\n' "${label}" "${needle}" "${haystack}" >&2
    failures=$((failures + 1))
  fi
}

assert_not_contains() {
  local needle="$1"
  local haystack="$2"
  local label="$3"
  if [[ " ${haystack} " == *" ${needle} "* ]]; then
    printf 'FAIL %s: <%s> unexpectedly found in <%s>\n' "${label}" "${needle}" "${haystack}" >&2
    failures=$((failures + 1))
  fi
}

assert_eq "kde" "$(XDG_CURRENT_DESKTOP=KDE XDG_SESSION_DESKTOP= eshot_desktop_backend)" "KDE backend"
assert_eq "gnome" "$(XDG_CURRENT_DESKTOP=GNOME XDG_SESSION_DESKTOP= eshot_desktop_backend)" "GNOME backend"
assert_eq "kde" "$(XDG_CURRENT_DESKTOP= XDG_SESSION_DESKTOP=plasma eshot_desktop_backend)" "Plasma session backend"
assert_eq "gtk" "$(XDG_CURRENT_DESKTOP=sway XDG_SESSION_DESKTOP=sway eshot_desktop_backend)" "fallback backend"
assert_eq "1" "$(XDG_CURRENT_DESKTOP=KDE XDG_SESSION_TYPE=wayland ESHOT_CAPTURE_BACKEND= eshot_xwayland_overlay_enabled)" "KDE Wayland overlay"
assert_eq "0" "$(XDG_CURRENT_DESKTOP=KDE XDG_SESSION_TYPE=wayland ESHOT_CAPTURE_BACKEND= eshot_native_wayland_overlay_enabled)" "KDE native overlay is opt-in"
assert_eq "1" "$(XDG_CURRENT_DESKTOP=KDE XDG_SESSION_TYPE=wayland ESHOT_CAPTURE_BACKEND=wayland eshot_native_wayland_overlay_enabled)" "KDE native overlay opted in"
assert_eq "1" "$(XDG_CURRENT_DESKTOP=GNOME XDG_SESSION_TYPE=wayland eshot_xwayland_overlay_enabled)" "GNOME Wayland overlay"
assert_eq "0" "$(XDG_CURRENT_DESKTOP=GNOME XDG_SESSION_TYPE=x11 eshot_xwayland_overlay_enabled)" "GNOME X11 native"
assert_eq "0" "$(XDG_CURRENT_DESKTOP=Hyprland XDG_SESSION_TYPE=wayland eshot_xwayland_overlay_enabled)" "generic Wayland native"

overlay_env() {
  (
    export XDG_CURRENT_DESKTOP="$1" XDG_SESSION_TYPE="$2"
    export QT_QPA_PLATFORM=existing ESHOT_WAYLAND_XWAYLAND_OVERLAY=9
    export ESHOT_CAPTURE_BACKEND="${3:-}"
    eshot_configure_overlay_backend
    printf '%s/%s\n' "${QT_QPA_PLATFORM}" "${ESHOT_WAYLAND_XWAYLAND_OVERLAY}"
  )
}
assert_eq 'xcb;wayland/1' "$(overlay_env KDE wayland)" 'KDE packaged XWayland backend'
assert_eq 'wayland/0' "$(overlay_env KDE wayland wayland)" 'KDE opt-in native backend'
assert_eq 'xcb;wayland/1' "$(overlay_env GNOME wayland)" 'GNOME retains XWayland'
assert_eq 'xcb;wayland/1' "$(overlay_env Ubuntu wayland)" 'Ubuntu GNOME matches C++ policy'
assert_eq 'xcb;wayland/1' "$(overlay_env KDE wayland xcb)" 'Explicit KDE XWayland backend'
assert_eq 'xcb;wayland/1' "$(overlay_env GNOME wayland wayland)" 'GNOME ignores native opt-in'
assert_eq 'existing/9' "$(overlay_env KDE x11)" 'KDE X11 environment unchanged'
assert_eq 'existing/9' "$(overlay_env GNOME x11)" 'GNOME X11 environment unchanged'
assert_eq 'existing/9' "$(overlay_env sway wayland)" 'Unsupported desktop environment unchanged'

kde_pacman="$(XDG_CURRENT_DESKTOP=KDE eshot_runtime_packages pacman)"
gnome_pacman="$(XDG_CURRENT_DESKTOP=GNOME eshot_runtime_packages pacman)"
gnome_apt="$(XDG_CURRENT_DESKTOP=GNOME eshot_runtime_packages apt)"
kde_dnf="$(XDG_CURRENT_DESKTOP=KDE eshot_runtime_packages dnf)"

assert_contains "xdg-desktop-portal-kde" "${kde_pacman}" "KDE portal package"
assert_not_contains "xdg-desktop-portal-gnome" "${kde_pacman}" "KDE excludes GNOME portal"
assert_contains "xdg-desktop-portal-gnome" "${gnome_pacman}" "GNOME portal package"
assert_contains "gnome-shell-extension-appindicator" "${gnome_pacman}" "GNOME tray extension"
assert_not_contains "xdg-desktop-portal-kde" "${gnome_pacman}" "GNOME excludes KDE portal"
assert_not_contains "gnome-shell-extension-appindicator" "${kde_pacman}" "KDE excludes GNOME tray extension"
assert_contains "gst-plugin-pipewire" "${gnome_pacman}" "Arch PipeWire GStreamer package"
assert_contains "gst-libav" "${gnome_pacman}" "Arch AAC encoder package"
assert_contains "gstreamer1.0-pipewire" "${gnome_apt}" "Debian PipeWire GStreamer package"
assert_contains "gstreamer1.0-libav" "${gnome_apt}" "Debian AAC encoder package"
assert_contains "ffmpeg" "${kde_pacman}" "FFmpeg runtime"
assert_contains "tesseract" "${kde_pacman}" "Tesseract runtime"
assert_contains "pipewire-gstreamer" "${kde_dnf}" "Fedora PipeWire GStreamer package"
assert_contains "gstreamer1-plugins-ugly-free" "${kde_dnf}" "Fedora GStreamer ugly package"
assert_contains "xdg-desktop-portal-kde" "${kde_dnf}" "Fedora KDE portal package"

pacman_ffmpeg="$(eshot_selected_packages pacman 1 0 '')"
pacman_ocr="$(eshot_selected_packages pacman 0 1 'eng,tur,bogus')"
pacman_gnome_desktop="$(XDG_CURRENT_DESKTOP=GNOME eshot_selected_packages pacman 0 0 '' 1)"
apt_ocr="$(eshot_selected_packages apt 0 1 'eng,tur,bogus')"
dnf_ocr="$(eshot_selected_packages dnf 0 1 'eng,tur,bogus')"
assert_contains "ffmpeg" "${pacman_ffmpeg}" "pacman FFmpeg-only"
assert_not_contains "tesseract" "${pacman_ffmpeg}" "pacman FFmpeg-only excludes OCR"
assert_contains "tesseract-data-tur" "${pacman_ocr}" "pacman Turkish OCR"
assert_not_contains "ffmpeg" "${pacman_ocr}" "pacman OCR-only excludes FFmpeg"
assert_contains "gnome-shell-extension-appindicator" "${pacman_gnome_desktop}" "GNOME desktop setup tray extension"
assert_not_contains "tesseract-data-bogus" "${pacman_ocr}" "unsupported pacman OCR language"
assert_contains "tesseract-ocr-tur" "${apt_ocr}" "apt Turkish OCR"
assert_not_contains "tesseract-ocr-bogus" "${apt_ocr}" "unsupported apt OCR language"
assert_contains "tesseract-langpack-tur" "${dnf_ocr}" "dnf Turkish OCR"
assert_not_contains "tesseract-langpack-bogus" "${dnf_ocr}" "unsupported dnf OCR language"
apt_chinese="$(eshot_selected_packages apt 0 1 'chi_sim,chi_tra')"
assert_contains "tesseract-ocr-chi-sim" "${apt_chinese}" "apt simplified Chinese OCR"
assert_contains "tesseract-ocr-chi-tra" "${apt_chinese}" "apt traditional Chinese OCR"
assert_not_contains "tesseract-ocr-chi_sim" "${apt_chinese}" "apt package names use hyphens"

assert_eq "pacman" "$(ESHOT_PACKAGE_MANAGER=pacman eshot_package_manager)" "forced pacman"
assert_eq "apt" "$(ESHOT_PACKAGE_MANAGER=apt eshot_package_manager)" "forced apt"
assert_eq "dnf" "$(ESHOT_PACKAGE_MANAGER=dnf eshot_package_manager)" "forced dnf"
assert_eq "No supported package manager was found (pacman, apt or dnf)." \
  "$(ESHOT_LANGUAGE=en eshot_setup_text unsupported_manager)" "English setup error"
assert_eq "Desteklenen bir paket yoneticisi bulunamadi (pacman, apt veya dnf)." \
  "$(ESHOT_LANGUAGE=tr eshot_setup_text unsupported_manager)" "Turkish setup error"
assert_eq "No supported package manager was found (pacman, apt or dnf)." \
  "$(ESHOT_LANGUAGE=ru eshot_setup_text unsupported_manager)" "unsupported language falls back to English"
assert_eq "/home/test/.local/opt/EShot/EShot.AppImage" \
  "$(HOME=/home/test XDG_DATA_HOME= eshot_installed_appimage_path)" "default AppImage install path"
assert_eq "/home/test/share/applications/io.github.benoks.EShot.desktop" \
  "$(HOME=/home/test XDG_DATA_HOME=/home/test/share eshot_desktop_file_path)" "XDG desktop path"

missing_gnome="$(XDG_CURRENT_DESKTOP=GNOME ESHOT_INSTALLED_PACKAGES='ffmpeg tesseract' \
  eshot_missing_runtime_packages pacman)"
assert_contains "xdg-desktop-portal-gnome" "${missing_gnome}" "missing GNOME portal is detected"
assert_not_contains "ffmpeg" "${missing_gnome}" "installed FFmpeg is excluded"

if (( failures > 0 )); then
  exit 1
fi

printf 'runtime-common tests passed\n'
