#include "RecordingStartCountdownPolicy.h"

int recordingStartDelayMs(int configuredSeconds)
{
    return qBound(0, configuredSeconds, MaxRecordingStartDelaySeconds) * 1000;
}

int recordingStartCountdownSeconds(int delayMs, qint64 elapsedMs)
{
    const qint64 remainingMs = qMax<qint64>(0, delayMs - qMax<qint64>(0, elapsedMs));
    return static_cast<int>((remainingMs + 999) / 1000);
}

QRect recordingStartCountdownRect(const QRect &captureRect,
                                  const QRect &screenRect,
                                  const QSize &size)
{
    QRect anchor = screenRect.isValid() ? captureRect.intersected(screenRect) : captureRect;
    if (!anchor.isValid())
        anchor = screenRect.isValid() ? screenRect : captureRect;

    QRect rect(QPoint(0, 0), size);
    rect.moveCenter(anchor.center());
    if (!screenRect.isValid())
        return rect;
    rect.moveLeft(qBound(screenRect.left(), rect.left(),
                         qMax(screenRect.left(), screenRect.right() - size.width() + 1)));
    rect.moveTop(qBound(screenRect.top(), rect.top(),
                        qMax(screenRect.top(), screenRect.bottom() - size.height() + 1)));
    return rect;
}
