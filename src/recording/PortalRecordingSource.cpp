#include "PortalRecordingSource.h"

#include "LinuxRecordingSupport.h"

#include <QGuiApplication>
#include <QScreen>

LinuxPortalScreenCast::Stream selectPortalRecordingStream(const QRect &captureRect,
                                                          const QRect &displayRect,
                                                          QString *error)
{
    auto fail = [error](const QString &reason) {
        if (error)
            *error = reason;
        return LinuxPortalScreenCast::Stream();
    };
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    if (!LinuxPortalScreenCast::isAvailable())
        return fail(QStringLiteral("Wayland ScreenCast portal is not available"));

    QString persistenceId;
    if (displayRect.isValid()) {
        if (QScreen *screen = QGuiApplication::screenAt(displayRect.center()))
            persistenceId = screen->name();
    }
    LinuxPortalScreenCast::Stream stream = LinuxPortalScreenCast::selectStream(
        nullptr, 120000, persistenceId);
    if (!stream.isValid())
        return fail(QStringLiteral("Wayland screen recording permission was not granted"));

    // Only the region check matters here; the recorders compute their own
    // output size when they build the pipeline.
    bool covered = portalCropGeometry(captureRect, displayRect, stream.position,
                                      stream.size, captureRect.size()).valid;
    if (!covered && stream.usedRestoreToken) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        LinuxPortalScreenCast::clearRestoreToken(persistenceId);
        stream = LinuxPortalScreenCast::selectStream(nullptr, 120000, persistenceId);
        covered = stream.isValid()
            && portalCropGeometry(captureRect, displayRect, stream.position,
                                  stream.size, captureRect.size()).valid;
    }
    if (!stream.isValid() || !covered) {
        LinuxPortalScreenCast::closeSession(stream.sessionHandle);
        return fail(QStringLiteral("Wayland recording source does not contain the selected region"));
    }
    return stream;
#else
    Q_UNUSED(captureRect);
    Q_UNUSED(displayRect);
    return fail(QStringLiteral("Wayland ScreenCast portal is not available"));
#endif
}
