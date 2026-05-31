#include "CaptureOverlay.h"
#include "PinnedWindow.h"
#include "annotation/AnnotationEngine.h"
#include "ui/AnnotationToolbar.h"

#include "../core/TranslationManager.h"

#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <functional>
#include <QVBoxLayout>
#include <QPushButton>
#include <QDebug>
#include <QDir>
#include <QPointer>
#include <QRegularExpression>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

CaptureOverlay::CaptureOverlay(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_isSelecting(false)
    , m_selectionComplete(false)
    , m_ignoreNextMouseRelease(false)
    , m_toolbar(nullptr)
    , m_actionPanel(nullptr)
    , m_annotationEngine(nullptr)
    , m_captureDelayTimer(nullptr)
    , m_overlayOpacity(100)
    , m_crosshairStyle("dash")
    , m_copyAfterCapture(false)
    , m_closeAfterCopy(false)
    , m_resizeMode(ResNone)
    , m_foregroundHwnd(nullptr)
    , m_isDraggingAnnotation(false)
    , m_textJustCommitted(false)
    , m_eyedropperActive(false)
    , m_selectionLocked(false)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    // Metin editörü (çoklu satır desteği)
    m_textEdit = new QTextEdit(this);
    m_textEdit->setAcceptRichText(false);
    m_textEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_textEdit->setStyleSheet(R"(
        QTextEdit {
            background-color: rgba(0, 0, 0, 180);
            color: white;
            border: 2px solid #0078D4;
            border-radius: 4px;
            padding: 4px 8px;
            font-size: 14px;
            font-family: 'Segoe UI', Arial;
        }
    )");
    m_textEdit->hide();
    m_textEdit->installEventFilter(this);
    connect(m_textEdit, &QTextEdit::textChanged, this, [this]() {
        // Otomatik yükseklik ayarı
        if (m_textEdit && m_textEdit->isVisible()) {
            QTextDocument *doc = m_textEdit->document();
            QSizeF size = doc->size();
            int h = qMax(30, static_cast<int>(size.height()) + 10);
            m_textEdit->setFixedHeight(h);
        }
    });

    m_annotationEngine = new AnnotationEngine(this);

    m_toolbar = new AnnotationToolbar(this);
    m_toolbar->hide();

    connect(m_toolbar, &AnnotationToolbar::toolSelected, this, &CaptureOverlay::onToolSelected);
    connect(m_toolbar, &AnnotationToolbar::undoRequested, [this]() {
        if (m_annotationEngine) { m_annotationEngine->undo(); update(); updateUndoRedoState(); }
    });
    connect(m_toolbar, &AnnotationToolbar::redoRequested, [this]() {
        if (m_annotationEngine) { m_annotationEngine->redo(); update(); updateUndoRedoState(); }
    });
    connect(m_toolbar, &AnnotationToolbar::colorChanged, [this](const QColor &c) {
        if (m_annotationEngine) m_annotationEngine->setColor(c);
    });
    connect(m_toolbar, &AnnotationToolbar::penWidthChanged, [this](int w) {
        if (m_annotationEngine) m_annotationEngine->setPenWidth(w);
    });
    connect(m_toolbar, &AnnotationToolbar::blurIntensityChanged, [this](int i) {
        if (m_annotationEngine) m_annotationEngine->setBlurIntensity(i);
    });
    connect(m_toolbar, &AnnotationToolbar::eyedropperRequested, this, &CaptureOverlay::onEyedropperRequested);
    connect(m_toolbar, &AnnotationToolbar::lockToggled, this, &CaptureOverlay::onSelectionLockToggled);
    connect(m_annotationEngine, &AnnotationEngine::annotationAdded, this, &CaptureOverlay::updateUndoRedoState);

    m_actionPanel = new QWidget(this);
    m_actionPanel->setStyleSheet(R"(
        QWidget { background-color: #2d2d2d; border: 1px solid #404040; border-radius: 10px; }
    )");
    QVBoxLayout *actionLayout = new QVBoxLayout(m_actionPanel);
    actionLayout->setContentsMargins(6, 8, 6, 8);
    actionLayout->setSpacing(5);
    {
        auto addBtn = [&](const QString &icon, const QString &tip, std::function<void()> handler) {
            QPushButton *btn = new QPushButton(this);
            btn->setToolTip(tip);
            btn->setCursor(Qt::PointingHandCursor);
            btn->setFixedSize(36, 36);
            btn->setIcon(QIcon(icon));
            btn->setIconSize(QSize(20, 20));
            connect(btn, &QPushButton::clicked, this, [handler](bool) { handler(); });
            actionLayout->addWidget(btn);
            return btn;
        };
        QPushButton *pinBtn = addBtn(":/icons/pin.svg", TranslationManager::actionPin(),
            [this]() { onPinToDesktop(); });
        pinBtn->setStyleSheet(R"(
            QPushButton { background-color: #3a3a3a; border: 1px solid #505050; border-radius: 8px; }
            QPushButton:hover { background-color: #454545; border-color: #606060; }
            QPushButton:pressed { background-color: #333333; }
        )");
        QPushButton *copyBtn = addBtn(":/icons/copy.svg", TranslationManager::actionCopy(),
            [this]() { onCopyToClipboard(); });
        copyBtn->setStyleSheet(R"(
            QPushButton { background-color: #3a3a3a; border: 1px solid #505050; border-radius: 8px; }
            QPushButton:hover { background-color: #454545; border-color: #606060; }
            QPushButton:pressed { background-color: #333333; }
        )");
        QPushButton *saveBtn = addBtn(":/icons/save.svg", TranslationManager::actionSave(),
            [this]() { onSave(); });
        saveBtn->setStyleSheet(R"(
            QPushButton { background-color: #3a3a3a; border: 1px solid #505050; border-radius: 8px; }
            QPushButton:hover { background-color: #454545; border-color: #606060; }
            QPushButton:pressed { background-color: #333333; }
        )");
        QPushButton *closeBtn = addBtn(":/icons/close.svg", TranslationManager::actionClose(),
            [this]() { onClose(); });
        closeBtn->setStyleSheet(R"(
            QPushButton { background-color: #3a3a3a; border: 1px solid #505050; border-radius: 8px; }
            QPushButton:hover { background-color: #454545; border-color: #606060; }
            QPushButton:pressed { background-color: #333333; }
        )");

    }
    m_actionPanel->hide();

    m_captureDelayTimer = new QTimer(this);
    m_captureDelayTimer->setSingleShot(true);
    connect(m_captureDelayTimer, &QTimer::timeout, this, &CaptureOverlay::performCapture);


}

CaptureOverlay::~CaptureOverlay() {}

void CaptureOverlay::refreshUI()
{
    // Toolbar buton tooltip'lerini güncelle
    if (m_toolbar) m_toolbar->refreshToolTips();

    // Action panel buton tooltip'lerini güncelle
    if (m_actionPanel) {
        QList<QPushButton*> buttons = m_actionPanel->findChildren<QPushButton*>();
        QStringList tips = {
            TranslationManager::actionPin(),
            TranslationManager::actionCopy(),
            TranslationManager::actionSave(),
            TranslationManager::actionClose()
        };
        for (int i = 0; i < qMin(buttons.size(), tips.size()); ++i) {
            buttons[i]->setToolTip(tips[i]);
        }
    }
}

void CaptureOverlay::startCapture()
{
    m_isSelecting = false;
    m_selectionComplete = false;
    m_ignoreNextMouseRelease = false;
    m_resizeMode = ResNone;
    m_selectionLocked = false;
    m_selectionStart = QPoint();
    m_selectionEnd = QPoint();
    m_eyedropperActive = false;

    if (m_textEdit) m_textEdit->hide();
    m_textJustCommitted = false;
    if (m_annotationEngine) m_annotationEngine->clear();
    hideToolbar();

    // Overlay'den önce aktif pencereyi kaydet (%T dosya adı değişkeni için)
#ifdef Q_OS_WIN
    m_foregroundHwnd = GetForegroundWindow();
#endif

    // Ayarları yükle
    QSettings s("EShot", "EShot");
    if (m_toolbar) {
        m_toolbar->refreshTools();
        m_toolbar->setSelectionLocked(false);
    }

    m_overlayOpacity = s.value("overlayOpacity", 100).toInt();
    m_crosshairStyle = s.value("crosshairStyle", "dash").toString();
    m_copyAfterCapture = s.value("copyAfterCapture", true).toBool();
    m_closeAfterCopy = s.value("closeAfterCopy", true).toBool();

    // Bulanıklık şiddeti ayarını yükle
    int blurIntensity = s.value("blurIntensity", 16).toInt();
    if (m_annotationEngine) m_annotationEngine->setBlurIntensity(blurIntensity);
    if (m_toolbar) m_toolbar->setBlurIntensity(blurIntensity);

    int delayMs = s.value("captureDelay", 0).toInt();
    if (delayMs > 0)
        m_captureDelayTimer->start(delayMs);
    else
        performCapture();
}

void CaptureOverlay::performCapture()
{
    captureAllScreens();

    // Ekran görüntüsünü annotation engine'a aktar (blur efekti için)
    if (m_annotationEngine)
        m_annotationEngine->setScreenSnapshot(m_screenSnapshot);

    setGeometry(m_virtualDesktopRect);
    show();

#ifdef Q_OS_WIN
    // Force the overlay to the very top of the z-order, even above elevated windows.
    // This works because EShot now runs with administrator privileges (see EShot.manifest).
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    // Bring it to the very front and give it keyboard focus
    ::SetForegroundWindow(hwnd);
    ::BringWindowToTop(hwnd);
#endif

    activateWindow();
    setFocus();
    raise();
}

void CaptureOverlay::captureAllScreens()
{
#ifdef Q_OS_WIN
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    if (vw <= 0 || vh <= 0) {
        QScreen *p = QGuiApplication::primaryScreen();
        if (p) {
            m_virtualDesktopRect = p->geometry();
            m_screenSnapshot = p->grabWindow(0);
            m_screenSnapshot.setDevicePixelRatio(1.0);
        }
        return;
    }

    m_virtualDesktopRect = QRect(vx, vy, vw, vh);

    HDC hScreen = GetDC(nullptr);
    if (!hScreen) return;
    HDC hMem = CreateCompatibleDC(hScreen);
    if (!hMem) { ReleaseDC(nullptr, hScreen); return; }
    HBITMAP hBmp = CreateCompatibleBitmap(hScreen, vw, vh);
    if (!hBmp) { DeleteDC(hMem); ReleaseDC(nullptr, hScreen); return; }

    HBITMAP hOld = (HBITMAP)SelectObject(hMem, hBmp);
    // CAPTUREBLT ensures layered/translucent windows are included in the grab
    BitBlt(hMem, 0, 0, vw, vh, hScreen, vx, vy, SRCCOPY | CAPTUREBLT);
    SelectObject(hMem, hOld);

    BITMAPINFOHEADER bi = {};
    bi.biSize = sizeof(bi);
    bi.biWidth = vw;
    bi.biHeight = -vh;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    QImage img(vw, vh, QImage::Format_ARGB32);
    GetDIBits(hMem, hBmp, 0, vh, img.bits(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

    m_screenSnapshot = QPixmap::fromImage(img);
    m_screenSnapshot.setDevicePixelRatio(1.0);

    DeleteObject(hBmp);
    DeleteDC(hMem);
    ReleaseDC(nullptr, hScreen);
#else
    QRect vr;
    auto screens = QGuiApplication::screens();
    for (auto *s : screens) {
        auto g = s->geometry();
        qreal d = s->devicePixelRatio();
        vr = vr.united(QRect(g.x()*d, g.y()*d, g.width()*d, g.height()*d));
    }
    m_virtualDesktopRect = vr;
    m_screenSnapshot = QPixmap(vr.size());
    m_screenSnapshot.setDevicePixelRatio(1.0);
    m_screenSnapshot.fill(Qt::black);
    QPainter p(&m_screenSnapshot);
    for (auto *s : screens) {
        auto grab = s->grabWindow(0);
        auto g = s->geometry();
        qreal d = s->devicePixelRatio();
        p.drawPixmap(g.x()*d - vr.x(), g.y()*d - vr.y(), grab);
    }
    p.end();
#endif
}

void CaptureOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter.drawPixmap(0, 0, width(), height(), m_screenSnapshot);
    painter.fillRect(rect(), QColor(0, 0, 0, m_overlayOpacity));

    QRect selRect = normalizedSelectionRect();

    if (!selRect.isEmpty()) {
        // Temiz alan
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawPixmap(selRect, m_screenSnapshot, selRect);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Annotation
        if (m_annotationEngine && m_selectionComplete) {
            painter.setClipRect(selRect);
            m_annotationEngine->render(&painter, selRect.topLeft());
            painter.setClipping(false);
        }

        // Çerçeve
        QPen borderPen(QColor(0, 122, 204), 2);
        painter.setPen(borderPen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(selRect);

        // Köşe tutamakları
        int hs = 5; // Handle size radius
        QColor hc(255, 255, 255);
        QColor hbc(0, 122, 204);
        
        auto drawHandle = [&](const QPoint &p) {
            QRect hr(p.x()-hs, p.y()-hs, hs*2, hs*2);
            painter.fillRect(hr, hc);
            painter.setPen(hbc);
            painter.drawRect(hr);
        };

        drawHandle(selRect.topLeft());
        drawHandle(selRect.topRight());
        drawHandle(selRect.bottomLeft());
        drawHandle(selRect.bottomRight());

        // Boyut bilgisi — her zaman görünür (seçim sol üstünde)
        if (m_isSelecting || m_selectionComplete) {
            QString dim = QString("%1 x %2").arg(selRect.width()).arg(selRect.height());
            QFont f = painter.font(); f.setPointSize(10); f.setBold(true);
            painter.setFont(f);
            QFontMetrics fm(f);
            int tw = fm.horizontalAdvance(dim) + 16;
            int th = fm.height() + 8;
            int lx = selRect.left();
            int ly = selRect.top() - th - 4;

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0,0,0,180));
            painter.drawRoundedRect(lx, ly, tw, th, 4, 4);
            painter.setPen(Qt::white);
            painter.drawText(lx + 8, ly + fm.ascent() + 4, dim);
        }
    }

    // Crosshair
    if (!m_isSelecting && !m_selectionComplete && m_crosshairStyle != "none" && !m_eyedropperActive) {
        QPoint cur = mapFromGlobal(QCursor::pos());
        QPen cp;
        cp.setColor(QColor(255,255,255,150));
        cp.setWidth(1);
        if (m_crosshairStyle == "dash")
            cp.setStyle(Qt::DashLine);
        else
            cp.setStyle(Qt::SolidLine);
        painter.setPen(cp);
        painter.drawLine(cur.x(), 0, cur.x(), height());
        painter.drawLine(0, cur.y(), width(), cur.y());
    }

    // Eyedropper: renk önizleme daire
    if (m_eyedropperActive) {
        QPoint cur = mapFromGlobal(QCursor::pos());
        if (rect().contains(cur)) {
            QColor pixelColor;
            if (m_screenSnapshot.rect().contains(cur)) {
                QImage img = m_screenSnapshot.toImage();
                pixelColor = QColor(img.pixel(cur));
            }
            if (pixelColor.isValid()) {
                // Daire
                painter.setPen(QPen(Qt::white, 2));
                painter.setBrush(pixelColor);
                painter.drawEllipse(cur, 12, 12);
                // Renk kodu etiketi
                QString colorName = pixelColor.name().toUpper();
                QFont f = painter.font(); f.setPointSize(9); f.setBold(true);
                painter.setFont(f);
                QFontMetrics fm(f);
                int tw = fm.horizontalAdvance(colorName) + 12;
                int th = fm.height() + 6;
                int lx = cur.x() + 18;
                int ly = cur.y() + 18;
                if (lx + tw > width() - 5) lx = cur.x() - tw - 18;
                if (ly + th > height() - 5) ly = cur.y() - th - 18;
                painter.setPen(Qt::NoPen);
                painter.setBrush(QColor(0,0,0,200));
                painter.drawRoundedRect(lx, ly, tw, th, 4, 4);
                painter.setPen(Qt::white);
                painter.drawText(lx + 6, ly + fm.ascent() + 3, colorName);
            }
        }
    }
}

void CaptureOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_toolbar && m_toolbar->isVisible() && m_toolbar->geometry().contains(event->pos()))
            return;
        if (m_actionPanel && m_actionPanel->isVisible() && m_actionPanel->geometry().contains(event->pos()))
            return;

        // Eyedropper modunda tıklama — renk al ve normale dön
        if (m_eyedropperActive) {
            if (m_screenSnapshot.rect().contains(event->pos())) {
                QImage img = m_screenSnapshot.toImage();
                QColor c = QColor(img.pixel(event->pos()));
                if (c.isValid()) {
                    if (m_annotationEngine) m_annotationEngine->setColor(c);
                    // Toolbar'daki renk butonunu güncelle
                    if (m_toolbar) m_toolbar->setColor(c);
                }
            }
            m_eyedropperActive = false;
            setCursor(Qt::CrossCursor);
            update();
            return;
        }

        if (m_selectionComplete && !m_selectionLocked) {
            QRect selRect = normalizedSelectionRect();

            ResizeMode mode = getResizeMode(event->pos());
            
            bool isDrawingTool = (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None);
            
            // Araç seçili değilken annotation tıklama → taşıma modu
            if (mode == ResNone && m_annotationEngine && selRect.contains(event->pos())) {
                QPoint rel = event->pos() - selRect.topLeft();
                int idx = m_annotationEngine->findAnnotationAt(rel);
                if (idx >= 0) {
                    m_isDraggingAnnotation = true;
                    m_dragAnnotationStart = rel;
                    m_annotationEngine->setSelectedIndex(idx);
                    setCursor(Qt::SizeAllCursor);
                    update();
                    return;
                }
            }

            if (isDrawingTool && (mode == ResMove || mode == ResNewSelection)) {
                if (selRect.contains(event->pos())) {
                     m_annotationEngine->beginDraw(event->pos() - selRect.topLeft());
                     update();
                     return;
                }
            }

            if (mode != ResNone && mode != ResNewSelection) {
                m_resizeMode = mode;
                if (mode == ResMove) {
                    m_moveOffset = event->pos() - selRect.topLeft();
                } else {
                    m_selectionStart = selRect.topLeft();
                    m_selectionEnd = selRect.bottomRight();
                }
                update();
                return;
            }
            
            if (selRect.contains(event->pos())) {
                // Seçim alanında tıklama — mevcut seçimi taşı
            } else {
                m_selectionComplete = false;
                m_isSelecting = true;
                m_resizeMode = ResNewSelection;
                m_selectionStart = event->pos();
                m_selectionEnd = event->pos();
                hideToolbar();
                if (m_annotationEngine) m_annotationEngine->clear();
                update();
            }
        } else if (m_selectionComplete && m_selectionLocked) {
            // Seçim kilitli — sadece annotation çizimine izin ver
            QRect selRect = normalizedSelectionRect();
            bool isDrawingTool = (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None);
            if (isDrawingTool && selRect.contains(event->pos())) {
                m_annotationEngine->beginDraw(event->pos() - selRect.topLeft());
                update();
            }
        } else {
            m_isSelecting = true;
            m_selectionStart = event->pos();
            m_selectionEnd = event->pos();
            hideToolbar();
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_eyedropperActive) {
            // Eyedropper iptal
            m_eyedropperActive = false;
            setCursor(Qt::CrossCursor);
            update();
            return;
        }
        if (m_selectionComplete) {
            m_selectionComplete = false;
            m_isSelecting = false;
            m_selectionLocked = false;
            m_selectionStart = m_selectionEnd = QPoint();
            if (m_toolbar) m_toolbar->setSelectionLocked(false);
            hideToolbar();
            if (m_annotationEngine) m_annotationEngine->clear();
            update();
        } else {
            onClose();
        }
    }
}

void CaptureOverlay::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || m_eyedropperActive || m_selectionLocked)
        return;
    if (m_toolbar && m_toolbar->isVisible() && m_toolbar->geometry().contains(event->pos()))
        return;
    if (m_actionPanel && m_actionPanel->isVisible() && m_actionPanel->geometry().contains(event->pos()))
        return;

    selectMonitorAt(event->pos());
    m_ignoreNextMouseRelease = true;
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
    // Eyedropper modunda — sadece repaint
    if (m_eyedropperActive) {
        update();
        return;
    }

    // Annotation taşıma (snap-to-edge ile)
    if (m_isDraggingAnnotation && m_annotationEngine && m_annotationEngine->selectedIndex() >= 0) {
        QRect selRect = normalizedSelectionRect();
        QPoint rel = event->pos() - selRect.topLeft();
        QPoint delta = rel - m_dragAnnotationStart;
        if (!delta.isNull()) {
            // Snap-to-edge: kenara 8px yakınsa yapıştır
            int snapThreshold = 8;
            QRect annBounds = m_annotationEngine->boundingRectOf(m_annotationEngine->selectedIndex());
            if (!annBounds.isEmpty()) {
                QPoint newPos = annBounds.topLeft() + delta;
                // Sol kenara snap
                if (qAbs(newPos.x()) < snapThreshold) delta.setX(-annBounds.left());
                // Sağ kenara snap
                if (qAbs(newPos.x() + annBounds.width() - selRect.width()) < snapThreshold)
                    delta.setX(selRect.width() - annBounds.width() - annBounds.left());
                // Üst kenara snap
                if (qAbs(newPos.y()) < snapThreshold) delta.setY(-annBounds.top());
                // Alt kenara snap
                if (qAbs(newPos.y() + annBounds.height() - selRect.height()) < snapThreshold)
                    delta.setY(selRect.height() - annBounds.height() - annBounds.top());
            }
            m_annotationEngine->moveAnnotation(m_annotationEngine->selectedIndex(), delta);
            m_dragAnnotationStart = rel;
            update();
        }
        return;
    }

    if (m_resizeMode != ResNone && m_resizeMode != ResNewSelection) {
        QRect oldSel = normalizedSelectionRect();
        
        if (m_resizeMode == ResMove) {
            QPoint newTopLeft = event->pos() - m_moveOffset;
            
            int w = oldSel.width();
            int h = oldSel.height(); // Assuming h is also defined, as it's used below.
            // Ekran sınırları
            newTopLeft.setX(qBound(0, newTopLeft.x(), width() - w));
            newTopLeft.setY(qBound(0, newTopLeft.y(), height() - h));

            m_selectionStart = newTopLeft;
            // QRect(p1, p2) yapıcı width = p2.x - p1.x + 1 mantığıyla çalışır.
            // Bu yüzden aynı boyutu korumak için 1 çıkarmalıyız.
            m_selectionEnd = newTopLeft + QPoint(w - 1, h - 1);
        } 
        else if (m_resizeMode == ResTopLeft) {
            m_selectionStart = event->pos();
        } 
        else if (m_resizeMode == ResTopRight) {
            m_selectionStart.setY(event->pos().y());
            m_selectionEnd.setX(event->pos().x());
        } 
        else if (m_resizeMode == ResBottomLeft) {
            m_selectionStart.setX(event->pos().x());
            m_selectionEnd.setY(event->pos().y());
        }
        else if (m_resizeMode == ResBottomRight) {
            m_selectionEnd = event->pos();
        }

        update();
        return;
    }

    if (m_isSelecting) {
        m_selectionEnd = event->pos();
        update();
    } else if (m_selectionComplete && m_annotationEngine &&
               m_annotationEngine->currentTool() != AnnotationEngine::None) {
        QRect selRect = normalizedSelectionRect();
        if (selRect.contains(event->pos())) {
            QPoint rel = event->pos() - selRect.topLeft();
            if (m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                if (m_annotationEngine->eraseAnnotationAt(rel))
                    update();
            } else {
                m_annotationEngine->continueDraw(rel);
                update();
            }
        }
    } else if (!m_selectionComplete) {
        update(); // crosshair
    } else {
        updateCursor(event->pos());
    }
}

void CaptureOverlay::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_ignoreNextMouseRelease) {
            m_ignoreNextMouseRelease = false;
            return;
        }

        // Annotation taşıma sonlandır
        if (m_isDraggingAnnotation) {
            m_isDraggingAnnotation = false;
            if (m_annotationEngine) m_annotationEngine->setSelectedIndex(-1);
            setCursor(Qt::CrossCursor);
            update();
            return;
        }

        if (m_resizeMode != ResNone && m_resizeMode != ResNewSelection) {
            m_resizeMode = ResNone;
            updateCursor(event->pos());
            showToolbar();
            return;
        }

        if (m_isSelecting) {
            m_isSelecting = false;
            m_selectionEnd = event->pos();
            QRect selRect = normalizedSelectionRect();
            if (selRect.width() > 10 && selRect.height() > 10) {
                m_selectionComplete = true;
                showToolbar();
                updateUndoRedoState();
                // copyAfterCapture aktifse otomatik kopyala
                if (m_copyAfterCapture) {
                    QPixmap result = getSelectedPixmap();
                    if (!result.isNull())
                        QGuiApplication::clipboard()->setPixmap(result);
                }
            }
            update();
        } else if (m_selectionComplete) {
            QRect selRect = normalizedSelectionRect();
            QPoint rel = event->pos() - selRect.topLeft();
            rel.setX(qBound(0, rel.x(), selRect.width()));
            rel.setY(qBound(0, rel.y(), selRect.height()));

            if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                if (selRect.contains(event->pos())) {
                    if (m_annotationEngine->eraseAnnotationAt(rel))
                        update();
                }
            } else if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Text) {
                if (selRect.contains(event->pos())) {
                    m_textEditPosition = event->pos();
                    m_textEdit->setGeometry(event->pos().x(), event->pos().y(), 200, 30);
                    m_textEdit->clear();
                    m_textEdit->show();
                    m_textEdit->setFocus();
                }
            } else if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Counter) {
                if (selRect.contains(event->pos())) {
                    m_annotationEngine->addCounterAnnotation(rel);
                    update();
                    updateUndoRedoState();
                }
            } else if (m_annotationEngine) {
                m_annotationEngine->endDraw(rel);
                update();
                updateUndoRedoState();
            }
        }
    }
}

void CaptureOverlay::keyPressEvent(QKeyEvent *event)
{
    // Shift durumunu annotation engine'e bildir
    if (event->key() == Qt::Key_Shift && m_annotationEngine) {
        m_annotationEngine->setShiftHeld(true);
    }

    if (event->key() == Qt::Key_Escape) {
        // Eyedropper aktifse sadece onu kapat
        if (m_eyedropperActive) {
            m_eyedropperActive = false;
            setCursor(Qt::CrossCursor);
            update();
            return;
        }
        // Text edit açıksa sadece onu kapat
        if (m_textEdit && m_textEdit->isVisible()) {
            m_textEdit->hide();
            m_textJustCommitted = false;
            setFocus();
            return;
        }
        if (m_selectionComplete) {
            m_selectionComplete = false;
            m_isSelecting = false;
            m_selectionLocked = false;
            m_selectionStart = m_selectionEnd = QPoint();
            if (m_toolbar) m_toolbar->setSelectionLocked(false);
            hideToolbar();
            if (m_annotationEngine) m_annotationEngine->clear();
            update();
        } else {
            onClose();
        }
    } else if (event->matches(QKeySequence::Copy)) {
        onCopyToClipboard();
    } else if (event->matches(QKeySequence::Save)) {
        onSave();
    } else if (event->matches(QKeySequence::Undo)) {
        if (m_annotationEngine) { m_annotationEngine->undo(); update(); updateUndoRedoState(); }
    } else if (event->matches(QKeySequence::Redo)) {
        if (m_annotationEngine) { m_annotationEngine->redo(); update(); updateUndoRedoState(); }
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // Text edit açıksa ve Shift basılıysa yeni satır ekle
        if (m_textEdit && m_textEdit->isVisible()) {
            if (event->modifiers() & Qt::ShiftModifier) {
                m_textEdit->insertPlainText("\n");
                return;
            }
            // Text edit kapatıldıysa Enter'ı yut
            commitText();
            return;
        }
        if (m_textJustCommitted) {
            m_textJustCommitted = false;
            return;
        }
        if (m_selectionComplete) onCopyToClipboard();
    } else if (m_selectionComplete && m_annotationEngine) {
        // Eyedropper kısayolu
        if (event->key() == Qt::Key_I && !(event->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
            onEyedropperRequested();
            return;
        }
        // Araç kısayol tuşları
        int toolId = AnnotationEngine::None;
        switch (event->key()) {
            case Qt::Key_P: toolId = AnnotationEngine::Pen; break;
            case Qt::Key_A: toolId = AnnotationEngine::Arrow; break;
            case Qt::Key_R: toolId = AnnotationEngine::Rectangle; break;
            case Qt::Key_C: toolId = AnnotationEngine::Circle; break;
            case Qt::Key_T: toolId = AnnotationEngine::Text; break;
            case Qt::Key_H: toolId = AnnotationEngine::Highlighter; break;
            case Qt::Key_B: toolId = AnnotationEngine::Blur; break;
            case Qt::Key_NumberSign: toolId = AnnotationEngine::Counter; break;
            case Qt::Key_X: toolId = AnnotationEngine::Eraser; break;
            case Qt::Key_L: toolId = AnnotationEngine::Line; break;
            case Qt::Key_D: toolId = AnnotationEngine::SemiRect; break;
            default: break;
        }
        if (toolId != AnnotationEngine::None) {
            m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(toolId));
            // Toolbar'daki seçimi güncelle
            if (m_toolbar) m_toolbar->selectTool(toolId);
        }
    }
}

void CaptureOverlay::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift && m_annotationEngine) {
        m_annotationEngine->setShiftHeld(false);
    }
}

bool CaptureOverlay::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_textEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (ke->modifiers() & Qt::ShiftModifier) {
                // Shift+Enter → yeni satır
                m_textEdit->insertPlainText("\n");
            } else {
                // Enter → doğrudan onayla
                commitText();
            }
            return true; // olayı tüket
        }
    }
    return QWidget::eventFilter(obj, event);
}

QPixmap CaptureOverlay::getSelectedPixmap()
{
    QRect selRect = normalizedSelectionRect();
    if (selRect.isEmpty()) return QPixmap();
    QPixmap result = m_screenSnapshot.copy(selRect);
    if (m_annotationEngine && m_annotationEngine->hasAnnotations()) {
        m_annotationEngine->setSelectionRect(selRect);
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing, true);
        m_annotationEngine->render(&p, QPoint(0,0));
        p.end();
    }
    return result;
}

QString CaptureOverlay::resolveFilenamePattern(const QString &pattern) const
{
    QDateTime now = QDateTime::currentDateTime();
    QString result = pattern;

    result.replace("%Y", now.toString("yyyy"));
    result.replace("%y", now.toString("yy"));
    result.replace("%M", now.toString("MM"));
    result.replace("%D", now.toString("dd"));
    result.replace("%h", now.toString("HH"));
    result.replace("%m", now.toString("mm"));
    result.replace("%s", now.toString("ss"));
    result.replace("%T", resolveWindowTitle());

    return result;
}

QString CaptureOverlay::resolveWindowTitle() const
{
#ifdef Q_OS_WIN
    // Kaydedilmiş aktif pencereyi kullan (overlay gösterilmeden önceki)
    HWND hwnd = m_foregroundHwnd;
    if (!hwnd) hwnd = GetForegroundWindow();
    if (hwnd) {
        wchar_t title[256];
        int len = GetWindowTextW(hwnd, title, 256);
        if (len > 0) {
            QString windowTitle = QString::fromWCharArray(title, len);
            windowTitle.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");
            return windowTitle.left(50);
        }
    }
#endif
    return "";
}

void CaptureOverlay::showToolbar()
{
    if (!m_toolbar) return;
    QRect selRect = normalizedSelectionRect();
    int margin = 12;

    // --- Alt toolbar: seçimin altında, ortalanmış ---
    int th = m_toolbar->sizeHint().height();
    int toolbarWidth = m_toolbar->sizeHint().width();
    int minToolbarWidth = toolbarWidth; // Butonların sığıdığı minimum

    int tx = selRect.center().x() - toolbarWidth / 2;
    int ty = selRect.bottom() + margin;

    // Ekran sınırları kontrolü
    if (tx < 5) tx = 5;
    if (tx + toolbarWidth > width() - 5) tx = width() - toolbarWidth - 5;
    if (ty + th > height() - 5) {
        // Altına sığmıyorsa üste dene
        ty = selRect.top() - th - margin;
    }
    if (ty < 5) {
        // Üste de sığmıyorsa çerçevenin içine, alta yapışık koy
        ty = selRect.bottom() - th - 2;
    }

    m_toolbar->setFixedWidth(toolbarWidth);
    m_toolbar->move(tx, ty);
    m_toolbar->show();
    m_toolbar->raise();

    // --- Sağ panel: seçimin sağında, dikey olarak ortalanmış ---
    if (m_actionPanel) {
        m_actionPanel->adjustSize();
        int pw = m_actionPanel->width();
        int ph = m_actionPanel->height();
        int px = selRect.right() + margin;
        int py = selRect.center().y() - ph / 2;

        // Sağa sığmıyorsa sola dene
        if (px + pw > width() - 5)
            px = selRect.left() - pw - margin;

        // Sola da sığmıyorsa çerçevenin içine, sağa yapışık koy
        if (px < 5) {
            px = selRect.right() - pw - 2;
            py = selRect.top() + 5;
        }

        // Üst-alt sınırları kontrolü (çerçeve içindeyken)
        if (py < selRect.top() + 2) py = selRect.top() + 2;
        if (py + ph > selRect.bottom() - 2) py = selRect.bottom() - ph - 2;

        m_actionPanel->move(px, py);
        m_actionPanel->show();
        m_actionPanel->raise();
    }
}

void CaptureOverlay::hideToolbar()
{
    if (m_toolbar) m_toolbar->hide();
    if (m_actionPanel) m_actionPanel->hide();
}

void CaptureOverlay::finishCapture()
{
    QPixmap result = getSelectedPixmap();
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;

    // Ses çal (ayar aktifse)
    QSettings s("EShot", "EShot");
    if (s.value("playSound", false).toBool()) {
#ifdef Q_OS_WIN
        MessageBeep(MB_OK);
#else
        QApplication::beep();
#endif
    }

    if (!result.isNull()) emit captureCompleted(result);
}

void CaptureOverlay::cancelCapture()
{
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    emit captureCancelled();
}

void CaptureOverlay::commitText()
{
    if (!m_textEdit || m_textEdit->isHidden()) return;
    QString text = m_textEdit->toPlainText().trimmed();
    if (!text.isEmpty() && m_annotationEngine) {
        QRect selRect = normalizedSelectionRect();
        QPoint rel = m_textEditPosition - selRect.topLeft();
        m_annotationEngine->addTextAnnotation(rel, text);
        update();
        updateUndoRedoState();
    }
    m_textEdit->hide();
    m_textJustCommitted = true;
    setFocus();
}

void CaptureOverlay::cancelTextEdit()
{
    if (m_textEdit) m_textEdit->hide();
    setFocus();
}

void CaptureOverlay::updateUndoRedoState()
{
    if (m_toolbar) {
        m_toolbar->setUndoEnabled(m_annotationEngine && m_annotationEngine->canUndo());
        m_toolbar->setRedoEnabled(m_annotationEngine && m_annotationEngine->canRedo());
    }
}

void CaptureOverlay::onClose() { cancelCapture(); }

void CaptureOverlay::onEyedropperRequested()
{
    m_eyedropperActive = true;
    setCursor(Qt::CrossCursor);
    update();
}

void CaptureOverlay::onSelectionLockToggled(bool locked)
{
    m_selectionLocked = locked;
}

void CaptureOverlay::onBlurIntensityChanged(int intensity)
{
    if (m_annotationEngine) m_annotationEngine->setBlurIntensity(intensity);
    // Ayarlara kaydet
    QSettings s("EShot", "EShot");
    s.setValue("blurIntensity", intensity);
}

QRect CaptureOverlay::normalizedSelectionRect() const
{
    return QRect(m_selectionStart, m_selectionEnd).normalized();
}

QRect CaptureOverlay::monitorRectAt(const QPoint &pos) const
{
#ifdef Q_OS_WIN
    POINT nativePoint = {
        pos.x() + m_virtualDesktopRect.x(),
        pos.y() + m_virtualDesktopRect.y()
    };
    HMONITOR monitor = MonitorFromPoint(nativePoint, MONITOR_DEFAULTTONULL);
    if (!monitor) return QRect();

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return QRect();

    return QRect(
        info.rcMonitor.left - m_virtualDesktopRect.x(),
        info.rcMonitor.top - m_virtualDesktopRect.y(),
        info.rcMonitor.right - info.rcMonitor.left,
        info.rcMonitor.bottom - info.rcMonitor.top
    ).intersected(rect());
#else
    for (QScreen *screen : QGuiApplication::screens()) {
        QRect geometry = screen->geometry();
        qreal dpr = screen->devicePixelRatio();
        QRect localRect(
            qRound(geometry.x() * dpr) - m_virtualDesktopRect.x(),
            qRound(geometry.y() * dpr) - m_virtualDesktopRect.y(),
            qRound(geometry.width() * dpr),
            qRound(geometry.height() * dpr)
        );
        if (localRect.contains(pos))
            return localRect.intersected(rect());
    }
    return QRect();
#endif
}

void CaptureOverlay::selectMonitorAt(const QPoint &pos)
{
    QRect monitorRect = monitorRectAt(pos);
    if (monitorRect.isEmpty()) return;

    if (m_textEdit) m_textEdit->hide();
    if (m_annotationEngine) m_annotationEngine->clear();

    m_isSelecting = false;
    m_selectionComplete = true;
    m_isDraggingAnnotation = false;
    m_resizeMode = ResNone;
    m_selectionStart = monitorRect.topLeft();
    m_selectionEnd = monitorRect.bottomRight();

    showToolbar();
    updateUndoRedoState();
    updateCursor(pos);

    if (m_copyAfterCapture) {
        QPixmap result = getSelectedPixmap();
        if (!result.isNull())
            QGuiApplication::clipboard()->setPixmap(result);
    }

    update();
}

void CaptureOverlay::onToolSelected(int toolId)
{
    if (m_annotationEngine) {
        m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(toolId));
        m_annotationEngine->setSelectedIndex(-1);
    }
    m_isDraggingAnnotation = false;
}

void CaptureOverlay::onCopyToClipboard()
{
    QPixmap result = getSelectedPixmap();
    if (result.isNull()) return;
    QGuiApplication::clipboard()->setPixmap(result);

    // closeAfterCopy aktifse overlay'i kapat
    if (m_closeAfterCopy) {
        finishCapture();
    }
}

void CaptureOverlay::onSave()
{
    QPixmap result = getSelectedPixmap();
    if (result.isNull()) return;

    QSettings s("EShot", "EShot");
    QString path = s.value("savePath",
        QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)).toString();
    QString format = s.value("imageFormat", "PNG").toString();
    int quality = s.value("imageQuality", 95).toInt();
    QString pattern = s.value("filenamePattern", "Screenshot_%Y-%M-%D_%h-%m-%s").toString();
    bool copyPathAfterSave = s.value("copyPathAfterSave", false).toBool();

    QDir dir(path);
    if (!dir.exists()) dir.mkpath(".");

    QString ext = format.toLower();
    if (ext == "jpeg") ext = "jpg";

    QString baseName = resolveFilenamePattern(pattern);
    QString filename = QString("%1/%2.%3").arg(path, baseName, ext);

    // Aynı isimde dosya varsa numara ekle
    if (QFile::exists(filename)) {
        int counter = 1;
        while (QFile::exists(QString("%1/%2_%3.%4").arg(path, baseName, QString::number(counter), ext)))
            counter++;
        filename = QString("%1/%2_%3.%4").arg(path, baseName, QString::number(counter), ext);
    }

    bool saved = false;
    if (format == "PNG") saved = result.save(filename, "PNG");
    else if (format == "JPEG") saved = result.save(filename, "JPEG", quality);
    else if (format == "BMP") saved = result.save(filename, "BMP");
    else saved = result.save(filename);

    if (saved) {
        qDebug() << "[CaptureOverlay] Saved:" << filename;
        if (copyPathAfterSave) {
            QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(filename));
        }
    } else {
        qWarning() << "[CaptureOverlay] Save failed:" << filename;
    }

    finishCapture();
}

void CaptureOverlay::onPinToDesktop()
{
    QPixmap result = getSelectedPixmap();
    if (result.isNull()) return;

    QRect selRect = normalizedSelectionRect();
    QPoint screenPos = mapToGlobal(selRect.topLeft());

    PinnedWindow *pin = new PinnedWindow(result, screenPos);
    m_pinnedWindows.append(QPointer<QWidget>(pin));
    emit pinnedWindowCreated(pin);

    hide();
    m_selectionComplete = false;
    m_isSelecting = false;

    qDebug() << "[CaptureOverlay] Pinned to desktop at" << screenPos;
}



CaptureOverlay::ResizeMode CaptureOverlay::getResizeMode(const QPoint &pos)
{
    QRect sel = normalizedSelectionRect();
    int h = 10; // Hit radius

    if (QRect(sel.topLeft() + QPoint(-h,-h), QSize(h*2,h*2)).contains(pos)) return ResTopLeft;
    if (QRect(sel.topRight() + QPoint(-h,-h), QSize(h*2,h*2)).contains(pos)) return ResTopRight;
    if (QRect(sel.bottomLeft() + QPoint(-h,-h), QSize(h*2,h*2)).contains(pos)) return ResBottomLeft;
    if (QRect(sel.bottomRight() + QPoint(-h,-h), QSize(h*2,h*2)).contains(pos)) return ResBottomRight;

    if (sel.contains(pos)) return ResMove;
    
    return ResNewSelection;
}

void CaptureOverlay::updateCursor(const QPoint &pos)
{
    if (!m_selectionComplete) {
        setCursor(Qt::CrossCursor);
        return;
    }

    ResizeMode mode = getResizeMode(pos);
    switch (mode) {
        case ResTopLeft:
        case ResBottomRight:
            setCursor(Qt::SizeFDiagCursor);
            break;
        case ResTopRight:
        case ResBottomLeft:
            setCursor(Qt::SizeBDiagCursor);
            break;
        case ResMove:
            if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::None)
                setCursor(Qt::SizeAllCursor);
            else
                setCursor(Qt::CrossCursor); // Çizim için
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}
