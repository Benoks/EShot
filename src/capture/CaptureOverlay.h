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
#include <QHash>
#include <QKeySequence>
#include <QStringList>
#include <QTextEdit>
#include <QPointer>

#include "recording/RecordingDrawerPolicy.h"
#include "annotation/AnnotationEngine.h"
#include <QIcon>

class HintBubble;
class ShortcutSheetLayer;
class QPainter;

#include "CaptureGeometry.h"
#include "CaptureScreenImage.h"
#include "WindowSnapPolicy.h"

#ifdef Q_OS_WIN
#include <windows.h>
using EShotNativeWindowHandle = HWND;
#else
using EShotNativeWindowHandle = void *;
#endif

class AnnotationToolbar;
class QComboBox;
class QFontComboBox;
class QLineEdit;
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
class CaptureScreenViews;


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
    // True while a capture is delayed, grabbing the screen or on screen.
    bool isCaptureActive() const;
    // A capture hotkey pressed again while the overlay is open: close it.
    // Ignored while the screenshot is still being taken.
    void cancelActiveCapture();
    // Captures the monitor under the mouse without showing the overlay,
    // then saves it like Save and copies it to the clipboard.
    void captureScreenNow();

signals:
    void captureCompleted(const QPixmap &pixmap);
    void captureSaved(const QString &path);
    void captureCancelled();
    // The screen could not be captured or saved; message is translated.
    void captureFailed(const QString &message);
    void regionSelected(QRect captureRect, QRect displayRect);
    void gifCaptureRequested(QRect captureRect, QRect displayRect);
    void videoCaptureRequested(QRect captureRect, QRect displayRect);
    // "Advanced settings…" in the recording drawer; the receiver opens the
    // Settings dialog on its recording tab before returning.
    void recordingSettingsRequested();

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
    QPixmap grabScreenUnderCursor();
    // Per-output captures keep only m_screenImages; the full composed
    // snapshot is built on demand for blur/pixelate and the eyedropper.
    void ensureComposedSnapshot();
    bool captureNativeScreenImages(const QRect &logicalRect, const QRect &physicalRect);
    void presentCapture();
    QPoint capturePositionFromGlobal(const QPoint &point) const;
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
    void applyAutoSelectionLock();
    bool handleSelectionNudgeKey(QKeyEvent *event);
    QRegion visibleSelectionUiRegion() const;
    void setHoveredWindowRect(const QRect &targetRect);
    void updateOverlayRegion(const QRegion &region);
    void resetGestureState();
    void resetSelection();
    void hideForModalDialog();
    void startAnnotationMove(int index, const QPoint &pos);
    bool handleAnnotationPress(const QPoint &pos, Qt::KeyboardModifiers modifiers);
    void beginEditExistingText(int index);
    void finishExistingTextEdit();
    void applyTextStyleToEngine(const AnnotationEngine::TextStyle &style);
    void refreshTextStyleButtons();
    QIcon textBackgroundIcon(int background) const;
    void drawSelectionHint(QPainter &painter);
    void showSelectionOnboarding();
    void showToolOnboarding(int toolId);
    HintBubble *hintBubble();
    void toggleShortcutSheet();
    bool isShortcutSheetOpen() const;
    // outputScale receives the image pixels per logical overlay pixel.
    QPixmap getSelectedPixmap(qreal *outputScale = nullptr);

    // Filename template parse
    QString resolveFilenamePattern(const QString &pattern) const;
    QString resolveWindowTitle() const;
    // Saves to the screenshot folder with the file name pattern and emits
    // captureSaved. useCliPath consumes a pending --save path.
    bool saveScreenshotFile(const QPixmap &image, bool useCliPath, QString *error);
    // Target of the last failed saveScreenshotFile(), for the CFA hint.
    QString m_lastFailedSavePath;

    QPixmap m_screenSnapshot;
    QVector<CaptureScreenImage> m_screenImages;
    CaptureScreenViews *m_screenViews = nullptr;
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
    QPointer<QWidget> m_managedOverlayEditor;
    bool m_managedOverlayEditorSelectAll = false;
    QTextEdit *m_textEdit;
    QPoint m_textEditPosition;
    QWidget *m_textEditPanel = nullptr;
    QToolButton *m_textMoveHandle = nullptr;
    QToolButton *m_textCommitButton = nullptr;
    QToolButton *m_textBoldButton = nullptr;
    QToolButton *m_textBackgroundButton = nullptr;
    // Index of the existing label open in the editor, or -1 for a new one.
    int m_editingTextIndex = -1;
    QPoint m_textEditStartPosition;
    ShortcutSheetLayer *m_shortcutSheet = nullptr;
    bool m_showSelectionHint = false;
    HintBubble *m_hintBubble = nullptr;
    QPushButton *m_helpButton = nullptr;
    AnnotationEngine::TextStyle m_styleBeforeTextEdit;
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
    // Activates the managed proxy without grabbing the keyboard so the
    // compositor deactivates a fullscreen app (releases its pointer lock).
    void acquireCaptureActivation();
    void beginCaptureActivation();
    void checkCaptureActivation();
    void noteCaptureInput();
    QString inputBlockedBannerText() const;
    void releaseTextKeyboardFocus();
    QWidget *managedOverlayInputAt(const QPoint &globalPosition) const;
    void updateUndoRedoState();
    bool matchesOverlayShortcut(QKeyEvent *event, const QString &key, const QString &fallback) const;
    // Overlay shortcut cache (cleared in startCaptureInternal)
    mutable QHash<QString, QKeySequence> m_overlayShortcutCache;
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
    // Counter tool options; only shown while the Counter tool is active.
    QWidget *m_quickCounterOptions = nullptr;
    QSpinBox *m_quickCounterStartSpin = nullptr;
    QSlider *m_quickCounterSizeSlider = nullptr;
    QLabel *m_quickCounterSizeValueLabel = nullptr;
    QCheckBox *m_quickAutoLockSelectionCheck = nullptr;
    // The one place for recording options before a recording starts; the
    // rest lives on the Settings dialog's recording tab.
    QWidget *m_recordingDrawer = nullptr;
    QPushButton *m_recordingGifModeButton = nullptr;
    QPushButton *m_recordingVideoModeButton = nullptr;
    QComboBox *m_recordingQualityCombo = nullptr;
    // Inline fields shown while the quality is "Custom".
    QWidget *m_recordingCustomQuality = nullptr;
    QWidget *m_recordingCustomGif = nullptr;
    QWidget *m_recordingCustomVideo = nullptr;
    QSlider *m_recordingGifFpsSlider = nullptr;
    QLabel *m_recordingGifFpsValue = nullptr;
    QComboBox *m_recordingGifMaxSideCombo = nullptr;
    QSlider *m_recordingVideoFpsSlider = nullptr;
    QLabel *m_recordingVideoFpsValue = nullptr;
    QSlider *m_recordingVideoQualitySlider = nullptr;
    QLabel *m_recordingVideoQualityValue = nullptr;
    QComboBox *m_recordingDurationCombo = nullptr;
    QComboBox *m_recordingStartDelayCombo = nullptr;
    QWidget *m_recordingVideoOptions = nullptr;
    QCheckBox *m_recordingDesktopAudioCheck = nullptr;
    QCheckBox *m_recordingMicrophoneCheck = nullptr;
    QComboBox *m_recordingMicrophoneDeviceCombo = nullptr;
    QLabel *m_recordingHotkeyHint = nullptr;
    QPushButton *m_recordingStartButton = nullptr;
    QPushButton *m_recordingCancelButton = nullptr;
    RecordingDrawerMode m_recordingDrawerMode = RecordingDrawerMode::None;
    void setupToolSettingsDrawer();
    void updateToolSettingsDrawerPosition();
    void setToolSettingsDrawerVisible(bool visible);
    void updateCounterQuickSettings();
    bool isToolSettingsUiAt(const QPoint &pos) const;
    void setupRecordingDrawer();
    void showRecordingDrawer(RecordingDrawerMode mode);
    void setRecordingDrawerMode(RecordingDrawerMode mode);
    void loadRecordingDrawerValues();
    void loadRecordingCustomQualityValues();
    void saveRecordingCustomQualityValues();
    void updateRecordingCustomQualityVisibility();
    void placeRecordingDrawer();
    void openRecordingSettings();
    void hideRecordingDrawer(bool restoreToolbar = true);
    void startRecordingFromDrawer();
    void ensureAudioDevicesLoaded();
    void populateAudioDeviceCombos();

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

    // Active window title (for %T)
    EShotNativeWindowHandle m_foregroundHwnd = nullptr;
    // Linux: title of the active window when the capture started.
    QString m_capturedWindowTitle;

    QTimer *m_captureDelayTimer;
    // Set while captureAllScreens() runs: portal and KWin grabs spin nested
    // event loops that also deliver hotkeys.
    bool m_captureInFlight = false;
    // Windows: false while an exclusive fullscreen app runs, so the overlay
    // is shown without taking the foreground (the game would minimise).
    bool m_takeForegroundOnPresent = true;
    // Incremented by every capture; delayed hint timers check it so a timer
    // from an earlier capture cannot change the current one.
    quint64 m_captureGeneration = 0;
    QElapsedTimer m_captureActivationTimer;
    bool m_captureInputSeen = true;
    bool m_compositorActivationRequested = false;
    bool m_showInputBlockedBanner = false;
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
    QList<QPair<QString, QString>> m_cachedMicrophoneAudioDevices;

    // New: Eyedropper mode
    bool m_eyedropperActive;

    // New: Selection lock
    bool m_selectionLocked;
    bool m_autoLockSelection = false;

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
    void onOcrRequested();
    void onUploadRequested();
    void onGoogleLensRequested();
    // Toolbar record button: the drawer in the last used mode.
    void onRecordRequested();
    // Overlay shortcuts (Ctrl+G, Ctrl+Shift+V): the drawer in that mode.
    void onGifRequested();
    void onVideoRequested();

    void performCapture();
};

#endif
