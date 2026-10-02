#ifndef RECORDINGSTARTCOUNTDOWN_H
#define RECORDINGSTARTCOUNTDOWN_H

#include <QElapsedTimer>
#include <QRect>
#include <QWidget>

class QLabel;
class QTimer;

// Small always-on-top countdown shown over the selected region while a
// recording start delay runs. It hides itself before finished() so it is
// never part of the first recorded frame.
class RecordingStartCountdown : public QWidget
{
    Q_OBJECT

public:
    RecordingStartCountdown(const QRect &captureRect, int delayMs,
                            const QString &cancelShortcut, QWidget *parent = nullptr);

    int remainingSeconds() const;

signals:
    void finished();
    void cancelRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void tick();

    QElapsedTimer m_elapsed;
    QTimer *m_timer = nullptr;
    QLabel *m_secondsLabel = nullptr;
    int m_delayMs = 0;
    bool m_done = false;
};

#endif
