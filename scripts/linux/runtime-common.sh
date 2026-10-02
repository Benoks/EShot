#!/usr/bin/env bash

eshot_desktop_backend() {
  local desktop="${XDG_CURRENT_DESKTOP:-${XDG_SESSION_DESKTOP:-}}"
  desktop="${desktop,,}"
  case "${desktop}" in
    *kde*|*plasma*) printf 'kde\n' ;;
    *gnome*|*ubuntu*) printf 'gnome\n' ;;
    *) printf 'gtk\n' ;;
  esac
}

# KDE and GNOME Wayland use one XWayland canvas across every output. Native
# Wayland cannot place top-level windows, so pinned images and the recording
# frame would open at compositor-chosen positions. KDE's per-output native
# surfaces (sharper previews on mixed-DPI setups) are therefore opt-in with
# ESHOT_CAPTURE_BACKEND=wayland.
eshot_xwayland_overlay_enabled() {
  local backend session="${XDG_SESSION_TYPE:-}"
  backend="$(eshot_desktop_backend)"
  if [[ "${session,,}" == "wayland" ]] \
     && [[ "${backend}" == "gnome" \
           || ( "${backend}" == "kde" && "${ESHOT_CAPTURE_BACKEND:-}" != "wayland" ) ]]; then
    printf '1\n'
  else
    printf '0\n'
  fi
}

eshot_native_wayland_overlay_enabled() {
  local session="${XDG_SESSION_TYPE:-}"
  if [[ "${session,,}" == "wayland" ]] \
     && [[ "$(eshot_desktop_backend)" == "kde" ]] \
     && [[ "${ESHOT_CAPTURE_BACKEND:-}" == "wayland" ]]; then
    printf '1\n'
  else
    printf '0\n'
  fi
}

eshot_configure_overlay_backend() {
  if [[ "$(eshot_native_wayland_overlay_enabled)" == "1" ]]; then
    export QT_QPA_PLATFORM=wayland
    export ESHOT_WAYLAND_XWAYLAND_OVERLAY=0
  elif [[ "$(eshot_xwayland_overlay_enabled)" == "1" ]]; then
    export QT_QPA_PLATFORM='xcb;wayland'
    export ESHOT_WAYLAND_XWAYLAND_OVERLAY=1
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
      # Fedora ships ffmpeg-free and the libav plugin itself; x264enc (MP4
      # recording) is only in RPM Fusion's gstreamer1-plugins-ugly.
      printf '%s\n' "ffmpeg-free tesseract tesseract-langpack-eng pipewire wireplumber pipewire-gstreamer gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free gstreamer1-plugins-ugly gstreamer1-plugin-libav libsecret xdg-desktop-portal ${portal}"
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
  if [[ "${ffmpeg}" == 1 ]]; then
    # Fedora's own repositories have ffmpeg-free; RPM Fusion's ffmpeg
    # conflicts with it, so keep whichever ffmpeg is already installed.
    if [[ "${manager}" != dnf ]]; then
      packages+=(ffmpeg)
    elif ! command -v ffmpeg >/dev/null 2>&1; then
      packages+=(ffmpeg-free)
    fi
  fi
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
      packages+=(pipewire wireplumber pipewire-gstreamer gstreamer1-plugins-base gstreamer1-plugins-good gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free gstreamer1-plugins-ugly gstreamer1-plugin-libav xdg-desktop-portal "${portal}")
    else
      packages+=(pipewire wireplumber gstreamer1.0-tools gstreamer1.0-pipewire gstreamer1.0-pulseaudio gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav xdg-desktop-portal "${portal}")
    fi
    # Stock GNOME has no tray; EShot enables this extension after setup.
    if [[ "${manager}" != pacman && "${backend}" == gnome ]]; then
      packages+=(gnome-shell-extension-appindicator)
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

# Prints the packages the configured repositories actually provide. dnf and
# PackageKit reject a whole request when one name is unknown, for example
# RPM Fusion packages on a stock Fedora install.
eshot_available_packages() {
  local manager="$1"; shift
  (( $# )) || return 0
  if [[ -n "${ESHOT_AVAILABLE_PACKAGES:-}" ]]; then
    local package available=()
    for package in "$@"; do
      [[ " ${ESHOT_AVAILABLE_PACKAGES} " == *" ${package} "* ]] && available+=("${package}")
    done
    printf '%s\n' "${available[*]}"
    return
  fi
  if [[ "${manager}" == dnf ]] && command -v dnf >/dev/null 2>&1; then
    local names
    if names="$(dnf -q repoquery --qf '%{name}\n' "$@" 2>/dev/null)" && [[ -n "${names}" ]]; then
      local package available=()
      for package in "$@"; do
        grep -qxF "${package}" <<<"${names}" && available+=("${package}")
      done
      printf '%s\n' "${available[*]}"
      return
    fi
  fi
  printf '%s\n' "$*"
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

  # UTF-8 text; kdialog, zenity and terminals render it directly.
  local unknown_option unsupported_manager missing_pkexec integration_unavailable
  case "${language}" in
    tr*)
      unknown_option='Bilinmeyen seçenek: '
      unsupported_manager='Desteklenen bir paket yöneticisi bulunamadı (pacman, apt veya dnf).'
      missing_pkexec='Paket kurulumu için PolicyKit (pkexec) bulunamadı.'
      integration_unavailable='AppImage masaüstü entegrasyonu kullanılamıyor.'
      ;;
    de*)
      unknown_option='Unbekannte Option: '
      unsupported_manager='Kein unterstützter Paketmanager gefunden (pacman, apt oder dnf).'
      missing_pkexec='Zum Installieren von Paketen wird PolicyKit (pkexec) benötigt.'
      integration_unavailable='AppImage-Desktopintegration ist nicht verfügbar.'
      ;;
    fr*)
      unknown_option='Option inconnue : '
      unsupported_manager='Aucun gestionnaire de paquets pris en charge n’a été trouvé (pacman, apt ou dnf).'
      missing_pkexec='PolicyKit (pkexec) est requis pour installer des paquets.'
      integration_unavailable='L’intégration de l’AppImage au bureau n’est pas disponible.'
      ;;
    es*)
      unknown_option='Opción desconocida: '
      unsupported_manager='No se encontró un gestor de paquetes compatible (pacman, apt o dnf).'
      missing_pkexec='Se necesita PolicyKit (pkexec) para instalar paquetes.'
      integration_unavailable='La integración del AppImage con el escritorio no está disponible.'
      ;;
    ja*)
      unknown_option='不明なオプション: '
      unsupported_manager='対応するパッケージマネージャーが見つかりません（pacman、apt、dnf）。'
      missing_pkexec='パッケージのインストールには PolicyKit (pkexec) が必要です。'
      integration_unavailable='AppImage のデスクトップ統合は利用できません。'
      ;;
    zh*)
      unknown_option='未知选项：'
      unsupported_manager='未找到受支持的包管理器（pacman、apt 或 dnf）。'
      missing_pkexec='安装软件包需要 PolicyKit (pkexec)。'
      integration_unavailable='AppImage 桌面集成不可用。'
      ;;
    ru*)
      unknown_option='Неизвестный параметр: '
      unsupported_manager='Не найден поддерживаемый менеджер пакетов (pacman, apt или dnf).'
      missing_pkexec='Для установки пакетов требуется PolicyKit (pkexec).'
      integration_unavailable='Интеграция AppImage с рабочим столом недоступна.'
      ;;
    *)
      unknown_option='Unknown option: '
      unsupported_manager='No supported package manager was found (pacman, apt or dnf).'
      missing_pkexec='PolicyKit (pkexec) is required to install packages.'
      integration_unavailable='AppImage desktop integration is unavailable.'
      ;;
  esac

  case "${key}" in
    unknown_option) printf '%s%s\n' "${unknown_option}" "${1:-}" ;;
    unsupported_manager) printf '%s\n' "${unsupported_manager}" ;;
    missing_pkexec) printf '%s\n' "${missing_pkexec}" ;;
    integration_unavailable) printf '%s\n' "${integration_unavailable}" ;;
    *) printf '%s\n' "${key}" ;;
  esac
}
