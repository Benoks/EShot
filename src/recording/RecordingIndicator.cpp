#include "RecordingIndicator.h"

#include "../core/TranslationManager.h"

#include <QAction>
#include <QFrame>
#include <QGuiApplication>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPushButton>
#include <QRegion>
#include <QScreen>
#include <QStyle>
#include <QToolButton>
#include <QTimer>

#if defined(Q_OS_LINUX) && defined(ESHOT_HAVE_X11)
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#endif

#ifdef Q_OS_WIN
#include <windows.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace {
constexpr int ControlHeight = 42;
constexpr int PreferredControlWidth = 324;
constexpr int CompactControlWidth = 214;
constexpr int WindowPadding = 6;

QRect recordingScreenWorkArea(QScreen *screen)
{
    if (!screen)
        return {};
    // Cache the _NET_WORKAREA query per screen; opening a fresh X display on
    // every call is expensive. The cache is dropped whenever a screen's work
    // area or geometry changes (panel resize, screen added/removed, ...).
    static QHash<QScreen *, QRect> workAreaCache;
    static const bool cacheConnected = []() {
        auto *app = qobject_cast<QGuiApplication *>(QGuiApplication::instance());
        if (!app)
            return true;
        auto watchScreen = [app](QScreen *screen) {
            QObject::connect(screen, &QScreen::availableGeometryChanged, app,
                             [](const QRect &) { workAreaCache.clear(); });
            QObject::connect(screen, &QScreen::geometryChanged, app,
                             [](const QRect &) { workAreaCache.clear(); });
        };
        for (QScreen *screen : QGuiApplication::screens())
            watchScreen(screen);
        QObject::connect(app, &QGuiApplication::screenAdded, app, watchScreen);
        QObject::connect(app, &QGuiApplication::screenRemoved, app,
                         [](QScreen *) { workAreaCache.clear(); });
        return true;
    }();
    Q_UNUSED(cacheConnected);
    if (const auto it = workAreaCache.constFind(screen); it != workAreaCache.cend())
        return *it;

    QRect workArea = screen->availableGeometry();
#if defined(Q_OS_LINUX) && defined(ESHOT_HAVE_X11)
    if (QGuiApplication::platformName().contains(QStringLiteral("xcb"),
                                                  Qt::CaseInsensitive)) {
        if (Display *display = XOpenDisplay(nullptr)) {
            const Atom property = XInternAtom(display, "_NET_WORKAREA", True);
            if (property != None) {
                Atom actualType = None;
                int actualFormat = 0;
                unsigned long itemCount = 0;
                unsigned long bytesAfter = 0;
                unsigned char *data = nullptr;
                if (XGetWindowProperty(display, DefaultRootWindow(display), property,
                                       0, 4, False, XA_CARDINAL, &actualType,
                                       &actualFormat, &itemCount, &bytesAfter,
                                       &data) == Success
                    && actualType == XA_CARDINAL && actualFormat == 32
                    && itemCount >= 4 && data) {
                    const auto *values = reinterpret_cast<unsigned long *>(data);
                    const QRect desktopWorkArea(static_cast<int>(values[0]),
                                                static_cast<int>(values[1]),
                                                static_cast<int>(values[2]),
                                                static_cast<int>(values[3]));
                    const QRect screenWorkArea = screen->geometry().intersected(desktopWorkArea);
                    if (screenWorkArea.isValid())
                        workArea = screenWorkArea;
                }
                if (data)
                    XFree(data);
            }
            XCloseDisplay(display);
        }
    }
#endif
    workAreaCache.insert(screen, workArea);
    return workArea;
}

bool isFullScreenRecordingRect(const QRect &captureRect)
{
    QRect virtualDesktop;
    for (QScreen *screen : QGuiApplication::screens()) {
        if (!screen)
            continue;
        const QRect screenRect = screen->geometry();
        virtualDesktop = virtualDesktop.united(screenRect);
        if (captureRect == screenRect)
            return true;
    }
    return virtualDesktop.isValid() && captureRect == virtualDesktop;
}

QString controlBarStyle()
{
    return QStringLiteral(R"(
        QFrame#recordingControlBar {
            background-color: #2d2d2d;
            border: 1px solid #404040;
            border-radius: 10px;
        }
        QLabel#recordingStatusLabel {
            color: #F4F6F8;
            background: transparent;
            border: none;
            font-size: 12px;
            font-weight: 600;
        }
        QToolButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
            padding: 0px;
        }
        QToolButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
        QToolButton:pressed {
            background-color: #333333;
            border-color: #505050;
        }
        QPushButton#recordingStopButton {
            color: white;
            background-color: #E5484D;
            border: 1px solid #F05B60;
            border-radius: 7px;
            padding: 0 9px;
            font-size: 11px;
            font-weight: 700;
        }
        QPushButton#recordingStopButton:hover {
            background-color: #F05257;
            border-color: #FF7377;
        }
        QPushButton#recordingStopButton:pressed {
            background-color: #C93B40;
            border-color: #DD4A4F;
        }
        QToolButton#recordingCancelButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
        }
        QToolButton#recordingBorderLockButton:checked {
            background-color: #244c70;
            border-color: #3f98d5;
        }
        QMenu {
            color: #F4F6F8;
            background-color: #2d2d2d;
            border: 1px solid #404040;
            border-radius: 8px;
            padding: 5px;
        }
        QMenu::item {
            padding: 6px 14px;
            border-radius: 5px;
        }
        QMenu::item:disabled {
            color: #d6d6d6;
        }
    )");
}

class RecordingBorderOverlay final : public QWidget
{
public:
    RecordingBorderOverlay(const QRect &captureRect, int borderWidth)
        : QWidget(nullptr), m_borderWidth(qMax(1, borderWidth))
    {
        setObjectName(QStringLiteral("recordingBorderOverlay"));
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setCaptureRect(captureRect);
        setLocked(true);
    }

    void setCaptureRect(const QRect &captureRect)
    {
        const QRect windowRect = captureRect.adjusted(-m_borderWidth, -m_borderWidth,
                                                       m_borderWidth, m_borderWidth);
        setGeometry(windowRect);
        m_borderRect = QRect(m_borderWidth, m_borderWidth,
                             captureRect.width(), captureRect.height());
        QRegion borderRegion;
        borderRegion += QRect(m_borderRect.left(), m_borderRect.top() - m_borderWidth,
                              m_borderRect.width(), m_borderWidth);
        borderRegion += QRect(m_borderRect.left(), m_borderRect.bottom() + 1,
                              m_borderRect.width(), m_borderWidth);
        borderRegion += QRect(m_borderRect.left() - m_borderWidth, m_borderRect.top(),
                              m_borderWidth, m_borderRect.height());
        borderRegion += QRect(m_borderRect.right() + 1, m_borderRect.top(),
                              m_borderWidth, m_borderRect.height());
        setMask(borderRegion);
    }

    void setLocked(bool locked)
    {
        const bool wasVisible = isVisible();
        m_locked = locked;
        Qt::WindowFlags flags = Qt::Tool | Qt::FramelessWindowHint
            | Qt::WindowDoesNotAcceptFocus;
        if (recordingBorderStaysOnTop(locked))
            flags |= Qt::WindowStaysOnTopHint;
        setWindowFlags(flags);
        if (wasVisible) {
            show();
            if (locked)
                raise();
        }
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event)
        QPainter painter(this);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(63, 156, 255, 235));
        painter.drawRect(QRect(m_borderRect.left(), m_borderRect.top() - m_borderWidth,
                               m_borderRect.width(), m_borderWidth));
        painter.drawRect(QRect(m_borderRect.left(), m_borderRect.bottom() + 1,
                               m_borderRect.width(), m_borderWidth));
        painter.drawRect(QRect(m_borderRect.left() - m_borderWidth, m_borderRect.top(),
                               m_borderWidth, m_borderRect.height()));
        painter.drawRect(QRect(m_borderRect.right() + 1, m_borderRect.top(),
                               m_borderWidth, m_borderRect.height()));
    }

private:
    QRect m_borderRect;
    int m_borderWidth = 2;
    bool m_locked = false;
};
}

RecordingIndicator::RecordingIndicator(const QRect &captureRect, QWidget *parent,
                                       int borderWidth, bool supportsPause,
                                       RecordingIndicatorMode mode)
    : QWidget(parent),
      m_captureRect(captureRect),
      m_mode(mode),
      m_supportsPause(supportsPause),
      m_borderWidth(qMax(1, borderWidth))
{
    Qt::WindowFlags windowFlags = Qt::Tool | Qt::FramelessWindowHint
        | Qt::WindowDoesNotAcceptFocus;
    setWindowFlags(windowFlags);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setProperty("recordingBorderWidth", m_borderWidth);

    // Keep controls inside the desktop work area. Plasma panels are
    // always-on-top surfaces on Wayland and can cover a Qt tool window placed
    // at the physical bottom edge of the screen.
    QRect screenRect = QGuiApplication::primaryScreen()
        ? recordingScreenWorkArea(QGuiApplication::primaryScreen())
        : captureRect;
    if (QScreen *screen = QGuiApplication::screenAt(captureRect.center()))
        screenRect = recordingScreenWorkArea(screen);
    m_layout = recordingControlLayout(captureRect, screenRect,
                                      QSize(PreferredControlWidth, ControlHeight),
                                      QSize(CompactControlWidth, ControlHeight));
    m_screenRect = m_layout.controlScreenRect;
    m_compact = m_layout.compact;
    const RecordingCaptureBackend captureBackend =
#ifdef Q_OS_WIN
        RecordingCaptureBackend::WindowsGdiGrab;
#else
        RecordingCaptureBackend::LinuxPortal;
#endif
    m_platformCanExcludeOverlay = recordingCaptureBackendCanExcludeOverlay(captureBackend);
#ifdef Q_OS_LINUX
    // Linux portals cannot exclude an individual overlay window. Keep the
    // controls as an ordinary, non-always-on-top tool window so other apps can
    // cover them; the capture hint exposes the actual recording shortcuts.
    m_visibilityPolicy = RecordingOverlayVisibility::AlwaysVisible;
#else
    m_visibilityPolicy = recordingOverlayVisibilityPolicy(
        m_layout.controlRect, m_captureRect, m_platformCanExcludeOverlay);
#endif
    setProperty("captureSafeInside", requiresCaptureSafePresentation());
    setProperty("recordingControlGlobalRect", m_layout.controlRect);

    const QRect paddedControlRect = m_layout.controlRect.adjusted(
        -WindowPadding, -WindowPadding, WindowPadding, WindowPadding);
    const QRect globalWindowRect = paddedControlRect;
    setGeometry(globalWindowRect);
    m_controlRect = m_layout.controlRect.translated(-globalWindowRect.topLeft());

    m_borderOverlay = new RecordingBorderOverlay(m_captureRect, m_borderWidth);
    setBorderLocked(true);
    createControlBar();
    // Windows GDI capture cannot reliably exclude this window. Hide
    // overlapping controls before the first mapped frame there.
    if (requiresCaptureSafePresentation())
        setOverlayVisible(false);
    updateControlMask();
    updateStatusLabel();
    updatePauseButton();

    show();
    if (m_borderOverlay)
        m_borderOverlay->show();
    QTimer::singleShot(100, this, [this, globalWindowRect]() {
        qInfo() << "[RecordingIndicator] capture=" << m_captureRect
                << "control=" << m_layout.controlRect
                << "window-requested=" << globalWindowRect
                << "window-actual=" << geometry()
                << "platform=" << QGuiApplication::platformName();
    });
#ifdef Q_OS_WIN
    SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), WDA_EXCLUDEFROMCAPTURE);
    if (m_borderOverlay)
        SetWindowDisplayAffinity(reinterpret_cast<HWND>(m_borderOverlay->winId()),
                                 WDA_EXCLUDEFROMCAPTURE);
#endif
}

RecordingIndicator::~RecordingIndicator()
{
    delete m_borderOverlay;
}

void RecordingIndicator::createControlBar()
{
    m_controlBar = new QFrame(this);
    m_controlBar->setObjectName(QStringLiteral("recordingControlBar"));
    m_controlBar->setAttribute(Qt::WA_StyledBackground, true);
    m_controlBar->setGeometry(m_controlRect);
    m_controlBar->setStyleSheet(controlBarStyle());

    auto *layout = new QHBoxLayout(m_controlBar);
    layout->setContentsMargins(m_compact ? 6 : 8, 5, m_compact ? 6 : 8, 5);
    layout->setSpacing(4);

    m_statusLabel = new QLabel(m_controlBar);
    m_statusLabel->setObjectName(QStringLiteral("recordingStatusLabel"));
    m_statusLabel->setMinimumWidth(m_compact ? 58 : 92);
    m_statusLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_statusLabel->setCursor(Qt::SizeAllCursor);
    m_statusLabel->setToolTip(TranslationManager::recordingDrag());
    m_statusLabel->installEventFilter(this);
    m_controlBar->installEventFilter(this);
    layout->addWidget(m_statusLabel, 1);

    m_borderLockButton = new QToolButton(m_controlBar);
    m_borderLockButton->setObjectName(QStringLiteral("recordingBorderLockButton"));
    m_borderLockButton->setFixedSize(30, 30);
    m_borderLockButton->setIconSize(QSize(17, 17));
    m_borderLockButton->setCheckable(true);
    m_borderLockButton->setCursor(Qt::PointingHandCursor);
    m_borderLockButton->setToolTip(TranslationManager::actionLock());
    connect(m_borderLockButton, &QToolButton::toggled, this,
            &RecordingIndicator::setBorderLocked);
    layout->addWidget(m_borderLockButton);
    setBorderLocked(true);

    m_pauseButton = new QToolButton(m_controlBar);
    m_pauseButton->setObjectName(QStringLiteral("recordingPauseButton"));
    m_pauseButton->setFixedSize(30, 30);
    m_pauseButton->setIconSize(QSize(18, 18));
    m_pauseButton->setCursor(Qt::PointingHandCursor);
    m_pauseButton->setToolTip(TranslationManager::recordingPauseResume());
    m_pauseButton->setVisible(m_supportsPause);
    connect(m_pauseButton, &QToolButton::clicked, this, [this]() {
        if (m_paused) {
            if (requiresCaptureSafePresentation()) {
                setOverlayVisible(false);
                QTimer::singleShot(120, this, [this]() { emit resumeRequested(); });
            } else {
                emit resumeRequested();
            }
        } else {
            emit pauseRequested();
        }
    });
    layout->addWidget(m_pauseButton);

    m_stopButton = new QPushButton(m_controlBar);
    m_stopButton->setObjectName(QStringLiteral("recordingStopButton"));
    m_stopButton->setFixedHeight(30);
    m_stopButton->setMinimumWidth(m_compact ? 30 : 64);
    m_stopButton->setMaximumWidth(m_compact ? 30 : 72);
    m_stopButton->setIcon(QIcon(QStringLiteral(":/icons/stop.svg")));
    m_stopButton->setIconSize(QSize(16, 16));
    m_stopButton->setText(m_compact ? QString() : TranslationManager::recordingStopShort());
    m_stopButton->setCursor(Qt::PointingHandCursor);
    m_stopButton->setToolTip(TranslationManager::recordingStop());
    connect(m_stopButton, &QPushButton::clicked, this, &RecordingIndicator::stopRequested);
    layout->addWidget(m_stopButton);

    m_cancelButton = new QToolButton(m_controlBar);
    m_cancelButton->setObjectName(QStringLiteral("recordingCancelButton"));
    m_cancelButton->setFixedSize(30, 30);
    m_cancelButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_cancelButton->setIconSize(QSize(17, 17));
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setToolTip(TranslationManager::recordingCancel());
    connect(m_cancelButton, &QToolButton::clicked, this, &RecordingIndicator::cancelRequested);
    layout->addWidget(m_cancelButton);

    m_detailsMenu = new QMenu(m_controlBar);
    m_detailsButton = new QToolButton(m_controlBar);
    m_detailsButton->setObjectName(QStringLiteral("recordingDetailsButton"));
    m_detailsButton->setFixedSize(30, 30);
    m_detailsButton->setIcon(QIcon(QStringLiteral(":/icons/more.svg")));
    m_detailsButton->setIconSize(QSize(18, 18));
    m_detailsButton->setCursor(Qt::PointingHandCursor);
    m_detailsButton->setToolTip(TranslationManager::recordingDetails());
    m_detailsButton->setPopupMode(QToolButton::InstantPopup);
    m_detailsButton->setMenu(m_detailsMenu);
    layout->addWidget(m_detailsButton);

    setDetails({QStringLiteral("%1 × %2").arg(m_captureRect.width()).arg(m_captureRect.height())});
}

void RecordingIndicator::setFrameCount(int count)
{
    m_frameCount = qMax(0, count);
    updateStatusLabel();
}

void RecordingIndicator::setRemainingSeconds(int seconds)
{
    m_remainingSeconds = seconds;
    updateStatusLabel();
}

void RecordingIndicator::setElapsedSeconds(int seconds)
{
    m_elapsedSeconds = qMax(0, seconds);
    updateStatusLabel();
}

void RecordingIndicator::setPaused(bool paused)
{
    if (m_paused == paused)
        return;
    m_paused = paused;
    if (requiresCaptureSafePresentation())
        setOverlayVisible(paused);
    updateStatusLabel();
    updatePauseButton();
}

void RecordingIndicator::setDetails(const QStringList &details)
{
    if (!m_detailsMenu)
        return;
    m_detailsMenu->clear();
    for (const QString &detail : details) {
        QAction *action = m_detailsMenu->addAction(detail);
        action->setEnabled(false);
    }
    m_detailsButton->setVisible(!details.isEmpty());
}

void RecordingIndicator::setShortcutHints(const QString &pauseResume, const QString &stop,
                                          const QString &cancel)
{
    auto tooltip = [](const QString &label, const QString &shortcut) {
        return shortcut.trimmed().isEmpty()
            ? label
            : QStringLiteral("%1 (%2)").arg(label, shortcut);
    };
    if (m_pauseButton)
        m_pauseButton->setToolTip(tooltip(TranslationManager::recordingPauseResume(), pauseResume));
    if (m_stopButton)
        m_stopButton->setToolTip(tooltip(TranslationManager::recordingStop(), stop));
    if (m_cancelButton)
        m_cancelButton->setToolTip(tooltip(TranslationManager::recordingCancel(), cancel));
}

void RecordingIndicator::stop()
{
    m_running = false;
    if (m_borderOverlay)
        m_borderOverlay->hide();
    close();
}

bool RecordingIndicator::controlsInside() const
{
    return m_layout.placement == RecordingControlPlacement::Inside;
}

bool RecordingIndicator::requiresCaptureSafePresentation() const
{
    return m_visibilityPolicy == RecordingOverlayVisibility::RevealWhilePaused;
}

void RecordingIndicator::startCaptureSafePresentation()
{
    if (!requiresCaptureSafePresentation() || m_captureSafePresentationStarted)
        return;
    m_captureSafePresentationStarted = true;
    // There is no reliable compositor API for excluding an overlay window
    // from a full-screen portal recording. Keep inside controls hidden until
    // the user deliberately pauses through the global shortcut or tray.
    setOverlayVisible(false);
}

void RecordingIndicator::updateStatusLabel()
{
    if (!m_statusLabel)
        return;
    const int minutes = m_elapsedSeconds / 60;
    const int seconds = m_elapsedSeconds % 60;
    const QString time = QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'));
    const QString mode = m_mode == RecordingIndicatorMode::Video
        ? QStringLiteral("VIDEO") : QStringLiteral("GIF");
    const bool visiblyPaused = m_paused;
    const QString dotColor = visiblyPaused ? QStringLiteral("#A3A9B3") : QStringLiteral("#FF525D");
    const QString text = m_compact
        ? QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2").arg(dotColor, time)
        : QStringLiteral("<span style='color:%1'>●</span>&nbsp; %2&nbsp;&nbsp;%3")
              .arg(dotColor, mode, time);
    m_statusLabel->setText(text);
}

void RecordingIndicator::updatePauseButton()
{
    if (!m_pauseButton)
        return;
    const bool visiblyPaused = m_paused;
    m_pauseButton->setIcon(QIcon(visiblyPaused
        ? QStringLiteral(":/icons/resume.svg")
        : QStringLiteral(":/icons/pause.svg")));
    m_pauseButton->setProperty("recordingState", visiblyPaused ? QStringLiteral("resume")
                                                                : QStringLiteral("pause"));
    m_pauseButton->style()->unpolish(m_pauseButton);
    m_pauseButton->style()->polish(m_pauseButton);
}

void RecordingIndicator::updateControlMask()
{
    if (QGuiApplication::platformName() == QStringLiteral("offscreen"))
        return;
    QRegion region;
    if (!m_overlayVisible && requiresCaptureSafePresentation()) {
        region += m_controlRect;
        setMask(region);
        return;
    }
    region += m_controlRect.adjusted(-3, -3, 3, 3);
    setMask(region);
}

void RecordingIndicator::setOverlayVisible(bool visible)
{
    if (m_overlayVisible == visible)
        return;
    m_overlayVisible = visible;
    setAttribute(Qt::WA_TransparentForMouseEvents,
                 !visible && requiresCaptureSafePresentation());
    if (m_controlBar)
        m_controlBar->setVisible(visible);
    if (m_borderOverlay)
        m_borderOverlay->setVisible(visible);
    updateControlMask();
    update();
}

void RecordingIndicator::setBorderLocked(bool locked)
{
    m_borderLocked = locked;
    if (m_borderOverlay)
        static_cast<RecordingBorderOverlay *>(m_borderOverlay)->setLocked(locked);
    if (m_borderLockButton) {
        const QSignalBlocker blocker(m_borderLockButton);
        m_borderLockButton->setChecked(locked);
        m_borderLockButton->setIcon(QIcon(locked
            ? QStringLiteral(":/icons/lock.svg")
            : QStringLiteral(":/icons/lock_open.svg")));
    }
}

void RecordingIndicator::updateCaptureSafetyPolicy()
{
#ifdef Q_OS_LINUX
    m_visibilityPolicy = RecordingOverlayVisibility::AlwaysVisible;
#else
    m_visibilityPolicy = recordingOverlayVisibilityPolicy(
        m_layout.controlRect, m_captureRect, m_platformCanExcludeOverlay);
#endif
    setProperty("captureSafeInside", requiresCaptureSafePresentation());
}

void RecordingIndicator::moveControlBar(const QPoint &requestedTopLeft)
{
    const QPoint requestedCenter = requestedTopLeft
        + QPoint(m_layout.controlRect.width() / 2, m_layout.controlRect.height() / 2);
    QRect requestedScreenRect = m_screenRect;
    if (QScreen *screen = QGuiApplication::screenAt(requestedCenter))
        requestedScreenRect = recordingScreenWorkArea(screen);
    const QRect globalControl = movedRecordingControlRect(
        m_layout.controlRect, requestedTopLeft, requestedScreenRect);
    if (globalControl == m_layout.controlRect)
        return;

    // When the current capture backend cannot exclude overlay windows, never
    // let an already-safe control bar cross into the recorded region. Hiding
    // it after the drag would strand the user without visible stop controls
    // and hover-based reveals would introduce pauses into the recording.
    const bool wouldLoseVisibleControls =
#if defined(Q_OS_LINUX)
        false;
#else
        !m_platformCanExcludeOverlay
        && !m_layout.controlRect.intersects(m_captureRect)
        && globalControl.intersects(m_captureRect);
#endif
    if (wouldLoseVisibleControls) {
        return;
    }

    m_screenRect = requestedScreenRect;
    m_layout.controlRect = globalControl;
    m_layout.controlScreenRect = m_screenRect;
    const QRect paddedControlRect = globalControl.adjusted(
        -WindowPadding, -WindowPadding, WindowPadding, WindowPadding);
    const QRect globalWindowRect = paddedControlRect;
    setGeometry(globalWindowRect);
    m_controlRect = globalControl.translated(-globalWindowRect.topLeft());
    if (m_controlBar)
        m_controlBar->setGeometry(m_controlRect);
    setProperty("recordingControlGlobalRect", globalControl);
    updateCaptureSafetyPolicy();
    updateControlMask();
    update();
}

bool RecordingIndicator::eventFilter(QObject *watched, QEvent *event)
{
    const bool dragSurface = watched == m_controlBar || watched == m_statusLabel;
    if (!dragSurface)
        return QWidget::eventFilter(watched, event);

    auto *mouse = dynamic_cast<QMouseEvent *>(event);
    if (!mouse)
        return QWidget::eventFilter(watched, event);

    if (event->type() == QEvent::MouseButtonPress
        && mouse->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = mouse->globalPosition().toPoint() - m_layout.controlRect.topLeft();
        if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
            if (auto *widget = qobject_cast<QWidget *>(watched))
                widget->grabMouse();
        }
        return true;
    }
    if (event->type() == QEvent::MouseMove && m_dragging
        && (mouse->buttons() & Qt::LeftButton)) {
        moveControlBar(mouse->globalPosition().toPoint() - m_dragOffset);
        return true;
    }
    if (event->type() == QEvent::MouseButtonRelease && m_dragging
        && mouse->button() == Qt::LeftButton) {
        m_dragging = false;
        if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
            if (auto *widget = qobject_cast<QWidget *>(watched))
                widget->releaseMouse();
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
