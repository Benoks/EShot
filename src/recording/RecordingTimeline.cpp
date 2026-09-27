#include "RecordingTimeline.h"

#include <QtGlobal>
#include <climits>

void RecordingTimeline::start(qint64 nowMs)
{
    m_startedMs = nowMs;
    m_pauseStartedMs = 0;
    m_pausedMs = 0;
    m_started = true;
    m_paused = false;
}

bool RecordingTimeline::pause(qint64 nowMs)
{
    if (!m_started || m_paused)
        return false;
    m_pauseStartedMs = qMax(nowMs, m_startedMs);
    m_paused = true;
    return true;
}

bool RecordingTimeline::resume(qint64 nowMs)
{
    if (!m_started || !m_paused)
        return false;
    m_pausedMs += qMax<qint64>(0, nowMs - m_pauseStartedMs);
    m_pauseStartedMs = 0;
    m_paused = false;
    return true;
}

qint64 RecordingTimeline::activeElapsedMs(qint64 nowMs) const
{
    if (!m_started)
        return 0;
    qint64 pausedMs = m_pausedMs;
    if (m_paused)
        pausedMs += qMax<qint64>(0, nowMs - m_pauseStartedMs);
    return qMax<qint64>(0, nowMs - m_startedMs - pausedMs);
}

void GifFrameClock::start(qint64 firstFrameMs)
{
    m_originMs = firstFrameMs;
    m_emittedCs = 0;
}

int GifFrameClock::pendingCs(qint64 frameEndMs) const
{
    const qint64 totalCs = (qMax<qint64>(0, frameEndMs - m_originMs) + 5) / 10;
    return static_cast<int>(qBound<qint64>(0, totalCs - m_emittedCs, qint64(INT_MAX)));
}

int GifFrameClock::takeDelayCs(qint64 frameEndMs)
{
    // GIF delays are 16-bit; a 0 delay is shown as "as fast as possible", so
    // borrow 1 cs from the following frames instead.
    const int delayCs = qBound(1, pendingCs(frameEndMs), 65535);
    m_emittedCs += delayCs;
    return delayCs;
}
