#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
overlay="${repo_root}/src/capture/CaptureOverlay.cpp"

drawer_block="$(sed -n '/void CaptureOverlay::setupRecordingDrawer()/,/void CaptureOverlay::showRecordingDrawer/p' "${overlay}")"

# The drawer offers choices, not raw spin boxes.
for combo in \
  m_recordingQualityCombo \
  m_recordingDurationCombo \
  m_recordingStartDelayCombo \
  m_recordingMicrophoneDeviceCombo; do
  grep -F "${combo} = new QComboBox" <<<"${drawer_block}" >/dev/null || {
    echo "${combo} must be created in the recording drawer" >&2
    exit 1
  }
done
if grep -F "new QSpinBox" <<<"${drawer_block}" >/dev/null; then
  echo "the recording drawer must not expose raw spin boxes" >&2
  exit 1
fi

grep -F "recordingDrawerStartsOnKey(" "${overlay}" >/dev/null || {
  echo "Enter must start the recording from the drawer" >&2
  exit 1
}

printf 'recording drawer input contract tests passed\n'
