#ifndef LINUXPORTALSCREENSHOT_H
#define LINUXPORTALSCREENSHOT_H

#include <QObject>
#include <QPixmap>
#include <QVariantMap>

class QWidget;
class QEventLoop;
class QScreen;

class LinuxPortalScreenshot : public QObject {
    Q_OBJECT

public:
    // Starts the portal service without requesting screen contents. Calling this
    // during app startup removes the portal activation cost from the first hotkey.
    static void warmup();
    static QPixmap grab(QWidget *parent = nullptr, int timeoutMs = 120000);
    static QPixmap grabScreen(QScreen *screen, QWidget *parent = nullptr, int timeoutMs = 5000);
    static QPixmap grabWorkspace(QWidget *parent = nullptr, int timeoutMs = 5000);
    static QPixmap grabSpectacleWorkspace(QWidget *parent = nullptr, int timeoutMs = 15000);
    // Rebuilds KDE's service cache in the background so KWin learns about
    // EShot's ScreenShot2 permission entry without delaying startup. A KWin
    // capture that starts before the rebuild finished waits for it.
    static void startKWinPermissionRefresh(const QString &cacheBuilder);
    static void waitForKWinPermissionRefresh(int timeoutMs = 10000);
    // Reads exactly size bytes from a pipe fd without blocking past the
    // deadline. Returns the number of bytes read; less than size means EOF,
    // a read error or the timeout. The fd is switched to non-blocking mode.
    static qint64 readPipeWithDeadline(int fd, char *buffer, qint64 size, int timeoutMs);

private:
    explicit LinuxPortalScreenshot(QObject *parent = nullptr);
    QPixmap waitForScreenshot(QWidget *parent, int timeoutMs);

private slots:
    void onPortalResponse(uint response, const QVariantMap &results);

private:
    QPixmap m_pixmap;
    bool m_finished = false;
    QEventLoop *m_loop = nullptr;
};

#endif
