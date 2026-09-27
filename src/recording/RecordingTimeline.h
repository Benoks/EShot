#ifndef RECORDINGTIMELINE_H
#define RECORDINGTIMELINE_H

#include <QtGlobal>

class RecordingTimeline
{
public:
    void start(qint64 nowMs);
    bool pause(qint64 nowMs);
    bool resume(qint64 nowMs);

    qint64 activeElapsedMs(qint64 nowMs) const;
    bool isPaused() const { return m_paused; }

private:
    qint64 m_startedMs = 0;
    qint64 m_pauseStartedMs = 0;
    qint64 m_pausedMs = 0;
    bool m_started = false;
    bool m_paused = false;
};

// Converts frame boundaries on the active recording timeline into GIF
// centisecond delays. Rounding the running total instead of each frame keeps
// the rounding error from accumulating (e.g. 33 ms frames at 30 fps).
class GifFrameClock
{
public:
    void start(qint64 firstFrameMs);
    // Centiseconds not yet emitted up to frameEndMs.
    int pendingCs(qint64 frameEndMs) const;
    // Delay for the frame ending at frameEndMs; always at least 1 cs.
    int takeDelayCs(qint64 frameEndMs);

private:
    qint64 m_originMs = 0;
    qint64 m_emittedCs = 0;
};

#endif
