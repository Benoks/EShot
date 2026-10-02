#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
overlay="${repo_root}/src/capture/CaptureOverlay.cpp"

# Native Wayland uses independent output surfaces sharing one widget model,
# rather than stretching one native surface across mixed-DPI outputs.
views="${repo_root}/src/capture/CaptureScreenViews.cpp"
grep -F 'captureNativeScreenImages(logicalRect, physicalRect)' "${overlay}" >/dev/null
grep -F 'm_screenViews->present(m_virtualDesktopRect, screens, active)' "${overlay}" >/dev/null
grep -F 'm_scene->addWidget(m_canvas)' "${views}" >/dev/null
grep -F 'view->windowHandle()->setScreen(screen)' "${views}" >/dev/null
grep -F 'view->showFullScreen()' "${views}" >/dev/null
grep -F 'view->setSceneRect(screen->geometry())' "${views}" >/dev/null
# Keep the single-output fallback for backends without per-output capture.
grep -F 'm_screenSnapshot = LinuxPortalScreenshot::grabScreen(screen, this)' "${overlay}" >/dev/null

printf 'native Wayland overlay tests passed\n'
