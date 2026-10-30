#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${script_dir}/runtime-common.sh"

ffmpeg=0; ocr=0; desktop=0; integrate_appimage=0; languages=eng
while (( $# )); do
  case "$1" in
    --ffmpeg) ffmpeg=1;; --ocr) ocr=1;; --desktop) desktop=1;;
    --integrate-appimage) integrate_appimage=1;;
    --ocr-languages) shift; [[ $# -gt 0 ]] || exit 2; languages="$1";;
    *) eshot_show_error "$(eshot_setup_text unknown_option "$1")"; exit 2;;
  esac
  shift
done

# Menu integration does not depend on the packages; do it first so a
# cancelled or failed package install does not skip it.
integration_failed=0
if (( integrate_appimage )); then
  if [[ -z "${APPIMAGE:-}" || ! -x "${APPIMAGE}" ]]; then
    eshot_show_error "$(eshot_setup_text integration_unavailable)"
    integration_failed=1
  elif ! "${APPIMAGE}" --integrate-only; then
    integration_failed=1
  fi
fi

trap 'status=$?; printf "[EShot setup] exit status: %d\n" "${status}" >&2' EXIT
packages=()
requested=()
manager=""
if (( ffmpeg || ocr || desktop )); then
  manager="$(eshot_package_manager)" || {
    eshot_show_error "$(eshot_setup_text unsupported_manager)"
    exit 1
  }
  read -r -a packages <<<"$(eshot_missing_selected_packages "${manager}" "${ffmpeg}" "${ocr}" "${languages}" "${desktop}")"
  requested=("${packages[@]}")
  read -r -a packages <<<"$(eshot_available_packages "${manager}" "${packages[@]}")"
  for package in "${requested[@]}"; do
    [[ " ${packages[*]} " == *" ${package} "* ]] \
      || printf '[EShot setup] not in the enabled repositories, skipped: %s\n' "${package}" >&2
  done
fi

printf '[EShot setup] attempted packages: %s\n' "${packages[*]:-(none)}" >&2
if (( ${#packages[@]} > 0 )); then
  packagekit_installer="${script_dir}/packagekit-install.sh"
  if [[ -x "${packagekit_installer}" ]]; then
    if "${packagekit_installer}" "${packages[@]}"; then
      printf '[EShot setup] PackageKit session installer completed.\n' >&2
    else
      packagekit_status=$?
      if [[ "${packagekit_status}" -ne 2 ]]; then
        printf '[EShot setup] PackageKit session installer failed.\n' >&2
        exit "${packagekit_status}"
      fi
      if ! command -v pkexec >/dev/null 2>&1; then
        eshot_show_error "$(eshot_setup_text missing_pkexec)"
        exit 1
      fi
      case "${manager}" in
        pacman)
          pkexec pacman -S --needed --noconfirm "${packages[@]}"
          ;;
        apt)
          pkexec apt-get update
          pkexec apt-get install -y "${packages[@]}"
          ;;
        dnf)
          pkexec dnf install -y "${packages[@]}"
          ;;
      esac
    fi
  elif ! command -v pkexec >/dev/null 2>&1; then
    eshot_show_error "$(eshot_setup_text missing_pkexec)"
    exit 1
  else
    case "${manager}" in
      pacman)
        pkexec pacman -S --needed --noconfirm "${packages[@]}"
        ;;
      apt)
        pkexec apt-get update
        pkexec apt-get install -y "${packages[@]}"
        ;;
      dnf)
        pkexec dnf install -y "${packages[@]}"
        ;;
    esac
  fi
fi

if (( desktop )) && [[ "$(eshot_desktop_backend)" == "gnome" ]] \
   && command -v gnome-extensions >/dev/null 2>&1; then
  appindicator_uuid="appindicatorsupport@rgcjonas.gmail.com"
  if ! gnome-extensions enable "${appindicator_uuid}" >/dev/null 2>&1 \
     && command -v gsettings >/dev/null 2>&1; then
    # A running GNOME Shell does not know an extension installed after it
    # started, so "enable" fails. Add it to the enabled list for next login.
    enabled_extensions="$(gsettings get org.gnome.shell enabled-extensions 2>/dev/null || true)"
    if [[ -n "${enabled_extensions}" && "${enabled_extensions}" != *"'${appindicator_uuid}'"* ]]; then
      if [[ "${enabled_extensions}" == "@as []" || "${enabled_extensions}" == "[]" ]]; then
        enabled_extensions="['${appindicator_uuid}']"
      else
        enabled_extensions="${enabled_extensions%]}, '${appindicator_uuid}']"
      fi
      gsettings set org.gnome.shell enabled-extensions "${enabled_extensions}" >/dev/null 2>&1 || true
    fi
  fi
  # GNOME Shell on Wayland loads a newly installed extension only at the next
  # login. Tell EShot so it can ask the user to log out and back in.
  appindicator_installed=0
  for extension_dir in "${XDG_DATA_HOME:-${HOME}/.local/share}/gnome-shell/extensions" \
                       /usr/share/gnome-shell/extensions /usr/local/share/gnome-shell/extensions; do
    [[ -d "${extension_dir}/${appindicator_uuid}" ]] && appindicator_installed=1
  done
  appindicator_info="$(gnome-extensions info "${appindicator_uuid}" 2>/dev/null || true)"
  if (( appindicator_installed )) \
     && ! grep -Eq 'State: (ACTIVE|ENABLED)' <<<"${appindicator_info}"; then
    printf 'ESHOT_SETUP_RELOGIN_REQUIRED\n'
  fi
fi

exit "${integration_failed}"
