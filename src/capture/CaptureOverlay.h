#ifndef CAPTUREOVERLAY_H
#define CAPTUREOVERLAY_H

#include <QWidget>
#include "core/VisualSearch.h"
#include <QPixmap>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QTimer>
#include <QElapsedTimer>
#include <QList>
#include <QStringList>
#include <QTextEdit>
#include <QPointer>

#include "recording/RecordingDrawerPolicy.h"

#include "CaptureGeometry.h"
#include "WindowSnapPolicy.h"

#ifdef Q_OS_WIN
#include <windows.h>
using EShotNativeWindowHandle = HWND;
#else
using EShotNativeWindowHandle = void *;
#endif

class AnnotationToolbar;
class AnnotationEngine;
class PinnedWindow;
class QComboBox;
class QFontComboBox;
class QSpinBox;
class QSlider;
class QPushButton;
class QToolButton;
class QCheckBox;
class QPropertyAnimation;
class QVariantAnimation;
class QLabel;
class ImageUploader;
class DebouncedSettingsWriter;
class QScreen;


class CaptureOverlay : public QWidget {
    Q_OBJECT

public:
    explicit CaptureOverlay(QWidget *parent = nullptr);
    ~CaptureOverlay();
    void startCapture();
    void startWindowCapture();
    void startInstantCapture();
    void startCaptureForRecording();
    void refreshUI();
    void prewarm();

signals:
    void captureCompleted(const QPixmap &pixmap);
    void captureSaved(const QString &path);
    void captureCancelled();
    void pinnedWindowCreated(PinnedWindow *window);
    void regionSelected(QRect captureRect, QRect displayRect);
    void regionCancelled();
    void gifCaptureRequested(QRect captureRect, QRect displayRect);
    void videoCaptureRequested(QRect captureRect, QRect displayRect);

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
    void startCaptureInternal(CaptureSelectionMode selectionMode, bool recordingMode);
    void showToolbar();
    void hideToolbar();
    void finishCapture();
    void cancelCapture();
    void releaseCaptureBuffers();
    QRect normalizedSelectionRect() const;
    QRect selectedCaptureRect() const;
    QRect selectedDisplayRect() const;
    QRect monitorRectAt(const QPoint &pos) const;
    void selectMonitorAt(const QPoint &pos);
    void completeSelection(const QRect &selectionRect);
    void setHoveredWindowRect(const QRect &targetRect);
    QPixmap getSelectedPixmap();

    // Filename template parse
    QString resolveFilenamePattern(const QString &pattern) const;
    QString resolveWindowTitle() const;

    QPixmap m_screenSnapshot;
    QImage m_eyedropperImage;
    QPoint m_selectionStart;
    QPoint m_selectionEnd;
    QRect m_selectionAnchorScreenRect;
    bool m_isSelecting;
    bool m_selectionComplete;
    bool m_ignoreNextMouseRelease;
    QPoint m_moveOffset;
    QVector<QRect> m_windowSnapCandidates;
    QRect m_hoveredWindowRect;
    QRect m_animatedWindowRect;
    QRect m_windowSnapAnimationStart;
    QVariantAnimation *m_windowSnapAnimation = nullptr;
    QRect m_pressedWindowRect;
    QPoint m_windowSnapPressPosition;
    bool m_windowSnapClickPending = false;
    CaptureSelectionMode m_selectionMode = CaptureSelectionMode::FreeRegion;

    QRect m_virtualDesktopRect;
    QScreen *m_captureScreen = nullptr;
    QPoint m_physicalVirtualDesktopTopLeft;
    QVector<CaptureMonitorGeometry> m_captureMonitors;
    // Device-pixel ratio bridging the overlay's logical coordinate system
    // (window geometry, mouse, selection) to the physical-pixel m_screenSnapshot.
    qreal m_dpr = 1.0;
    QRect logicalToSnapshot(const QRect &r) const {
        return snapshotRectFromLogical(r, size(), m_screenSnapshot.size(), m_dpr);
    }
    QPoint logicalToSnapshot(const QPoint &p) const {
        const qreal sx = width() > 0 ? (m_screenSnapshot.width() / static_cast<qreal>(width())) : m_dpr;
        const qreal sy = height() > 0 ? (m_screenSnapshot.height() / static_cast<qreal>(height())) : m_dpr;
        return QPoint(qRound(p.x() * sx), qRound(p.y() * sy));
    }

    AnnotationToolbar *m_toolbar;
    QWidget *m_actionPanel;
    AnnotationEngine *m_annotationEngine;
    ImageUploader *m_googleLensUploader = nullptr;
    VisualSearchOperationState m_visualSearchOperations;
    VisualSearchUploadFallbackState m_visualSearchUploadFallbacks;
    QString m_visualSearchImagePath;

    // Text editing — multi-line support
    QTextEdit *m_textFocusProxy = nullptr;
    QTextEdit *m_textEdit;
    QPoint m_textEditPosition;
    QWidget *m_textEditPanel = nullptr;
    QToolButton *m_textMoveHandle = nullptr;
    QToolButton *m_textCommitButton = nullptr;
    QToolButton *m_textCancelButton = nullptr;
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
    void acquireTextKeyboardFocus();
    void acquireCaptureKeyboardFocus();
    void releaseTextKeyboardFocus();
    void updateUndoRedoState();
    bool matchesOverlayShortcut(QKeyEvent *event, const QString &key, const QString &fallback) const;
    void selectAnnotationTool(int toolId);
    void restoreAfterModalDialog();
    void startNextVisualSearchUpload(quint64 generation, VisualSearchProvider provider);
    void clearVisualSearchUpload();

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
    QWidget *m_recordingDrawer = nullptr;
    QWidget *m_recordingGifOptions = nullptr;
    QWidget *m_recordingVideoOptions = nullptr;
    QLabel *m_recordingDrawerTitle = nullptr;
    QSpinBox *m_recordingGifFpsSpin = nullptr;
    QSpinBox *m_recordingGifSecondsSpin = nullptr;
    QComboBox *m_recordingGifLoopCombo = nullptr;
    QSpinBox *m_recordingVideoFpsSpin = nullptr;
    QSpinBox *m_recordingVideoSecondsSpin = nullptr;
    QSpinBox *m_recordingVideoCrfSpin = nullptr;
    QCheckBox *m_recordingDesktopAudioCheck = nullptr;
    QCheckBox *m_recordingMicrophoneCheck = nullptr;
    QComboBox *m_recordingMicrophoneDeviceCombo = nullptr;
    QPushButton *m_recordingStartButton = nullptr;
    QPushButton *m_recordingCancelButton = nullptr;
    RecordingDrawerMode m_recordingDrawerMode = RecordingDrawerMode::None;
    void setupToolSettingsDrawer();
    void updateToolSettingsDrawerPosition();
    void setToolSettingsDrawerVisible(bool visible);
    bool isToolSettingsUiAt(const QPoint &pos) const;
    void setupRecordingDrawer();
    void showRecordingDrawer(RecordingDrawerMode mode);
    void hideRecordingDrawer(bool restoreToolbar = true);
    void startRecordingFromDrawer();
    void ensureAudioDevicesLoaded();

    // Resize and move
    enum ResizeMode {
        ResNone,
        ResTopLeft,
        ResTop,
        ResTopRight,
        ResRight,
        ResBottomRight,
        ResBottom,
        ResBottomLeft,
        ResLeft,
        ResMove,
        ResNewSelection
    };
    ResizeMode m_resizeMode;
    ResizeMode getResizeMode(const QPoint &pos);
    void updateCursor(const QPoint &pos);

    // Annotation move
    bool m_isDraggingAnnotation;
    QPoint m_dragAnnotationStart;
    bool m_isRotatingAnnotation = false;
    QPointF m_rotationCenter;
    qreal m_rotationDragStartAngle = 0.0;
    qreal m_rotationStartDegrees = 0.0;
    bool m_isResizingTextAnnotation = false;
    enum TextResizeHandle { TextResizeNone, TextResizeTopLeft, TextResizeTopRight,
                            TextResizeBottomLeft, TextResizeBottomRight };
    TextResizeHandle m_textResizeHandle = TextResizeNone;
    QPointF m_textResizeDragStart;
    QRectF m_textResizeStartBounds;

    // Text confirm flag
    bool m_textJustCommitted;
    bool m_textEditing;

    // Active window title (for %T)
    EShotNativeWindowHandle m_foregroundHwnd = nullptr;

    QTimer *m_captureDelayTimer;
    QElapsedTimer m_captureLatencyTimer;
    bool m_logNextCapturePaint = false;

    // Opacity setting
    int m_overlayOpacity;

    // Crosshair style
    QString m_crosshairStyle;
    QPoint m_crosshairPosition;
    bool m_hasCrosshairPosition = false;

    // Settings
    bool m_copyAfterCapture;
    bool m_closeAfterCopy;
    bool m_instantCopyAfterSelection;
    bool m_showCaptureHints;
    bool m_showHighlighterStraightHint = false;
    DebouncedSettingsWriter *m_settingsWriter = nullptr;
    bool m_audioDevicesLoaded = false;
    QStringList m_cachedDesktopAudioDevices;
    QList<QPair<QString, QString>> m_cachedMicrophoneAudioDevices;

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
    void onGoogleLensRequested();
    void onGifRequested();
    void onVideoRequested();

    void performCapture();
};

#endif
