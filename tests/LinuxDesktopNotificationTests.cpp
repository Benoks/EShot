#include <QtTest>

#include "core/LinuxDesktopNotification.h"

class LinuxDesktopNotificationTests : public QObject
{
    Q_OBJECT

private slots:
    void savedFileNotificationHasClickableDefaultAction()
    {
        const QStringList actions = LinuxDesktopNotification::actions(QStringLiteral("Open Folder"));
        QCOMPARE(actions, QStringList({QStringLiteral("default"), QStringLiteral("Open Folder")}));
    }

    void failureNotificationHasNoFileAction()
    {
        QCOMPARE(LinuxDesktopNotification::actions(QString()), QStringList());
    }

    void savedFileNotificationCarriesKdeFileUrl()
    {
        const QVariantMap hints = LinuxDesktopNotification::hintsForPath(
            QStringLiteral("/tmp/EShot/video.mp4"));
        QCOMPARE(hints.value(QStringLiteral("desktop-entry")).toString(),
                 QStringLiteral("io.github.benoks.EShot"));
        QCOMPARE(hints.value(QStringLiteral("x-kde-urls")).toStringList(),
                 QStringList({QStringLiteral("file:///tmp/EShot/video.mp4")}));
    }
};

QTEST_APPLESS_MAIN(LinuxDesktopNotificationTests)
#include "LinuxDesktopNotificationTests.moc"
