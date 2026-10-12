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
#include <QThread>
#include <QMimeData>

#include <functional>

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
#include "core/LinuxUninstaller.h"
#endif
#include "capture/CaptureOverlay.h"
#include "core/ComponentPaths.h"
#include "recording/ScreenRecorder.h"
#include "recording/VideoRecorder.h"
#include "recording/RecordingIndicator.h"
#include "recording/RecordingFailureMessages.h"
#include "recording/RecordingSavedNotification.h"
#include "recording/RecordingSettingsPolicy.h"
#include "recording/RecordingStartCountdown.h"
#include "recording/RecordingStartCountdownPolicy.h"
#include "recording/WindowsCaptureBackend.h"
#include "ui/SettingsDialog.h"
#include "ui/SettingsLayoutPolicy.h"
#include "ui/ApplicationTheme.h"
#include "ui/AboutDialog.h"
#include "ui/ControlCenterDialog.h"
#include "ui/FirstRunWizard.h"
#include "ui/OnboardingTips.h"
#include "core/WindowsElevatedStartup.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <exdisp.h>
#include <shldisp.h>
#include <shlobj.h>
#include <shlguid.h>
#endif

namespace {

#ifdef Q_OS_WIN
// Starts a program through the desktop shell, so it runs as the signed-in
// user without the elevated token of the calling process.
bool launchAsDesktopUser(const QString &program, const QString &arguments)
{
    bool launched = false;
    IShellWindows *shellWindows = nullptr;
    IDispatch *desktopDispatch = nullptr;
    IServiceProvider *serviceProvider = nullptr;
    IShellBrowser *shellBrowser = nullptr;
    IShellView *shellView = nullptr;
    IDispatch *backgroundDispatch = nullptr;
    IShellFolderViewDual *folderView = nullptr;
    IDispatch *applicationDispatch = nullptr;
    IShellDispatch2 *shellDispatch = nullptr;

    VARIANT location;
    VariantInit(&location);
    location.vt = VT_I4;
    location.lVal = CSIDL_DESKTOP;
    VARIANT empty;
    VariantInit(&empty);
    long desktopWindow = 0;

    if (SUCCEEDED(CoCreateInstance(CLSID_ShellWindows, nullptr, CLSCTX_LOCAL_SERVER,
                                   IID_PPV_ARGS(&shellWindows)))
        && shellWindows->FindWindowSW(&location, &empty, SWC_DESKTOP, &desktopWindow,
                                      SWFO_NEEDDISPATCH, &desktopDispatch) == S_OK
        && desktopDispatch
        && SUCCEEDED(desktopDispatch->QueryInterface(IID_PPV_ARGS(&serviceProvider)))
        && SUCCEEDED(serviceProvider->QueryService(SID_STopLevelBrowser,
                                                   IID_PPV_ARGS(&shellBrowser)))
        && SUCCEEDED(shellBrowser->QueryActiveShellView(&shellView))
        && SUCCEEDED(shellView->GetItemObject(SVGIO_BACKGROUND,
                                              IID_PPV_ARGS(&backgroundDispatch)))
        && SUCCEEDED(backgroundDispatch->QueryInterface(IID_PPV_ARGS(&folderView)))
        && SUCCEEDED(folderView->get_Application(&applicationDispatch))
        && SUCCEEDED(applicationDispatch->QueryInterface(IID_PPV_ARGS(&shellDispatch)))) {
        const QString nativeProgram = QDir::toNativeSeparators(program);
        BSTR file = SysAllocString(reinterpret_cast<const OLECHAR *>(nativeProgram.utf16()));
        VARIANT args;
        VariantInit(&args);
        args.vt = VT_BSTR;
        args.bstrVal = SysAllocString(reinterpret_cast<const OLECHAR *>(arguments.utf16()));
        VARIANT show;
        VariantInit(&show);
        show.vt = VT_I4;
        show.lVal = SW_SHOWNORMAL;
        launched = SUCCEEDED(shellDispatch->ShellExecute(file, args, empty, empty, show));
        VariantClear(&args);
        SysFreeString(file);
    }

    if (shellDispatch) shellDispatch->Release();
    if (applicationDispatch) applicationDispatch->Release();
    if (folderView) folderView->Release();
    if (backgroundDispatch) backgroundDispatch->Release();
    if (shellView) shellView->Release();
    if (shellBrowser) shellBrowser->Release();
    if (serviceProvider) serviceProvider->Release();
    if (desktopDispatch) desktopDispatch->Release();
    if (shellWindows) shellWindows->Release();
    return launched;
}
#endif

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
    const QString entryPath = QDir(applicationsDirectory).filePath(
        QStringLiteral("io.github.benoks.EShot.KWinScreenshot.desktop"));
    QByteArray previousEntry;
    {
        QFile previous(entryPath);
        if (previous.open(QIODevice::ReadOnly))
            previousEntry = previous.readAll();
    }
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

    // An unchanged entry is already in the service cache (installed builds;
    // an AppImage's mount path changes on every launch).
    {
        QFile current(desktopPath.isEmpty() ? entryPath : desktopPath);
        if (!previousEntry.isEmpty() && current.open(QIODevice::ReadOnly)
            && current.readAll() == previousEntry) {
            return;
        }
    }

    // KWin checks the service cache before allowing ScreenShot2 access. Wait
    // for this small rebuild so the very first capture is not raced against
    // authorization setup, but never block startup on a hung rebuild.
    QElapsedTimer timer;
    timer.start();
    QProcess builder;
    builder.start(cacheBuilder, {});
    const bool finished = builder.waitForStarted(3000) && builder.waitForFinished(10000);
    if (!finished)
        builder.kill();
    const int exitCode = finished && builder.exitStatus() == QProcess::NormalExit
        ? builder.exitCode() : -1;
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

class EShotApp : public QObject {
    Q_OBJECT

public:
    explicit EShotApp(bool initializeHotkeys = true,
                      bool prewarmOverlayAtStartup = true,
                      QObject *parent = nullptr)
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
        if (prewarmOverlayAtStartup)
            QTimer::singleShot(100, this, [this]() { ensureOverlay(); });
#ifdef Q_OS_WIN
        // Find out once (cached per FFmpeg build) whether Desktop Duplication
        // and a hardware encoder work, so the first recording can use them.
        QTimer::singleShot(3000, this, []() {
            requestWindowsCaptureProbe(ComponentPaths::ffmpegPath());
        });
#endif
    }

    ~EShotApp()
    {
        if (m_trayIcon) m_trayIcon->hide();
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator->deleteLater(); m_recordingIndicator = nullptr; }
        // The event loop has stopped, so deleteLater() would never run and the
        // overlay's debounced settings would not be flushed.
        if (m_overlay) { delete m_overlay; m_overlay = nullptr; }
        if (m_screenRecorder) { m_screenRecorder->stop(); m_screenRecorder->deleteLater(); m_screenRecorder = nullptr; }
        if (m_trayMenu) { delete m_trayMenu; m_trayMenu = nullptr; }
    }

public slots:
    // Fresh installs only: say where EShot went and how to start a capture.
    void showTrayWelcome()
    {
        if (OnboardingTips::isSeen(OnboardingTips::TrayWelcome)
            || !m_trayIcon || !m_trayIcon->isVisible() || !m_hotkeysInitialized) {
            return;
        }
        // The hotkey failure notification takes precedence; welcome next time.
        if (!HotkeyManager::instance().failedHotkeys().isEmpty())
            return;
        OnboardingTips::markSeen(OnboardingTips::TrayWelcome);
        // Name the key that is actually registered, not the saved one.
        const QString key = HotkeyManager::instance().activeShortcutText(
            HotkeyManager::HOTKEY_CAPTURE);
        m_trayIcon->showMessage(TranslationManager::tr("trayWelcomeTitle"),
                                key.isEmpty()
                                    ? TranslationManager::tr("trayWelcomeBodyNoHotkey")
                                    : TranslationManager::tr("trayWelcomeBody").arg(key),
                                QSystemTrayIcon::Information, 8000);
    }

    void showHotkeyFailureNotification(const QList<int> &ids)
    {
        QStringList keys;
        const QList<HotkeyBinding> failures = HotkeyManager::instance().failedHotkeys();
        for (const HotkeyBinding &failure : failures) {
            if (ids.contains(failure.id))
                keys.append(HotkeyManager::shortcutText(failure.modifiers, failure.virtualKey));
        }
        keys.removeDuplicates();
        if (keys.isEmpty())
            return;
        QString message = TranslationManager::hotkeyNotActiveBody().arg(
            keys.join(QStringLiteral(", ")), TranslationManager::tabHotkey());
        const QString activeCapture = HotkeyManager::instance().activeShortcutText(
            HotkeyManager::HOTKEY_CAPTURE);
        if (ids.contains(HotkeyManager::HOTKEY_CAPTURE) && !activeCapture.isEmpty())
            message += QLatin1Char(' ') + TranslationManager::hotkeyCaptureFallback().arg(activeCapture);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (m_linuxNotification
            && m_linuxNotification->show(TranslationManager::hotkeyNotActiveTitle(), message,
                                         QString(), QString(), 10000)) {
            return;
        }
#endif
        if (m_trayIcon) {
            m_trayIcon->showMessage(TranslationManager::hotkeyNotActiveTitle(), message,
                                    QSystemTrayIcon::Warning, 10000);
        }
    }

    void prewarmOverlay()
    {
        ensureOverlay();
    }

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

    void showInfoNotification(const QString &message, int timeoutMs)
    {
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (m_linuxNotification
            && m_linuxNotification->show(TranslationManager::notifCaptureTitle(), message,
                                         QString(), QString(), timeoutMs)) {
            return;
        }
#endif
        if (m_trayIcon) {
            if (!m_trayIcon->isVisible())
                m_trayIcon->show();
            m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(), message,
                                    QSystemTrayIcon::Information, timeoutMs);
        }
    }

    // Saved GIF/video: short text, and on Linux buttons to open, reveal or
    // copy the file. Windows balloons have no buttons; clicking one keeps the
    // existing open-folder behaviour and the tray offers "Last recording".
    void showRecordingSavedNotification(const QString &message, const QString &path,
                                        int timeoutMs)
    {
        if (!m_trayIcon || !m_showNotifications)
            return;
        m_lastNotificationPath = path;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (m_linuxNotification
            && m_linuxNotification->showWithActions(
                TranslationManager::notifCaptureTitle(), message, path,
                {{QStringLiteral("open-file"), TranslationManager::tr("recOpenFile")},
                 {QStringLiteral("show-in-folder"), TranslationManager::tr("recShowInFolder")},
                 {QStringLiteral("copy-file"), TranslationManager::tr("recCopyFile")}},
                timeoutMs)) {
            return;
        }
#endif
        if (!m_trayIcon->isVisible())
            m_trayIcon->show();
        m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(), message,
                                QSystemTrayIcon::Information, timeoutMs);
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
        // Suppress only a capture-completed notification that arrives shortly
        // after a save (the save shows its own notification); stale tokens
        // must not swallow later captures.
        if (m_skipNextCaptureNotificationMs > 0
            && QDateTime::currentMSecsSinceEpoch() - m_skipNextCaptureNotificationMs < 2000) {
            m_skipNextCaptureNotificationMs = 0;
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
        m_skipNextCaptureNotificationMs = QDateTime::currentMSecsSinceEpoch();
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

    // Quitting kills the recorder processes, which leaves an unplayable file.
    // Stop and save a running recording first, then quit.
    void onQuitAction()
    {
        QObject *recorder = nullptr;
        if (m_videoRecorder && m_videoRecorder->isRecording())
            recorder = m_videoRecorder;
        else if (m_screenRecorder && m_screenRecorder->isRecording())
            recorder = m_screenRecorder;
        if (!recorder) {
            qApp->quit();
            return;
        }
        if (m_quitAfterRecording)
            return;
        m_quitAfterRecording = true;
        const auto quitSoon = []() { QTimer::singleShot(1500, qApp, &QCoreApplication::quit); };
        if (auto *video = qobject_cast<VideoRecorder *>(recorder)) {
            connect(video, &VideoRecorder::recordingStopped, qApp, quitSoon, Qt::QueuedConnection);
            connect(video, &VideoRecorder::recordingFailed, qApp, quitSoon, Qt::QueuedConnection);
            stopActiveRecording();
        } else if (auto *gif = qobject_cast<ScreenRecorder *>(recorder)) {
            connect(gif, &ScreenRecorder::recordingStopped, qApp, quitSoon, Qt::QueuedConnection);
            connect(gif, &ScreenRecorder::recordingFailed, qApp, quitSoon, Qt::QueuedConnection);
            stopActiveRecording();
        }
        // Never hang on a stuck encoder; GIF conversion can take a while.
        QTimer::singleShot(120000, qApp, &QCoreApplication::quit);
    }

    bool isRecordingActive() const
    {
        return (m_videoRecorder && m_videoRecorder->isRecording())
            || (m_screenRecorder && m_screenRecorder->isRecording());
    }

    void onNotificationClicked()
    {
        if (!m_notificationOpenFolder)
            return;
        qInfo() << "[Notification] clicked path=" << m_lastNotificationPath;
        showPathInFolder(m_lastNotificationPath);
    }

    void showPathInFolder(const QString &path)
    {
#ifdef Q_OS_WIN
        constexpr NotificationDesktop desktop = NotificationDesktop::Windows;
#elif defined(Q_OS_LINUX)
        constexpr NotificationDesktop desktop = NotificationDesktop::Linux;
#else
        constexpr NotificationDesktop desktop = NotificationDesktop::Other;
#endif
        const bool opened = openNotificationFolder(
            path, desktop,
            [](const QString &program, const QStringList &arguments) {
                return QProcess::startDetached(program, arguments);
            },
            [](const QUrl &url) {
                return QDesktopServices::openUrl(url);
            });
        qInfo() << "[Notification] folder open requested=" << opened
                << "directory=" << notificationDirectoryForPath(path);
    }

    void openRecordingFile(const QString &path)
    {
        if (!QFileInfo::exists(path))
            return;
        const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        qInfo() << "[Recording] open file requested=" << opened << "path=" << path;
    }

    void copyRecordingFile(const QString &path)
    {
        if (!QFileInfo::exists(path))
            return;
        QGuiApplication::clipboard()->setMimeData(recordingFileMimeData(path));
        qInfo() << "[Recording] copied file to clipboard path=" << path;
    }

    void handleRecordingFileAction(const QString &path, const QString &action)
    {
        if (action == QStringLiteral("dismiss-recording-tray-tip")) {
            OnboardingTips::markSeen(OnboardingTips::RecordingTrayTip);
            return;
        }
        if (action == QStringLiteral("open-file"))
            openRecordingFile(path);
        else if (action == QStringLiteral("show-in-folder"))
            showPathInFolder(path);
        else if (action == QStringLiteral("copy-file"))
            copyRecordingFile(path);
    }

    void onUpdateRequested()
    {
        if (m_updateManager)
            m_updateManager->installUpdate();
    }

    void onSettingsRequested()
    {
        openSettings(false);
    }

    void onRecordingSettingsRequested()
    {
        openSettings(true);
    }

    void openSettings(bool recordingTab)
    {
        // Two dialogs would each save the values they loaded when opened and
        // overwrite each other; bring the open one forward instead.
        if (m_settingsDialog) {
            if (recordingTab)
                m_settingsDialog->showRecordingTab();
            m_settingsDialog->raise();
            m_settingsDialog->activateWindow();
            return;
        }
        SettingsDialog dlg;
        m_settingsDialog = &dlg;
        if (recordingTab)
            dlg.showRecordingTab();
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
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen) {
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
            // Applies a changed black-icon setting to either icon variant.
            if (m_updateAvailable)
                setTrayIconUpdate();
            else
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
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (screen) {
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

    // GIF and video share one recording indicator and one set of stop/cancel
    // hotkeys, so only one recording may run or be starting at a time. A
    // portal start also spins a nested event loop; replacing the recorder
    // during it would delete an object that is still on the stack.
    bool recordingBusy() const
    {
        // A start that never reports back (e.g. a silent portal failure) must
        // not block recording forever: the portal dialog times out at 120 s
        // and the start delay is at most 10 s.
        constexpr qint64 MaxRecordingStartMs = 150000;
        const bool starting = m_recordingStartPending
            && m_recordingStartTimer.isValid()
            && m_recordingStartTimer.elapsed() < MaxRecordingStartMs;
        return starting
            || (m_videoRecorder && m_videoRecorder->isRecording())
            || (m_screenRecorder && m_screenRecorder->isRecording());
    }

    void onRecordGifRequested()
    {
        if (m_screenRecorder && m_screenRecorder->isRecording()) {
            stopActiveRecording();
            return;
        }
        if (cancelPendingRecordingStart()) return;
        if (recordingBusy()) return;
        if (m_overlay && m_overlay->isVisible()) return;
        m_pendingMode = 2;
        ensureOverlay();
        m_overlay->startCaptureForRecording();
    }

    void onRecordVideoRequested()
    {
        if (m_videoRecorder && m_videoRecorder->isRecording()) {
            stopActiveRecording();
            return;
        }
        if (cancelPendingRecordingStart()) return;
        if (recordingBusy()) return;
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
            stopActiveRecording();
            return;
        }
        if (recordingBusy()) return;
        if (m_videoRecorder) { m_videoRecorder->deleteLater(); m_videoRecorder = nullptr; }
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator->deleteLater(); m_recordingIndicator = nullptr; }

        m_videoRecorder = new VideoRecorder(this);
        m_recordingStartPending = true;
        m_recordingStartTimer.start();
        connect(m_videoRecorder, &VideoRecorder::recordingStarted, this, &EShotApp::onVideoRecordingStarted);
        connect(m_videoRecorder, &VideoRecorder::recordingStopped, this, &EShotApp::onVideoRecordingStopped);
        connect(m_videoRecorder, &VideoRecorder::recordingFailed, this, &EShotApp::onVideoRecordingFailed);
        connect(m_videoRecorder, &VideoRecorder::remainingTimeChanged,
                this, &EShotApp::onRecordingRemainingTimeChanged);
        connect(m_videoRecorder, &VideoRecorder::elapsedTimeChanged,
                this, &EShotApp::onRecordingElapsedTimeChanged);
        connect(m_videoRecorder, &VideoRecorder::pausedChanged,
                this, &EShotApp::onRecordingPausedChanged);

        QSettings s("EShot", "EShot");
        const int fps = s.value("videoRecordingFps", DefaultVideoFps).toInt();
        const int maxSec = s.value("videoRecordingMaxSeconds", DefaultVideoMaxSeconds).toInt();
        const int crf = s.value("videoRecordingCrf", DefaultVideoCrf).toInt();
        const bool desktopAudio = loadRecordingAudioEnabled(s, RecordingAudioSource::Desktop);
        const int desktopVolume = s.value("videoDesktopAudioVolume", 80).toInt();
        const QString desktopDevice = s.value("videoDesktopAudioDevice",
#ifdef Q_OS_WIN
                                            "__wasapi__"
#else
                                            "@DEFAULT_MONITOR@"
#endif
                                            ).toString();
        const bool microphoneAudio = loadRecordingAudioEnabled(s, RecordingAudioSource::Microphone);
        const int microphoneVolume = s.value("videoMicrophoneVolume", 80).toInt();
        const QString microphoneDevice = s.value("videoMicrophoneDevice", "default").toString();
        const int startDelayMs = recordingStartDelayMs(s.value("recordingStartDelaySeconds", 0).toInt());
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
        if (startDelayMs > 0) {
            beginDelayedRecordingStart(
                displayRect.isValid() ? displayRect : rect, startDelayMs,
                [rec = QPointer<VideoRecorder>(m_videoRecorder), rect, displayRect]() {
                    return rec && rec->prepareSource(rect, displayRect);
                },
                startVideo);
        } else {
            startVideo();
        }
    }

    void onRecordGifSelected(QRect rect, QRect displayRect = QRect())
    {
        if (rect.isEmpty()) return;
        if (recordingBusy()) return;
        if (m_screenRecorder) { m_screenRecorder->stop(); m_screenRecorder->deleteLater(); m_screenRecorder = nullptr; }
        if (m_recordingIndicator) { m_recordingIndicator->stop(); m_recordingIndicator->deleteLater(); m_recordingIndicator = nullptr; }
        m_screenRecorder = new ScreenRecorder(this);
        m_recordingStartPending = true;
        m_recordingStartTimer.start();
        connect(m_screenRecorder, &ScreenRecorder::recordingStarted, this, &EShotApp::onRecordingStarted);
        connect(m_screenRecorder, &ScreenRecorder::recordingStopped, this, &EShotApp::onRecordingStopped);
        connect(m_screenRecorder, &ScreenRecorder::recordingFailed, this, &EShotApp::onRecordingFailed);
        connect(m_screenRecorder, &ScreenRecorder::frameCaptured, this, [this](int n) {
            if (m_recordingIndicator) m_recordingIndicator->setFrameCount(n);
        });
        connect(m_screenRecorder, &ScreenRecorder::remainingTimeChanged,
                this, &EShotApp::onRecordingRemainingTimeChanged);
        connect(m_screenRecorder, &ScreenRecorder::elapsedTimeChanged,
                this, &EShotApp::onRecordingElapsedTimeChanged);
        connect(m_screenRecorder, &ScreenRecorder::pausedChanged,
                this, &EShotApp::onRecordingPausedChanged);

        QSettings s("EShot", "EShot");
        int fps = s.value("recordingFps", DefaultGifFps).toInt();
        int maxSec = s.value("recordingMaxSeconds", DefaultGifMaxSeconds).toInt();
        int loop = s.value("recordingLoop", 0).toInt();
        const int startDelayMs = recordingStartDelayMs(s.value("recordingStartDelaySeconds", 0).toInt());
        auto startGif = [rec = QPointer<ScreenRecorder>(m_screenRecorder), rect, fps, maxSec, loop, displayRect]() {
            if (rec)
                rec->start(rect, fps, maxSec, loop, QString(), displayRect);
        };
        if (startDelayMs > 0) {
            beginDelayedRecordingStart(
                displayRect.isValid() ? displayRect : rect, startDelayMs,
                [rec = QPointer<ScreenRecorder>(m_screenRecorder), rect, displayRect]() {
                    return rec && rec->prepareSource(rect, displayRect);
                },
                startGif);
        } else {
            startGif();
        }
    }

    void onRecordingStarted()
    {
        m_recordingStartPending = false;
        if (m_screenRecorder) {
            beginRecordingUi(true, m_screenRecorder->maxSeconds());
            QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
            showRecordingIndicator(
                m_screenRecorder->captureRect(), RecordingIndicatorMode::Gif,
                m_screenRecorder->captureBackend(),
                {QStringLiteral("%1 × %2")
                     .arg(m_screenRecorder->captureRect().width())
                     .arg(m_screenRecorder->captureRect().height()),
                 QStringLiteral("%1 FPS").arg(settings.value("recordingFps", DefaultGifFps).toInt())});
        }
        rebuildTrayMenu();
    }

    void onRecordingStopped(QString outputPath)
    {
        m_recordingStartPending = false;
        finishRecordingUi();
        const QFileInfo fi(outputPath);
        rememberLastRecording(fi);
        copyRecordingAfterSaveIfEnabled(fi);
        if (m_trayIcon && m_showNotifications && m_notifyGif) {
            showRecordingSavedNotification(
                recordingSavedNotificationText(RecordingSavedKind::Gif, fi.absoluteFilePath(),
                                               fi.size(), notificationLocale()),
                fi.absoluteFilePath(), 5000);
        }
        rebuildTrayMenu();
    }

    void onRecordingFailed(QString reason)
    {
        m_recordingStartPending = false;
        finishRecordingUi();
        m_lastNotificationPath.clear();
        const RecordingFailureMessage failure = recordingFailureMessage(reason);
        logRecordingFailure(reason, failure);
        if (!failure.silent) {
            // Long enough to read where a kept recording is.
            showFailureNotification(TranslationManager::recordingFailed() + QStringLiteral(": ")
                                        + failure.text,
                                    failure.text.contains(QLatin1Char('\n')) ? 10000 : 5000);
        }
        rebuildTrayMenu();
    }

    void onVideoRecordingStarted()
    {
        m_recordingStartPending = false;
        if (m_videoRecorder) {
            beginRecordingUi(false, m_videoRecorder->maxSeconds());
            QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
            const bool desktopAudio = settings.value("videoDesktopAudioEnabled", false).toBool();
            const bool microphone = settings.value("videoMicrophoneEnabled", false).toBool();
            const QString audio = desktopAudio && microphone
                ? TranslationManager::audioDesktopMic()
                : desktopAudio ? TranslationManager::audioDesktop()
                               : microphone ? TranslationManager::audioMicrophone()
                                            : TranslationManager::audioNone();
            showRecordingIndicator(
                m_videoRecorder->captureRect(), RecordingIndicatorMode::Video,
                m_videoRecorder->captureBackend(),
                {QStringLiteral("%1 × %2")
                     .arg(m_videoRecorder->captureRect().width())
                     .arg(m_videoRecorder->captureRect().height()),
                 QStringLiteral("%1 FPS").arg(settings.value("videoRecordingFps", DefaultVideoFps).toInt()),
                 audio});
        }
        rebuildTrayMenu();
    }

    void onVideoRecordingStopped(QString outputPath)
    {
        m_recordingStartPending = false;
        finishRecordingUi();
        // Warnings (e.g. a dropped audio source) are shown even when video
        // notifications are off: the saved file differs from what was asked.
        const QStringList warnings = m_videoRecorder ? m_videoRecorder->warnings() : QStringList();
        const QString warningText = warnings.join(QStringLiteral("\n"));
        const QFileInfo fi(outputPath);
        rememberLastRecording(fi);
        copyRecordingAfterSaveIfEnabled(fi);
        if (m_trayIcon && m_showNotifications && m_notifyVideo) {
            QString message = recordingSavedNotificationText(
                RecordingSavedKind::Video, fi.absoluteFilePath(), fi.size(), notificationLocale());
            if (!warningText.isEmpty())
                message += QStringLiteral("\n") + warningText;
            showRecordingSavedNotification(message, fi.absoluteFilePath(),
                                           warningText.isEmpty() ? 5000 : 10000);
        } else if (!warningText.isEmpty()) {
            showFailureNotification(TranslationManager::videoSaved() + QStringLiteral("\n") + warningText, 10000);
        }
        rebuildTrayMenu();
    }

    void onVideoRecordingFailed(QString reason)
    {
        m_recordingStartPending = false;
        finishRecordingUi();
        m_lastNotificationPath.clear();
        const RecordingFailureMessage failure = recordingFailureMessage(reason);
        logRecordingFailure(reason, failure);
        if (!failure.silent) {
            showFailureNotification(TranslationManager::videoFailed() + QStringLiteral(": ")
                                        + failure.text,
                                    failure.text.contains(QLatin1Char('\n')) ? 10000 : 5000);
        }
        rebuildTrayMenu();
    }

private:
    void onRecordingRemainingTimeChanged(int seconds)
    {
        if (m_recordingIndicator)
            m_recordingIndicator->setRemainingSeconds(seconds);
        // The recorder stops itself at the limit; show that it is saving.
        if (seconds == 0 && m_recordingTimeLimitSeconds > 0 && isRecordingActive()
            && !recordingFinalizing()) {
            markRecordingFinalizing();
        }
    }

    void onRecordingElapsedTimeChanged(int seconds)
    {
        m_recordingElapsedSeconds = qMax(0, seconds);
        if (m_recordingIndicator)
            m_recordingIndicator->setElapsedSeconds(seconds);
        updateRecordingTrayToolTip();
    }

    void onRecordingPausedChanged(bool paused)
    {
        if (m_recordingIndicator)
            m_recordingIndicator->setPaused(paused);
        updateRecordingTrayToolTip();
        // Pause may come from a tray action; never rebuild its menu inside it.
        QTimer::singleShot(0, this, &EShotApp::rebuildTrayMenu);
    }

    // With a tray icon the recording is controlled from the tray menu and
    // only the border is drawn; without one the floating bar is the only
    // way to stop it.
    RecordingControlsPresentation currentRecordingControlsPresentation() const
    {
        return recordingControlsPresentation(QSystemTrayIcon::isSystemTrayAvailable(),
                                             m_trayIcon && m_trayIcon->isVisible());
    }

    void showRecordingIndicator(const QRect &captureRect, RecordingIndicatorMode mode,
                                RecordingCaptureBackend backend, const QStringList &details)
    {
        const RecordingControlsPresentation presentation = currentRecordingControlsPresentation();
        m_recordingDetails = details;
        m_recordingIndicator = new RecordingIndicator(captureRect, nullptr, 2, true, mode,
                                                      backend, presentation);
        m_recordingIndicator->setTimeLimitSeconds(m_recordingTimeLimitSeconds);
        m_recordingIndicator->setDetails(details);
        m_recordingIndicator->setShortcutHints(
            HotkeyManager::instance().recordingPauseShortcutText(),
            HotkeyManager::instance().recordingStopShortcutText(),
            HotkeyManager::instance().recordingCancelShortcutText());
        connectRecordingIndicator();
        m_recordingIndicator->startCaptureSafePresentation();
        if (presentation == RecordingControlsPresentation::TrayMenu)
            showRecordingTrayTip();
        else if (m_recordingIndicator->requiresCaptureSafePresentation())
            showRecordingControlsHiddenHint();
    }

    // Once per session, for the first few sessions, until "Don't show again".
    void showRecordingTrayTip()
    {
        if (!shouldShowRecordingTrayTip(m_recordingTrayTipShown,
                                        OnboardingTips::isSeen(OnboardingTips::RecordingTrayTip),
                                        OnboardingTips::count(OnboardingTips::RecordingTrayTipCount))) {
            return;
        }
        m_recordingTrayTipShown = true;
        OnboardingTips::increment(OnboardingTips::RecordingTrayTipCount);
        const QString stopKey = HotkeyManager::instance().recordingStopShortcutText().trimmed();
        const QString message = stopKey.isEmpty()
            ? TranslationManager::tr("recTrayControlsTipNoHotkey")
            : TranslationManager::tr("recTrayControlsTip").arg(stopKey);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
        if (m_linuxNotification
            && m_linuxNotification->showWithActions(
                TranslationManager::notifCaptureTitle(), message, QString(),
                {{QStringLiteral("dismiss-recording-tray-tip"),
                  TranslationManager::tr("tipDontShowAgain")}},
                8000)) {
            return;
        }
#endif
        if (m_trayIcon)
            m_trayIcon->showMessage(TranslationManager::notifCaptureTitle(), message,
                                    QSystemTrayIcon::Information, 8000);
    }

    void setRecordingBorderLocked(bool locked)
    {
        if (m_recordingIndicator)
            m_recordingIndicator->setBorderLocked(locked);
    }

    // "● Recording · 00:12 / 00:30", "Recording paused · …" or "Saving…".
    QString recordingTrayStatusText() const
    {
        if (recordingFinalizing())
            return TranslationManager::tr(m_recordingIsGif ? "recConvertingGif" : "recSaving");
        const QString time = recordingStatusTimeText(m_recordingElapsedSeconds,
                                                     m_recordingTimeLimitSeconds);
        if (activeRecordingPaused())
            return QStringLiteral("○ ") + TranslationManager::tr("trayRecordingPausedTooltip").arg(time);
        return QStringLiteral("● ") + TranslationManager::tr("trayRecordingTooltip").arg(time);
    }

    void updateRecordingTrayStatus()
    {
        if (m_trayRecordingStatusAction)
            m_trayRecordingStatusAction->setText(recordingTrayStatusText());
    }

    void connectRecordingIndicator()
    {
        if (!m_recordingIndicator)
            return;
        connect(m_recordingIndicator, &RecordingIndicator::stopRequested,
                this, &EShotApp::stopActiveRecording);
        connect(m_recordingIndicator, &RecordingIndicator::pauseRequested,
                this, [this]() { setActiveRecordingPaused(true); });
        connect(m_recordingIndicator, &RecordingIndicator::resumeRequested,
                this, [this]() { setActiveRecordingPaused(false); });
        connect(m_recordingIndicator, &RecordingIndicator::cancelRequested,
                this, &EShotApp::cancelActiveRecording);
    }

    bool recordingFinalizing() const
    {
        return m_recordingFinalizing
            || (m_videoRecorder && m_videoRecorder->isFinalizing())
            || (m_screenRecorder && m_screenRecorder->isFinalizing());
    }

    bool activeRecordingPaused() const
    {
        if (m_videoRecorder && m_videoRecorder->isRecording())
            return m_videoRecorder->isPaused();
        if (m_screenRecorder && m_screenRecorder->isRecording())
            return m_screenRecorder->isPaused();
        return false;
    }

    // One stop path for the indicator, hotkeys, tray and quit: the controls
    // switch to "Saving…" right away so nothing can cancel the file while it
    // is being written.
    void stopActiveRecording()
    {
        const bool video = m_videoRecorder && m_videoRecorder->isRecording();
        const bool gif = !video && m_screenRecorder && m_screenRecorder->isRecording();
        if (!video && !gif)
            return;
        if (recordingFinalizing())
            return;
        markRecordingFinalizing();
        if (video)
            m_videoRecorder->stop();
        else
            m_screenRecorder->stop();
    }

    void markRecordingFinalizing()
    {
        m_recordingFinalizing = true;
        if (m_recordingIndicator)
            m_recordingIndicator->setFinalizing(true);
        updateRecordingTrayToolTip();
        QTimer::singleShot(0, this, &EShotApp::rebuildTrayMenu);
    }

    void setActiveRecordingPaused(bool paused)
    {
        if (recordingFinalizing())
            return;
        if (m_videoRecorder && m_videoRecorder->isRecording()) {
            if (paused) m_videoRecorder->pause();
            else m_videoRecorder->resume();
        } else if (m_screenRecorder && m_screenRecorder->isRecording()) {
            if (paused) m_screenRecorder->pause();
            else m_screenRecorder->resume();
        }
    }

    void toggleActiveRecordingPause()
    {
        setActiveRecordingPaused(!activeRecordingPaused());
    }

    void cancelActiveRecording()
    {
        if (cancelPendingRecordingStart())
            return;
        // Cancelling while the file is being written would delete it.
        if (recordingFinalizing())
            return;
        if (m_videoRecorder && m_videoRecorder->isRecording())
            m_videoRecorder->cancel();
        else if (m_screenRecorder && m_screenRecorder->isRecording())
            m_screenRecorder->cancel();
        else
            return;
        finishRecordingUi();
        // Deferred: this may run inside a tray action of the menu being rebuilt.
        QTimer::singleShot(0, this, &EShotApp::rebuildTrayMenu);
    }

    void beginRecordingUi(bool gif, int timeLimitSeconds)
    {
        m_recordingFinalizing = false;
        m_recordingIsGif = gif;
        m_recordingElapsedSeconds = 0;
        m_recordingTimeLimitSeconds = qMax(0, timeLimitSeconds);
        m_recordingTrayActive = true;
        setTrayIconRecording();
        activateRecordingHotkeys();
    }

    // Removes the controls and restores the tray after stop, failure or cancel.
    void finishRecordingUi()
    {
        m_recordingFinalizing = false;
        deactivateRecordingHotkeys();
        if (m_recordingIndicator) {
            m_recordingIndicator->stop();
            m_recordingIndicator->deleteLater();
            m_recordingIndicator = nullptr;
        }
        if (m_recordingTrayActive) {
            m_recordingTrayActive = false;
            if (m_updateAvailable)
                setTrayIconUpdate();
            else
                setTrayIconNormal();
        }
    }

    void showRecordingControlsHiddenHint()
    {
        auto keyText = [](const QString &shortcut) {
            return shortcut.trimmed().isEmpty() ? TranslationManager::hotkeyNoneActive()
                                                : shortcut;
        };
        showInfoNotification(
            TranslationManager::tr("recControlsHidden").arg(
                keyText(HotkeyManager::instance().recordingStopShortcutText()),
                keyText(HotkeyManager::instance().recordingPauseShortcutText())),
            7000);
    }

    void copyRecordingAfterSaveIfEnabled(const QFileInfo &file)
    {
        const QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
        if (settings.value(QStringLiteral("recordingCopyToClipboard"), false).toBool() && file.exists())
            copyRecordingFile(file.absoluteFilePath());
    }

    // Pause/stop/cancel keys exist only while a recording or its countdown
    // runs; a key that cannot be taken is reported but never stops it.
    void activateRecordingHotkeys()
    {
        if (!m_hotkeysInitialized || HotkeyManager::instance().recordingHotkeysActive())
            return;
        if (HotkeyManager::instance().activateRecordingHotkeys())
            return;
        QStringList keys;
        for (const HotkeyBinding &failure : HotkeyManager::instance().failedHotkeys()) {
            if (failure.id == HotkeyManager::HOTKEY_RECORDING_PAUSE
                || failure.id == HotkeyManager::HOTKEY_RECORDING_STOP
                || failure.id == HotkeyManager::HOTKEY_RECORDING_CANCEL) {
                keys.append(HotkeyManager::shortcutText(failure.modifiers, failure.virtualKey));
            }
        }
        keys.removeDuplicates();
        qWarning() << "[Recording] recording hotkeys not active:" << keys;
        showFailureNotification(TranslationManager::tr("recHotkeysNotActive")
                                    .arg(keys.join(QStringLiteral(", "))),
                                8000);
    }

    void deactivateRecordingHotkeys()
    {
        if (m_hotkeysInitialized && HotkeyManager::instance().recordingHotkeysActive())
            HotkeyManager::instance().deactivateRecordingHotkeys();
    }

    void rememberLastRecording(const QFileInfo &file)
    {
        if (!file.exists())
            return;
        m_lastRecordingPath = file.absoluteFilePath();
        qInfo() << "[Recording] saved" << QDir::toNativeSeparators(m_lastRecordingPath)
                << "bytes=" << file.size();
    }

    void logRecordingFailure(const QString &reason, const RecordingFailureMessage &failure)
    {
        if (failure.silent) {
            qInfo() << "[Recording] aborted without notification:" << reason;
            return;
        }
        if (failure.unexpected)
            qWarning().noquote() << "[Recording] recorder failure details:" << reason;
        else
            qWarning().noquote() << "[Recording] failed:" << reason;
    }

    static QLocale notificationLocale()
    {
        return QLocale(TranslationManager::langCode());
    }

    // Runs the configured start delay behind a visible, cancellable countdown.
    // On Wayland the portal source picker opens first, so the delay ends right
    // before frames are captured instead of before the picker.
    void beginDelayedRecordingStart(const QRect &indicatorRect, int delayMs,
                                    const std::function<bool()> &prepare,
                                    const std::function<void()> &start)
    {
        // A failed prepare has already reported recordingFailed(), which
        // clears the pending start.
        if (!prepare() || !m_recordingStartPending)
            return;
        // The portal picker may have taken a while; the busy timeout covers
        // the countdown from here.
        m_recordingStartTimer.start();
        m_recordingStartCountdown = new RecordingStartCountdown(
            indicatorRect, delayMs, HotkeyManager::instance().recordingCancelShortcutText());
        connect(m_recordingStartCountdown, &RecordingStartCountdown::cancelRequested, this, [this]() {
            cancelPendingRecordingStart();
        });
        connect(m_recordingStartCountdown, &RecordingStartCountdown::finished, this, [this, start]() {
            dismissRecordingStartCountdown();
            start();
        });
        // Stop/cancel during the countdown abort the start.
        activateRecordingHotkeys();
        rebuildTrayMenu();
    }

    void dismissRecordingStartCountdown()
    {
        if (!m_recordingStartCountdown)
            return;
        m_recordingStartCountdown->hide();
        m_recordingStartCountdown->deleteLater();
        m_recordingStartCountdown = nullptr;
    }

    // Aborts a start that is still counting down and drops its recorder
    // (closing a prepared portal session). Returns false when no countdown is
    // running so callers fall through to their normal recording handling.
    bool cancelPendingRecordingStart()
    {
        if (!m_recordingStartCountdown)
            return false;
        dismissRecordingStartCountdown();
        m_recordingStartPending = false;
        deactivateRecordingHotkeys();
        if (m_videoRecorder && !m_videoRecorder->isRecording()) {
            m_videoRecorder->deleteLater();
            m_videoRecorder = nullptr;
        }
        if (m_screenRecorder && !m_screenRecorder->isRecording()) {
            m_screenRecorder->deleteLater();
            m_screenRecorder = nullptr;
        }
        rebuildTrayMenu();
        return true;
    }

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
        m_updateManager->setBusyCheck([this]() {
            return isRecordingActive() || m_recordingStartPending
                || (m_overlay && m_overlay->isVisible());
        });
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

        auto withShortcut = [](const QString &label, const QString &shortcut) {
            return shortcut.trimmed().isEmpty()
                ? label : QStringLiteral("%1 (%2)").arg(label, shortcut);
        };
        const bool videoRecording = m_videoRecorder && m_videoRecorder->isRecording();
        const bool gifRecording = m_screenRecorder && m_screenRecorder->isRecording();
        // Every action below is deferred: rebuilding the tray menu deletes the
        // action whose triggered() handler is still running.
        if (videoRecording || gifRecording) {
            // The tray is where a running recording is controlled: state and
            // controls come first.
            const bool finalizing = recordingFinalizing();
            const bool paused = activeRecordingPaused();
            QAction *statusAction = m_trayMenu->addAction(recordingTrayStatusText());
            statusAction->setEnabled(false);
            m_trayRecordingStatusAction = statusAction;
            if (!m_recordingDetails.isEmpty()) {
                QAction *detailsAction = m_trayMenu->addAction(
                    m_recordingDetails.join(QStringLiteral(" · ")));
                detailsAction->setEnabled(false);
            }
            m_trayMenu->addSeparator();
            QAction *stopAction = m_trayMenu->addAction(
                trayIcon(":/icons/stop.svg"),
                withShortcut(finalizing ? TranslationManager::tr(gifRecording ? "recConvertingGif" : "recSaving")
                                        : TranslationManager::tr("trayStopAndSave"),
                             finalizing ? QString()
                                        : HotkeyManager::instance().recordingStopShortcutText()));
            stopAction->setEnabled(!finalizing);
            connect(stopAction, &QAction::triggered, this, [this]() {
                QTimer::singleShot(0, this, &EShotApp::stopActiveRecording);
            });
            if (!finalizing) {
                const QString pauseLabel = TranslationManager::tr(
                    paused ? "trayResumeRecording" : "trayPauseRecording");
                QAction *pauseAction = m_trayMenu->addAction(
                    trayIcon(paused ? ":/icons/resume.svg" : ":/icons/pause.svg"),
                    withShortcut(pauseLabel, HotkeyManager::instance().recordingPauseShortcutText()));
                connect(pauseAction, &QAction::triggered, this, [this]() {
                    QTimer::singleShot(0, this, &EShotApp::toggleActiveRecordingPause);
                });
            }
            QAction *cancelRecordingAction = m_trayMenu->addAction(
                trayIcon(":/icons/close.svg"),
                withShortcut(TranslationManager::trayCancelRecording(),
                             finalizing ? QString()
                                        : HotkeyManager::instance().recordingCancelShortcutText()));
            cancelRecordingAction->setEnabled(!finalizing);
            connect(cancelRecordingAction, &QAction::triggered, this, [this]() {
                QTimer::singleShot(0, this, &EShotApp::cancelActiveRecording);
            });
            if (m_recordingIndicator && !finalizing) {
                const bool locked = m_recordingIndicator->isBorderLocked();
                QAction *lockAction = m_trayMenu->addAction(
                    trayIcon(locked ? ":/icons/lock_open.svg" : ":/icons/lock.svg"),
                    TranslationManager::tr(locked ? "trayUnlockFrame" : "trayLockFrame"));
                lockAction->setToolTip(TranslationManager::tr("trayFrameLockTip"));
                connect(lockAction, &QAction::triggered, this, [this, locked]() {
                    QTimer::singleShot(0, this, [this, locked]() {
                        setRecordingBorderLocked(!locked);
                        rebuildTrayMenu();
                    });
                });
            }
            m_trayMenu->addSeparator();
        }

        QAction *captureAction = m_trayMenu->addAction(trayIcon(":/icons/copy.svg"), TranslationManager::trayCapture());
        QSettings hotkeySettings("EShot", "EShot");
        // Show the key that is actually registered. Before hotkeys are set up
        // (first-run wizard) only the saved key is known.
        auto hotkeyText = [this](int id, UINT savedModifiers, UINT savedVirtualKey) {
            if (!m_hotkeysInitialized)
                return HotkeyManager::shortcutText(savedModifiers, savedVirtualKey);
            const QString active = HotkeyManager::instance().activeShortcutText(id);
            return active.isEmpty() ? TranslationManager::hotkeyNoneActive() : active;
        };
        captureAction->setToolTip(QStringLiteral("%1 (%2)").arg(
            TranslationManager::trayCapture(),
            hotkeyText(HotkeyManager::HOTKEY_CAPTURE,
                       static_cast<UINT>(hotkeySettings.value("hotkeyModifiers", 0).toUInt()),
                       static_cast<UINT>(hotkeySettings.value("hotkeyVKey", VK_SNAPSHOT).toUInt()))));
        connect(captureAction, &QAction::triggered, this, &EShotApp::onCaptureRequested);
#ifdef Q_OS_WIN
        QAction *windowCaptureAction = m_trayMenu->addAction(
            trayIcon(":/icons/rectangle.svg"), TranslationManager::trayWindowCapture());
        windowCaptureAction->setToolTip(QStringLiteral("%1 (%2)").arg(
            TranslationManager::trayWindowCapture(),
            hotkeyText(HotkeyManager::HOTKEY_WINDOW_CAPTURE,
                       static_cast<UINT>(hotkeySettings.value("windowCaptureHotkeyModifiers", MOD_SHIFT).toUInt()),
                       static_cast<UINT>(hotkeySettings.value("windowCaptureHotkeyVKey", VK_SNAPSHOT).toUInt()))));
        connect(windowCaptureAction, &QAction::triggered,
                this, &EShotApp::onWindowCaptureRequested);
#endif

        if (videoRecording || gifRecording) {
            m_trayMenu->addSeparator();
        } else if (m_recordingStartCountdown) {
            QAction *cancelRecordingAction = m_trayMenu->addAction(
                trayIcon(":/icons/close.svg"), TranslationManager::trayCancelRecording());
            connect(cancelRecordingAction, &QAction::triggered, this, [this]() {
                QTimer::singleShot(0, this, [this]() { cancelPendingRecordingStart(); });
            });
            m_trayMenu->addSeparator();
        } else {
            auto recordingHotkey = [this](int id) {
                return m_hotkeysInitialized ? HotkeyManager::instance().activeShortcutText(id)
                                            : QString();
            };
            QAction *recordGifAction = m_trayMenu->addAction(
                trayIcon(":/icons/gif.svg"), TranslationManager::tr("trayRecordGif"));
            recordGifAction->setToolTip(withShortcut(
                TranslationManager::tr("trayRecordGif"),
                recordingHotkey(HotkeyManager::HOTKEY_GIF_CAPTURE)));
            connect(recordGifAction, &QAction::triggered, this, [this]() {
                QTimer::singleShot(0, this, &EShotApp::onRecordGifRequested);
            });
            QAction *recordVideoAction = m_trayMenu->addAction(
                trayIcon(":/icons/video.svg"), TranslationManager::tr("trayRecordVideo"));
            recordVideoAction->setToolTip(withShortcut(
                TranslationManager::tr("trayRecordVideo"),
                recordingHotkey(HotkeyManager::HOTKEY_VIDEO_CAPTURE)));
            connect(recordVideoAction, &QAction::triggered, this, [this]() {
                QTimer::singleShot(0, this, &EShotApp::onRecordVideoRequested);
            });
            m_trayMenu->addSeparator();
        }

        if (m_lastRecordingMenu)
            m_lastRecordingMenu->deleteLater();
        if (!m_lastRecordingPath.isEmpty() && QFileInfo::exists(m_lastRecordingPath)) {
            m_lastRecordingMenu = new QMenu(TranslationManager::tr("trayLastRecording"), m_trayMenu);
            m_lastRecordingMenu->setToolTipsVisible(true);
            const QString path = m_lastRecordingPath;
            const QString nativePath = QDir::toNativeSeparators(path);
            m_lastRecordingMenu->menuAction()->setToolTip(nativePath);
            QAction *openAction = m_lastRecordingMenu->addAction(
                TranslationManager::tr("recOpenFile"));
            openAction->setToolTip(nativePath);
            connect(openAction, &QAction::triggered, this, [this, path]() {
                QTimer::singleShot(0, this, [this, path]() { openRecordingFile(path); });
            });
            QAction *folderAction = m_lastRecordingMenu->addAction(
                TranslationManager::tr("recShowInFolder"));
            connect(folderAction, &QAction::triggered, this, [this, path]() {
                QTimer::singleShot(0, this, [this, path]() { showPathInFolder(path); });
            });
            QAction *copyAction = m_lastRecordingMenu->addAction(
                trayIcon(":/icons/copy.svg"), TranslationManager::tr("recCopyFile"));
            connect(copyAction, &QAction::triggered, this, [this, path]() {
                QTimer::singleShot(0, this, [this, path]() { copyRecordingFile(path); });
            });
            m_trayMenu->addMenu(m_lastRecordingMenu);
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

        if (m_recordingTrayActive)
            updateRecordingTrayToolTip();
        else if (!m_updateAvailable)
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
        connect(m_linuxNotification, &LinuxDesktopNotification::actionInvoked,
                this, &EShotApp::handleRecordingFileAction);
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
        connect(m_overlay, &CaptureOverlay::recordingSettingsRequested,
                this, &EShotApp::onRecordingSettingsRequested);
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
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingPauseRequested,
                this, &EShotApp::toggleActiveRecordingPause);
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingStopRequested, this, [this]() {
            if (cancelPendingRecordingStart()) return;
            stopActiveRecording();
        });
        connect(&HotkeyManager::instance(), &HotkeyManager::recordingCancelRequested,
                this, &EShotApp::cancelActiveRecording);
        connect(&HotkeyManager::instance(), &HotkeyManager::hotkeyRegistrationFailed,
                this, [this](const QList<int> &ids) {
            rebuildTrayMenu();
            showHotkeyFailureNotification(ids);
        });
        // Keys refused while the manager started up; give the tray a moment
        // to appear so the notification is not dropped.
        QList<int> startupFailures;
        for (const HotkeyBinding &failure : HotkeyManager::instance().failedHotkeys())
            startupFailures.append(failure.id);
        if (!startupFailures.isEmpty()) {
            QTimer::singleShot(1000, this, [this, startupFailures]() {
                showHotkeyFailureNotification(startupFailures);
            });
        }
    }

    bool closeBlockingDialogs()
    {
        bool closed = false;
        const auto widgets = QApplication::topLevelWidgets();
        for (QWidget *widget : widgets) {
            if (!widget || widget == m_overlay || !widget->isVisible())
                continue;
            // Rejecting the wizard deletes it and kills a running Linux
            // dependency installer; let the capture open over it instead.
            if (qobject_cast<FirstRunWizard *>(widget))
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

    void setTrayIconRecording()
    {
        if (!m_trayIcon) return;
        QIcon icon(m_blackTrayIcon
            ? QStringLiteral(":/icons/pen_tray_recording_black.svg")
            : QStringLiteral(":/icons/pen_tray_recording.svg"));
        if (!icon.isNull())
            m_trayIcon->setIcon(icon);
        updateRecordingTrayToolTip();
    }

    void updateRecordingTrayToolTip()
    {
        if (!m_trayIcon || !m_recordingTrayActive) return;
        QString state;
        if (recordingFinalizing()) {
            state = TranslationManager::tr(m_recordingIsGif ? "recConvertingGif" : "recSaving");
        } else {
            const QString time = recordingStatusTimeText(m_recordingElapsedSeconds,
                                                         m_recordingTimeLimitSeconds);
            state = TranslationManager::tr(activeRecordingPaused() ? "trayRecordingPausedTooltip"
                                                                   : "trayRecordingTooltip")
                        .arg(time);
        }
        m_trayIcon->setToolTip(QStringLiteral("%1 — %2").arg(TranslationManager::appTitle(), state));
        updateRecordingTrayStatus();
    }

    void setTrayIconUpdate()
    {
        if (!m_trayIcon) return;
        // The recording icon wins; finishRecordingUi() restores this one.
        if (m_recordingTrayActive) return;
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
        if (m_recordingTrayActive) return;
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
    QString m_lastNotificationPath;
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    LinuxDesktopNotification *m_linuxNotification = nullptr;
#endif
    qint64 m_skipNextCaptureNotificationMs = 0;
    bool m_hotkeysInitialized = false;
    ScreenRecorder *m_screenRecorder = nullptr;
    VideoRecorder *m_videoRecorder = nullptr;
    bool m_recordingStartPending = false;
    bool m_quitAfterRecording = false;
    QPointer<SettingsDialog> m_settingsDialog;
    QElapsedTimer m_recordingStartTimer;
    RecordingIndicator *m_recordingIndicator = nullptr;
    QPointer<RecordingStartCountdown> m_recordingStartCountdown;
    int m_pendingMode = 0;
    bool m_recordingFinalizing = false;
    bool m_recordingTrayActive = false;
    bool m_recordingIsGif = false;
    int m_recordingElapsedSeconds = 0;
    int m_recordingTimeLimitSeconds = 0;
    QString m_lastRecordingPath;
    QPointer<QMenu> m_lastRecordingMenu;
    QPointer<QAction> m_trayRecordingStatusAction;
    QStringList m_recordingDetails;
    bool m_recordingTrayTipShown = false;
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
    QCommandLineOption relaunchedOption("relaunched-as-user");
    relaunchedOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(relaunchedOption);
    QCommandLineOption fromElevatedTaskOption(
        QString::fromLatin1(WindowsElevatedStartup::TaskArgument).mid(2));
    fromElevatedTaskOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(fromElevatedTaskOption);
    QCommandLineOption testGifOption("test-gif", "Run internal GIF encoder test and exit.");
    parser.addOption(testGifOption);
    QCommandLineOption testRecordGifOption("test-record-gif", "Run internal GIF recording test and exit.");
    parser.addOption(testRecordGifOption);
    QCommandLineOption testOcrOption("test-ocr", "Run internal OCR test and exit. Requires a PNG path.", "path");
    parser.addOption(testOcrOption);
#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    QCommandLineOption uninstallOption(
        "uninstall", "Restore Print Screen and remove EShot's desktop integration for this user.");
    parser.addOption(uninstallOption);
#endif
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

#ifdef Q_OS_WIN
    // Older releases required administrator rights, so their updater and
    // legacy autostart task start this build elevated. Settings would then
    // live under the elevated account; hand over to the signed-in user. The
    // marker stops a loop when the desktop shell itself is elevated (UAC off).
    // Skipped when the user chose "Start as administrator" in Settings.
    const bool elevatedStartupChosen = parser.isSet(fromElevatedTaskOption)
        || settings.value("runElevated", false).toBool();
    if (parser.isSet(silentOption) && !parser.isSet(relaunchedOption)
        && !elevatedStartupChosen && WindowsElevatedStartup::isProcessElevated()) {
        const HRESULT comInit = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
        const bool relaunched = launchAsDesktopUser(
            QCoreApplication::applicationFilePath(),
            QStringLiteral("--silent --relaunched-as-user"));
        if (SUCCEEDED(comInit))
            CoUninitialize();
        if (relaunched)
            return 0;
    }
#endif

    // Keep the lock private to this user. A shared name in /tmp (or a
    // machine-wide pipe on Windows) lets another account's instance, or a
    // squatting process, swallow our commands.
#ifdef Q_OS_WIN
    const QString instanceName = QStringLiteral("EShot.SingleInstance.%1")
        .arg(qEnvironmentVariable("USERNAME"));
#else
    const QString runtimeDir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    const QString instanceName = runtimeDir.isEmpty()
        ? QStringLiteral("EShot.SingleInstance.%1").arg(qEnvironmentVariable("USER"))
        : QDir(runtimeDir).filePath(QStringLiteral("EShot.SingleInstance"));
#endif
    // Returns true if the command was forwarded to an already-running instance.
    auto forwardToRunningInstance = [&parser, &controlOption, &captureOption,
                                     &settingsOption, &saveOption, &quitOption,
                                     &silentOption, &instanceName]() {
        QLocalSocket socket;
        socket.connectToServer(instanceName);
        if (!socket.waitForConnected(150))
            return false;
        const auto command = parser.isSet(controlOption)
            ? ApplicationInstanceCommand::Control
            : ApplicationInstanceCommand::fromInvocation(
                parser.isSet(captureOption), parser.isSet(settingsOption),
                parser.isSet(saveOption), parser.isSet(quitOption),
                !parser.isSet(silentOption));
        const QByteArray wireCommand = ApplicationInstanceCommand::toWire(command);
        if (!wireCommand.isEmpty()) {
            socket.write(wireCommand);
            socket.waitForBytesWritten(500);
        }
        socket.disconnectFromServer();
        qDebug() << "[EShot] Forwarded command to the running instance:"
                 << wireCommand.trimmed();
        return true;
    };
    // Asks a running instance to quit and waits briefly until it is gone.
    auto quitRunningInstance = [&instanceName]() {
        QLocalSocket running;
        running.connectToServer(instanceName);
        if (!running.waitForConnected(150))
            return;
        running.write(ApplicationInstanceCommand::toWire(ApplicationInstanceCommand::Quit));
        running.waitForBytesWritten(500);
        running.disconnectFromServer();
        QElapsedTimer waited;
        waited.start();
        while (waited.elapsed() < 5000) {
            QLocalSocket probe;
            probe.connectToServer(instanceName);
            if (!probe.waitForConnected(100))
                break;
            probe.disconnectFromServer();
            QThread::msleep(100);
        }
    };
    // The elevated task replaces a normal instance that is still running
    // (e.g. right after "Start as administrator" was switched on).
    if (parser.isSet(fromElevatedTaskOption))
        quitRunningInstance();

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    // Terminal counterpart of Settings > Remove EShot. The running instance
    // must exit first so it cannot re-register its shortcuts afterwards.
    if (parser.isSet(uninstallOption)) {
        quitRunningInstance();
        const LinuxUninstaller::Report report = LinuxUninstaller::run();
        QTextStream out(stdout);
        for (const QString &path : report.removed)
            out << "Removed " << path << '\n';
        for (const QString &error : report.errors)
            out << "Error: " << error << '\n';
        if (report.kind == LinuxUninstallPolicy::InstallKind::Package) {
            out << "Desktop integration removed. Remove the EShot package with your package"
                   " manager, for example: sudo pacman -R eshot-bin\n";
        } else {
            out << "EShot was removed for this user. Screenshots and settings"
                   " (~/.config/EShot) were kept.\n";
        }
        out.flush();
        return report.errors.isEmpty() ? 0 : 1;
    }
#endif

    if (forwardToRunningInstance())
        return 0;

#ifdef Q_OS_WIN
    // "Start as administrator": a normal start hands over to the elevated
    // task. The task marks its own start, so a standard user (whose highest
    // run level is not elevated) cannot loop here.
    if (settings.value("runElevated", false).toBool() && !parser.isSet(fromElevatedTaskOption)
        && !WindowsElevatedStartup::isProcessElevated() && WindowsElevatedStartup::launchElevated()) {
        return 0;
    }
#endif

    if (parser.isSet(quitOption))
        return 0;

    QLocalServer instanceServer;
    instanceServer.setSocketOptions(QLocalServer::UserAccessOption);
    if (!instanceServer.listen(instanceName)) {
        // Another instance may have grabbed the lock between the probe above
        // and this listen() call. Retry the connection before removing the
        // socket file so a live server is never clobbered.
        if (forwardToRunningInstance())
            return 0;
        // Stale socket file left behind by a crashed instance.
        QLocalServer::removeServer(instanceName);
        if (!instanceServer.listen(instanceName)) {
            qWarning() << "[EShot] Could not create single-instance lock:" << instanceServer.errorString();
        }
    }
    const bool silent = parser.isSet(silentOption);
    const bool controlRequested = parser.isSet(controlOption)
        || (!silent && !parser.isSet(captureOption) && !parser.isSet(settingsOption)
            && !parser.isSet(saveOption) && !parser.isSet(quitOption));
    const bool firstRunRequired = !silent && FirstRunWizard::shouldShow();
    const bool prewarmOverlayAtStartup = !LinuxDesktopIntegration::deferOverlayPrewarmUntilFirstRunCompletes(
        firstRunRequired);
    EShotApp eshotApp(!firstRunRequired, prewarmOverlayAtStartup);

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
            // Do not persist it: closing the wizard must keep the saved language.
            TranslationManager::setLanguage(TranslationManager::English, false);
#endif
            auto *wizard = new FirstRunWizard();
            wizard->setAttribute(Qt::WA_DeleteOnClose);
#ifdef Q_OS_LINUX
            QObject::connect(wizard, &QObject::destroyed, []() { TranslationManager::init(); });
#endif
            wizard->show();
            QApplication::processEvents();
            QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
            if (!screen)
                screen = QGuiApplication::primaryScreen();
            if (screen) {
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
            eshotApp.prewarmOverlay();
            QTimer::singleShot(1500, &eshotApp, &EShotApp::showTrayWelcome);
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
            // Do not persist savePath here: a one-shot --save invocation must
            // not permanently change the configured save directory.
            QSettings s("EShot", "EShot");
            s.setValue("cliSaveFullPath", fi.absoluteFilePath());
            s.setValue("cliSaveRequestedAt", QDateTime::currentSecsSinceEpoch());
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
