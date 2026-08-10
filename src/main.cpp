#include <QApplication>
#include "version.h"
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QPixmapCache>
#include <QImage>
#include <QDebug>
#include <QSettings>
#include <QPalette>
#include <QList>
#include <QPointer>
#include <QCommandLineParser>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QTimer>
#include <QPainter>
#include <QUrlQuery>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QClipboard>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QScreen>
#include <QEventLoop>
#include <QLabel>
#include <QVersionNumber>
#include <QDialog>
#include <QLocalServer>
#include <QLocalSocket>
#include <QProcess>
#include <QElapsedTimer>

#include "core/HotkeyManager.h"
#include "core/TranslationManager.h"
#include "core/UpdateManager.h"
#include "core/LinuxScreenshotPolicy.h"
#include "core/LinuxDesktopIntegration.h"
#include "core/NotificationFolderOpener.h"
#include "core/ApplicationInstanceCommand.h"
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include "core/LinuxDesktopNotification.h"
#include "core/LinuxPortalHostRegistry.h"
#endif
#include "capture/CaptureOverlay.h"
#include "capture/PinnedWindow.h"
#include "capture/PinManager.h"
#include "recording/ScreenRecorder.h"
#include "recording/VideoRecorder.h"
#include "recording/RecordingIndicator.h"
#include "recording/RecordingSettingsPolicy.h"
#include "ui/SettingsDialog.h"
#include "ui/SettingsLayoutPolicy.h"
#include "ui/ApplicationTheme.h"
#include "ui/AboutDialog.h"
#include "ui/ControlCenterDialog.h"
#include "ui/FirstRunWizard.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

void prepareKWinScreenshotPermission()
{
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const QString executablePath = QFileInfo(QCoreApplication::applicationFilePath())
                                       .canonicalFilePath();
    if (!LinuxScreenshotPolicy::shouldPrepareKWinPermission(
            qEnvironmentVariable("XDG_CURRENT_DESKTOP"),
            qEnvironmentVariable("XDG_SESSION_DESKTOP"),
            qEnvironmentVariable("XDG_SESSION_TYPE"),
            qEnvironmentVariable("APPIMAGE"),
            executablePath)) {
        return;
    }

    const QString dataLocation = QStandardPaths::writableLocation(
        QStandardPaths::GenericDataLocation);
    const QString applicationsDirectory = QDir(dataLocation).filePath(
        QStringLiteral("applications"));
    QString desktopPath;
    QString error;
    if (!LinuxScreenshotPolicy::installKWinPermissionDesktopEntry(
            applicationsDirectory, executablePath, &desktopPath, &error)) {
        qWarning() << "[KWinPermission] could not install restricted-interface entry:"
                   << error;
        return;
    }

    const QString cacheBuilder = QStandardPaths::findExecutable(
        QStringLiteral("kbuildsycoca6"));
    if (cacheBuilder.isEmpty()) {
        qWarning() << "[KWinPermission] kbuildsycoca6 is unavailable;"
                      " direct KWin capture may remain unauthorized";
        return;
    }

    QElapsedTimer timer;
    timer.start();
    const int exitCode = QProcess::execute(cacheBuilder, {});
    if (exitCode != 0) {
        qWarning() << "[KWinPermission] cache refresh failed with exit code="
                   << exitCode << "desktop=" << desktopPath;
        return;
    }
    qInfo() << "[KWinPermission] direct KWin capture prepared in"
            << timer.elapsed() << "ms executable=" << executablePath;
#endif
}

}

QString localizedRecordingFailureReason(const QString &reason)
{
    if (reason == QStringLiteral("ffmpeg.exe not found") || reason == QStringLiteral("ffmpeg not found"))
        return TranslationManager::videoFfmpegMissing();
    if (reason == QStringLiteral("gstreamer not found"))
        return TranslationManager::videoGstreamerMissing();
    if (reason == QStringLiteral("Wayland ScreenCast portal is not available"))
        return TranslationManager::videoWaylandPortalMissing();
    if (reason == QStringLiteral("Wayland screen recording permission was not granted"))
        return TranslationManager::videoWaylandPermissionDenied();
    if (reason == QStringLiteral("Wayland recording source does not contain the selected region"))
        return TranslationManager::videoWaylandWrongSource();
    if (reason == QStringLiteral("cannot start gstreamer"))
        return TranslationManager::videoGstreamerStartFailed();
    if (reason == QStringLiteral("Wayland PipeWire remote could not be opened"))
        return TranslationManager::videoPipeWireRemoteFailed();
    return reason;
}

class EShotApp : public QObject {
    Q_OBJECT

public:
    explicit EShotApp(bool initializeHotkeys = true, QObject *parent = nullptr)
        : QObject(parent)
    {
        TranslationManager::init();
        loadSettings();
        prepareKWinScreenshotPermission();
        setupUpdater();
        setupTrayIcon();
        if (initializeHotkeys)
            initializeHotkeyConnections();
        checkForUpdates();
        QTimer::singleShot(100, this, [this]() { ensureOverlay(); });
    }

    ~EShotApp()
    {
        if (m_trayIcon) m_trayIcon->hide();
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator->deleteLater(); m_recordingIndicator = nullptr; }
        if (m_overlay) { m_overlay->deleteLater(); m_overlay = nullptr; }
        if (m_screenRecorder) { m_screenRecorder->stop(); m_screenRecorder->deleteLater(); m_screenRecorder = nullptr; }
        if (m_trayMenu) { delete m_trayMenu; m_trayMenu = nullptr; }
    }

public slots:
    void initializeHotkeyConnections()
    {
        if (m_hotkeysInitialized)
            return;
        m_hotkeysInitialized = true;
        setupHotkey();
        rebuildTrayMenu();
    }

    void onCaptureRequested()
    {
        if (closeBlockingDialogs()) {
            QTimer::singleShot(80, this, &EShotApp::onCaptureRequested);
            return;
        }
        if (m_overlay && m_overlay->isVisible()) return;
        ensureOverlay();
        m_overlay->startCapture();
    }

    void onWindowCaptureRequested()
    {
#ifdef Q_OS_WIN
        if (closeBlockingDialogs()) {
            QTimer::singleShot(80, this, &EShotApp::onWindowCaptureRequested);
            return;
        }
        if (m_overlay && m_overlay->isVisible()) return;
        ensureOverlay();
        m_overlay->startWindowCapture();
#endif
    }

    void showSuccessNotification(const QString &message, const QString &path, int timeoutMs)
    {
        if (!m_trayIcon || !m_showNotifications)
            return;
        if (!path.isEmpty())
            m_lastNotificationPath = path;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (!path.isEmpty() && m_linuxNotification
            && m_linuxNotification->show(TranslationManager::notifCaptureTitle(), message,
                                         m_notificationOpenFolder ? path : QString(),
                                         m_notificationOpenFolder
                                             ? TranslationManager::openFolder() : QString(),
                                         timeoutMs)) {
            return;
        }
#endif
        if (!m_trayIcon->isVisible())
            m_trayIcon->show();
        m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(),
                                message,
                                QSystemTrayIcon::Information,
                                timeoutMs);
    }

    void showFailureNotification(const QString &message, int timeoutMs)
    {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (m_linuxNotification
            && m_linuxNotification->show(TranslationManager::notifCaptureTitle(), message,
                                         QString(), QString(), timeoutMs)) {
            return;
        }
#endif
        if (m_trayIcon) {
            m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(), message,
                                    QSystemTrayIcon::Warning, timeoutMs);
        }
    }

    void onCaptureCompleted(const QPixmap &pixmap)
    {
        if (m_skipNextCaptureNotification) {
            m_skipNextCaptureNotification = false;
            return;
        }
        m_lastNotificationPath.clear();
        if (m_trayIcon && m_showNotifications && m_notifyCopy) {
            showSuccessNotification(
                TranslationManager::notifCaptureMsg(pixmap.width(), pixmap.height()),
                QString(), 2000);
        }
    }

    void onCaptureSaved(const QString &path)
    {
        m_lastNotificationPath = path;
        m_skipNextCaptureNotification = true;
        if (m_trayIcon && m_showNotifications && m_notifySave) {
            QTimer::singleShot(250, this, [this, path]() {
                if (!m_trayIcon || !m_showNotifications || !m_notifySave)
                    return;
                QFileInfo fi(path);
                showSuccessNotification(
                    QStringLiteral("%1\n%2").arg(TranslationManager::captureSaved(), QDir::toNativeSeparators(fi.absoluteFilePath())),
                    fi.absoluteFilePath(), 4000);
            });
        }
    }

    void onCaptureCancelled() {}

    void onRegionSelected(QRect captureRect, QRect displayRect)
    {
        if (m_pendingMode == 2) {
            onRecordGifSelected(captureRect, displayRect);
        } else if (m_pendingMode == 3) {
            onRecordVideoSelected(captureRect, displayRect);
        }
        m_pendingMode = 0;
    }

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::DoubleClick) {
            onCaptureRequested();
        } else if (reason == QSystemTrayIcon::Trigger && m_trayMenu) {
            rebuildTrayMenu();
            m_trayMenu->popup(QCursor::pos());
        }
    }

    void onQuitAction() { qApp->quit(); }

    void onNotificationClicked()
    {
        if (!m_notificationOpenFolder)
            return;
#ifdef Q_OS_WIN
        constexpr NotificationDesktop desktop = NotificationDesktop::Windows;
#elif defined(Q_OS_LINUX)
        constexpr NotificationDesktop desktop = NotificationDesktop::Linux;
#else
        constexpr NotificationDesktop desktop = NotificationDesktop::Other;
#endif
        qInfo() << "[Notification] clicked path=" << m_lastNotificationPath;
        const bool opened = openNotificationFolder(
            m_lastNotificationPath, desktop,
            [](const QString &program, const QStringList &arguments) {
                return QProcess::startDetached(program, arguments);
            },
            [](const QUrl &url) {
                return QDesktopServices::openUrl(url);
            });
        qInfo() << "[Notification] folder open requested=" << opened
                << "directory=" << notificationDirectoryForPath(m_lastNotificationPath);
    }

    void onUpdateRequested()
    {
        if (m_updateManager)
            m_updateManager->installUpdate();
    }

    void onSettingsRequested()
    {
        SettingsDialog dlg;
        if (m_updateManager) {
            dlg.setUpdateInfo(m_updateManager->updateAvailable(),
                              m_updateManager->latestVersion(),
                              m_updateManager->isBusy(),
                              m_updateManager->statusText());
            QPointer<SettingsDialog> dlgPtr(&dlg);
            connect(m_updateManager, &UpdateManager::statusChanged, &dlg, [this, dlgPtr]() {
                if (!dlgPtr || !m_updateManager) return;
                dlgPtr->setUpdateInfo(m_updateManager->updateAvailable(),
                                      m_updateManager->latestVersion(),
                                      m_updateManager->isBusy(),
                                      m_updateManager->statusText());
            });
            connect(&dlg, &SettingsDialog::updateRequested, this, &EShotApp::onUpdateRequested);
        }
        dlg.show();
        QApplication::processEvents(); // Let ARM64 DWM finalize frame geometry and draw the title bar
        if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
            if (!screen) screen = QGuiApplication::primaryScreen();
            QRect avail = screen->availableGeometry();
            
            if (settingsDialogUsesAdaptiveSize(dlg.remembersWindowSize())) {
                // Programmatically simulate a user resize to force layout compression and fix Sandbox double-render.
                dlg.resize(dlg.minimumSizeHint());
                QApplication::processEvents();
            }
            
            int nx = avail.center().x() - dlg.width() / 2;
            int ny = avail.center().y() - dlg.height() / 2;
            ny = qMax(avail.top() + 40, ny); // GUARANTEE title bar is grabbable
            dlg.move(nx, ny); // Resync ARM64 drag margins
        }
        if (dlg.exec() == QDialog::Accepted) {
            loadSettings();
            if (!m_updateAvailable)
                setTrayIconNormal();
            rebuildTrayMenu();
            if (m_overlay) m_overlay->refreshUI();
        }
    }

    void onControlRequested()
    {
        ControlCenterDialog dialog;
        if (dialog.exec() != QDialog::Accepted)
            return;

        switch (dialog.selectedAction()) {
        case ControlCenterDialog::Action::Capture:
            onCaptureRequested();
            break;
        case ControlCenterDialog::Action::Settings:
            onSettingsRequested();
            break;
        case ControlCenterDialog::Action::About:
            onAboutRequested();
            break;
        case ControlCenterDialog::Action::Quit:
            onQuitAction();
            break;
        case ControlCenterDialog::Action::None:
            break;
        }
    }

    void onFixPrintScreenConflict()
    {
        if (!HotkeyManager::setWindowsPrintScreenSnippingEnabled(false)) {
            if (m_trayIcon) {
                m_trayIcon->showMessage(
                    TranslationManager::errTitle(),
                    TranslationManager::printScreenConflictMessage(),
                    QSystemTrayIcon::Warning,
                    7000);
            }
            return;
        }

        if (m_trayIcon) {
            m_trayIcon->showMessage(
                TranslationManager::notifCaptureTitle(),
                TranslationManager::printScreenConflictDisabled(),
                QSystemTrayIcon::Information,
                4000);
        }
        HotkeyManager::instance().reRegisterCaptureHotkey(
            HotkeyManager::instance().captureModifiers(),
            HotkeyManager::instance().captureVirtualKey());
        rebuildTrayMenu();
    }

    void onAboutRequested()
    {
        AboutDialog dlg;
        if (m_updateManager) {
            const auto refreshAboutUpdate = [this, &dlg]() {
                dlg.setUpdateInfo(m_updateManager->updateAvailable(),
                                  m_updateManager->latestVersion(),
                                  m_updateManager->isBusy(),
                                  m_updateManager->statusText());
            };
            refreshAboutUpdate();
            connect(&dlg, &AboutDialog::checkForUpdatesRequested, this, [this]() {
                if (m_updateManager) m_updateManager->checkForUpdates(true);
            });
            connect(&dlg, &AboutDialog::updateRequested, this, &EShotApp::onUpdateRequested);
            connect(m_updateManager, &UpdateManager::statusChanged, &dlg, refreshAboutUpdate);
            connect(m_updateManager, &UpdateManager::updateCheckFinished, &dlg,
                    [refreshAboutUpdate](bool, const QString &) { refreshAboutUpdate(); });
        }
        dlg.show();
        QApplication::processEvents();
        if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
            if (!screen) screen = QGuiApplication::primaryScreen();
            QRect avail = screen->availableGeometry();
            
            dlg.resize(dlg.minimumSizeHint());
            QApplication::processEvents();
            
            int nx = avail.center().x() - dlg.width() / 2;
            int ny = avail.center().y() - dlg.height() / 2;
            ny = qMax(avail.top() + 40, ny);
            dlg.move(nx, ny);
        }
        dlg.exec();
    }

    void onCloseAllPins()
    {
        for (auto &w : m_pinnedWindows) {
            if (w) w->close();
        }
        m_pinnedWindows.clear();
    }

    void onRecordGifRequested()
    {
        if (m_screenRecorder && m_screenRecorder->isRecording()) {
            m_screenRecorder->stop();
            return;
        }
        if (m_overlay && m_overlay->isVisible()) return;
        m_pendingMode = 2;
        ensureOverlay();
        m_overlay->startCaptureForRecording();
    }

    void onRecordVideoRequested()
    {
        if (m_videoRecorder && m_videoRecorder->isRecording()) {
            m_videoRecorder->stop();
            return;
        }
        if (m_overlay && m_overlay->isVisible()) return;
        m_pendingMode = 3;
        ensureOverlay();
        m_overlay->startCaptureForRecording();
    }

    void onInstantCaptureRequested()
    {
        if (closeBlockingDialogs()) {
            QTimer::singleShot(80, this, &EShotApp::onInstantCaptureRequested);
            return;
        }
        if (m_overlay && m_overlay->isVisible()) return;
        ensureOverlay();
        m_overlay->startInstantCapture();
    }

    void onRecordVideoSelected(QRect rect, QRect displayRect = QRect())
    {
        if (rect.isEmpty()) return;
        if (m_videoRecorder && m_videoRecorder->isRecording()) {
            m_videoRecorder->stop();
            return;
        }
        if (m_videoRecorder) { m_videoRecorder->deleteLater(); m_videoRecorder = nullptr; }
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator->deleteLater(); m_recordingIndicator = nullptr; }

        m_videoRecorder = new VideoRecorder(this);
        connect(m_videoRecorder, &VideoRecorder::recordingStarted, this, &EShotApp::onVideoRecordingStarted);
        connect(m_videoRecorder, &VideoRecorder::recordingStopped, this, &EShotApp::onVideoRecordingStopped);
        connect(m_videoRecorder, &VideoRecorder::recordingFailed, this, &EShotApp::onVideoRecordingFailed);
        connect(m_videoRecorder, &VideoRecorder::remainingTimeChanged, this, [this](int sec) {
            if (m_recordingIndicator) m_recordingIndicator->setRemainingSeconds(sec);
        });
        connect(m_videoRecorder, &VideoRecorder::elapsedTimeChanged, this, [this](int sec) {
            if (m_recordingIndicator) m_recordingIndicator->setElapsedSeconds(sec);
        });
        connect(m_videoRecorder, &VideoRecorder::pausedChanged, this, [this](bool paused) {
            if (m_recordingIndicator) m_recordingIndicator->setPaused(paused);
        });

        QSettings s("EShot", "EShot");
        const int fps = s.value("videoRecordingFps", 30).toInt();
        const int maxSec = s.value("videoRecordingMaxSeconds", 0).toInt();
        const int crf = s.value("videoRecordingCrf", 24).toInt();
        const bool desktopAudio = loadRecordingAudioEnabled(s, RecordingAudioSource::Desktop);
        const int desktopVolume = s.value("videoDesktopAudioVolume", 80).toInt();
        const QString desktopDevice = s.value("videoDesktopAudioDevice",
#ifdef Q_OS_WIN
                                            "__wasapi__"
#else
                                            "@DEFAULT_SINK@.monitor"
#endif
                                            ).toString();
        const bool microphoneAudio = loadRecordingAudioEnabled(s, RecordingAudioSource::Microphone);
        const int microphoneVolume = s.value("videoMicrophoneVolume", 80).toInt();
        const QString microphoneDevice = s.value("videoMicrophoneDevice", "default").toString();
        const int startDelayMs = qBound(0, s.value("recordingStartDelaySeconds", 0).toInt(), 10) * 1000;
        auto startVideo = [rec = QPointer<VideoRecorder>(m_videoRecorder), rect, fps, maxSec, crf,
                           desktopAudio, desktopVolume, desktopDevice,
                           microphoneAudio, microphoneVolume, microphoneDevice, displayRect]() {
            if (!rec)
                return;
            rec->start(rect, fps, maxSec, crf,
                       desktopAudio, desktopVolume, desktopDevice,
                       microphoneAudio, microphoneVolume,
                       microphoneDevice,
                       QString(),
                       displayRect);
        };
        if (startDelayMs > 0)
            QTimer::singleShot(startDelayMs, this, startVideo);
        else
            startVideo();
    }

    void onRecordGifSelected(QRect rect, QRect displayRect = QRect())
    {
        if (rect.isEmpty()) return;
        if (m_screenRecorder) { m_screenRecorder->stop(); m_screenRecorder->deleteLater(); m_screenRecorder = nullptr; }
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator->deleteLater(); m_recordingIndicator = nullptr; }
        m_screenRecorder = new ScreenRecorder(this);
        connect(m_screenRecorder, &ScreenRecorder::recordingStarted, this, &EShotApp::onRecordingStarted);
        connect(m_screenRecorder, &ScreenRecorder::recordingStopped, this, &EShotApp::onRecordingStopped);
        connect(m_screenRecorder, &ScreenRecorder::recordingFailed, this, &EShotApp::onRecordingFailed);
        connect(m_screenRecorder, &ScreenRecorder::frameCaptured, this, [this](int n) {
            if (m_recordingIndicator) m_recordingIndicator->setFrameCount(n);
        });
        connect(m_screenRecorder, &ScreenRecorder::remainingTimeChanged, this, [this](int sec) {
            if (m_recordingIndicator) m_recordingIndicator->setRemainingSeconds(sec);
        });
        connect(m_screenRecorder, &ScreenRecorder::elapsedTimeChanged, this, [this](int sec) {
            if (m_recordingIndicator) m_recordingIndicator->setElapsedSeconds(sec);
        });
        connect(m_screenRecorder, &ScreenRecorder::pausedChanged, this, [this](bool paused) {
            if (m_recordingIndicator) m_recordingIndicator->setPaused(paused);
        });

        QSettings s("EShot", "EShot");
        int fps = s.value("recordingFps", 10).toInt();
        int maxSec = s.value("recordingMaxSeconds", 30).toInt();
        int loop = s.value("recordingLoop", 0).toInt();
        const int startDelayMs = qBound(0, s.value("recordingStartDelaySeconds", 0).toInt(), 10) * 1000;
        auto startGif = [rec = QPointer<ScreenRecorder>(m_screenRecorder), rect, fps, maxSec, loop, displayRect]() {
            if (rec)
                rec->start(rect, fps, maxSec, loop, QString(), displayRect);
        };
        if (startDelayMs > 0)
            QTimer::singleShot(startDelayMs, this, startGif);
        else
            startGif();
    }

    void onRecordingStarted()
    {
        if (m_screenRecorder) {
            m_recordingIndicator = new RecordingIndicator(
                m_screenRecorder->captureRect(), nullptr, 2, true,
                RecordingIndicatorMode::Gif);
            QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
            m_recordingIndicator->setDetails({
                QStringLiteral("%1 × %2")
                    .arg(m_screenRecorder->captureRect().width())
                    .arg(m_screenRecorder->captureRect().height()),
                QStringLiteral("%1 FPS").arg(settings.value("recordingFps", 10).toInt())
            });
            m_recordingIndicator->setShortcutHints(
                HotkeyManager::instance().recordingPauseShortcutText(),
                HotkeyManager::instance().recordingStopShortcutText(),
                HotkeyManager::instance().recordingCancelShortcutText());
            connect(m_recordingIndicator, &RecordingIndicator::stopRequested, this, [this]() {
                if (m_screenRecorder && m_screenRecorder->isRecording())
                    m_screenRecorder->stop();
            });
            connect(m_recordingIndicator, &RecordingIndicator::pauseRequested, this, [this]() {
                if (m_screenRecorder && m_screenRecorder->isRecording())
                    m_screenRecorder->pause();
            });
            connect(m_recordingIndicator, &RecordingIndicator::resumeRequested, this, [this]() {
                if (m_screenRecorder && m_screenRecorder->isRecording())
                    m_screenRecorder->resume();
            });
            connect(m_recordingIndicator, &RecordingIndicator::cancelRequested, this, [this]() {
                if (m_screenRecorder && m_screenRecorder->isRecording())
                    m_screenRecorder->cancel();
                if (m_recordingIndicator) {
                    m_recordingIndicator->stop();
                    m_recordingIndicator = nullptr;
                }
            });
            m_recordingIndicator->startCaptureSafePresentation();
        }
        rebuildTrayMenu();
    }

    void onRecordingStopped(QString outputPath)
    {
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
        if (m_trayIcon && m_showNotifications && m_notifyGif) {
            QFileInfo fi(outputPath);
            showSuccessNotification(TranslationManager::recordingSaved() + QStringLiteral("\n") + QDir::toNativeSeparators(fi.absoluteFilePath()),
                                    fi.absoluteFilePath(), 5000);
        }
        rebuildTrayMenu();
    }

    void onRecordingFailed(QString reason)
    {
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
        m_lastNotificationPath.clear();
        reason = localizedRecordingFailureReason(reason);
        showFailureNotification(TranslationManager::recordingFailed() + QStringLiteral(": ") + reason,
                                3000);
        rebuildTrayMenu();
    }

    void onVideoRecordingStarted()
    {
        if (m_videoRecorder) {
            m_recordingIndicator = new RecordingIndicator(
                m_videoRecorder->captureRect(), nullptr, 2, true,
                RecordingIndicatorMode::Video);
            QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
            const bool desktopAudio = settings.value("videoDesktopAudioEnabled", false).toBool();
            const bool microphone = settings.value("videoMicrophoneEnabled", false).toBool();
            const QString audio = desktopAudio && microphone
                ? TranslationManager::audioDesktopMic()
                : desktopAudio ? TranslationManager::audioDesktop()
                               : microphone ? TranslationManager::audioMicrophone()
                                            : TranslationManager::audioNone();
            m_recordingIndicator->setDetails({
                QStringLiteral("%1 × %2")
                    .arg(m_videoRecorder->captureRect().width())
                    .arg(m_videoRecorder->captureRect().height()),
                QStringLiteral("%1 FPS").arg(settings.value("videoRecordingFps", 30).toInt()),
                audio
            });
            m_recordingIndicator->setShortcutHints(
                HotkeyManager::instance().recordingPauseShortcutText(),
                HotkeyManager::instance().recordingStopShortcutText(),
                HotkeyManager::instance().recordingCancelShortcutText());
            connect(m_recordingIndicator, &RecordingIndicator::stopRequested, this, [this]() {
                if (m_videoRecorder && m_videoRecorder->isRecording())
                    m_videoRecorder->stop();
            });
            connect(m_recordingIndicator, &RecordingIndicator::pauseRequested, this, [this]() {
                if (m_videoRecorder && m_videoRecorder->isRecording())
                    m_videoRecorder->pause();
            });
            connect(m_recordingIndicator, &RecordingIndicator::resumeRequested, this, [this]() {
                if (m_videoRecorder && m_videoRecorder->isRecording())
                    m_videoRecorder->resume();
            });
            connect(m_recordingIndicator, &RecordingIndicator::cancelRequested, this, [this]() {
                if (m_videoRecorder && m_videoRecorder->isRecording())
                    m_videoRecorder->cancel();
                if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
            });
            m_recordingIndicator->startCaptureSafePresentation();
        }
        rebuildTrayMenu();
    }

    void onVideoRecordingStopped(QString outputPath)
    {
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
        if (m_trayIcon && m_showNotifications && m_notifyVideo) {
            QFileInfo fi(outputPath);
            showSuccessNotification(TranslationManager::videoSaved() + QStringLiteral("\n") + QDir::toNativeSeparators(fi.absoluteFilePath()),
                                    fi.absoluteFilePath(), 5000);
        }
        rebuildTrayMenu();
    }

    void onVideoRecordingFailed(QString reason)
    {
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
        m_lastNotificationPath.clear();
        reason = localizedRecordingFailureReason(reason);
        showFailureNotification(TranslationManager::videoFailed() + QStringLiteral(": ") + reason,
                                5000);
        rebuildTrayMenu();
    }

private:
    void loadSettings()
    {
        QSettings s("EShot", "EShot");
        m_showNotifications = s.value("showNotifications", true).toBool();
        m_notifyCopy = s.value("notifyCopy", false).toBool();
        m_notifySave = s.value("notifySave", true).toBool();
        m_notifyGif = s.value("notifyGif", true).toBool();
        m_notifyVideo = s.value("notifyVideo", true).toBool();
        m_notificationOpenFolder = s.value("notificationOpenFolder", true).toBool();
        m_blackTrayIcon = s.value("blackTrayIcon", false).toBool();
    }

    void setupUpdater()
    {
        m_updateManager = new UpdateManager(this);
        connect(m_updateManager, &UpdateManager::updateCheckFinished, this,
                [this](bool available, const QString &version) {
            m_updateAvailable = available;
            m_latestVersion = version;
            if (available)
                setTrayIconUpdate();
            else
                setTrayIconNormal();
            rebuildTrayMenu();
        });
        connect(m_updateManager, &UpdateManager::statusChanged, this, [this]() {
            if (!m_updateManager) return;
            m_updateAvailable = m_updateManager->updateAvailable();
            m_latestVersion = m_updateManager->latestVersion();
            if (m_updateAvailable)
                setTrayIconUpdate();
            else
                setTrayIconNormal();
            rebuildTrayMenu();
        });
        connect(m_updateManager, &UpdateManager::failed, this, [this](const QString &message) {
            if (m_trayIcon && m_showNotifications && !m_updateManager->isSilentUpdate()) {
                m_trayIcon->showMessage(
                    TranslationManager::errTitle(),
                    TranslationManager::updateStatusFailed(message),
                    QSystemTrayIcon::Warning,
                    7000);
            }
        });
        connect(m_updateManager, &UpdateManager::installerLaunched, this, [this]() {
            if (m_trayIcon && !m_updateManager->isSilentUpdate()) {
                m_trayIcon->showMessage(
                    TranslationManager::updateTitle(),
                    TranslationManager::updateStatusRestarting(),
                    QSystemTrayIcon::Information,
                    4000);
            }
        });
    }

    static QIcon trayIcon(const QString &path, const QSize &size = QSize(16, 16))
    {
        QIcon src(path);
        if (src.isNull()) return src;
        QPixmap pm = src.pixmap(size, QIcon::Normal, QIcon::On);
        QIcon out;
        out.addPixmap(pm);
        return out;
    }

    void rebuildTrayMenu()
    {
        if (!m_trayMenu) return;
        m_trayMenu->clear();

        QAction *captureAction = m_trayMenu->addAction(trayIcon(":/icons/copy.svg"), TranslationManager::trayCapture());
        QSettings hotkeySettings("EShot", "EShot");
        const UINT captureModifiers = static_cast<UINT>(hotkeySettings.value("hotkeyModifiers", 0).toUInt());
        const UINT captureVirtualKey = static_cast<UINT>(hotkeySettings.value("hotkeyVKey", VK_SNAPSHOT).toUInt());
        captureAction->setToolTip(QStringLiteral("%1 (%2)").arg(
            TranslationManager::trayCapture(),
            HotkeyManager::shortcutText(captureModifiers, captureVirtualKey)));
        connect(captureAction, &QAction::triggered, this, &EShotApp::onCaptureRequested);
#ifdef Q_OS_WIN
        QAction *windowCaptureAction = m_trayMenu->addAction(
            trayIcon(":/icons/rectangle.svg"), TranslationManager::trayWindowCapture());
        windowCaptureAction->setToolTip(QStringLiteral("%1 (%2)").arg(
            TranslationManager::trayWindowCapture(),
            HotkeyManager::shortcutText(
                static_cast<UINT>(hotkeySettings.value("windowCaptureHotkeyModifiers", MOD_SHIFT).toUInt()),
                static_cast<UINT>(hotkeySettings.value("windowCaptureHotkeyVKey", VK_SNAPSHOT).toUInt()))));
        connect(windowCaptureAction, &QAction::triggered,
                this, &EShotApp::onWindowCaptureRequested);
#endif

        const bool videoRecording = m_videoRecorder && m_videoRecorder->isRecording();
        const bool gifRecording = m_screenRecorder && m_screenRecorder->isRecording();
        if (videoRecording || gifRecording) {
            QAction *cancelRecordingAction = m_trayMenu->addAction(
                trayIcon(":/icons/close.svg"), TranslationManager::trayCancelRecording());
            connect(cancelRecordingAction, &QAction::triggered, this, [this]() {
                if (m_videoRecorder && m_videoRecorder->isRecording())
                    m_videoRecorder->cancel();
                else if (m_screenRecorder && m_screenRecorder->isRecording())
                    m_screenRecorder->cancel();
                if (m_recordingIndicator) {
                    m_recordingIndicator->stop();
                    m_recordingIndicator = nullptr;
                }
            });
            m_trayMenu->addSeparator();
        }

        if (hasPrintScreenConflict()) {
            QAction *fixPrintScreenAction = m_trayMenu->addAction(
                trayIcon(":/icons/gear.svg"),
                TranslationManager::printScreenConflictFix());
            connect(fixPrintScreenAction, &QAction::triggered,
                    this, &EShotApp::onFixPrintScreenConflict);
            m_trayMenu->addSeparator();
        }

        if (m_updateAvailable) {
            const QString updateText = m_updateManager && m_updateManager->isBusy()
                ? m_updateManager->statusText()
                : QString("%1 v%2").arg(TranslationManager::updateNow(), m_latestVersion);
            QAction *updateAction = m_trayMenu->addAction(
                trayIcon(":/icons/pen_tray_update.svg"),
                updateText);
            updateAction->setEnabled(!m_updateManager || !m_updateManager->isBusy());
            connect(updateAction, &QAction::triggered, this, &EShotApp::onUpdateRequested);
            m_trayMenu->addSeparator();
        }

        QAction *settingsAction = m_trayMenu->addAction(trayIcon(":/icons/gear.svg"), TranslationManager::traySettings());
        connect(settingsAction, &QAction::triggered, this, &EShotApp::onSettingsRequested);
        QAction *aboutAction = m_trayMenu->addAction(trayIcon(":/icons/pen.svg"), TranslationManager::trayAbout());
        connect(aboutAction, &QAction::triggered, this, &EShotApp::onAboutRequested);
        QAction *quitAction = m_trayMenu->addAction(trayIcon(":/icons/close.svg"), TranslationManager::trayQuit());
        connect(quitAction, &QAction::triggered, this, &EShotApp::onQuitAction);

        m_trayIcon->setToolTip(QString("%1 v%2").arg(TranslationManager::appTitle(), QCoreApplication::applicationVersion()));
    }

    void setupTrayIcon()
    {
        m_trayIcon = new QSystemTrayIcon(this);

        setTrayIconNormal();
        m_trayIcon->setToolTip(QString("%1 v%2").arg(TranslationManager::appTitle(), QCoreApplication::applicationVersion()));

        m_trayMenu = new QMenu();
        m_trayMenu->setToolTipsVisible(true);
        m_trayMenu->setStyleSheet(QStringLiteral(
            "QMenu {"
            "  background: #2b2b2b;"
            "  color: #ffffff;"
            "  border: 1px solid #4a4a4a;"
            "  border-radius: 3px;"
            "  padding: 3px;"
            "}"
            "QMenu::item {"
            "  min-height: 22px;"
            "  padding: 3px 18px 3px 24px;"
            "  border-radius: 2px;"
            "}"
            "QMenu::item:selected { background: #3a3a3a; }"
            "QMenu::item:disabled { color: #8a8a8a; }"
            "QMenu::icon { width: 16px; height: 16px; left: 5px; }"
            "QMenu::separator { height: 1px; background: #424242; margin: 4px 5px; }"));
        rebuildTrayMenu();

        m_trayIcon->setContextMenu(m_trayMenu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, &EShotApp::onTrayActivated);
        connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, &EShotApp::onNotificationClicked);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        m_linuxNotification = new LinuxDesktopNotification(this);
        connect(m_linuxNotification, &LinuxDesktopNotification::pathActivated,
                this, [this](const QString &path) {
            m_lastNotificationPath = path;
            onNotificationClicked();
        });
#endif
        m_trayIcon->show();
    }

    void ensureOverlay()
    {
        if (m_overlay) return;
        m_overlay = new CaptureOverlay();
        connect(m_overlay, &CaptureOverlay::captureCompleted, this, &EShotApp::onCaptureCompleted);
        connect(m_overlay, &CaptureOverlay::captureSaved, this, &EShotApp::onCaptureSaved);
        connect(m_overlay, &CaptureOverlay::captureCancelled, this, &EShotApp::onCaptureCancelled);
        connect(m_overlay, &CaptureOverlay::regionSelected, this, &EShotApp::onRegionSelected);
        connect(m_overlay, &CaptureOverlay::gifCaptureRequested, this, &EShotApp::onRecordGifSelected);
        connect(m_overlay, &CaptureOverlay::videoCaptureRequested, this, &EShotApp::onRecordVideoSelected);
        connect(m_overlay, &CaptureOverlay::pinnedWindowCreated, this, [this](PinnedWindow *w) {
            m_pinnedWindows.append(QPointer<PinnedWindow>(w));
        });
        // Pre-warm: force first paint of overlay + toolbar offscreen at startup
        // to avoid 2-3 s stall on first user capture.
        m_overlay->prewarm();
    }

    void setupHotkey()
    {
        connect(&HotkeyManager::instance(), &HotkeyManager::captureRequested,
                this, &EShotApp::onCaptureRequested);
        connect(&HotkeyManager::instance(), &HotkeyManager::instantCaptureRequested,
                this, &EShotApp::onInstantCaptureRequested);
        connect(&HotkeyManager::instance(), &HotkeyManager::gifCaptureRequested,
                this, &EShotApp::onRecordGifRequested);
        connect(&HotkeyManager::instance(), &HotkeyManager::videoCaptureRequested,
                this, &EShotApp::onRecordVideoRequested);
        connect(&HotkeyManager::instance(), &HotkeyManager::windowCaptureRequested,
                this, &EShotApp::onWindowCaptureRequested);
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingPauseRequested, this, [this]() {
            if (m_videoRecorder && m_videoRecorder->isRecording()) {
                if (m_videoRecorder->isPaused()) m_videoRecorder->resume();
                else m_videoRecorder->pause();
            } else if (m_screenRecorder && m_screenRecorder->isRecording()) {
                if (m_screenRecorder->isPaused()) m_screenRecorder->resume();
                else m_screenRecorder->pause();
            }
        });
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingStopRequested, this, [this]() {
            if (m_videoRecorder && m_videoRecorder->isRecording()) m_videoRecorder->stop();
            else if (m_screenRecorder && m_screenRecorder->isRecording()) m_screenRecorder->stop();
        });
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingCancelRequested, this, [this]() {
            if (m_videoRecorder && m_videoRecorder->isRecording()) {
                m_videoRecorder->cancel();
                if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
            } else if (m_screenRecorder && m_screenRecorder->isRecording()) {
                m_screenRecorder->cancel();
                if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator = nullptr; }
            }
        });
    }

    bool closeBlockingDialogs()
    {
        bool closed = false;
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets) {
            if (!widget || widget == m_overlay || !widget->isVisible())
                continue;
            auto *dialog = qobject_cast<QDialog *>(widget);
            if (!dialog)
                continue;
            dialog->reject();
            closed = true;
        }
        if (closed)
            QApplication::processEvents();
        return closed;
    }

    bool hasPrintScreenConflict() const
    {
#ifdef Q_OS_WIN
        return HotkeyManager::isPlainPrintScreen(
                   static_cast<UINT>(QSettings("EShot", "EShot").value("hotkeyModifiers", 0).toUInt()),
                   static_cast<UINT>(QSettings("EShot", "EShot").value("hotkeyVKey", VK_SNAPSHOT).toUInt()))
            && HotkeyManager::isWindowsPrintScreenSnippingEnabled();
#else
        return false;
#endif
    }

    void checkForUpdates()
    {
        if (m_updateManager)
            m_updateManager->checkForUpdates(false);
    }

    static bool isNewerVersion(const QString &latest, const QString &current)
    {
        QVersionNumber latestVersion = QVersionNumber::fromString(latest.trimmed());
        QVersionNumber currentVersion = QVersionNumber::fromString(current.trimmed());
        if (latestVersion.isNull() || currentVersion.isNull())
            return latest.trimmed() != current.trimmed();
        return QVersionNumber::compare(latestVersion, currentVersion) > 0;
    }

    void setTrayIconUpdate()
    {
        if (!m_trayIcon) return;
        m_trayIcon->setIcon(QIcon(m_blackTrayIcon
            ? QStringLiteral(":/icons/pen_tray_update_black.svg")
            : QStringLiteral(":/icons/pen_tray_update.svg")));
        m_trayIcon->setToolTip(QString("%1 v%2 — %3").arg(
            TranslationManager::appTitle(),
            QCoreApplication::applicationVersion(),
            TranslationManager::updateTitle()));
    }

    void setTrayIconNormal()
    {
        if (!m_trayIcon) return;
        QIcon trayIcon(m_blackTrayIcon
            ? QStringLiteral(":/icons/pen_tray_black.svg")
            : QStringLiteral(":/icons/pen_tray.svg"));
        if (trayIcon.isNull()) {
            QPixmap pix(32, 32); pix.fill(Qt::blue);
            trayIcon = QIcon(pix);
        }
        m_trayIcon->setIcon(trayIcon);
        m_trayIcon->setToolTip(QString("%1 v%2").arg(TranslationManager::appTitle(), QCoreApplication::applicationVersion()));
    }

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    UpdateManager *m_updateManager = nullptr;
    CaptureOverlay *m_overlay = nullptr;
    bool m_showNotifications = true;
    bool m_notifyCopy = false;
    bool m_notifySave = true;
    bool m_notifyGif = true;
    bool m_notifyVideo = true;
    bool m_notificationOpenFolder = true;
    bool m_blackTrayIcon = false;
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QString m_latestReleaseUrl;
    QString m_lastNotificationPath;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    LinuxDesktopNotification *m_linuxNotification = nullptr;
#endif
    bool m_skipNextCaptureNotification = false;
    bool m_hotkeysInitialized = false;
    QList<QPointer<PinnedWindow>> m_pinnedWindows;
    ScreenRecorder *m_screenRecorder = nullptr;
    VideoRecorder *m_videoRecorder = nullptr;
    RecordingIndicator *m_recordingIndicator = nullptr;
    int m_pendingMode = 0;
};

#include "main.moc"

#include "recording/GifEncoder.h"
#include "core/OcrEngine.h"
#include <QFile>
#include <QFileInfo>
#include <QPainter>
#include <QTextStream>

static void writeTestLog(const QString &msg)
{
    QFile f("test_log.txt");
    if (f.open(QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&f);
        out << msg << "\n";
    }
    qDebug() << msg;
}

static void runGifEncoderTest()
{
    QFile::remove("test_log.txt");
    QFile::remove("test_output.gif");
    writeTestLog("[TEST] GIF encoder test starting...");
    const int W = 64;
    const int H = 64;
    GifEncoder enc;
    if (!enc.open("test_output.gif", W, H, 0)) {
        writeTestLog(QString("[TEST] FAIL: open failed: %1").arg(enc.errorString()));
        return;
    }
    writeTestLog("[TEST] open OK");
    for (int i = 0; i < 4; ++i) {
        QImage img(W, H, QImage::Format_RGB32);
        img.fill(Qt::white);
        QPainter p(&img);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(i * 60, 100, 200 - i * 50));
        p.drawRect(i * 8, i * 8, 32, 32);
        p.setPen(Qt::black);
        p.drawText(4, 56, QString("Frame %1").arg(i));
        p.end();
        if (!enc.addFrame(img, 10)) {
            writeTestLog(QString("[TEST] FAIL: addFrame %1: %2").arg(i).arg(enc.errorString()));
            return;
        }
        writeTestLog(QString("[TEST] frame %1 added").arg(i));
    }
    if (!enc.close()) {
        writeTestLog(QString("[TEST] FAIL: close: %1").arg(enc.errorString()));
        return;
    }
    writeTestLog("[TEST] close OK");
    QFileInfo fi("test_output.gif");
    if (!fi.exists()) {
        writeTestLog("[TEST] FAIL: output file missing");
        return;
    }
    writeTestLog(QString("[TEST] Output size: %1 bytes").arg(fi.size()));
    QFile f("test_output.gif");
    if (!f.open(QIODevice::ReadOnly)) {
        writeTestLog("[TEST] FAIL: cannot read output");
        return;
    }
    QByteArray header = f.read(6);
    f.close();
    if (header != "GIF89a" && header != "GIF87a") {
        writeTestLog(QString("[TEST] FAIL: BAD HEADER: %1").arg(QString::fromLatin1(header)));
        return;
    }
    writeTestLog(QString("[TEST] GIF signature OK: %1").arg(QString::fromLatin1(header)));
    writeTestLog("[TEST] GIF ENCODER TEST PASSED");
}

static void runGifRecordingTest()
{
    QFile::remove("test_log.txt");
    QFile::remove("test_record_output.gif");
    writeTestLog("[TEST] GIF recording test starting...");

    QLabel pattern;
    pattern.setWindowFlags(Qt::Tool | Qt::WindowStaysOnTopHint);
    pattern.setText("EShot GIF TEST\nColor bars");
    pattern.setAlignment(Qt::AlignCenter);
    pattern.setStyleSheet(
        "QLabel { color: white; font: bold 22px Segoe UI; "
        "background: qlineargradient(x1:0,y1:0,x2:1,y2:1, "
        "stop:0 #ff3355, stop:0.33 #1fa2ff, stop:0.66 #20c997, stop:1 #ffd43b); }");
    pattern.resize(320, 180);
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect sg = screen ? screen->geometry() : QRect(100, 100, 800, 600);
    pattern.move(sg.center() - QPoint(pattern.width() / 2, pattern.height() / 2));
    pattern.show();
    pattern.raise();
    QCoreApplication::processEvents();

    QRect rect = pattern.frameGeometry();
    writeTestLog(QString("[TEST] capture rect: %1,%2 %3x%4")
        .arg(rect.x()).arg(rect.y()).arg(rect.width()).arg(rect.height()));

    ScreenRecorder recorder;
    QEventLoop loop;
    bool done = false;
    bool ok = false;
    QString outputPath;

    QObject::connect(&recorder, &ScreenRecorder::recordingStopped,
                     [&loop, &done, &ok, &outputPath](const QString &path) {
        outputPath = path;
        done = true;
        ok = true;
        loop.quit();
    });
    QObject::connect(&recorder, &ScreenRecorder::recordingFailed,
                     [&loop, &done](const QString &reason) {
        writeTestLog(QString("[TEST] FAIL: recorder failed: %1").arg(reason));
        done = true;
        loop.quit();
    });
    QObject::connect(&recorder, &ScreenRecorder::frameCaptured,
                     [](int frame) {
        if (frame == 1) writeTestLog("[TEST] first frame captured");
    });

    QTimer::singleShot(300, [&recorder, rect]() {
        recorder.start(rect, 5, 1, 0, "test_record_output.gif");
    });
    QTimer::singleShot(5000, &loop, [&loop, &done]() {
        if (!done) {
            writeTestLog("[TEST] FAIL: recording timeout");
            done = true;
            loop.quit();
        }
    });
    loop.exec();

    if (!ok) return;

    QFileInfo fi(outputPath);
    if (!fi.exists() || fi.size() <= 16) {
        writeTestLog("[TEST] FAIL: output file missing or too small");
        return;
    }
    QFile f(outputPath);
    if (!f.open(QIODevice::ReadOnly)) {
        writeTestLog("[TEST] FAIL: cannot read output");
        return;
    }
    QByteArray header = f.read(6);
    f.close();
    if (header != "GIF89a" && header != "GIF87a") {
        writeTestLog(QString("[TEST] FAIL: BAD HEADER: %1").arg(QString::fromLatin1(header)));
        return;
    }
    writeTestLog(QString("[TEST] Output size: %1 bytes").arg(fi.size()));
    writeTestLog(QString("[TEST] GIF signature OK: %1").arg(QString::fromLatin1(header)));
    writeTestLog("[TEST] GIF RECORDING TEST PASSED");
}

static void runOcrTest(const QString &imagePath)
{
    QFile::remove("test_log.txt");
    writeTestLog(QString("[TEST] OCR test starting with image: %1").arg(imagePath));
    if (!QFile::exists(imagePath)) {
        writeTestLog("[TEST] FAIL: image does not exist");
        return;
    }
    QImage img(imagePath);
    if (img.isNull()) {
        writeTestLog("[TEST] FAIL: cannot load image");
        return;
    }
    writeTestLog(QString("[TEST] image size: %1x%2").arg(img.width()).arg(img.height()));

    OcrEngine engine;
    QEventLoop loop;
    bool done = false;
    QObject::connect(&engine, &OcrEngine::textReady, [&loop, &done](const QString &text) {
        writeTestLog(QString("[TEST] OCR result length: %1").arg(text.size()));
        writeTestLog(QString("[TEST] OCR result: %1").arg(text));
        done = true;
        loop.quit();
    });
    QObject::connect(&engine, &OcrEngine::failed, [&loop, &done](const QString &reason) {
        writeTestLog(QString("[TEST] OCR failed: %1").arg(reason));
        done = true;
        loop.quit();
    });
    engine.recognize(QPixmap::fromImage(img), QStringLiteral("auto"),
                     TranslationManager::langCode());
    QTimer::singleShot(30000, &loop, [&loop, &done]() {
        if (!done) {
            writeTestLog("[TEST] OCR timeout (30s)");
            loop.quit();
        }
    });
    loop.exec();
}

int main(int argc, char *argv[])
{
    // Qt 6 handles High-DPI automatically. Legacy overrides removed to prevent Windows ARM DWM corruption.
    QApplication app(argc, argv);
    app.setApplicationName("EShot");
    app.setApplicationVersion(ESHOT_VERSION_STRING);
    app.setOrganizationName("EShot");
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    app.setDesktopFileName(QStringLiteral("io.github.benoks.EShot"));
    // AppImages are unsandboxed host applications. Register this D-Bus peer
    // before any portal method call so GlobalShortcuts receives a stable app id.
    LinuxPortalHostRegistry::registerApplication();
#endif
    app.setQuitOnLastWindowClosed(false);
    app.setStyle("Fusion");
    app.setWindowIcon(QIcon(":/icons/pen.svg"));

    QPixmapCache::setCacheLimit(4096);

        // Command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("EShot - Screenshot Tool");
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption captureOption("capture", "Capture screenshot immediately.");
    parser.addOption(captureOption);
    QCommandLineOption settingsOption("settings", "Open EShot settings.");
    parser.addOption(settingsOption);
    QCommandLineOption controlOption("control", "Open the EShot control menu.");
    parser.addOption(controlOption);
    QCommandLineOption quitOption("quit", "Quit the running EShot instance.");
    parser.addOption(quitOption);
    QCommandLineOption saveOption("save", "Save screenshot to specified path.", "path");
    parser.addOption(saveOption);
    QCommandLineOption silentOption("silent", "Start silently in the background (used by autostart).");
    parser.addOption(silentOption);
    QCommandLineOption testGifOption("test-gif", "Run internal GIF encoder test and exit.");
    parser.addOption(testGifOption);
    QCommandLineOption testRecordGifOption("test-record-gif", "Run internal GIF recording test and exit.");
    parser.addOption(testRecordGifOption);
    QCommandLineOption testOcrOption("test-ocr", "Run internal OCR test and exit. Requires a PNG path.", "path");
    parser.addOption(testOcrOption);
    parser.process(app);

    if (parser.isSet(testGifOption)) {
        runGifEncoderTest();
        return 0;
    }
    if (parser.isSet(testRecordGifOption)) {
        runGifRecordingTest();
        return 0;
    }
    if (parser.isSet(testOcrOption)) {
        runOcrTest(parser.value(testOcrOption));
        return 0;
    }

    QSettings settings("EShot", "EShot");
    bool highContrast = settings.value("highContrast", false).toBool();
    bool darkMode = settings.value("darkMode", true).toBool();

    applyEShotApplicationTheme(app, darkMode, highContrast);

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "[EShot] System tray is not available yet; keeping the app alive for startup.";
    }

    const QString instanceName = QStringLiteral("EShot.SingleInstance");
    QLocalSocket instanceSocket;
    instanceSocket.connectToServer(instanceName);
    if (instanceSocket.waitForConnected(150)) {
        const auto command = parser.isSet(controlOption)
            ? ApplicationInstanceCommand::Control
            : ApplicationInstanceCommand::fromInvocation(
                parser.isSet(captureOption), parser.isSet(settingsOption),
                parser.isSet(saveOption), parser.isSet(quitOption), true);
        const QByteArray wireCommand = ApplicationInstanceCommand::toWire(command);
        if (!wireCommand.isEmpty()) {
            instanceSocket.write(wireCommand);
            instanceSocket.waitForBytesWritten(500);
        }
        instanceSocket.disconnectFromServer();
        qDebug() << "[EShot] Forwarded command to the running instance:"
                 << wireCommand.trimmed();
        return 0;
    }

    if (parser.isSet(quitOption))
        return 0;

    QLocalServer::removeServer(instanceName);
    QLocalServer instanceServer;
    if (!instanceServer.listen(instanceName)) {
        qWarning() << "[EShot] Could not create single-instance lock:" << instanceServer.errorString();
    }
    const bool silent = parser.isSet(silentOption);
    const bool controlRequested = parser.isSet(controlOption)
        || (!silent && !parser.isSet(captureOption) && !parser.isSet(settingsOption)
            && !parser.isSet(saveOption) && !parser.isSet(quitOption));
    const bool firstRunRequired = !silent && FirstRunWizard::shouldShow();
    EShotApp eshotApp(!firstRunRequired);

    QObject::connect(&instanceServer, &QLocalServer::newConnection,
                     [&instanceServer, &eshotApp]() {
        while (QLocalSocket *socket = instanceServer.nextPendingConnection()) {
            const auto dispatch = [socket, &eshotApp]() {
                const auto command = ApplicationInstanceCommand::fromWire(socket->readAll());
                if (command == ApplicationInstanceCommand::Capture) {
                    QMetaObject::invokeMethod(&eshotApp, "onCaptureRequested",
                                              Qt::QueuedConnection);
                } else if (command == ApplicationInstanceCommand::Settings) {
                    QMetaObject::invokeMethod(&eshotApp, "onSettingsRequested",
                                              Qt::QueuedConnection);
                } else if (command == ApplicationInstanceCommand::Control) {
                    QMetaObject::invokeMethod(&eshotApp, "onControlRequested",
                                              Qt::QueuedConnection);
                } else if (command == ApplicationInstanceCommand::Quit) {
                    QMetaObject::invokeMethod(&eshotApp, "onQuitAction",
                                              Qt::QueuedConnection);
                }
            };
            QObject::connect(socket, &QLocalSocket::readyRead, socket, dispatch);
            QObject::connect(socket, &QLocalSocket::disconnected,
                             socket, &QObject::deleteLater);
            if (socket->bytesAvailable() > 0)
                dispatch();
        }
    });

    // --silent (used by autostart): skip the first-run wizard so the app starts
    // without prompting. On first run, delay global shortcut registration
    // until the wizard closes so GNOME's permission dialog cannot race it.
    if (firstRunRequired) {
        QTimer::singleShot(100, &app, [&eshotApp, controlRequested]() {
#ifdef Q_OS_LINUX
            // The Linux setup is a fresh onboarding flow even when an older
            // Windows configuration was carried over. Start it in English;
            // the user can choose another application language in the wizard.
            TranslationManager::setLanguage(TranslationManager::English);
#endif
            auto *wizard = new FirstRunWizard();
            wizard->setAttribute(Qt::WA_DeleteOnClose);
            wizard->show();
            QApplication::processEvents();
            if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
                if (!screen) screen = QGuiApplication::primaryScreen();
                QRect avail = screen->availableGeometry();
                
                const int targetWidth = qMin(680, qMax(wizard->minimumWidth(), avail.width() - 40));
                const int targetHeight = qMin(760, qMax(wizard->minimumHeight(), avail.height() - 80));
                wizard->resize(targetWidth, targetHeight);
                QApplication::processEvents();
                
                int nx = avail.center().x() - wizard->width() / 2;
                int ny = avail.center().y() - wizard->height() / 2;
                ny = qMax(avail.top() + 40, ny);
                wizard->move(nx, ny);
            }
            wizard->exec();
            eshotApp.initializeHotkeyConnections();
            if (controlRequested) {
                QMetaObject::invokeMethod(&eshotApp, "onControlRequested",
                                          Qt::QueuedConnection);
            }
        });
    }

    // Command line processing
    QString cliSavePath;
    if (parser.isSet(saveOption)) {
        cliSavePath = parser.value(saveOption);
        if (cliSavePath.isEmpty()) {
            qWarning() << "[EShot] --save requires a path argument";
        } else {
            QFileInfo fi(cliSavePath);
            QDir().mkpath(fi.absolutePath());
            QSettings s("EShot", "EShot");
            s.setValue("savePath", fi.absolutePath());
            s.setValue("cliSaveFullPath", fi.absoluteFilePath());
        }
    }
    if (parser.isSet(captureOption) || !cliSavePath.isEmpty()) {
        QMetaObject::invokeMethod(&eshotApp, "onCaptureRequested", Qt::QueuedConnection);
    } else if (controlRequested && !firstRunRequired) {
        QMetaObject::invokeMethod(&eshotApp, "onControlRequested", Qt::QueuedConnection);
    } else if (parser.isSet(settingsOption)) {
        QMetaObject::invokeMethod(&eshotApp, "onSettingsRequested", Qt::QueuedConnection);
    }

    return app.exec();
}
