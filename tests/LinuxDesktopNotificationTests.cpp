#include <QtTest>

#include "core/LinuxDesktopNotification.h"

class LinuxDesktopNotificationTests : public QObject
{
    Q_OBJECT

private slots:
    void savedFileNotificationHasExplicitFolderAction()
    {
        const QStringList actions = LinuxDesktopNotification::actions(QStringLiteral("Open Folder"));
        QCOMPARE(actions, QStringList({QStringLiteral("open-folder"), QStringLiteral("Open Folder")}));
    }

    void savedRecordingNotificationListsEveryActionInOrder()
    {
        const QStringList actions = LinuxDesktopNotification::actions(
            QList<LinuxDesktopNotification::Action>{
                {QStringLiteral("open-file"), QStringLiteral("Open")},
                {QStringLiteral("show-in-folder"), QStringLiteral("Show in Folder")},
                {QStringLiteral("copy-file"), QStringLiteral("Copy File")},
                {QStringLiteral("default"), QStringLiteral("ignored")},
                {QStringLiteral("empty-label"), QString()},
            });
        QCOMPARE(actions, QStringList({QStringLiteral("open-file"), QStringLiteral("Open"),
                                       QStringLiteral("show-in-folder"),
                                       QStringLiteral("Show in Folder"),
                                       QStringLiteral("copy-file"),
                                       QStringLiteral("Copy File")}));
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

    // Regression: a saved-recording notification reused the id of the
    // previous EShot notification. Plasma keeps expired notifications in its
    // history and replaces them silently, so no popup appeared.
    void everyNotificationIsNewInsteadOfReplacingAnEarlierOne()
    {
        const QVariantList args = LinuxDesktopNotification::notifyArguments(
            QStringLiteral("EShot"), QStringLiteral("Video saved"),
            QStringLiteral("/tmp/EShot/video.mp4"),
            {QStringLiteral("open-file"), QStringLiteral("Open")}, 5000);
        QCOMPARE(args.size(), 8);
        QCOMPARE(args.at(0).toString(), QStringLiteral("EShot"));
        QCOMPARE(args.at(1).typeId(), QMetaType::UInt);
        QCOMPARE(args.at(1).toUInt(), 0u);
        QCOMPARE(args.at(3).toString(), QStringLiteral("EShot"));
        QCOMPARE(args.at(4).toString(), QStringLiteral("Video saved"));
        QCOMPARE(args.at(5).toStringList(),
                 QStringList({QStringLiteral("open-file"), QStringLiteral("Open"),
                              QStringLiteral("default"), QStringLiteral("EShot")}));
        QCOMPARE(args.at(7).toInt(), 5000);
    }
};

QTEST_APPLESS_MAIN(LinuxDesktopNotificationTests)
#include "LinuxDesktopNotificationTests.moc"
