#ifndef RECORDINGCAPTUREEXCLUSION_H
#define RECORDINGCAPTUREEXCLUSION_H

#include "RecordingControlLayout.h"

#include <QDebug>
#include <QOperatingSystemVersion>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

// Windows build number of the running system (e.g. 19045, 22631), 0 elsewhere.
inline int recordingWindowsBuildNumber()
{
#ifdef Q_OS_WIN
    // Qt reads the real version through RtlGetVersion, so this is not
    // affected by application manifest compatibility shims.
    const QOperatingSystemVersion version = QOperatingSystemVersion::current();
    if (version.majorVersion() < 10)
        return 0;
    return qMax(0, version.microVersion());
#else
    return 0;
#endif
}

// Removes a top-level widget's native window from screen captures. Creates
// the native window when needed, so call it before the first show() and
// again after anything that recreates the window (setWindowFlags()).
// Returns true only when Windows confirms WDA_EXCLUDEFROMCAPTURE.
inline bool excludeRecordingWindowFromCapture(QWidget *widget)
{
#ifdef Q_OS_WIN
    if (!widget || !widget->isWindow())
        return false;
    // Before Windows 10 2004 the flag degrades to WDA_MONITOR, which would
    // record the window as a black box instead of leaving it out.
    if (recordingWindowsBuildNumber() < RecordingWindowsCaptureExclusionMinimumBuild)
        return false;
    const HWND hwnd = reinterpret_cast<HWND>(widget->winId());
    if (!hwnd)
        return false;
    if (!SetWindowDisplayAffinity(hwnd, WDA_EXCLUDEFROMCAPTURE)) {
        qWarning() << "[RecordingCaptureExclusion] SetWindowDisplayAffinity failed for"
                   << widget->objectName() << "error=" << GetLastError();
        return false;
    }
    DWORD affinity = WDA_NONE;
    if (!GetWindowDisplayAffinity(hwnd, &affinity)) {
        qWarning() << "[RecordingCaptureExclusion] GetWindowDisplayAffinity failed for"
                   << widget->objectName() << "error=" << GetLastError();
        return false;
    }
    const bool excluded = affinity == WDA_EXCLUDEFROMCAPTURE;
    if (!excluded) {
        qWarning() << "[RecordingCaptureExclusion] unexpected display affinity"
                   << affinity << "for" << widget->objectName();
    }
    return excluded;
#else
    Q_UNUSED(widget)
    return false;
#endif
}

#endif
