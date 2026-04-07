#include <QApplication>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QIcon>
#include <QPixmap>
#include <QDebug>
#include <QSettings>
#include <QPalette>

#include "core/HotkeyManager.h"
#include "capture/CaptureOverlay.h"
#include "ui/SettingsDialog.h"
#include "ui/AboutDialog.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class EShotApp : public QObject {
    Q_OBJECT

public:
    EShotApp(QObject *parent = nullptr) : QObject(parent)
    {
        loadSettings();
        setupTrayIcon();
        setupOverlay();
        setupHotkey();
        qDebug() << "[EShot] Application started. Press PrtSc to capture.";
    }

    ~EShotApp()
    {
        if (m_trayIcon) m_trayIcon->hide();
        if (m_overlay) { delete m_overlay; m_overlay = nullptr; }
    }

private slots:
    void onCaptureRequested()
    {
        qDebug() << "[EShot] Capture requested";
        // Fix: eğer overlay zaten görünürdeyse (ss modu aktifse) yeniden başlatma
        if (m_overlay && m_overlay->isVisible()) {
            qDebug() << "[EShot] Capture already active, ignoring hotkey.";
            return;
        }
        if (m_overlay) m_overlay->startCapture();
    }

    void onCaptureCompleted(const QPixmap &pixmap)
    {
        Q_UNUSED(pixmap);
        qDebug() << "[EShot] Capture completed, size:" << pixmap.size();
        if (m_trayIcon && m_showNotifications) {
            m_trayIcon->showMessage("EShot",
                QString("Ekran görüntüsü alındı (%1x%2)")
                    .arg(pixmap.width()).arg(pixmap.height()),
                QSystemTrayIcon::Information, 2000);
        }
    }

    void onCaptureCancelled()
    {
        qDebug() << "[EShot] Capture cancelled";
    }

    void onTrayActivated(QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::DoubleClick) onCaptureRequested();
    }

    void onQuitAction() { qApp->quit(); }

    void onSettingsRequested()
    {
        SettingsDialog dlg;
        if (dlg.exec() == QDialog::Accepted) loadSettings();
    }

    void onAboutRequested()
    {
        AboutDialog dlg;
        dlg.exec();
    }

private:
    void loadSettings()
    {
        QSettings s("EShot", "EShot");
        m_showNotifications = s.value("showNotifications", true).toBool();
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
        m_trayIcon->setToolTip("EShot - Ekran Görüntüsü Aracı\nPrtSc ile yakalama yapın");

        QMenu *menu = new QMenu();
        QAction *captureAction = menu->addAction(QIcon(":/icons/copy.svg"), "Yakala");
        connect(captureAction, &QAction::triggered, this, &EShotApp::onCaptureRequested);
        QAction *settingsAction = menu->addAction(QIcon(":/icons/save.svg"), "Ayarlar");
        connect(settingsAction, &QAction::triggered, this, &EShotApp::onSettingsRequested);
        QAction *aboutAction = menu->addAction(QIcon(":/icons/pen.svg"), "Hakkında");
        connect(aboutAction, &QAction::triggered, this, &EShotApp::onAboutRequested);
        menu->addSeparator();
        QAction *quitAction = menu->addAction(QIcon(":/icons/close.svg"), "Çıkış");
        connect(quitAction, &QAction::triggered, this, &EShotApp::onQuitAction);

        m_trayIcon->setContextMenu(menu);
        connect(m_trayIcon, &QSystemTrayIcon::activated, this, &EShotApp::onTrayActivated);
        m_trayIcon->show();
    }

    void setupOverlay()
    {
        m_overlay = new CaptureOverlay();
        connect(m_overlay, &CaptureOverlay::captureCompleted, this, &EShotApp::onCaptureCompleted);
        connect(m_overlay, &CaptureOverlay::captureCancelled, this, &EShotApp::onCaptureCancelled);
    }

    void setupHotkey()
    {
        connect(&HotkeyManager::instance(), &HotkeyManager::captureRequested,
                this, &EShotApp::onCaptureRequested);
    }

    QSystemTrayIcon *m_trayIcon = nullptr;
    CaptureOverlay *m_overlay = nullptr;
    bool m_showNotifications = true;
};

#include "main.moc"

int main(int argc, char *argv[])
{
#ifdef Q_OS_WIN
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#endif
    QGuiApplication::setHighDpiScaleFactorRoundingPolicy(
        Qt::HighDpiScaleFactorRoundingPolicy::PassThrough);

    QApplication app(argc, argv);
    app.setApplicationName("EShot");
    app.setApplicationVersion("1.1.0");
    app.setOrganizationName("EShot");
    app.setQuitOnLastWindowClosed(false);
    app.setStyle("Fusion");

    // Koyu tema
    QSettings settings("EShot", "EShot");
    if (settings.value("darkMode", true).toBool()) {
        QPalette p;
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
        app.setPalette(p);
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        qWarning() << "[EShot] System tray is not available!";
        return 1;
    }

    EShotApp eshotApp;
    return app.exec();
}