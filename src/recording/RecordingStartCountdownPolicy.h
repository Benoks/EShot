#ifndef RECORDINGSTARTCOUNTDOWNPOLICY_H
#define RECORDINGSTARTCOUNTDOWNPOLICY_H

#include <QRect>
#include <QSize>
#include <QtGlobal>

// Start delay is configured in whole seconds and capped at 10 s.
constexpr int MaxRecordingStartDelaySeconds = 10;

int recordingStartDelayMs(int configuredSeconds);

// Seconds shown while counting down: rounded up, so "1" stays on screen until
// the delay has fully elapsed and 0 means the recording may start.
int recordingStartCountdownSeconds(int delayMs, qint64 elapsedMs);

// Centers the countdown in the visible part of the capture region, kept
// inside the screen work area.
QRect recordingStartCountdownRect(const QRect &captureRect,
                                  const QRect &screenRect,
                                  const QSize &size);

#endif
