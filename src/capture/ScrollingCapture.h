#ifndef SCROLLINGCAPTURE_H
#define SCROLLINGCAPTURE_H

#include <QObject>
#include <QRect>
#include <QTimer>
#include <QImage>
#include <QList>
#include <QString>

class ScrollingCapture : public QObject {
    Q_OBJECT

public:
    explicit ScrollingCapture(QObject *parent = nullptr);
    ~ScrollingCapture();

    bool isRunning() const { return m_running; }
    int frameCount() const { return m_frames.size(); }

    void start(const QRect &captureRect, int scrollAmount, int intervalMs, int maxFrames);
    void stop(const QString &reason = QString());

signals:
    void started();
    void frameAdded(int count);
    void progress(int count);
    void finished(const QImage &result);
    void failed(const QString &reason);

private:
    void captureNext();
    QImage grabScreenRegion(const QRect &rect) const;
    void scrollContent(int amount);
    QImage stitchFrames() const;

    QTimer *m_timer = nullptr;
    QRect m_captureRect;
    int m_scrollAmount = 0;
    int m_intervalMs = 100;
    int m_maxFrames = 20;
    int m_initialDelayMs = 0;
    QList<QImage> m_frames;
    bool m_running = false;
};

#endif
