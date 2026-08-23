#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
overlay="${repo_root}/src/capture/CaptureOverlay.cpp"

drawer_block="$(sed -n '/void CaptureOverlay::setupRecordingDrawer()/,/void CaptureOverlay::showRecordingDrawer/p' "${overlay}")"

for spin in \
  m_recordingGifFpsSpin \
  m_recordingGifSecondsSpin \
  m_recordingVideoFpsSpin \
  m_recordingVideoSecondsSpin \
  m_recordingVideoCrfSpin; do
  grep -F "configureOverlaySpinBox(${spin});" <<<"${drawer_block}" >/dev/null || {
    echo "${spin} must use the editable overlay spin-box configuration" >&2
    exit 1
  }
done

printf 'recording drawer input contract tests passed\n'
