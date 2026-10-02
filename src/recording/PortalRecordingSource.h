#ifndef PORTALRECORDINGSOURCE_H
#define PORTALRECORDINGSOURCE_H

#include "core/LinuxPortalScreenCast.h"

#include <QRect>
#include <QString>

// Opens the Wayland ScreenCast portal for a recording region ahead of the
// recorder start, so a start delay can run after the source was picked. A
// stream restored from a saved token that does not cover the region is
// dropped and the picker is shown once more. On failure the stream is invalid
// and *error holds the same reason the recorders report from start().
LinuxPortalScreenCast::Stream selectPortalRecordingStream(const QRect &captureRect,
                                                          const QRect &displayRect,
                                                          QString *error);

#endif
