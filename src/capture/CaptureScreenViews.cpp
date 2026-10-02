#include "CaptureScreenViews.h"

#include <QEvent>
#include <QGuiApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QScreen>
#include <QScopedValueRollback>
#include <QWidget>
#include <QWindow>

CaptureScreenViews::CaptureScreenViews(QWidget *canvas)
    : m_canvas(canvas)
    , m_scene(new QGraphicsScene(this))
{
    canvas->installEventFilter(this);
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this]() {
        if (m_active) {
            emit closeRequested();
            hideViews();
        }
    });
}

CaptureScreenViews::~CaptureScreenViews()
{
    hideViews();
    if (m_canvas)
        m_canvas->removeEventFilter(this);
    // QGraphicsProxyWidget otherwise shares ownership of the canvas. The
    // application owns it, so detach it before destroying the scene.
    if (m_proxy)
        m_proxy->setWidget(nullptr);
    for (const auto &view : m_views)
        delete view;
}

void CaptureScreenViews::present(const QRect &virtualDesktop,
                                 const QList<QScreen *> &screens,
                                 QScreen *activeScreen)
{
    if (!m_canvas || !virtualDesktop.isValid() || screens.isEmpty())
        return;
    QScopedValueRollback<bool> presenting(m_presenting, true);
    hideViews();
    for (const auto &view : m_views)
        delete view;
    m_views.clear();

    m_canvas->resize(virtualDesktop.size());
    if (!m_proxy) {
        m_proxy = m_scene->addWidget(m_canvas);
        m_proxy->setCacheMode(QGraphicsItem::NoCache);
        m_proxy->setFlag(QGraphicsItem::ItemUsesExtendedStyleOption, true);
    }
    m_scene->setSceneRect(virtualDesktop);
    m_proxy->setPos(virtualDesktop.topLeft());

    QGraphicsView *activeView = nullptr;
    for (QScreen *screen : screens) {
        auto *view = new QGraphicsView(m_scene);
        view->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        view->setAttribute(Qt::WA_ShowWithoutActivating, screen != activeScreen);
        view->setWindowTitle(QStringLiteral("EShot Capture %1").arg(screen->name()));
        view->setProperty("captureScreenName", screen->name());
        view->setFrameShape(QFrame::NoFrame);
        view->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        view->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        view->setSceneRect(screen->geometry());
        view->setBackgroundBrush(Qt::black);
        view->setRenderHint(QPainter::SmoothPixmapTransform, false);
        // Whole-pixel outputs can retain unchanged backing-store pixels. Do
        // not repaint both complete screens for a moving frame/crosshair.
        // Fractional outputs keep the conservative full repaint to avoid
        // stale rounded edge pixels.
        const qreal scale = screen->devicePixelRatio();
        const bool wholePixelScale = qFuzzyCompare(scale, qreal(qRound(scale)));
        view->setViewportUpdateMode(wholePixelScale ? QGraphicsView::MinimalViewportUpdate
                                                  : QGraphicsView::FullViewportUpdate);
        view->setMouseTracking(true);
        view->viewport()->setMouseTracking(true);
        view->setFocusPolicy(Qt::StrongFocus);
        view->installEventFilter(this);
        view->winId();
        view->windowHandle()->setScreen(screen);
        view->setGeometry(screen->geometry());
        m_views.append(view);
        if (screen == activeScreen)
            activeView = view;
    }
    m_canvas->show();
    for (const auto &view : m_views)
        view->showFullScreen();
    m_active = true;
    if (!activeView)
        activeView = m_views.first();
    activeView->raise();
    activeView->activateWindow();
    activeView->setFocus(Qt::OtherFocusReason);
    m_proxy->setFocus(Qt::OtherFocusReason);
    m_canvas->setFocus(Qt::OtherFocusReason);
}

void CaptureScreenViews::hideViews()
{
    m_active = false;
    if (m_canvas && m_canvas->isVisible())
        m_canvas->hide();
    for (const auto &view : m_views) {
        if (view)
            view->hide();
    }
}

QVector<QGraphicsView *> CaptureScreenViews::views() const
{
    QVector<QGraphicsView *> result;
    for (const auto &view : m_views) {
        if (view)
            result.append(view);
    }
    return result;
}

bool CaptureScreenViews::eventFilter(QObject *object, QEvent *event)
{
    if (object == m_canvas && event->type() == QEvent::Hide && m_active && !m_presenting)
        hideViews();
    if (event->type() == QEvent::Close && m_active && !m_presenting) {
        for (const auto &view : m_views) {
            if (object == view) {
                emit closeRequested();
                hideViews();
                break;
            }
        }
    }
    return QObject::eventFilter(object, event);
}
