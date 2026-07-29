#!/usr/bin/env bash

eshot_desktop_backend() {
  local desktop="${XDG_CURRENT_DESKTOP:-${XDG_SESSION_DESKTOP:-}}"
  desktop="${desktop,,}"
  case "${desktop}" in
    *kde*|*plasma*) printf 'kde\n' ;;
    *gnome*) printf 'gnome\n' ;;
    *) printf 'gtk\n' ;;
  esac
}

eshot_xwayland_overlay_enabled() {
  local backend
  backend="$(eshot_desktop_backend)"
  if [[ "${XDG_SESSION_TYPE:-}" == "wayland" ]] \
     && [[ "${backend}" == "kde" || "${backend}" == "gnome" ]]; then
    printf '1\n'
  else
    printf '0\n'
  fi
}

eshot_package_manager() {
  if [[ -n "${ESHOT_PACKAGE_MANAGER:-}" ]]; then
    printf '%s\n' "${ESHOT_PACKAGE_MANAGER}"
  elif command -v pacman >/dev/null 2>&1; then
    printf 'pacman\n'
  elif command -v dnf >/dev/null 2>&1; then
    printf 'dnf\n'
  elif command -v apt-get >/dev/null 2>&1; then
    printf 'apt\n'
  else
    printf 'unsupported\n'
    return 1
  fi
}

eshot_runtime_packages() {
  local manager="$1"
  local backend
  backend="$(eshot_desktop_backend)"

  case "${manager}" in
    pacman)
      local portal="xdg-desktop-portal-gtk"
      [[ "${backend}" == "kde" ]] && portal="xdg-desktop-portal-kde"
      [[ "${backend}" == "gnome" ]] && portal="xdg-desktop-portal-gnome"
      local tray=""
      [[ "${backend}" == "gnome" ]] && tray="gnome-shell-extension-appindicator"
      printf '%s\n' "ffmpeg tesseract tesseract-data-eng pipewire wireplumber gst-plugin-pipewire gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav libsecret xdg-desktop-portal ${portal} ${tray}"
      ;;
    apt)
      local portal="xdg-desktop-portal-gtk"
      [[ "${backend}" == "kde" ]] && portal="xdg-desktop-portal-kde"
      [[ "${backend}" == "gnome" ]] && portal="xdg-desktop-portal-gnome"
      printf '%s\n' "ffmpeg tesseract-ocr tesseract-ocr-eng pipewire wireplumber gstreamer1.0-tools gstreamer1.0-pipewire gstreamer1.0-pulseaudio gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav libsecret-1-0 xdg-desktop-portal ${portal}"
      ;;
    dnf)
      local portal="xdg-desktop-portal-gtk"
      [[ "${backend}" == "kde" ]] && portal="xdg-desktop-portal-kde"
      [[ "${backend}" == "gnome" ]] && portal="xdg-desktop-portal-gnome"
      printf '%s\n' "ffmpeg tesseract tesseract-langpack-eng pipewire wireplumber pipewire-gstreamer gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free gstreamer1-libav libsecret xdg-desktop-portal ${portal}"
      ;;
    *) return 1 ;;
  esac
}

eshot_supported_ocr_language() {
  case "$1" in eng|tur|deu|fra|spa|ita|por|rus|ukr|ara|chi_sim|chi_tra|jpn|kor|nld|pol) return 0;; esac
  return 1
}

eshot_selected_packages() {
  local manager="$1" ffmpeg="$2" ocr="$3" languages="${4:-}" desktop="${5:-0}"
  local packages=() code portal backend
  [[ "${ffmpeg}" == 1 ]] && packages+=(ffmpeg)
  if [[ "${ocr}" == 1 ]]; then
    case "${manager}" in
      pacman|dnf) packages+=(tesseract) ;;
      apt) packages+=(tesseract-ocr) ;;
    esac
    IFS=',' read -r -a codes <<<"${languages:-eng}"
    for code in "${codes[@]}"; do
      eshot_supported_ocr_language "${code}" || continue
      if [[ "${manager}" == pacman ]]; then
        packages+=("tesseract-data-${code}")
      elif [[ "${manager}" == dnf ]]; then
        packages+=("tesseract-langpack-${code}")
      else
        packages+=("tesseract-ocr-${code//_/-}")
      fi
    done
  fi
  if [[ "${desktop}" == 1 ]]; then
    backend="$(eshot_desktop_backend)"; portal="xdg-desktop-portal-gtk"
    [[ "${backend}" == kde ]] && portal="xdg-desktop-portal-kde"
    [[ "${backend}" == gnome ]] && portal="xdg-desktop-portal-gnome"
    if [[ "${manager}" == pacman ]]; then
      packages+=(pipewire wireplumber gst-plugin-pipewire gst-plugins-base gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav xdg-desktop-portal "${portal}")
      [[ "${backend}" == gnome ]] && packages+=(gnome-shell-extension-appindicator)
    elif [[ "${manager}" == dnf ]]; then
      packages+=(pipewire wireplumber pipewire-gstreamer gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free gstreamer1-libav xdg-desktop-portal "${portal}")
    else
      packages+=(pipewire wireplumber gstreamer1.0-tools gstreamer1.0-pipewire gstreamer1.0-pulseaudio gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav xdg-desktop-portal "${portal}")
    fi
  fi
  printf '%s\n' "${packages[*]}"
}

eshot_missing_selected_packages() {
  local manager="$1" package missing=(); shift
  local selected=(); read -r -a selected <<<"$(eshot_selected_packages "${manager}" "$@")"
  for package in "${selected[@]}"; do eshot_package_installed "${manager}" "${package}" || missing+=("${package}"); done
  printf '%s\n' "${missing[*]}"
}

eshot_package_installed() {
  local manager="$1"
  local package="$2"
  if [[ -n "${ESHOT_INSTALLED_PACKAGES:-}" ]]; then
    [[ " ${ESHOT_INSTALLED_PACKAGES} " == *" ${package} "* ]]
    return
  fi

  case "${manager}" in
    pacman) pacman -Q "${package}" >/dev/null 2>&1 ;;
    apt) dpkg-query -W -f='${Status}' "${package}" 2>/dev/null | grep -q 'install ok installed' ;;
    dnf) rpm -q "${package}" >/dev/null 2>&1 ;;
    *) return 1 ;;
  esac
}

eshot_missing_runtime_packages() {
  local manager="$1"
  local package
  local missing=()
  local required=()
  read -r -a required <<<"$(eshot_runtime_packages "${manager}")"
  for package in "${required[@]}"; do
    if ! eshot_package_installed "${manager}" "${package}"; then
      missing+=("${package}")
    fi
  done
  printf '%s\n' "${missing[*]}"
}

eshot_runtime_ready() {
  local command_name
  for command_name in ffmpeg tesseract gst-launch-1.0 gst-inspect-1.0; do
    command -v "${command_name}" >/dev/null 2>&1 || return 1
  done

  local plugin
  for plugin in pipewiresrc pulsesrc x264enc h264parse mp4mux; do
    gst-inspect-1.0 "${plugin}" >/dev/null 2>&1 || return 1
  done
  if ! gst-inspect-1.0 fdkaacenc >/dev/null 2>&1 \
     && ! gst-inspect-1.0 avenc_aac >/dev/null 2>&1 \
     && ! gst-inspect-1.0 faac >/dev/null 2>&1 \
     && ! gst-inspect-1.0 voaacenc >/dev/null 2>&1; then
    return 1
  fi
  return 0
}

eshot_installed_appimage_path() {
  printf '%s/.local/opt/EShot/EShot.AppImage\n' "${HOME}"
}

eshot_desktop_file_path() {
  local data_home="${XDG_DATA_HOME:-${HOME}/.local/share}"
  printf '%s/applications/io.github.benoks.EShot.desktop\n' "${data_home}"
}

eshot_show_error() {
  local message="$1"
  if command -v kdialog >/dev/null 2>&1; then
    kdialog --error "${message}" --title "EShot"
  elif command -v zenity >/dev/null 2>&1; then
    zenity --error --title="EShot" --text="${message}"
  else
    printf 'EShot: %s\n' "${message}" >&2
  fi
}

eshot_setup_text() {
  local key="$1"
  shift || true
  local language="${ESHOT_LANGUAGE:-en}"
  language="${language,,}"

  if [[ "${language}" == tr* ]]; then
    case "${key}" in
      unknown_option) printf 'Bilinmeyen secenek: %s\n' "${1:-}" ;;
      unsupported_manager) printf 'Desteklenen bir paket yoneticisi bulunamadi (pacman, apt veya dnf).\n' ;;
      missing_pkexec) printf 'Paket kurulumu icin PolicyKit (pkexec) bulunamadi.\n' ;;
      integration_unavailable) printf 'AppImage masaustu entegrasyonu kullanilamiyor.\n' ;;
      *) printf '%s\n' "${key}" ;;
    esac
    return
  fi

  case "${key}" in
    unknown_option) printf 'Unknown option: %s\n' "${1:-}" ;;
    unsupported_manager) printf 'No supported package manager was found (pacman, apt or dnf).\n' ;;
    missing_pkexec) printf 'PolicyKit (pkexec) is required to install packages.\n' ;;
    integration_unavailable) printf 'AppImage desktop integration is unavailable.\n' ;;
    *) printf '%s\n' "${key}" ;;
  esac
}
