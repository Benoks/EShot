#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
build_script="${repo_root}/scripts/linux/build-appimage.sh"
workflow="${repo_root}/.github/workflows/build.yml"
desktop_launcher="${repo_root}/scripts/linux/desktop-launch.sh"
installed_launcher="${repo_root}/scripts/linux/eshot-launcher"
source_desktop_entry="${repo_root}/EShot-Linux.desktop"
release_version_script="${repo_root}/scripts/linux/apply-release-version.sh"
ubuntu_deps="${repo_root}/scripts/linux/install-ubuntu-deps.sh"
linux_package_script="${repo_root}/scripts/linux/package-linux.sh"
portability_script="${repo_root}/scripts/linux/verify-appimage-portability.sh"

grep -F 'EShot-v${version}-x86_64.AppImage' "${build_script}" >/dev/null
grep -F 'linuxdeploy/releases/download/1-alpha-20251107-1/linuxdeploy-x86_64.AppImage' "${build_script}" >/dev/null
grep -F 'linuxdeploy-plugin-qt/releases/download/1-alpha-20250213-1/linuxdeploy-plugin-qt-x86_64.AppImage' "${build_script}" >/dev/null
grep -F 'appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage' "${build_script}" >/dev/null
grep -F 'packaging/linux/AppRun' "${build_script}" >/dev/null
grep -F -- '--appimage-extract-and-run' "${workflow}" >/dev/null
grep -F 'packages/EShot-v${APP_VERSION}-x86_64.AppImage' "${workflow}" >/dev/null
grep -F 'release-assets/EShot-linux-packages/EShot-${{ github.ref_name }}-linux-x64.tar.gz' "${workflow}" >/dev/null
grep -F 'EShot-v${version}-linux-x64.tar.gz' "${linux_package_script}" >/dev/null
if grep -F 'if: false # Linux releases are built and published locally.' "${workflow}" >/dev/null; then
  echo 'portable Linux release artifacts must be built on the baseline CI runner' >&2
  exit 1
fi
grep -F 'scripts/linux/verify-appimage-portability.sh' "${workflow}" >/dev/null
grep -F 'bash "${repo_root}/scripts/linux/verify-appimage-portability.sh"' "${build_script}" >/dev/null
grep -F 'x86-64-v[234]' "${portability_script}" >/dev/null
grep -F 'max_glibc="2.35"' "${portability_script}" >/dev/null
grep -F 'release-assets/EShot-Setup-x64/*.exe' "${workflow}" >/dev/null
grep -F 'release-assets/EShot-Setup-arm64/*.exe' "${workflow}" >/dev/null
grep -F '      - linux-build' "${workflow}" >/dev/null
grep -F 'release-assets/EShot-linux-packages/EShot-${{ github.ref_name }}-x86_64.AppImage' "${workflow}" >/dev/null
grep -F 'body_path: packaging/release-notes/v4.1.5.md' "${workflow}" >/dev/null
grep -F 'https://img.shields.io/github/v/tag/Benoks/EShot?label=release&sort=semver' "${repo_root}/README.md" >/dev/null
grep -F "TESSERACT_RUNTIME_RELEASE: 'v4.1.2'" "${workflow}" >/dev/null
grep -F 'releases/download/$release/$archive' "${workflow}" >/dev/null
grep -F 'EShot-$release-${{ matrix.arch }}-portable.zip' "${workflow}" >/dev/null
if grep -F 'vcpkg.exe" install tesseract' "${workflow}" >/dev/null; then
  echo 'release builds must reuse the verified Tesseract runtime instead of compiling it' >&2
  exit 1
fi
grep -F 'if: startsWith(github.ref, '"'"'refs/tags/v'"'"')' "${workflow}" >/dev/null
grep -F 'scripts/linux/apply-release-version.sh' "${workflow}" >/dev/null
grep -F 'libqt6svg6' "${workflow}" >/dev/null
grep -F 'xvfb-run -a env HOME=' "${workflow}" >/dev/null
grep -F 'xauth' "${workflow}" >/dev/null
grep -F 'libqt6svg6' "${ubuntu_deps}" >/dev/null
grep -F 'libqt6svg6' "${linux_package_script}" >/dev/null
grep -F 'list(APPEND SOURCES src/core/LinuxKGlobalAccelShortcuts.cpp)' "${repo_root}/CMakeLists.txt" >/dev/null

version_fixture="$(mktemp)"
trap 'rm -f "${version_fixture}"' EXIT
printf 'project(EShot VERSION 3.1.0 LANGUAGES CXX)\n' >"${version_fixture}"
resolved_version="$(bash "${release_version_script}" v9.8.7 "${version_fixture}")"
[[ "${resolved_version}" == "9.8.7" ]]
grep -F 'project(EShot VERSION 9.8.7 LANGUAGES CXX)' "${version_fixture}" >/dev/null
if bash "${release_version_script}" nightly "${version_fixture}" >/dev/null 2>&1; then
  echo 'invalid release tag was accepted' >&2
  exit 1
fi
grep -F 'install-user.sh' "${desktop_launcher}" >/dev/null
grep -F 'ESHOT_WAYLAND_XWAYLAND_OVERLAY=1' "${installed_launcher}" >/dev/null
grep -F 'Terminal=false' "${source_desktop_entry}" >/dev/null
grep -F 'StartupNotify=false' "${source_desktop_entry}" >/dev/null
grep -F 'Icon=io.github.benoks.EShot-v4' "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" >/dev/null
grep -F 'StartupWMClass=EShot' "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" >/dev/null
grep -F '#include "../core/LinuxAutoStartPolicy.h"' "${repo_root}/src/ui/SettingsDialog.cpp" >/dev/null
[[ "$(grep -Fc 'LinuxAutoStartPolicy::executablePath(' "${repo_root}/src/ui/SettingsDialog.cpp")" -ge 2 ]] || {
  echo 'Linux autostart creation and detection must share the stable AppImage path policy' >&2
  exit 1
}
grep -F 'Icon=io.github.benoks.EShot-v4' "${repo_root}/src/ui/SettingsDialog.cpp" >/dev/null
grep -F 'm_textFocusProxy = new QTextEdit(nullptr);' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F 'm_textFocusProxy->activateWindow();' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F 'painter.drawPixmap(rect(), m_screenSnapshot, m_screenSnapshot.rect());' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F ':/icons/drag.svg' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F ':/icons/chevron_down.svg' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F ':/icons/chevron_up.svg' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F ':/icons/check.svg' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F 'connect(m_textCommitButton' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F 'connect(m_textCancelButton' "${repo_root}/src/capture/CaptureOverlay.cpp" >/dev/null
grep -F 'QSystemTrayIcon::Trigger' "${repo_root}/src/main.cpp" >/dev/null
grep -F 'm_trayMenu->popup(QCursor::pos())' "${repo_root}/src/main.cpp" >/dev/null
grep -F 'environment.insert(QStringLiteral("ESHOT_LANGUAGE"), TranslationManager::langCode())' "${repo_root}/src/ui/FirstRunWizard.cpp" >/dev/null
grep -F 'RecordingIndicatorMode::Video' "${repo_root}/src/main.cpp" >/dev/null
grep -F '&ScreenRecorder::pausedChanged' "${repo_root}/src/main.cpp" >/dev/null
grep -F '&RecordingIndicator::pauseRequested' "${repo_root}/src/main.cpp" >/dev/null
grep -F '&RecordingIndicator::resumeRequested' "${repo_root}/src/main.cpp" >/dev/null
grep -F 'm_recordingIndicator->startCaptureSafePresentation()' "${repo_root}/src/main.cpp" >/dev/null
grep -F 'X-KDE-DBUS-Restricted-Interfaces=org.kde.KWin.ScreenShot2' "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" >/dev/null
grep -F '"${pixmap_dir}/io.github.benoks.EShot.svg"' "${repo_root}/scripts/linux/install-user.sh" >/dev/null
grep -F '"${pixmap_dir}/io.github.benoks.EShot-v4.svg"' "${repo_root}/scripts/linux/install-user.sh" >/dev/null
grep -F '"${icon_dir}/io.github.benoks.EShot-v4.svg"' "${repo_root}/scripts/linux/install-user.sh" >/dev/null
grep -F 'kbuildsycoca6 --noincremental' "${repo_root}/scripts/linux/install-user.sh" >/dev/null
grep -F 'X-KDE-StartupNotify=false' "${source_desktop_entry}" >/dev/null
grep -F 'X-GNOME-UsesNotifications=true' "${source_desktop_entry}" >/dev/null
grep -Fx 'Categories=Graphics;Photography;' "${source_desktop_entry}" >/dev/null || {
  echo 'desktop entry must use one main category so GNOME does not list EShot twice' >&2
  exit 1
}
grep -F 'Actions=Capture;Settings;Quit;' "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" >/dev/null
grep -F 'Exec=eshot-launcher --quit' "${repo_root}/packaging/linux/io.github.benoks.EShot.desktop" >/dev/null
grep -F 'export NO_STRIP=1' "${build_script}" >/dev/null
grep -F 'qmake-appimage-wrapper.sh' "${build_script}" >/dev/null
grep -F 'qt-plugin-dir.sh' "${build_script}" >/dev/null
grep -F 'libqwayland*.so' "${build_script}" >/dev/null || {
  echo 'AppImage must bundle a native Qt Wayland platform plugin for the XWayland fallback' >&2
  exit 1
}
grep -F 'wayland-shell-integration' "${build_script}" >/dev/null || {
  echo 'AppImage must bundle Qt Wayland client integration plugins' >&2
  exit 1
}
grep -F 'wayland_dependency_args+=(--deploy-deps-only "${destination}")' "${build_script}" >/dev/null || {
  echo 'AppImage must deploy each Qt Wayland plugin dependency explicitly' >&2
  exit 1
}
grep -F -- '--runtime-file "${runtime_file}"' "${build_script}" >/dev/null || {
  echo 'AppImage build must use a pre-verified runtime instead of downloading one inside appimagetool' >&2
  exit 1
}
grep -F 'install -Dm755 "${repo_root}/scripts/linux/qmake-appimage-wrapper.sh" "${qmake_wrapper}"' "${build_script}" >/dev/null
if grep -F 'systemd-run --user --unit=eshot' "${repo_root}/packaging/linux/AppRun" >/dev/null; then
  echo 'AppRun must preserve the desktop-launch application identity for portals' >&2
  exit 1
fi
if grep -F '/usr/lib/qt6/plugins' "${build_script}" >/dev/null; then
  echo 'AppImage build hardcodes an Arch-only Qt plugin path' >&2
  exit 1
fi
fixture_bin="$(mktemp -d)"
trap 'rm -f "${version_fixture}"; rm -rf "${fixture_bin}"' EXIT
cat >"${fixture_bin}/qtpaths6" <<'EOF'
#!/usr/bin/env bash
[[ "$1" == "--plugin-dir" ]] && printf '/opt/qt/lib/plugins\n'
EOF
chmod +x "${fixture_bin}/qtpaths6"
[[ "$(PATH="${fixture_bin}:${PATH}" bash "${repo_root}/scripts/linux/qt-plugin-dir.sh")" == /opt/qt/lib/plugins ]]
desktop-file-validate "${source_desktop_entry}"

printf 'package contract tests passed\n'
