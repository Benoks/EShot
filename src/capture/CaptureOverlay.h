#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include <QWidget>
#include <QPixmap>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QList>
#include <QTextEdit>
#include <QPointer>

class AnnotationToolbar;
class AnnotationEngine;
class PinnedWindow;
class QComboBox;
class QFontComboBox;
class QSpinBox;
class QSlider;
class QPushButton;
class QCheckBox;
class QPropertyAnimation;
class QLabel;


class CaptureOverlay : public QWidget {
    Q_OBJECT

public:
    explicit CaptureOverlay(QWidget *parent = nullptr);
    ~CaptureOverlay();
    void startCapture();
    void startCaptureForRecording();
    void refreshUI();
    void prewarm();

signals:
    void captureCompleted(const QPixmap &pixmap);
    void captureSaved(const QString &path);
    void captureCancelled();
    void pinnedWindowCreated(PinnedWindow *window);
    void regionSelected(QRect rect);
    void regionCancelled();
    void gifCaptureRequested(QRect rect);
    void videoCaptureRequested(QRect rect);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    void captureAllScreens();
    void showToolbar();
    void hideToolbar();
    void finishCapture();
    void cancelCapture();
    QRect normalizedSelectionRect() const;
    QRect monitorRectAt(const QPoint &pos) const;
    void selectMonitorAt(const QPoint &pos);
    QPixmap getSelectedPixmap();

    // Filename template parse
    QString resolveFilenamePattern(const QString &pattern) const;
    QString resolveWindowTitle() const;

    QPixmap m_screenSnapshot;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    bool m_isSelecting;
    bool m_selectionComplete;
    bool m_ignoreNextMouseRelease;
    QPoint m_moveOffset;

    QRect m_virtualDesktopRect;
    // Device-pixel ratio bridging the overlay's logical coordinate system
    // (window geometry, mouse, selection) to the physical-pixel m_screenSnapshot.
    qreal m_dpr = 1.0;
    QRect logicalToSnapshot(const QRect &r) const {
        return QRect(qRound(r.x() * m_dpr), qRound(r.y() * m_dpr),
                     qRound(r.width() * m_dpr), qRound(r.height() * m_dpr));
    }
    QPoint logicalToSnapshot(const QPoint &p) const {
        return QPoint(qRound(p.x() * m_dpr), qRound(p.y() * m_dpr));
    }

    AnnotationToolbar *m_toolbar;
    QWidget *m_actionPanel;
    AnnotationEngine *m_annotationEngine;

    // Text editing — multi-line support
    QTextEdit *m_textEdit;
    QPoint m_textEditPosition;
    QWidget *m_textEditPanel = nullptr;
    QPushButton *m_textMoveHandle = nullptr;
    QFontComboBox *m_textInlineFontCombo = nullptr;
    QSpinBox *m_textInlineSizeSpin = nullptr;
    bool m_textPanelDragging = false;
    QPoint m_textPanelDragOffset;
    void commitText();
    void cancelTextEdit();
    void beginTextEditAt(const QPoint &pos);
    void moveTextEditorTo(const QPoint &pos);
    void updateTextEditorStyle();
    void updateTextEditPanelPosition();
    void updateUndoRedoState();
    bool matchesOverlayShortcut(QKeyEvent *event, const QString &key, const QString &fallback) const;
    void selectAnnotationTool(int toolId);

    QPushButton *m_toolSettingsButton = nullptr;
    QWidget *m_toolSettingsDrawer = nullptr;
    QPropertyAnimation *m_toolSettingsAnimation = nullptr;
    QPropertyAnimation *m_toolSettingsButtonAnimation = nullptr;
    QSlider *m_quickPenWidthSlider = nullptr;
    QSlider *m_quickBlurSlider = nullptr;
    QLabel *m_quickPenWidthValueLabel = nullptr;
    QLabel *m_quickBlurValueLabel = nullptr;
    QSpinBox *m_quickGifFpsSpin = nullptr;
    QSpinBox *m_quickGifSecondsSpin = nullptr;
    QComboBox *m_quickGifLoopCombo = nullptr;
    QSpinBox *m_quickVideoFpsSpin = nullptr;
    QSpinBox *m_quickVideoSecondsSpin = nullptr;
    QSpinBox *m_quickVideoCrfSpin = nullptr;
    QCheckBox *m_quickDesktopAudioCheck = nullptr;
    QSlider *m_quickDesktopVolumeSlider = nullptr;
    QLabel *m_quickDesktopVolumeLabel = nullptr;
    QComboBox *m_quickDesktopAudioDeviceCombo = nullptr;
    QCheckBox *m_quickMicrophoneCheck = nullptr;
    QSlider *m_quickMicrophoneVolumeSlider = nullptr;
    QLabel *m_quickMicrophoneVolumeLabel = nullptr;
    QComboBox *m_quickMicrophoneDeviceCombo = nullptr;
    void setupToolSettingsDrawer();
    void updateToolSettingsDrawerPosition();
    void setToolSettingsDrawerVisible(bool visible);
    bool isToolSettingsUiAt(const QPoint &pos) const;

    // Resize and move
    enum ResizeMode { ResNone, ResTopLeft, ResTopRight, ResBottomRight, ResBottomLeft, ResMove, ResNewSelection };
    ResizeMode m_resizeMode;
    ResizeMode getResizeMode(const QPoint &pos);
    void updateCursor(const QPoint &pos);

    // Annotation move
    bool m_isDraggingAnnotation;
    QPoint m_dragAnnotationStart;

    // Text confirm flag
    bool m_textJustCommitted;
    bool m_textEditing;

    // Active window title (for %T)
    HWND m_foregroundHwnd;

    QTimer *m_captureDelayTimer;

    // Opacity setting
    int m_overlayOpacity;

    // Crosshair style
    QString m_crosshairStyle;

    // Settings
    bool m_copyAfterCapture;
    bool m_closeAfterCopy;

    // Pinned windows list (for lifetime management)
    QList<QPointer<QWidget>> m_pinnedWindows;

    // New: Eyedropper mode
    bool m_eyedropperActive;

    // New: Selection lock
    bool m_selectionLocked;

    // New: Special capture mode (recording, scrolling, etc.)
    enum CaptureMode { ModeNormal, ModeRecording };
    CaptureMode m_captureMode;

private slots:
    void onToolSelected(int toolId);
    void onCopyToClipboard();
    void onSave();
    void onClose();
    void onPinToDesktop();
    void onEyedropperRequested();
    void onSelectionLockToggled(bool locked);
    void onBlurIntensityChanged(int intensity);
    void onOcrRequested();
    void onUploadRequested();
    void onGifRequested();
    void onVideoRequested();

    void performCapture();
};

#endif
