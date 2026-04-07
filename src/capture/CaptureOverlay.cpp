#include "CaptureOverlay.h"
#include "PinnedWindow.h"
#include "annotation/AnnotationEngine.h"
#include "ui/AnnotationToolbar.h"

#include <QPainter>
#include <QScreen>
#include <QGuiApplication>
#include <QClipboard>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QFileDialog>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <QInputDialog>
#include <QDebug>
#include <QDir>
#include <QPointer>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

CaptureOverlay::CaptureOverlay(QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool)
    , m_isSelecting(false)
    , m_selectionComplete(false)
    , m_isMovingSelection(false)
    , m_toolbar(nullptr)
    , m_annotationEngine(nullptr)
    , m_captureDelayTimer(nullptr)
    , m_overlayOpacity(100)
    , m_crosshairStyle("dash")
    , m_resizeMode(ResNone)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    m_annotationEngine = new AnnotationEngine(this);

    m_toolbar = new AnnotationToolbar(this);
    m_toolbar->hide();

    connect(m_toolbar, &AnnotationToolbar::toolSelected, this, &CaptureOverlay::onToolSelected);
    connect(m_toolbar, &AnnotationToolbar::copyRequested, this, &CaptureOverlay::onCopyToClipboard);
    connect(m_toolbar, &AnnotationToolbar::saveRequested, this, &CaptureOverlay::onSave);
    connect(m_toolbar, &AnnotationToolbar::closeRequested, this, &CaptureOverlay::onClose);
    connect(m_toolbar, &AnnotationToolbar::pinRequested, this, &CaptureOverlay::onPinToDesktop);

    connect(m_toolbar, &AnnotationToolbar::undoRequested, [this]() {
        if (m_annotationEngine) { m_annotationEngine->undo(); update(); }
    });
    connect(m_toolbar, &AnnotationToolbar::redoRequested, [this]() {
        if (m_annotationEngine) { m_annotationEngine->redo(); update(); }
    });
    connect(m_toolbar, &AnnotationToolbar::colorChanged, [this](const QColor &c) {
        if (m_annotationEngine) m_annotationEngine->setColor(c);
    });
    connect(m_toolbar, &AnnotationToolbar::penWidthChanged, [this](int w) {
        if (m_annotationEngine) m_annotationEngine->setPenWidth(w);
    });

    m_captureDelayTimer = new QTimer(this);
    m_captureDelayTimer->setSingleShot(true);
    connect(m_captureDelayTimer, &QTimer::timeout, this, &CaptureOverlay::performCapture);
}

CaptureOverlay::~CaptureOverlay() {}

void CaptureOverlay::startCapture()
{
    m_isSelecting = false;
    m_selectionComplete = false;
    m_isSelecting = false;
    m_selectionComplete = false;
    m_resizeMode = ResNone;
    m_selectionStart = QPoint();
    m_selectionEnd = QPoint();

    if (m_annotationEngine) m_annotationEngine->clear();
    hideToolbar();

    // Ayarları yükle
    QSettings s("EShot", "EShot");
    if (m_toolbar) m_toolbar->refreshTools();

    m_overlayOpacity = s.value("overlayOpacity", 100).toInt();
    m_crosshairStyle = s.value("crosshairStyle", "dash").toString();

    int delayMs = s.value("captureDelay", 0).toInt();
    if (delayMs > 0)
        m_captureDelayTimer->start(delayMs);
    else
        performCapture();
}

void CaptureOverlay::performCapture()
{
    captureAllScreens();
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

        // Boyut bilgisi
        if (m_isSelecting || m_selectionComplete) {
            QString dim = QString("%1 x %2").arg(selRect.width()).arg(selRect.height());
            QFont f = painter.font(); f.setPointSize(10); f.setBold(true);
            painter.setFont(f);
            QFontMetrics fm(f);
            int tw = fm.horizontalAdvance(dim) + 16;
            int th = fm.height() + 8;
            int lx = selRect.left();
            int ly = selRect.top() - th - 4;
            if (ly < 0) ly = selRect.top() + 4;

            painter.setPen(Qt::NoPen);
            painter.setBrush(QColor(0,0,0,180));
            painter.drawRoundedRect(lx, ly, tw, th, 4, 4);
            painter.setPen(Qt::white);
            painter.drawText(lx + 8, ly + fm.ascent() + 4, dim);
        }
    }

    // Crosshair
    if (!m_isSelecting && !m_selectionComplete && m_crosshairStyle != "none") {
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
}

void CaptureOverlay::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (m_toolbar && m_toolbar->isVisible() && m_toolbar->geometry().contains(event->pos()))
            return;

        if (m_selectionComplete) {
            QRect selRect = normalizedSelectionRect();

            ResizeMode mode = getResizeMode(event->pos());
            
            bool isDrawingTool = (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None);
            
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
                // ...
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
        } else {
            m_isSelecting = true;
            m_selectionStart = event->pos();
            m_selectionEnd = event->pos();
            hideToolbar();
            update();
        }
    } else if (event->button() == Qt::RightButton) {
        if (m_selectionComplete) {
            m_selectionComplete = false;
            m_isSelecting = false;
            m_selectionStart = m_selectionEnd = QPoint();
            hideToolbar();
            if (m_annotationEngine) m_annotationEngine->clear();
            update();
        } else {
            onClose();
        }
    }
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
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

        showToolbar();
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
            m_annotationEngine->continueDraw(event->pos() - selRect.topLeft());
            update();
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
        if (m_resizeMode != ResNone && m_resizeMode != ResNewSelection) {
            m_resizeMode = ResNone;
            updateCursor(event->pos());
            return;
        }

        if (m_isSelecting) {
            m_isSelecting = false;
            m_selectionEnd = event->pos();
            QRect selRect = normalizedSelectionRect();
            if (selRect.width() > 10 && selRect.height() > 10) {
                m_selectionComplete = true;
                showToolbar();
            }
            update();
        } else if (m_selectionComplete) {
            QRect selRect = normalizedSelectionRect();
            QPoint rel = event->pos() - selRect.topLeft();
            rel.setX(qBound(0, rel.x(), selRect.width()));
            rel.setY(qBound(0, rel.y(), selRect.height()));

            if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Text) {
                if (selRect.contains(event->pos())) {
                    bool ok;
                    QString text = QInputDialog::getText(this, tr("Metin Ekle"),
                        tr("Yazı:"), QLineEdit::Normal, "", &ok);
                    if (ok && !text.isEmpty()) {
                        m_annotationEngine->addTextAnnotation(rel, text);
                        update();
                    }
                }
            } else if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Counter) {
                if (selRect.contains(event->pos())) {
                    m_annotationEngine->addCounterAnnotation(rel);
                    update();
                }
            } else if (m_annotationEngine) {
                m_annotationEngine->endDraw(rel);
                update();
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
        if (m_selectionComplete) {
            m_selectionComplete = false;
            m_isSelecting = false;
            m_selectionStart = m_selectionEnd = QPoint();
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
        if (m_annotationEngine) { m_annotationEngine->undo(); update(); }
    } else if (event->matches(QKeySequence::Redo)) {
        if (m_annotationEngine) { m_annotationEngine->redo(); update(); }
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (m_selectionComplete) onCopyToClipboard();
    }
}

void CaptureOverlay::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Shift && m_annotationEngine) {
        m_annotationEngine->setShiftHeld(false);
    }
}

QPixmap CaptureOverlay::getSelectedPixmap()
{
    QRect selRect = normalizedSelectionRect();
    if (selRect.isEmpty()) return QPixmap();
    QPixmap result = m_screenSnapshot.copy(selRect);
    if (m_annotationEngine && m_annotationEngine->hasAnnotations()) {
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
    result.replace("%T", "");  // Pencere başlığı — opsiyonel, şimdilik boş

    return result;
}

void CaptureOverlay::showToolbar()
{
    if (!m_toolbar) return;
    QRect selRect = normalizedSelectionRect();
    int tw = m_toolbar->sizeHint().width();
    int x = selRect.left() + (selRect.width() - tw) / 2;
    int y = selRect.bottom() + 10;
    if (x < 5) x = 5;
    if (x + tw > width() - 5) x = width() - tw - 5;
    if (y + m_toolbar->height() > height() - 5)
        y = selRect.top() - m_toolbar->height() - 10;
    if (y < 5) y = selRect.bottom() + 10;
    m_toolbar->move(x, y);
    m_toolbar->show();
    m_toolbar->raise();
}

void CaptureOverlay::hideToolbar() { if (m_toolbar) m_toolbar->hide(); }

void CaptureOverlay::finishCapture()
{
    QPixmap result = getSelectedPixmap();
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    if (!result.isNull()) emit captureCompleted(result);
}

void CaptureOverlay::cancelCapture()
{
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    emit captureCancelled();
}

void CaptureOverlay::onClose() { cancelCapture(); }

QRect CaptureOverlay::normalizedSelectionRect() const
{
    return QRect(m_selectionStart, m_selectionEnd).normalized();
}

void CaptureOverlay::onToolSelected(int toolId)
{
    if (m_annotationEngine)
        m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(toolId));
}

void CaptureOverlay::onCopyToClipboard()
{
    QPixmap result = getSelectedPixmap();
    if (result.isNull()) return;
    QGuiApplication::clipboard()->setPixmap(result);
    finishCapture();
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

    // Yeni PinnedWindow oluştur
    PinnedWindow *pin = new PinnedWindow(result, screenPos);
    m_pinnedWindows.append(QPointer<QWidget>(pin));

    // Overlay'i kapat
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