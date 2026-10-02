#include <QtTest/QTest>

#include "capture/CaptureScreenViews.h"
#include "capture/CaptureScreenImage.h"

#include <QApplication>
#include <QGraphicsProxyWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QImage>
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QMouseEvent>
#include <QScreen>
#include <QSignalSpy>
#include <QScopeGuard>

namespace {
class TestCanvas : public QWidget
{
public:
    QPoint lastPress;
    QPoint lastRelease;
    QVector<CaptureScreenImage> images;
    int paintCount = 0;
    qint64 paintedArea = 0;

protected:
    void paintEvent(QPaintEvent *event) override
    {
        ++paintCount;
        for (const QRect &rect : event->region())
            paintedArea += qint64(rect.width()) * rect.height();
        QPainter painter(this);
        if (!images.isEmpty()) {
            paintCaptureImages(&painter, images, QPoint());
            return;
        }
        painter.fillRect(rect(), Qt::white);
        for (int x = 0; x < width(); x += 2)
            painter.fillRect(QRect(x, 0, 1, height()), Qt::black);
    }
    void mousePressEvent(QMouseEvent *event) override { lastPress = event->pos(); }
    void mouseReleaseEvent(QMouseEvent *event) override { lastRelease = event->pos(); }
};

void setupView(QGraphicsView &view, const QRect &rect)
{
    view.setFrameShape(QFrame::NoFrame);
    view.setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    view.setAlignment(Qt::AlignLeft | Qt::AlignTop);
    view.setSceneRect(rect);
    view.resize(rect.size());
    view.show();
    QApplication::processEvents();
}
}

class CaptureScreenViewsTests : public QObject
{
    Q_OBJECT

private slots:
    void createsOutputViewsAndClosesThemWithTheCanvas();
    void doesNotTakeOwnershipOfTheCanvas();
    void closesAllViewsWhenOneOutputWindowIsClosed();
    void rendersSharedWidgetAtBothPixelDensities();
    void routesInputFromBothViewsToTheSameWidget();
    void preservesDragCoordinatesAcrossTheOutputBoundary();
    void routesChildWidgetClicksFromEitherView();
    void smallChangesDoNotRepaintTheWholeOutput();
};

void CaptureScreenViewsTests::createsOutputViewsAndClosesThemWithTheCanvas()
{
    TestCanvas canvas;
    CaptureScreenViews views(&canvas);
    const auto screens = QApplication::screens();
    const QRect desktop = screens.first()->geometry();
    views.present(desktop, screens, screens.first());
    QVERIFY(views.isActive());
    QCOMPARE(views.views().size(), screens.size());
    QVERIFY(canvas.isVisible());
    canvas.hide();
    QVERIFY(!views.isActive());
    for (auto *view : views.views())
        QVERIFY(!view->isVisible());
    views.present(desktop, screens, screens.first());
    QVERIFY(views.isActive());
}

void CaptureScreenViewsTests::doesNotTakeOwnershipOfTheCanvas()
{
    QPointer<QWidget> canvas = new TestCanvas;
    {
        CaptureScreenViews views(canvas);
        const auto screens = QApplication::screens();
        views.present(screens.first()->geometry(), screens, screens.first());
    }
    QVERIFY(canvas);
    delete canvas;
}

void CaptureScreenViewsTests::closesAllViewsWhenOneOutputWindowIsClosed()
{
    TestCanvas canvas;
    CaptureScreenViews views(&canvas);
    QSignalSpy closeRequests(&views, &CaptureScreenViews::closeRequested);
    const auto screens = QApplication::screens();
    views.present(screens.first()->geometry(), screens, screens.first());
    views.views().first()->close();
    QCOMPARE(closeRequests.count(), 1);
    QVERIFY(!views.isActive());
    QVERIFY(!canvas.isVisible());
}

void CaptureScreenViewsTests::rendersSharedWidgetAtBothPixelDensities()
{
    TestCanvas canvas;
    canvas.resize(128, 32);
    for (int i = 0; i < 2; ++i) {
        const int scale = i + 1;
        QImage image(QSize(64 * scale, 32 * scale), QImage::Format_ARGB32_Premultiplied);
        for (int y = 0; y < image.height(); ++y) {
            auto *row = reinterpret_cast<QRgb *>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x)
                row[x] = x % 2 ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
        }
        canvas.images.append({QRect(64 * i, 0, 64, 32), image, qreal(scale)});
    }
    QGraphicsScene scene;
    auto *proxy = scene.addWidget(&canvas);
    const auto detach = qScopeGuard([proxy]() { proxy->setWidget(nullptr); });
    proxy->setCacheMode(QGraphicsItem::NoCache);
    for (const qreal scale : {1.0, 2.0}) {
        QImage image(QSize(qRound(64 * scale), qRound(32 * scale)),
                     QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(scale);
        image.fill(Qt::red);
        QPainter painter(&image);
        const int left = scale == 1.0 ? 0 : 64;
        scene.render(&painter, QRectF(0, 0, 64, 32), QRectF(left, 0, 64, 32));
        painter.end();
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const QRgb expected = x % 2 ? qRgb(255, 255, 255) : qRgb(0, 0, 0);
                QCOMPARE(image.pixel(x, y), expected);
            }
        }
    }
    proxy->setWidget(nullptr);
}

void CaptureScreenViewsTests::routesInputFromBothViewsToTheSameWidget()
{
    TestCanvas canvas;
    canvas.resize(128, 32);
    QGraphicsScene scene;
    auto *proxy = scene.addWidget(&canvas);
    const auto detach = qScopeGuard([proxy]() { proxy->setWidget(nullptr); });
    QGraphicsView left(&scene), right(&scene);
    setupView(left, QRect(0, 0, 64, 32));
    setupView(right, QRect(64, 0, 64, 32));
    QTest::mouseClick(left.viewport(), Qt::LeftButton, Qt::NoModifier,
                      left.mapFromScene(QPointF(10, 10)));
    QCOMPARE(canvas.lastPress, QPoint(10, 10));
    QTest::mouseClick(right.viewport(), Qt::LeftButton, Qt::NoModifier,
                      right.mapFromScene(QPointF(74, 10)));
    QCOMPARE(canvas.lastPress, QPoint(74, 10));
    proxy->setWidget(nullptr);
}

void CaptureScreenViewsTests::preservesDragCoordinatesAcrossTheOutputBoundary()
{
    TestCanvas canvas;
    canvas.resize(128, 32);
    QGraphicsScene scene;
    auto *proxy = scene.addWidget(&canvas);
    const auto detach = qScopeGuard([proxy]() { proxy->setWidget(nullptr); });
    QGraphicsView left(&scene), right(&scene);
    setupView(left, QRect(0, 0, 64, 32));
    setupView(right, QRect(64, 0, 64, 32));
    // Wayland's implicit grab keeps delivering to the initiating surface,
    // including positions outside it after the pointer crosses an output.
    QTest::mousePress(left.viewport(), Qt::LeftButton, Qt::NoModifier,
                     left.mapFromScene(QPointF(60, 10)));
    QTest::mouseRelease(left.viewport(), Qt::LeftButton, Qt::NoModifier,
                       left.mapFromScene(QPointF(68, 10)));
    QCOMPARE(canvas.lastPress, QPoint(60, 10));
    QCOMPARE(canvas.lastRelease, QPoint(68, 10));
}

void CaptureScreenViewsTests::routesChildWidgetClicksFromEitherView()
{
    QWidget canvas;
    canvas.resize(400, 100);
    QPushButton leftButton("Left", &canvas), rightButton("Right", &canvas);
    leftButton.setGeometry(10, 10, 80, 40);
    rightButton.setGeometry(210, 10, 80, 40);
    QSignalSpy leftClicks(&leftButton, &QPushButton::clicked);
    QSignalSpy rightClicks(&rightButton, &QPushButton::clicked);
    QGraphicsScene scene;
    auto *proxy = scene.addWidget(&canvas);
    const auto detach = qScopeGuard([proxy]() { proxy->setWidget(nullptr); });
    QGraphicsView left(&scene), right(&scene);
    setupView(left, QRect(0, 0, 200, 100));
    setupView(right, QRect(200, 0, 200, 100));
    QTest::mouseClick(left.viewport(), Qt::LeftButton, Qt::NoModifier,
                      left.mapFromScene(QPointF(40, 30)));
    QTest::mouseClick(right.viewport(), Qt::LeftButton, Qt::NoModifier,
                      right.mapFromScene(QPointF(240, 30)));
    QCOMPARE(leftClicks.count(), 1);
    QCOMPARE(rightClicks.count(), 1);
    proxy->setWidget(nullptr);
}

void CaptureScreenViewsTests::smallChangesDoNotRepaintTheWholeOutput()
{
    TestCanvas canvas;
    CaptureScreenViews views(&canvas);
    const auto screens = QApplication::screens();
    views.present(screens.first()->geometry(), screens, screens.first());
    QTRY_VERIFY(canvas.paintCount > 0);
    for (auto *view : views.views())
        QCOMPARE(view->viewportUpdateMode(), QGraphicsView::MinimalViewportUpdate);
    canvas.paintCount = 0;
    canvas.paintedArea = 0;
    canvas.update(QRect(80, 80, 16, 16));
    QTRY_VERIFY(canvas.paintCount > 0);
    const qint64 fullArea = qint64(canvas.width()) * canvas.height();
    QVERIFY2(canvas.paintedArea < fullArea / 4,
             qPrintable(QStringLiteral("Small update repainted %1 of %2 logical pixels")
                            .arg(canvas.paintedArea).arg(fullArea)));
}

QTEST_MAIN(CaptureScreenViewsTests)

#include "CaptureScreenViewsTests.moc"
