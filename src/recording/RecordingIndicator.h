#ifndef RECORDINGINDICATOR_H
#define RECORDINGINDICATOR_H

#include "RecordingControlLayout.h"

#include <QRect>
#include <QStringList>
#include <QWidget>

class QFrame;
class QLabel;
class QMenu;
class QPushButton;
class QToolButton;

enum class RecordingIndicatorMode {
    Gif,
    Video
};

class RecordingIndicator : public QWidget
{
    Q_OBJECT

public:
    explicit RecordingIndicator(const QRect &captureRect, QWidget *parent = nullptr,
                                int borderWidth = 2, bool supportsPause = true,
                                RecordingIndicatorMode mode = RecordingIndicatorMode::Gif,
                                RecordingCaptureBackend captureBackend = defaultRecordingCaptureBackend(),
                                RecordingControlsPresentation presentation
                                    = RecordingControlsPresentation::ControlBar);
    ~RecordingIndicator() override;

    void setFrameCount(int count);
    // Recording time limit in seconds; 0 or less means unlimited.
    void setTimeLimitSeconds(int seconds);
    void setRemainingSeconds(int seconds);
    void setElapsedSeconds(int seconds);
    void setPaused(bool paused);
    void setDetails(const QStringList &details);
    void setShortcutHints(const QString &pauseResume, const QString &stop,
                          const QString &cancel);
    void stop();
    // Stop was requested and the file is being written: the controls can no
    // longer stop, pause or cancel, and the status shows the saving step.
    void setFinalizing(bool finalizing);
    bool isFinalizing() const { return m_finalizing; }

    // With RecordingControlsPresentation::TrayMenu there is no control
    // window, only the border; the tray menu drives the recording.
    bool hasControlBar() const { return m_controlBar != nullptr; }
    // A locked border stays above other windows; unlocked, they may cover it.
    void setBorderLocked(bool locked);
    bool isBorderLocked() const { return m_borderLocked; }

    bool requiresCaptureSafePresentation() const;
    bool canExcludeFromCapture() const { return m_platformCanExcludeOverlay; }
    bool controlStaysOnTop() const { return m_controlStaysOnTop; }
    void startCaptureSafePresentation();

signals:
    void stopRequested();
    void pauseRequested();
    void resumeRequested();
    void cancelRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void createControlBar();
    void updateControlMask();
    void updateStatusLabel();
    void updatePauseButton();
    void setOverlayVisible(bool visible);
    void moveControlBar(const QPoint &requestedTopLeft);
    void updateCaptureSafetyPolicy();
    void applyControlWindowFlags(bool staysOnTop);
    void updateControlStaysOnTop();
    void updateButtonStates();

    QRect m_captureRect;
    QRect m_screenRect;
    QRect m_borderRect;
    QRect m_controlRect;
    RecordingControlLayout m_layout;
    RecordingOverlayVisibility m_visibilityPolicy = RecordingOverlayVisibility::AlwaysVisible;
    RecordingIndicatorMode m_mode = RecordingIndicatorMode::Gif;
    QFrame *m_controlBar = nullptr;
    QWidget *m_borderOverlay = nullptr;
    QLabel *m_statusLabel = nullptr;
    QToolButton *m_pauseButton = nullptr;
    QToolButton *m_borderLockButton = nullptr;
    QPushButton *m_stopButton = nullptr;
    QToolButton *m_cancelButton = nullptr;
    QToolButton *m_detailsButton = nullptr;
    QMenu *m_detailsMenu = nullptr;
    int m_frameCount = 0;
    int m_elapsedSeconds = 0;
    int m_timeLimitSeconds = 0;
    int m_remainingSeconds = -1;
    bool m_finalizing = false;
    bool m_borderLocked = true;
    bool m_controlStaysOnTop = false;
    bool m_controlExcludedFromCapture = false;
    bool m_running = true;
    bool m_supportsPause = true;
    bool m_paused = false;
    bool m_overlayVisible = true;
    bool m_captureSafePresentationStarted = false;
    bool m_platformCanExcludeOverlay = false;
    bool m_hideFullscreenBorderForCapture = false;
    bool m_dragging = false;
    QPoint m_dragOffset;
    bool m_compact = false;
    int m_borderWidth = 2;
};

#endif
