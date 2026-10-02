#include "RecordingStartCountdown.h"

#include "RecordingStartCountdownPolicy.h"
#include "../core/TranslationManager.h"

#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include <windows.h>
#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#endif

namespace {
constexpr int CountdownWidth = 236;
constexpr int CountdownHeight = 150;
// Let the compositor unmap the countdown before the recorder grabs frames.
constexpr int HideBeforeStartMs = 150;

QString countdownStyle()
{
    return QStringLiteral(R"(
        QFrame#recordingStartCountdown {
            background-color: rgba(45, 45, 45, 235);
            border: 1px solid #404040;
            border-radius: 12px;
        }
        QLabel {
            color: #F4F6F8;
            background: transparent;
            border: none;
        }
        QLabel#recordingStartCountdownTitle {
            font-size: 12px;
            font-weight: 600;
        }
        QLabel#recordingStartCountdownSeconds {
            font-size: 40px;
            font-weight: 700;
        }
        QLabel#recordingStartCountdownHint {
            color: #B8BEC6;
            font-size: 11px;
        }
        QPushButton#recordingStartCountdownCancel {
            color: #F4F6F8;
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 7px;
            padding: 3px 12px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton#recordingStartCountdownCancel:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
    )");
}
}

RecordingStartCountdown::RecordingStartCountdown(const QRect &captureRect, int delayMs,
                                                 const QString &cancelShortcut,
                                                 QWidget *parent)
    : QWidget(parent),
      m_delayMs(qMax(0, delayMs))
{
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setFocusPolicy(Qt::StrongFocus);

    QScreen *screen = QGuiApplication::screenAt(captureRect.center());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect screenRect = screen ? screen->availableGeometry() : QRect();
    setGeometry(recordingStartCountdownRect(captureRect, screenRect,
                                            QSize(CountdownWidth, CountdownHeight)));

    auto *frame = new QFrame(this);
    frame->setObjectName(QStringLiteral("recordingStartCountdown"));
    frame->setAttribute(Qt::WA_StyledBackground, true);
    frame->setStyleSheet(countdownStyle());
    frame->setGeometry(rect());

    auto *layout = new QVBoxLayout(frame);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(2);

    auto *title = new QLabel(TranslationManager::recordingStartsIn(), frame);
    title->setObjectName(QStringLiteral("recordingStartCountdownTitle"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    m_secondsLabel = new QLabel(frame);
    m_secondsLabel->setObjectName(QStringLiteral("recordingStartCountdownSeconds"));
    m_secondsLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_secondsLabel, 1);

    auto *bottom = new QHBoxLayout();
    bottom->setSpacing(8);
    auto *hint = new QLabel(cancelShortcut.isEmpty()
                                ? QStringLiteral("Esc")
                                : QStringLiteral("Esc / %1").arg(cancelShortcut),
                            frame);
    hint->setObjectName(QStringLiteral("recordingStartCountdownHint"));
    bottom->addWidget(hint, 1);
    auto *cancelButton = new QPushButton(TranslationManager::recordingCancel(), frame);
    cancelButton->setObjectName(QStringLiteral("recordingStartCountdownCancel"));
    cancelButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    cancelButton->setIconSize(QSize(14, 14));
    cancelButton->setCursor(Qt::PointingHandCursor);
    cancelButton->setFocusPolicy(Qt::NoFocus);
    connect(cancelButton, &QPushButton::clicked, this, &RecordingStartCountdown::cancelRequested);
    bottom->addWidget(cancelButton);
    layout->addLayout(bottom);

    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &RecordingStartCountdown::tick);
    m_elapsed.start();
    m_timer->start();
    tick();

    if (!m_done) {
        show();
        raise();
        // Esc only reaches the countdown when the window manager lets it take
        // focus; the cancel button and the cancel hotkey work regardless.
        activateWindow();
        setFocus(Qt::ActiveWindowFocusReason);
#ifdef Q_OS_WIN
        SetWindowDisplayAffinity(reinterpret_cast<HWND>(winId()), WDA_EXCLUDEFROMCAPTURE);
#endif
    }
}

int RecordingStartCountdown::remainingSeconds() const
{
    return recordingStartCountdownSeconds(m_delayMs, m_elapsed.elapsed());
}

void RecordingStartCountdown::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        emit cancelRequested();
        return;
    }
    QWidget::keyPressEvent(event);
}

void RecordingStartCountdown::tick()
{
    if (m_done)
        return;
    const int seconds = remainingSeconds();
    if (seconds > 0) {
        m_secondsLabel->setText(QString::number(seconds));
        return;
    }
    m_done = true;
    m_timer->stop();
    hide();
    QTimer::singleShot(HideBeforeStartMs, this, &RecordingStartCountdown::finished);
}
