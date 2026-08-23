#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
overlay="${repo_root}/src/capture/CaptureOverlay.cpp"

# Gesture history is intentionally coalesced while the pointer is down. The
# controller must open and close every move/rotate gesture or later gestures
# of the same annotation are folded into one undo action.
press_block="$(sed -n '/void CaptureOverlay::mousePressEvent/,/void CaptureOverlay::mouseDoubleClickEvent/p' "${overlay}")"
release_block="$(sed -n '/void CaptureOverlay::mouseReleaseEvent/,/void CaptureOverlay::keyPressEvent/p' "${overlay}")"

[[ "${press_block}" == *'m_annotationEngine->beginRotate(selectedIndex);'* ]] || {
  echo 'annotation rotation does not begin an undo gesture' >&2
  exit 1
}
[[ "${press_block}" == *'m_annotationEngine->beginMove(idx);'* ]] || {
  echo 'annotation move does not begin an undo gesture' >&2
  exit 1
}
[[ "${release_block}" == *'m_annotationEngine->endRotate();'* ]] || {
  echo 'annotation rotation does not end its undo gesture' >&2
  exit 1
}
[[ "${release_block}" == *'m_annotationEngine->endMove();'* ]] || {
  echo 'annotation move does not end its undo gesture' >&2
  exit 1
}

printf 'annotation gesture wiring tests passed\n'
