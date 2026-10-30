#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
aur_dir="${repo_root}/packaging/aur/eshot-bin"
workflow="${repo_root}/.github/workflows/publish-aur.yml"
update_script="${repo_root}/scripts/linux/update-aur-package.sh"

test -f "${aur_dir}/PKGBUILD"
test -f "${aur_dir}/eshot"
grep -F "pkgname=eshot-bin" "${aur_dir}/PKGBUILD" >/dev/null
grep -F "options=('!strip')" "${aur_dir}/PKGBUILD" >/dev/null
grep -F "EShot-v\${pkgver}-x86_64.AppImage" "${aur_dir}/PKGBUILD" >/dev/null
grep -F -- '--appimage-extract' "${aur_dir}/PKGBUILD" >/dev/null
grep -F 'io.github.benoks.EShot.desktop' "${aur_dir}/PKGBUILD" >/dev/null
grep -F "xdg-desktop-portal-gnome: GNOME screenshot and recording integration" "${aur_dir}/PKGBUILD" >/dev/null
grep -F "Exec=eshot\\1" "${aur_dir}/PKGBUILD" >/dev/null
grep -F 'io.github.benoks.EShot-v4.svg' "${aur_dir}/PKGBUILD" >/dev/null
grep -F 'io.github.benoks.EShot.metainfo.xml' "${aur_dir}/PKGBUILD" >/dev/null
# Run the extracted AppImage from a stable path (no FUSE, no /tmp/.mount_*).
grep -Fx 'exec /opt/eshot/AppRun "$@"' "${aur_dir}/eshot" >/dev/null
grep -F 'cp -a "${srcdir}/squashfs-root" "${pkgdir}/opt/eshot"' "${aur_dir}/PKGBUILD" >/dev/null
grep -F 'ln -s AppRun "${pkgdir}/opt/eshot/EShot.AppImage"' "${aur_dir}/PKGBUILD" >/dev/null
if grep -E "^depends=.*fuse" "${aur_dir}/PKGBUILD" >/dev/null; then
  echo 'the extracted AUR package must not depend on FUSE' >&2
  exit 1
fi
for plugin in gst-plugins-good gst-plugins-bad gst-plugins-ugly gst-libav gst-plugin-pipewire; do
  grep -F "'${plugin}: " "${aur_dir}/PKGBUILD" >/dev/null || {
    echo "PKGBUILD must list ${plugin} as an optional recording dependency" >&2
    exit 1
  }
done
grep -F 'AUR_SSH_PRIVATE_KEY' "${workflow}" >/dev/null
grep -F 'aur.archlinux.org/eshot-bin.git' "${workflow}" >/dev/null
grep -F 'runuser -u builder' "${workflow}" >/dev/null
test -x "${update_script}"

fixture="$(mktemp -d)"
trap 'rm -rf "${fixture}"' EXIT
cp "${aur_dir}/PKGBUILD" "${fixture}/PKGBUILD"
bash "${update_script}" 4.0.7 \
    aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa \
    "${fixture}"
grep -F 'pkgver=4.0.7' "${fixture}/PKGBUILD" >/dev/null
grep -F "sha256sums=('aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa'" \
    "${fixture}/PKGBUILD" >/dev/null
