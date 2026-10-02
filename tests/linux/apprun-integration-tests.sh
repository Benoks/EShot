#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
temp_root="$(mktemp -d)"
trap 'rm -rf "${temp_root}"' EXIT

home_dir="${temp_root}/home user"
appdir="${temp_root}/EShot.AppDir"
source_appimage="${temp_root}/Downloaded EShot.AppImage"
data_home="${home_dir}/share"

mkdir -p "${home_dir}" \
  "${appdir}/usr/bin" \
  "${appdir}/usr/lib/eshot" \
  "${appdir}/usr/share/applications" \
  "${appdir}/usr/share/icons/hicolor/scalable/apps"
printf 'fake-appimage\n' >"${source_appimage}"
printf '#!/usr/bin/env bash\nexit 0\n' >"${appdir}/usr/bin/EShot"
chmod +x "${appdir}/usr/bin/EShot"
cp "${repo_root}/scripts/linux/runtime-common.sh" "${appdir}/usr/lib/eshot/runtime-common.sh"
cp "${repo_root}/scripts/linux/install-runtime-deps.sh" "${appdir}/usr/lib/eshot/install-runtime-deps.sh"
# AppRun must reach the GUI without invoking the dependency installer.
printf '#!/usr/bin/env bash\necho invoked >"${ESHOT_INSTALL_LOG}"\nexit 99\n' >"${appdir}/usr/lib/eshot/install-runtime-deps.sh"
chmod +x "${appdir}/usr/lib/eshot/install-runtime-deps.sh"
cp "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" \
  "${appdir}/usr/share/applications/io.github.benoks.EShot.desktop"
cp "${repo_root}/packaging/linux/io.github.benoks.EShot.svg" \
  "${appdir}/usr/share/icons/hicolor/scalable/apps/io.github.benoks.EShot.svg"
cp "${repo_root}/packaging/linux/io.github.benoks.EShot.svg" \
  "${appdir}/usr/share/icons/hicolor/scalable/apps/io.github.benoks.EShot-v4.svg"

# A normal first launch must run the application without silently installing
# desktop files or copying the AppImage.
normal_log="${temp_root}/normal-launch.log"
printf '#!/usr/bin/env bash\nprintf "%%s\\n" "$*" >"${ESHOT_NORMAL_LOG}"\n' >"${appdir}/usr/bin/EShot"
chmod +x "${appdir}/usr/bin/EShot"
HOME="${home_dir}" XDG_DATA_HOME="${data_home}" APPDIR="${appdir}" \
APPIMAGE="${source_appimage}" ESHOT_DESKTOP_LAUNCH=1 ESHOT_SYSTEMD_CHILD=1 \
ESHOT_NORMAL_LOG="${normal_log}" bash "${repo_root}/packaging/linux/AppRun"
[[ -f "${normal_log}" ]] || { echo "normal AppRun launch did not reach GUI" >&2; exit 1; }
[[ ! -e "${home_dir}/.local/opt/EShot/EShot.AppImage" ]] || { echo "normal launch copied AppImage" >&2; exit 1; }
[[ ! -e "${data_home}/applications/io.github.benoks.EShot.desktop" ]] || { echo "normal launch installed desktop entry" >&2; exit 1; }

HOME="${home_dir}" \
XDG_DATA_HOME="${data_home}" \
APPDIR="${appdir}" \
APPIMAGE="${source_appimage}" \
ESHOT_SKIP_RUNTIME_SETUP=1 \
bash "${repo_root}/packaging/linux/AppRun" --integrate-only

installed="${home_dir}/.local/opt/EShot/EShot.AppImage"
desktop="${data_home}/applications/io.github.benoks.EShot.desktop"
icon="${data_home}/icons/hicolor/scalable/apps/io.github.benoks.EShot.svg"
versioned_icon="${data_home}/icons/hicolor/scalable/apps/io.github.benoks.EShot-v4.svg"
pixmap="${data_home}/pixmaps/io.github.benoks.EShot-v4.svg"

[[ -f "${installed}" ]] || { echo "installed AppImage missing" >&2; exit 1; }
cmp -s "${source_appimage}" "${installed}" || { echo "installed AppImage differs" >&2; exit 1; }
if [[ "$(uname -s)" != MINGW* ]]; then
  [[ -x "${installed}" ]] || { echo "installed AppImage is not executable" >&2; exit 1; }
fi
[[ -f "${desktop}" ]] || { echo "desktop entry missing" >&2; exit 1; }
[[ -f "${icon}" ]] || { echo "icon missing" >&2; exit 1; }
[[ -f "${versioned_icon}" ]] || { echo "versioned icon missing" >&2; exit 1; }
[[ -f "${pixmap}" ]] || { echo "versioned pixmap missing" >&2; exit 1; }
cmp -s "${icon}" "${versioned_icon}" || { echo "icon compatibility copy differs" >&2; exit 1; }
grep -F "Exec=/usr/bin/env ESHOT_DESKTOP_LAUNCH=1 \"${installed}\"" "${desktop}" >/dev/null || {
  echo "desktop Exec does not point to installed AppImage" >&2
  exit 1
}
grep -F "Exec=/usr/bin/env ESHOT_DESKTOP_LAUNCH=1 \"${installed}\" --capture" "${desktop}" >/dev/null
grep -F "Exec=/usr/bin/env ESHOT_DESKTOP_LAUNCH=1 \"${installed}\" --settings" "${desktop}" >/dev/null
grep -F "Exec=/usr/bin/env ESHOT_DESKTOP_LAUNCH=1 \"${installed}\" --quit" "${desktop}" >/dev/null

# Manually launching a newly downloaded AppImage must execute that current
# image, even when an older integrated copy already exists.
manual_log="${temp_root}/manual-current.log"
printf '#!/usr/bin/env bash\nprintf "current-image\\n" >"${ESHOT_MANUAL_LOG}"\n' >"${appdir}/usr/bin/EShot"
chmod +x "${appdir}/usr/bin/EShot"
HOME="${home_dir}" XDG_DATA_HOME="${data_home}" APPDIR="${appdir}" \
APPIMAGE="${source_appimage}" ESHOT_SYSTEMD_CHILD=1 \
ESHOT_MANUAL_LOG="${manual_log}" bash "${repo_root}/packaging/linux/AppRun"
grep -F 'current-image' "${manual_log}" >/dev/null || {
  echo "manual launch redirected to the integrated AppImage" >&2
  exit 1
}

# A desktop-entry launch already targets the integrated image directly; AppRun
# must execute its own payload instead of bouncing through the desktop service.
desktop_launch_log="${temp_root}/desktop-current.log"
HOME="${home_dir}" \
XDG_DATA_HOME="${data_home}" \
XDG_CURRENT_DESKTOP=KDE \
APPDIR="${appdir}" \
APPIMAGE="${installed}" \
ESHOT_DESKTOP_LAUNCH=1 \
ESHOT_SYSTEMD_CHILD=1 \
ESHOT_MANUAL_LOG="${desktop_launch_log}" \
ESHOT_INSTALL_LOG="${temp_root}/install.log" \
bash "${repo_root}/packaging/linux/AppRun"

[[ ! -e "${temp_root}/install.log" ]] || { echo "AppRun invoked dependency installer before GUI" >&2; exit 1; }
grep -F 'current-image' "${desktop_launch_log}" >/dev/null || {
  echo "desktop launch did not execute the integrated image payload" >&2
  exit 1
}

# Exercise the packaged entry point, not just the shell policy in isolation.
backend_log="${temp_root}/backend.log"
cat >"${appdir}/usr/bin/EShot" <<'SH'
#!/usr/bin/env bash
printf '%s/%s\n' "${QT_QPA_PLATFORM:-}" "${ESHOT_WAYLAND_XWAYLAND_OVERLAY:-}" >"${ESHOT_BACKEND_LOG}"
SH
chmod +x "${appdir}/usr/bin/EShot"
for row in 'KDE wayland wayland/0' 'GNOME wayland xcb;wayland/1' \
           'KDE x11 existing/9' 'GNOME x11 existing/9' 'sway wayland existing/9'; do
  read -r desktop_name session_type expected <<<"${row}"
  HOME="${home_dir}" XDG_DATA_HOME="${data_home}" APPDIR="${appdir}" \
    APPIMAGE="${source_appimage}" XDG_CURRENT_DESKTOP="${desktop_name}" \
    XDG_SESSION_TYPE="${session_type}" ESHOT_CAPTURE_BACKEND= \
    QT_QPA_PLATFORM=existing ESHOT_WAYLAND_XWAYLAND_OVERLAY=9 \
    ESHOT_BACKEND_LOG="${backend_log}" bash "${repo_root}/packaging/linux/AppRun" --silent
  [[ "$(cat "${backend_log}")" == "${expected}" ]] || {
    echo "AppRun selected wrong backend for ${desktop_name} ${session_type}" >&2
    exit 1
  }
done

printf 'AppRun integration tests passed\n'
