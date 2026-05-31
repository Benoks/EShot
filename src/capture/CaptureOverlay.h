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


class CaptureOverlay : public QWidget {
    Q_OBJECT

public:
    explicit CaptureOverlay(QWidget *parent = nullptr);
    ~CaptureOverlay();
    void startCapture();
    void refreshUI();

signals:
    void captureCompleted(const QPixmap &pixmap);
    void captureCancelled();
    void pinnedWindowCreated(PinnedWindow *window);

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

    // Dosya adı şablonu parse
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

    AnnotationToolbar *m_toolbar;
    QWidget *m_actionPanel;
    AnnotationEngine *m_annotationEngine;

    // Metin düzenleme — çoklu satır desteği
    QTextEdit *m_textEdit;
    QPoint m_textEditPosition;
    void commitText();
    void cancelTextEdit();
    void updateUndoRedoState();

    // Boyutlandırma ve Taşıma
    enum ResizeMode { ResNone, ResTopLeft, ResTopRight, ResBottomRight, ResBottomLeft, ResMove, ResNewSelection };
    ResizeMode m_resizeMode;
    ResizeMode getResizeMode(const QPoint &pos);
    void updateCursor(const QPoint &pos);

    // Annotation taşıma
    bool m_isDraggingAnnotation;
    QPoint m_dragAnnotationStart;

    // Text onaylama flag'i
    bool m_textJustCommitted;

    // Aktif pencere başlığı (%T için)
    HWND m_foregroundHwnd;

    QTimer *m_captureDelayTimer;

    // Opaklık ayarı
    int m_overlayOpacity;

    // Crosshair stili
    QString m_crosshairStyle;

    // Ayarlar
    bool m_copyAfterCapture;
    bool m_closeAfterCopy;

    // Pinned pencereler listesi (ömür yönetimi için)
    QList<QPointer<QWidget>> m_pinnedWindows;

    // Yeni: Renk seçici (eyedropper) modu
    bool m_eyedropperActive;

    // Yeni: Seçim kilidi
    bool m_selectionLocked;

private slots:
    void onToolSelected(int toolId);
    void onCopyToClipboard();
    void onSave();
    void onClose();
    void onPinToDesktop();
    void onEyedropperRequested();
    void onSelectionLockToggled(bool locked);
    void onBlurIntensityChanged(int intensity);

    void performCapture();
};

#endif
