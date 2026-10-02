#pragma once

#include <QObject>
#include <QPointer>
#include <QRect>
#include <QVector>

class QGraphicsProxyWidget;
class QGraphicsScene;
class QGraphicsView;
class QScreen;
class QWidget;

// One shared QWidget/scene, displayed by one native surface per output. Qt's
// proxy widget routes selection, child-widget input and IME to the same model;
// each view paints that model using its own output's device-pixel ratio.
class CaptureScreenViews : public QObject
{
    Q_OBJECT

public:
    explicit CaptureScreenViews(QWidget *canvas);
    ~CaptureScreenViews() override;

    void present(const QRect &virtualDesktop, const QList<QScreen *> &screens,
                 QScreen *activeScreen);
    void hideViews();
    bool isActive() const { return m_active; }
    QVector<QGraphicsView *> views() const;

signals:
    void closeRequested();

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    QPointer<QWidget> m_canvas;
    QGraphicsScene *m_scene;
    QGraphicsProxyWidget *m_proxy = nullptr;
    QVector<QPointer<QGraphicsView>> m_views;
    bool m_active = false;
    bool m_presenting = false;
};
