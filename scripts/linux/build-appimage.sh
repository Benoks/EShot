#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_dir="${ESHOT_APPIMAGE_BUILD_DIR:-${repo_root}/build-appimage}"
appdir="${ESHOT_APPIMAGE_APPDIR:-${repo_root}/EShot.AppDir}"
tools_dir="${repo_root}/build-appimage-tools"
package_dir="${repo_root}/packages"
version="$(sed -n 's/^project(EShot VERSION \([0-9][0-9.]*\).*/\1/p' "${repo_root}/CMakeLists.txt" | head -n1)"
output="${package_dir}/EShot-v${version}-x86_64.AppImage"

if [[ "$(uname -m)" != "x86_64" ]]; then
  printf 'EShot AppImage build currently targets x86_64 only.\n' >&2
  exit 1
fi

linuxdeploy_url="https://github.com/linuxdeploy/linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage"
linuxdeploy_sha256="c20cd71e3a4e3b80c3483cef793cda3f4e990aca14014d23c544ca3ce1270b4d"
qt_plugin_url="https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage"
qt_plugin_sha256="15106be885c1c48a021198e7e1e9a48ce9d02a86dd0a1848f00bdbf3c1c92724"
appimagetool_url="https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage"
appimagetool_sha256="a6d71e2b6cd66f8e8d16c37ad164658985e0cf5fcaa950c90a482890cb9d13e0"
# Pinned to a tagged release: the "continuous" asset is replaced upstream
# without notice, which breaks the checksum and every release build.
runtime_url="https://github.com/AppImage/type2-runtime/releases/download/20251108/runtime-x86_64"
runtime_sha256="2fca8b443c92510f1483a883f60061ad09b46b978b2631c807cd873a47ec260d"

download_verified() {
  local url="$1"
  local expected="$2"
  local destination="$3"
  if [[ ! -f "${destination}" ]] || ! printf '%s  %s\n' "${expected}" "${destination}" | sha256sum -c - >/dev/null 2>&1; then
    rm -f -- "${destination}"
    curl --fail --location --retry 3 --output "${destination}" "${url}"
  fi
  printf '%s  %s\n' "${expected}" "${destination}" | sha256sum -c -
  chmod 0755 "${destination}"
}

mkdir -p "${tools_dir}" "${package_dir}"
linuxdeploy="${tools_dir}/linuxdeploy-x86_64.AppImage"
qt_plugin_image="${tools_dir}/linuxdeploy-plugin-qt-x86_64.AppImage"
qt_plugin="${tools_dir}/linuxdeploy-plugin-qt"
appimagetool="${tools_dir}/appimagetool-x86_64.AppImage"
runtime_file="${tools_dir}/runtime-x86_64"
qmake_wrapper="${tools_dir}/qmake-appimage-wrapper"
qt_plugin_dir="${build_dir}/qt-plugins"
system_qt_plugin_dir="$(bash "${repo_root}/scripts/linux/qt-plugin-dir.sh")"
[[ -d "${system_qt_plugin_dir}" ]] || { printf 'Qt plugin directory not found: %s\n' "${system_qt_plugin_dir}" >&2; exit 1; }

download_verified "${linuxdeploy_url}" "${linuxdeploy_sha256}" "${linuxdeploy}"
download_verified "${qt_plugin_url}" "${qt_plugin_sha256}" "${qt_plugin_image}"
download_verified "${appimagetool_url}" "${appimagetool_sha256}" "${appimagetool}"
download_verified "${runtime_url}" "${runtime_sha256}" "${runtime_file}"
cp -f -- "${qt_plugin_image}" "${qt_plugin}"
chmod 0755 "${qt_plugin}"
install -Dm755 "${repo_root}/scripts/linux/qmake-appimage-wrapper.sh" "${qmake_wrapper}"

rm -rf "${appdir}"
cmake -S "${repo_root}" -B "${build_dir}" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DBUILD_TESTING=OFF
cmake --build "${build_dir}" --parallel
DESTDIR="${appdir}" cmake --install "${build_dir}"

# Do not let linuxdeploy scan every optional Qt/KDE image plugin installed on
# the build host. Some of those plugins can have unrelated, stale system
# dependencies (for example kimg_heif). EShot only needs this curated runtime
# set.
rm -rf "${qt_plugin_dir}"
mkdir -p "${qt_plugin_dir}"
for category in platforms imageformats platforminputcontexts tls networkinformation; do
  mkdir -p "${qt_plugin_dir}/${category}"
done
install -Dm755 "${system_qt_plugin_dir}/platforms/libqxcb.so" \
  "${qt_plugin_dir}/platforms/libqxcb.so"
shopt -s nullglob
wayland_platform_plugins=("${system_qt_plugin_dir}"/platforms/libqwayland*.so)
shopt -u nullglob
if (( ${#wayland_platform_plugins[@]} == 0 )); then
  printf 'Qt Wayland platform plugin not found under %s. Install qt6-wayland.\n' \
    "${system_qt_plugin_dir}/platforms" >&2
  exit 1
fi
for plugin in "${wayland_platform_plugins[@]}"; do
  install -Dm755 "${plugin}" "${qt_plugin_dir}/platforms/$(basename "${plugin}")"
done
for plugin in libqgif.so libqico.so libqjpeg.so libqsvg.so; do
  install -Dm755 "${system_qt_plugin_dir}/imageformats/${plugin}" \
    "${qt_plugin_dir}/imageformats/${plugin}"
done
for plugin in "${system_qt_plugin_dir}"/platforminputcontexts/libcomposeplatforminputcontextplugin.so \
              "${system_qt_plugin_dir}"/tls/*.so \
              "${system_qt_plugin_dir}"/networkinformation/*.so; do
  [[ -e "${plugin}" ]] || continue
  relative_plugin="${plugin#"${system_qt_plugin_dir}"/}"
  install -Dm755 "${plugin}" "${qt_plugin_dir}/${relative_plugin}"
done

export APPIMAGE_EXTRACT_AND_RUN=1
export NO_STRIP=1
export ESHOT_REAL_QMAKE="$(command -v qmake6)"
export ESHOT_QT_PLUGIN_DIR="${qt_plugin_dir}"
export QMAKE="${qmake_wrapper}"
export PATH="${tools_dir}:${PATH}"
"${linuxdeploy}" --appdir "${appdir}" --plugin qt

# linuxdeploy-plugin-qt deploys the platform used by the build process (xcb),
# but EShot deliberately falls back to native Wayland when XWayland is absent.
# Copy the Qt Wayland client plugins explicitly, then let linuxdeploy resolve
# their shared-library dependencies. Server/compositor plugins are not needed.
wayland_dependency_args=()
for category in wayland-decoration-client \
                wayland-graphics-integration-client \
                wayland-shell-integration; do
  shopt -s nullglob
  category_plugins=("${system_qt_plugin_dir}/${category}"/*.so)
  shopt -u nullglob
  for plugin in "${category_plugins[@]}"; do
    destination="${appdir}/usr/plugins/${category}/$(basename "${plugin}")"
    install -Dm755 "${plugin}" "${destination}"
    wayland_dependency_args+=(--deploy-deps-only "${destination}")
  done
done
for plugin in "${wayland_platform_plugins[@]}"; do
  destination="${appdir}/usr/plugins/platforms/$(basename "${plugin}")"
  install -Dm755 "${plugin}" "${destination}"
  wayland_dependency_args+=(--deploy-deps-only "${destination}")
done
"${linuxdeploy}" --appdir "${appdir}" \
  "${wayland_dependency_args[@]}"
install -Dm755 "${repo_root}/packaging/linux/AppRun" "${appdir}/AppRun"

rm -f -- "${package_dir}"/EShot-v*-x86_64.AppImage
ARCH=x86_64 "${appimagetool}" --runtime-file "${runtime_file}" "${appdir}" "${output}"
chmod 0755 "${output}"
test -s "${output}"
bash "${repo_root}/scripts/linux/verify-appimage-portability.sh" "${output}"

printf '\nBuilt EShot AppImage:\n  %s\n' "${output}"
