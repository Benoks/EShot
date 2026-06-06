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

#include "core/HotkeyManager.h"
#include "core/TranslationManager.h"
#include "capture/CaptureOverlay.h"
#include "capture/PinnedWindow.h"
#include "capture/PinManager.h"
#include "recording/ScreenRecorder.h"
#include "recording/VideoRecorder.h"
#include "recording/RecordingIndicator.h"
#include "ui/SettingsDialog.h"
#include "ui/AboutDialog.h"
#include "ui/FirstRunWizard.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class EShotApp : public QObject {
    Q_OBJECT

public:
    EShotApp(QObject *parent = nullptr) : QObject(parent)
    {
        TranslationManager::init();
        loadSettings();
        setupTrayIcon();
        setupHotkey();
        checkForUpdates();
        QTimer::singleShot(700, this, [this]() { ensureOverlay(); });
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

    void showSuccessNotification(const QString &message, const QString &path, int timeoutMs)
    {
        if (!m_trayIcon || !m_showNotifications)
            return;
        if (!path.isEmpty())
            m_lastNotificationPath = path;
        if (!m_trayIcon->isVisible())
            m_trayIcon->show();
        m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(),
                                message,
                                QSystemTrayIcon::Information,
                                timeoutMs);
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

    void onRegionSelected(QRect rect)
    {
        if (m_pendingMode == 2) {
            onRecordGifSelected(rect);
        }
        m_pendingMode = 0;
    }

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::DoubleClick) onCaptureRequested();
    }

    void onQuitAction() { qApp->quit(); }

    void onNotificationClicked()
    {
        if (!m_lastNotificationPath.isEmpty()) {
            QFileInfo fi(m_lastNotificationPath);
            QString dir = fi.isDir() ? fi.absoluteFilePath() : fi.absolutePath();
            if (dir.isEmpty() || !QDir(dir).exists())
                dir = fi.absoluteDir().absolutePath();
            if (!dir.isEmpty() && QDir(dir).exists()) {
                if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir)))
                    QProcess::startDetached(QStringLiteral("explorer.exe"), {QDir::toNativeSeparators(dir)});
            }
        }
    }

    void onUpdateRequested()
    {
        QDesktopServices::openUrl(QUrl(m_latestReleaseUrl.isEmpty()
            ? QStringLiteral("https://github.com/Benoks/EShot/releases/latest")
            : m_latestReleaseUrl));
    }

    void onSettingsRequested()
    {
        SettingsDialog dlg;
        dlg.show();
        QApplication::processEvents(); // Let ARM64 DWM finalize frame geometry and draw the title bar
        if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
            if (!screen) screen = QGuiApplication::primaryScreen();
            QRect avail = screen->availableGeometry();
            
            // Programmatically simulate a user resize to force layout compression and fix Sandbox double-render
            dlg.resize(dlg.minimumSizeHint());
            QApplication::processEvents();
            
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

    void onFixPrintScreenConflict()
    {
        if (!HotkeyManager::setWindowsPrintScreenSnippingEnabled(false)) {
            if (m_trayIcon) {
                m_trayIcon->showMessage(
                    QStringLiteral("EShot"),
                    QStringLiteral("Could not change the Windows Print Screen setting. Open Settings > Accessibility > Keyboard and turn off the Print Screen Snipping Tool option."),
                    QSystemTrayIcon::Warning,
                    7000);
            }
            return;
        }

        if (m_trayIcon) {
            m_trayIcon->showMessage(
                QStringLiteral("EShot"),
                QStringLiteral("Windows Print Screen Snipping Tool shortcut disabled. If Print Screen does not open EShot immediately, restart Windows once."),
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

    void onRecordVideoSelected(QRect rect)
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
        const bool desktopAudio = s.value("videoDesktopAudioEnabled", false).toBool();
        const int desktopVolume = s.value("videoDesktopAudioVolume", 80).toInt();
        const QString desktopDevice = s.value("videoDesktopAudioDevice", "__wasapi__").toString();
        const bool microphoneAudio = s.value("videoMicrophoneEnabled", false).toBool();
        const int microphoneVolume = s.value("videoMicrophoneVolume", 80).toInt();
        const QString microphoneDevice = s.value("videoMicrophoneDevice", "default").toString();
        m_videoRecorder->start(rect, fps, maxSec, crf,
                               desktopAudio, desktopVolume, desktopDevice,
                               microphoneAudio, microphoneVolume,
                               microphoneDevice);
    }

    void onRecordGifSelected(QRect rect)
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

        QSettings s("EShot", "EShot");
        int fps = s.value("recordingFps", 10).toInt();
        int maxSec = s.value("recordingMaxSeconds", 30).toInt();
        int loop = s.value("recordingLoop", 0).toInt();
        m_screenRecorder->start(rect, fps, maxSec, loop, QString());
    }

    void onRecordingStarted()
    {
        if (m_screenRecorder) {
            m_recordingIndicator = new RecordingIndicator(m_screenRecorder->captureRect(), nullptr);
            connect(m_recordingIndicator, &RecordingIndicator::stopRequested, this, [this]() {
                if (m_screenRecorder && m_screenRecorder->isRecording())
                    m_screenRecorder->stop();
            });
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
        if (m_trayIcon) m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(),
                                                TranslationManager::recordingFailed() + QStringLiteral(": ") + reason,
                                                QSystemTrayIcon::Warning, 3000);
        rebuildTrayMenu();
    }

    void onVideoRecordingStarted()
    {
        if (m_videoRecorder) {
            m_recordingIndicator = new RecordingIndicator(m_videoRecorder->captureRect(), nullptr, 2, true);
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
        if (reason == QStringLiteral("ffmpeg.exe not found"))
            reason = TranslationManager::videoFfmpegMissing();
        if (m_trayIcon) m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(),
                                                TranslationManager::videoFailed() + QStringLiteral(": ") + reason,
                                                QSystemTrayIcon::Warning, 5000);
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
        connect(captureAction, &QAction::triggered, this, &EShotApp::onCaptureRequested);

        if (hasPrintScreenConflict()) {
            QAction *fixPrintScreenAction = m_trayMenu->addAction(
                trayIcon(":/icons/gear.svg"),
                QStringLiteral("Fix Print Screen shortcut"));
            connect(fixPrintScreenAction, &QAction::triggered,
                    this, &EShotApp::onFixPrintScreenConflict);
            m_trayMenu->addSeparator();
        }

        if (m_updateAvailable) {
            QAction *updateAction = m_trayMenu->addAction(
                trayIcon(":/icons/upload.svg"),
                QString("%1 v%2").arg(TranslationManager::updateTitle(), m_latestVersion));
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

        QIcon trayIcon(":/icons/pen.svg");
        if (trayIcon.isNull()) {
            QPixmap pix(32, 32); pix.fill(Qt::blue);
            trayIcon = QIcon(pix);
        }
        m_trayIcon->setIcon(trayIcon);
        m_trayIcon->setToolTip(QString("%1 v%2").arg(TranslationManager::appTitle(), QCoreApplication::applicationVersion()));

        m_trayMenu = new QMenu();
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
            "  padding: 3px 28px 3px 28px;"
            "  border-radius: 2px;"
            "}"
            "QMenu::item:selected { background: #3a3a3a; }"
            "QMenu::item:disabled { color: #8a8a8a; }"
            "QMenu::icon { width: 16px; height: 16px; left: 7px; }"
            "QMenu::separator { height: 1px; background: #424242; margin: 4px 5px; }"));
        rebuildTrayMenu();

        m_trayIcon->setContextMenu(m_trayMenu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, &EShotApp::onTrayActivated);
        connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this, &EShotApp::onNotificationClicked);
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
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingPauseRequested, this, [this]() {
            if (m_videoRecorder && m_videoRecorder->isRecording()) {
                if (m_videoRecorder->isPaused()) m_videoRecorder->resume();
                else m_videoRecorder->pause();
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
        return HotkeyManager::isPlainPrintScreen(
                   HotkeyManager::instance().captureModifiers(),
                   HotkeyManager::instance().captureVirtualKey())
            && HotkeyManager::isWindowsPrintScreenSnippingEnabled();
    }

    void checkForUpdates()
    {
        QNetworkAccessManager *mgr = new QNetworkAccessManager(this);
        connect(mgr, &QNetworkAccessManager::finished, this, [this, mgr](QNetworkReply *reply) {
            bool iconChanged = false;
            if (reply->error() == QNetworkReply::NoError) {
                QByteArray data = reply->readAll();
                QJsonDocument doc = QJsonDocument::fromJson(data);
                if (doc.isObject()) {
                    QJsonObject obj = doc.object();
                    QString latestTag = obj["tag_name"].toString();
                    if (latestTag.startsWith("v") || latestTag.startsWith("V"))
                        latestTag = latestTag.mid(1);
                    QString currentVersion = QCoreApplication::applicationVersion();
                    qDebug() << "[EShot] Update check: current=" << currentVersion << "latest=" << latestTag;
                    if (!latestTag.isEmpty() && isNewerVersion(latestTag, currentVersion)) {
                        qDebug() << "[EShot] Update available:" << latestTag;
                        m_updateAvailable = true;
                        m_latestVersion = latestTag;
                        m_latestReleaseUrl = obj["html_url"].toString();
                        setTrayIconUpdate();
                        iconChanged = true;
                    } else {
                        qDebug() << "[EShot] Already up to date";
                        m_updateAvailable = false;
                        m_latestVersion.clear();
                        m_latestReleaseUrl.clear();
                        setTrayIconNormal();
                        iconChanged = true;
                    }
                }
            } else {
                qDebug() << "[EShot] Update check failed:" << reply->errorString();
            }
            if (iconChanged) rebuildTrayMenu();
            reply->deleteLater();
            mgr->deleteLater();
        });
        QUrl url("https://api.github.com/repos/Benoks/EShot/releases/latest");
        QUrlQuery query;
        query.addQueryItem("_t", QString::number(QDateTime::currentMSecsSinceEpoch()));
        url.setQuery(query);
        mgr->get(QNetworkRequest(url));
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
        // Build yellow-pen icon
        QPixmap base(":/icons/pen.svg");
        if (!base.isNull()) {
            QPixmap yellow(base.size());
            yellow.fill(Qt::transparent);
            QPainter p(&yellow);
            p.setCompositionMode(QPainter::CompositionMode_SourceOver);
            // Draw original icon
            p.drawPixmap(0, 0, base);
            // Apply yellow color mask
            p.setCompositionMode(QPainter::CompositionMode_SourceIn);
            p.fillRect(yellow.rect(), QColor(255, 200, 0));
            p.end();
            m_trayIcon->setIcon(QIcon(yellow));
        }
        m_trayIcon->setToolTip(QString("%1 v%2 — %3").arg(
            TranslationManager::appTitle(),
            QCoreApplication::applicationVersion(),
            TranslationManager::updateTitle()));
    }

    void setTrayIconNormal()
    {
        if (!m_trayIcon) return;
        QIcon trayIcon(":/icons/pen.svg");
        if (trayIcon.isNull()) {
            QPixmap pix(32, 32); pix.fill(Qt::blue);
            trayIcon = QIcon(pix);
        }
        m_trayIcon->setIcon(trayIcon);
        m_trayIcon->setToolTip(QString("%1 v%2").arg(TranslationManager::appTitle(), QCoreApplication::applicationVersion()));
    }

    QSystemTrayIcon *m_trayIcon = nullptr;
    QMenu *m_trayMenu = nullptr;
    CaptureOverlay *m_overlay = nullptr;
    bool m_showNotifications = true;
    bool m_notifyCopy = false;
    bool m_notifySave = true;
    bool m_notifyGif = true;
    bool m_notifyVideo = true;
    bool m_updateAvailable = false;
    QString m_latestVersion;
    QString m_latestReleaseUrl;
    QString m_lastNotificationPath;
    bool m_skipNextCaptureNotification = false;
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
    engine.recognize(QPixmap::fromImage(img), "en-US");
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
    QCommandLineOption saveOption("save", "Save screenshot to specified path.", "path");
    parser.addOption(saveOption);
    QCommandLineOption silentOption("silent", "Start silently in system tray (used by Windows autostart).");
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

    if (highContrast || darkMode) {
        QPalette p;
        if (highContrast) {
            p.setColor(QPalette::Window, Qt::black);
            p.setColor(QPalette::WindowText, Qt::white);
            p.setColor(QPalette::Base, Qt::black);
            p.setColor(QPalette::AlternateBase, QColor(30,30,30));
            p.setColor(QPalette::ToolTipBase, Qt::black);
            p.setColor(QPalette::ToolTipText, Qt::yellow);
            p.setColor(QPalette::Text, Qt::white);
            p.setColor(QPalette::Button, Qt::black);
            p.setColor(QPalette::ButtonText, Qt::white);
            p.setColor(QPalette::BrightText, Qt::red);
            p.setColor(QPalette::Link, Qt::cyan);
            p.setColor(QPalette::Highlight, Qt::yellow);
            p.setColor(QPalette::HighlightedText, Qt::black);
        } else {
            p.setColor(QPalette::Window, QColor(53,53,53));
            p.setColor(QPalette::WindowText, Qt::white);
            p.setColor(QPalette::Base, QColor(42,42,42));
            p.setColor(QPalette::AlternateBase, QColor(66,66,66));
            p.setColor(QPalette::ToolTipBase, QColor(53,53,53));
            p.setColor(QPalette::ToolTipText, Qt::white);
            p.setColor(QPalette::Text, Qt::white);
            p.setColor(QPalette::Button, QColor(53,53,53));
            p.setColor(QPalette::ButtonText, Qt::white);
            p.setColor(QPalette::BrightText, Qt::red);
            p.setColor(QPalette::Link, QColor(42,130,218));
            p.setColor(QPalette::Highlight, QColor(42,130,218));
            p.setColor(QPalette::HighlightedText, Qt::black);
        }
        app.setPalette(p);
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "[EShot] System tray is not available yet; keeping the app alive for Windows startup.";
    }

    const QString instanceName = QStringLiteral("EShot.SingleInstance");
    QLocalSocket instanceSocket;
    instanceSocket.connectToServer(instanceName);
    if (instanceSocket.waitForConnected(150)) {
        qDebug() << "[EShot] Another instance is already running.";
        return 0;
    }

    QLocalServer::removeServer(instanceName);
    QLocalServer instanceServer;
    if (!instanceServer.listen(instanceName)) {
        qWarning() << "[EShot] Could not create single-instance lock:" << instanceServer.errorString();
    }
    QObject::connect(&instanceServer, &QLocalServer::newConnection, [&instanceServer]() {
        while (QLocalSocket *socket = instanceServer.nextPendingConnection()) {
            socket->disconnectFromServer();
            socket->deleteLater();
        }
    });

    EShotApp eshotApp;

    // --silent (used by Windows autostart): skip the first-run wizard so the
    // app starts cleanly in the tray without any UI prompting.
    const bool silent = parser.isSet(silentOption);
    // First-run wizard
    if (!silent && FirstRunWizard::shouldShow()) {
        QTimer::singleShot(100, &app, []() {
            auto *wizard = new FirstRunWizard();
            wizard->setAttribute(Qt::WA_DeleteOnClose);
            wizard->show();
            QApplication::processEvents();
            if (QScreen *screen = QGuiApplication::screenAt(QCursor::pos())) {
                if (!screen) screen = QGuiApplication::primaryScreen();
                QRect avail = screen->availableGeometry();
                
                wizard->resize(wizard->minimumSizeHint());
                QApplication::processEvents();
                
                int nx = avail.center().x() - wizard->width() / 2;
                int ny = avail.center().y() - wizard->height() / 2;
                ny = qMax(avail.top() + 40, ny);
                wizard->move(nx, ny);
            }
            wizard->exec();
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
    }

    return app.exec();
}
