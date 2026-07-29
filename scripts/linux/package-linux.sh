#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dist_dir="${repo_root}/dist-linux"
package_dir="${repo_root}/packages"
version="$(sed -n 's/^project(EShot VERSION \([0-9][0-9.]*\).*/\1/p' "${repo_root}/CMakeLists.txt" | head -n1)"
version="${version:-0.0.0}"
tar_path="${package_dir}/EShot-v${version}-linux-x64.tar.gz"
deb_root="${package_dir}/deb/eshot_${version}_amd64"
deb_path="${package_dir}/eshot_${version}_amd64.deb"

if [[ ! -x "${dist_dir}/bin/EShot" ]]; then
  "${repo_root}/scripts/linux/build-linux.sh"
fi

mkdir -p "${package_dir}"
tar -C "${dist_dir}" -czf "${tar_path}" .

if command -v dpkg-deb >/dev/null 2>&1; then
  rm -rf "${deb_root}"
  install -Dm755 "${dist_dir}/bin/EShot" "${deb_root}/usr/bin/EShot"
  install -Dm755 "${dist_dir}/bin/eshot-launcher" "${deb_root}/usr/bin/eshot-launcher"
  ln -s eshot-launcher "${deb_root}/usr/bin/eshot"
  install -Dm644 "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" \
    "${deb_root}/usr/share/applications/io.github.benoks.EShot.desktop"
  install -Dm644 "${repo_root}/packaging/linux/io.github.benoks.EShot.svg" \
    "${deb_root}/usr/share/icons/hicolor/scalable/apps/io.github.benoks.EShot.svg"
  install -Dm644 "${repo_root}/packaging/linux/io.github.benoks.EShot.metainfo.xml" \
    "${deb_root}/usr/share/metainfo/io.github.benoks.EShot.metainfo.xml"
  install -Dm644 "${repo_root}/LICENSE" "${deb_root}/usr/share/doc/eshot/copyright"
  install -Dm644 "${repo_root}/README.md" "${deb_root}/usr/share/doc/eshot/README.md"

  installed_size="$(du -sk "${deb_root}/usr" | cut -f1)"
  mkdir -p "${deb_root}/DEBIAN"
  cat > "${deb_root}/DEBIAN/control" <<EOF
Package: eshot
Version: ${version}
Section: graphics
Priority: optional
Architecture: amd64
Installed-Size: ${installed_size}
Maintainer: Benoks <benoks@users.noreply.github.com>
Depends: libc6, libstdc++6, libqt6core6t64 | libqt6core6, libqt6gui6, libqt6widgets6, libqt6network6, libqt6dbus6, libqt6svg6, qt6-qpa-plugins, qt6-wayland, libx11-6, libxcb-cursor0, libsecret-1-0, ffmpeg, tesseract-ocr, gstreamer1.0-tools, gstreamer1.0-pipewire, xdg-desktop-portal, xdg-desktop-portal-gnome | xdg-desktop-portal-gtk | xdg-desktop-portal-kde | xdg-desktop-portal-wlr
Description: Screenshot, annotation, OCR, upload, GIF and video capture tool
 EShot is a native desktop screenshot workflow tool for Windows and Linux.
EOF
  dpkg-deb --build "${deb_root}" "${deb_path}"
  dpkg-deb --info "${deb_path}" >/dev/null
  if command -v desktop-file-validate >/dev/null 2>&1; then
    desktop-file-validate "${deb_root}/usr/share/applications/io.github.benoks.EShot.desktop"
  fi
  if command -v appstreamcli >/dev/null 2>&1; then
    appstreamcli validate-tree --no-net "${deb_root}"
  fi
fi

echo
echo "Packaged EShot Linux:"
echo "  ${tar_path}"
if [[ -f "${deb_path}" ]]; then
  echo "  ${deb_path}"
fi
if compgen -G "${package_dir}/EShot-v${version}-*.AppImage" >/dev/null; then
  compgen -G "${package_dir}/EShot-v${version}-*.AppImage" | sed 's/^/  /'
fi
