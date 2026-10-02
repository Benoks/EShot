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
  gnome-extensions enable appindicatorsupport@rgcjonas.gmail.com >/dev/null 2>&1 || true
fi

if (( integrate_appimage )); then
  if [[ -z "${APPIMAGE:-}" || ! -x "${APPIMAGE}" ]]; then
    eshot_show_error "$(eshot_setup_text integration_unavailable)"
    exit 1
  fi
  "${APPIMAGE}" --integrate-only
fi
