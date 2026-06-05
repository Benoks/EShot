#include "CaptureOverlay.h"
#include "PinnedWindow.h"
#include "annotation/AnnotationEngine.h"
#include "ui/AnnotationToolbar.h"
#include "ui/OcrDialog.h"
#include "ui/UploadDialog.h"

#include "../core/TranslationManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
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
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFrame>
#include <QToolButton>
#include <QFontComboBox>
#include <QSpinBox>
#include <QAbstractSpinBox>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QSignalBlocker>
#include <QPropertyAnimation>
#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QDebug>
#include <QDir>
#include <QPointer>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QProcess>

#ifdef Q_OS_WIN
#include <windows.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#endif

namespace {
class SideTabButton : public QPushButton
{
public:
    explicit SideTabButton(const QString &text, QWidget *parent = nullptr)
        : QPushButton(text, parent)
    {
        setFixedSize(28, 118);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QLinearGradient gradient(rect().topLeft(), rect().bottomRight());
        if (underMouse()) {
            gradient.setColorAt(0.0, QColor(82, 82, 82, 245));
            gradient.setColorAt(1.0, QColor(54, 54, 54, 245));
        } else {
            gradient.setColorAt(0.0, QColor(62, 62, 62, 235));
            gradient.setColorAt(1.0, QColor(43, 43, 43, 235));
        }
        QRectF r = rect().adjusted(0, 0, -1, -1);
        constexpr qreal radius = 6.0;
        QPainterPath tabPath;
        tabPath.moveTo(r.left(), r.top());
        tabPath.lineTo(r.right() - radius, r.top());
        tabPath.quadTo(r.right(), r.top(), r.right(), r.top() + radius);
        tabPath.lineTo(r.right(), r.bottom() - radius);
        tabPath.quadTo(r.right(), r.bottom(), r.right() - radius, r.bottom());
        tabPath.lineTo(r.left(), r.bottom());
        tabPath.closeSubpath();

        painter.setPen(QPen(QColor(255, 255, 255, 55), 1));
        painter.setBrush(gradient);
        painter.drawPath(tabPath);

        painter.setPen(QPen(QColor(0, 0, 0, 90), 1));
        painter.drawLine(rect().topLeft(), rect().bottomLeft());

        QFont labelFont = font();
        labelFont.setPointSize(8);
        labelFont.setBold(true);
        labelFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
        painter.setFont(labelFont);
        painter.setPen(QColor(245, 245, 245));
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(-90);
        painter.drawText(QRect(-height() / 2, -width() / 2, height(), width()),
                         Qt::AlignCenter,
                         text());
    }
};

QString defaultSaveDirectory()
{
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.trimmed().isEmpty())
        picturesPath = QDir::homePath();
    return QDir(picturesPath).filePath(QStringLiteral("EShot"));
}

#ifdef Q_OS_WIN
void appendDeviceProperty(IPropertyStore *store, const PROPERTYKEY &key, QStringList &devices)
{
    PROPVARIANT value;
    PropVariantInit(&value);
    if (SUCCEEDED(store->GetValue(key, &value)) && value.vt == VT_LPWSTR && value.pwszVal) {
        const QString name = QString::fromWCharArray(value.pwszVal).trimmed();
        if (!name.isEmpty() && !devices.contains(name))
            devices.append(name);
    }
    PropVariantClear(&value);
}
#endif

QString localFfmpegPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("ffmpeg/ffmpeg.exe")),
        QDir(appDir).filePath(QStringLiteral("ffmpeg.exe")),
        QDir(appDir).filePath(QStringLiteral("../third_party/ffmpeg/bin/ffmpeg.exe")),
        QDir::current().filePath(QStringLiteral("third_party/ffmpeg/bin/ffmpeg.exe"))
    };
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return QFileInfo(path).absoluteFilePath();
    }
    return QString();
}

QStringList windowsAudioInputDevices()
{
    QStringList devices;
#ifdef Q_OS_WIN
    HRESULT initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool shouldUninit = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE)
        return devices;

    IMMDeviceEnumerator *enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  __uuidof(IMMDeviceEnumerator),
                                  reinterpret_cast<void **>(&enumerator));
    if (SUCCEEDED(hr) && enumerator) {
        IMMDeviceCollection *collection = nullptr;
        hr = enumerator->EnumAudioEndpoints(eCapture, DEVICE_STATE_ACTIVE, &collection);
        if (SUCCEEDED(hr) && collection) {
            UINT count = 0;
            collection->GetCount(&count);
            for (UINT i = 0; i < count; ++i) {
                IMMDevice *device = nullptr;
                if (FAILED(collection->Item(i, &device)) || !device)
                    continue;
                IPropertyStore *store = nullptr;
                if (SUCCEEDED(device->OpenPropertyStore(STGM_READ, &store)) && store) {
                    appendDeviceProperty(store, PKEY_DeviceInterface_FriendlyName, devices);
                    appendDeviceProperty(store, PKEY_Device_FriendlyName, devices);
                    appendDeviceProperty(store, PKEY_Device_DeviceDesc, devices);
                    store->Release();
                }
                device->Release();
            }
            collection->Release();
        }
        enumerator->Release();
    }
    if (shouldUninit)
        CoUninitialize();
#endif
    return devices;
}

QStringList dshowAudioDevices()
{
    const QString ffmpeg = localFfmpegPath();
    QStringList devices;
    if (ffmpeg.isEmpty())
        return windowsAudioInputDevices();

    QProcess process;
    process.setProgram(ffmpeg);
    process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-list_devices"), QStringLiteral("true"),
                          QStringLiteral("-f"), QStringLiteral("dshow"), QStringLiteral("-i"), QStringLiteral("dummy")});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(1800))
        process.kill();

    const QString output = QString::fromLocal8Bit(process.readAll());
    QRegularExpression re(QStringLiteral("\"([^\"]+)\"\\s*\\(audio\\)"));
    auto it = re.globalMatch(output);
    while (it.hasNext()) {
        const QString name = it.next().captured(1).trimmed();
        if (!name.isEmpty() && !devices.contains(name))
            devices.append(name);
    }
    for (const QString &name : windowsAudioInputDevices()) {
        if (!devices.contains(name))
            devices.append(name);
    }
    return devices;
}

bool localFfmpegSupportsFormat(const QString &format)
{
    const QString ffmpeg = localFfmpegPath();
    if (ffmpeg.isEmpty())
        return false;

    QProcess process;
    process.setProgram(ffmpeg);
    process.setArguments({QStringLiteral("-hide_banner"), QStringLiteral("-formats")});
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start();
    if (!process.waitForFinished(1800))
        process.kill();
    const QString output = QString::fromLocal8Bit(process.readAll());
    return output.contains(QRegularExpression(QStringLiteral("\\b%1\\b").arg(QRegularExpression::escape(format))));
}
}

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
    , m_textEditing(false)
    , m_eyedropperActive(false)
    , m_selectionLocked(false)
{
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

    // Text editor (multi-line support)
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
        // Auto height adjust
        if (m_textEdit && m_textEdit->isVisible()) {
            QTextDocument *doc = m_textEdit->document();
            QSizeF size = doc->size();
            int h = qMax(30, static_cast<int>(size.height()) + 10);
            m_textEdit->setFixedHeight(h);
            updateTextEditPanelPosition();
        }
    });

    m_textEditPanel = new QWidget(this);
    m_textEditPanel->setObjectName(QStringLiteral("textEditPanel"));
    m_textEditPanel->setStyleSheet(R"(
        QWidget#textEditPanel {
            background-color: rgba(34, 34, 34, 235);
            border: 1px solid rgba(255, 255, 255, 55);
            border-radius: 6px;
        }
        QPushButton {
            background-color: rgba(255, 255, 255, 24);
            color: white;
            border: 1px solid rgba(255, 255, 255, 42);
            border-radius: 4px;
            padding: 2px 8px;
            font-size: 11px;
            font-weight: 600;
        }
        QPushButton:hover { background-color: rgba(255, 255, 255, 38); }
        QFontComboBox, QSpinBox {
            background: #2b2b2b;
            color: white;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 2px 5px;
            min-height: 22px;
        }
        QFontComboBox:hover, QSpinBox:hover {
            border-color: #6a6a6a;
            background: #303030;
        }
        QFontComboBox:focus, QSpinBox:focus {
            border-color: #0078D4;
        }
        QFontComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 22px;
            border-left: 1px solid #444;
            border-top-right-radius: 4px;
            border-bottom-right-radius: 4px;
            background: #252525;
        }
        QFontComboBox::down-arrow {
            image: none;
            width: 0;
            height: 0;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #d8d8d8;
            margin-right: 7px;
        }
        QSpinBox {
            padding-right: 20px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border;
            width: 18px;
            background: #252525;
            border-left: 1px solid #444;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-top-right-radius: 4px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-bottom-right-radius: 4px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background: #3a3a3a;
        }
        QSpinBox::up-arrow {
            image: none;
            width: 0;
            height: 0;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-bottom: 5px solid #d8d8d8;
        }
        QSpinBox::down-arrow {
            image: none;
            width: 0;
            height: 0;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #d8d8d8;
        }
    )");
    QHBoxLayout *textPanelLayout = new QHBoxLayout(m_textEditPanel);
    textPanelLayout->setContentsMargins(4, 4, 4, 4);
    textPanelLayout->setSpacing(4);
    m_textMoveHandle = new QPushButton(QStringLiteral("Move"), m_textEditPanel);
    m_textMoveHandle->setCursor(Qt::SizeAllCursor);
    m_textMoveHandle->installEventFilter(this);
    m_textEditPanel->installEventFilter(this);
    m_textInlineFontCombo = new QFontComboBox(m_textEditPanel);
    m_textInlineFontCombo->setFixedWidth(132);
    m_textInlineSizeSpin = new QSpinBox(m_textEditPanel);
    m_textInlineSizeSpin->setRange(8, 72);
    m_textInlineSizeSpin->setFixedWidth(54);
    textPanelLayout->addWidget(m_textMoveHandle);
    textPanelLayout->addWidget(m_textInlineFontCombo);
    textPanelLayout->addWidget(m_textInlineSizeSpin);
    m_textEditPanel->hide();
    connect(m_textInlineFontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        if (m_annotationEngine) m_annotationEngine->setTextFontFamily(font.family());
        updateTextEditorStyle();
    });
    connect(m_textInlineSizeSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int size) {
        if (m_annotationEngine) m_annotationEngine->setTextFontSize(size);
        updateTextEditorStyle();
        if (m_textEdit && m_textEdit->isVisible()) {
            QTextDocument *doc = m_textEdit->document();
            int h = qMax(30, static_cast<int>(doc->size().height()) + 10);
            m_textEdit->setFixedHeight(h);
            updateTextEditPanelPosition();
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
        if (m_quickPenWidthSlider) {
            QSignalBlocker blocker(m_quickPenWidthSlider);
            m_quickPenWidthSlider->setValue(w);
        }
    });
    connect(m_toolbar, &AnnotationToolbar::textFontFamilyChanged, [this](const QString &family) {
        if (m_annotationEngine) m_annotationEngine->setTextFontFamily(family);
        if (m_textEdit && m_textEdit->isVisible()) {
            if (m_textInlineFontCombo) {
                QSignalBlocker blocker(m_textInlineFontCombo);
                m_textInlineFontCombo->setCurrentFont(QFont(family));
            }
            updateTextEditorStyle();
            updateTextEditPanelPosition();
        }
    });
    connect(m_toolbar, &AnnotationToolbar::textFontSizeChanged, [this](int size) {
        if (m_annotationEngine) m_annotationEngine->setTextFontSize(size);
        if (m_textEdit && m_textEdit->isVisible()) {
            if (m_textInlineSizeSpin) {
                QSignalBlocker blocker(m_textInlineSizeSpin);
                m_textInlineSizeSpin->setValue(size);
            }
            updateTextEditorStyle();
            updateTextEditPanelPosition();
        }
    });
    connect(m_toolbar, &AnnotationToolbar::blurIntensityChanged, [this](int i) {
        if (m_annotationEngine) m_annotationEngine->setBlurIntensity(i);
        if (m_quickBlurSlider) {
            QSignalBlocker blocker(m_quickBlurSlider);
            m_quickBlurSlider->setValue(i);
        }
    });
    connect(m_toolbar, &AnnotationToolbar::eyedropperRequested, this, &CaptureOverlay::onEyedropperRequested);
    connect(m_toolbar, &AnnotationToolbar::lockToggled, this, &CaptureOverlay::onSelectionLockToggled);
    connect(m_toolbar, &AnnotationToolbar::ocrRequested, this, &CaptureOverlay::onOcrRequested);
    connect(m_toolbar, &AnnotationToolbar::uploadRequested, this, &CaptureOverlay::onUploadRequested);
    connect(m_toolbar, &AnnotationToolbar::gifRequested, this, &CaptureOverlay::onGifRequested);
    connect(m_toolbar, &AnnotationToolbar::videoRequested, this, &CaptureOverlay::onVideoRequested);
    connect(m_annotationEngine, &AnnotationEngine::annotationAdded, this, &CaptureOverlay::updateUndoRedoState);

    setupToolSettingsDrawer();

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

void CaptureOverlay::setupToolSettingsDrawer()
{
    m_toolSettingsButton = new SideTabButton(TranslationManager::quickSettings(), this);
    m_toolSettingsButton->setToolTip(TranslationManager::quickSettings());
    m_toolSettingsButton->setCursor(Qt::PointingHandCursor);
    m_toolSettingsButton->hide();
    connect(m_toolSettingsButton, &QPushButton::clicked, this, [this]() {
        setToolSettingsDrawerVisible(m_toolSettingsDrawer && !m_toolSettingsDrawer->isVisible());
    });

    m_toolSettingsDrawer = new QWidget(this);
    m_toolSettingsDrawer->setObjectName(QStringLiteral("toolSettingsDrawer"));
    m_toolSettingsDrawer->setFixedWidth(258);
    m_toolSettingsDrawer->setStyleSheet(R"(
        QWidget#toolSettingsDrawer {
            background-color: rgba(37, 37, 37, 248);
            border: 1px solid rgba(255, 255, 255, 50);
            border-top-left-radius: 0px;
            border-bottom-left-radius: 0px;
            border-top-right-radius: 7px;
            border-bottom-right-radius: 7px;
        }
        QLabel { color: #d6d6d6; font-size: 11px; }
        QLabel[section="true"] { color: #f2f2f2; font-size: 12px; font-weight: 700; }
        QLabel[valueBadge="true"] {
            color: #f4f4f4;
            background-color: rgba(255, 255, 255, 18);
            border: 1px solid rgba(255, 255, 255, 38);
            border-radius: 4px;
            padding: 1px 5px;
            min-width: 24px;
            qproperty-alignment: AlignCenter;
            font-size: 10px;
            font-weight: 700;
        }
        QSlider::groove:horizontal {
            background: #484848;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #e8e8e8;
            border: 1px solid #9a9a9a;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSpinBox, QComboBox {
            background: rgba(52, 52, 52, 235);
            color: #f1f1f1;
            border: 1px solid rgba(255, 255, 255, 55);
            border-radius: 5px;
            min-height: 25px;
            padding: 2px 8px;
            selection-background-color: #4a4a4a;
        }
        QSpinBox:hover, QComboBox:hover {
            background: rgba(62, 62, 62, 240);
            border-color: rgba(255, 255, 255, 82);
        }
        QSpinBox:focus, QComboBox:focus {
            border-color: rgba(255, 255, 255, 110);
        }
        QCheckBox {
            color: #d6d6d6;
            font-size: 11px;
            spacing: 7px;
        }
        QCheckBox::indicator {
            width: 13px;
            height: 13px;
            border-radius: 3px;
            border: 1px solid rgba(255, 255, 255, 70);
            background: rgba(255, 255, 255, 12);
        }
        QCheckBox::indicator:checked {
            background: #4ea1ff;
            border-color: #77bbff;
        }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 22px;
            border-left: 1px solid rgba(255, 255, 255, 36);
            border-top-right-radius: 5px;
            border-bottom-right-radius: 5px;
            background: rgba(255, 255, 255, 10);
        }
        QComboBox::down-arrow {
            image: none;
            width: 0px;
            height: 0px;
        }
    )");

    QVBoxLayout *drawerLayout = new QVBoxLayout(m_toolSettingsDrawer);
    drawerLayout->setContentsMargins(12, 10, 12, 11);
    drawerLayout->setSpacing(8);

    QLabel *title = new QLabel(TranslationManager::quickSettings(), m_toolSettingsDrawer);
    title->setProperty("section", true);
    drawerLayout->addWidget(title);

    auto addSliderRow = [&](const QString &label, QSlider *&slider, QLabel *&valueLabel, int min, int max, int value) {
        QLabel *rowLabel = new QLabel(label, m_toolSettingsDrawer);
        slider = new QSlider(Qt::Horizontal, m_toolSettingsDrawer);
        slider->setRange(min, max);
        slider->setValue(value);
        valueLabel = new QLabel(QString::number(value), m_toolSettingsDrawer);
        valueLabel->setProperty("valueBadge", true);
        valueLabel->setFixedWidth(34);
        drawerLayout->addWidget(rowLabel);
        auto *valueRow = new QHBoxLayout();
        valueRow->setContentsMargins(0, 0, 0, 0);
        valueRow->setSpacing(7);
        valueRow->addWidget(slider, 1);
        valueRow->addWidget(valueLabel);
        drawerLayout->addLayout(valueRow);
    };

    addSliderRow(TranslationManager::quickPenWidth(), m_quickPenWidthSlider, m_quickPenWidthValueLabel, 1, 20, 3);
    addSliderRow(TranslationManager::quickBlurIntensity(), m_quickBlurSlider, m_quickBlurValueLabel, 4, 64, 16);

    QLabel *gifTitle = new QLabel(TranslationManager::quickGifRecording(), m_toolSettingsDrawer);
    gifTitle->setProperty("section", true);
    drawerLayout->addWidget(gifTitle);

    auto *fpsRow = new QHBoxLayout();
    fpsRow->addWidget(new QLabel(TranslationManager::recordingFpsLabel().remove(QChar(':')), m_toolSettingsDrawer));
    m_quickGifFpsSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickGifFpsSpin->setRange(1, 30);
    m_quickGifFpsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    fpsRow->addWidget(m_quickGifFpsSpin);
    drawerLayout->addLayout(fpsRow);

    auto *timeRow = new QHBoxLayout();
    timeRow->addWidget(new QLabel(TranslationManager::quickMaxSeconds(), m_toolSettingsDrawer));
    m_quickGifSecondsSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickGifSecondsSpin->setRange(0, 600);
    m_quickGifSecondsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    timeRow->addWidget(m_quickGifSecondsSpin);
    drawerLayout->addLayout(timeRow);

    auto *loopRow = new QHBoxLayout();
    loopRow->addWidget(new QLabel(TranslationManager::recordingLoop().remove(QChar(':')), m_toolSettingsDrawer));
    m_quickGifLoopCombo = new QComboBox(m_toolSettingsDrawer);
    m_quickGifLoopCombo->addItem(TranslationManager::recordingLoopInfinite(), 0);
    m_quickGifLoopCombo->addItem(QStringLiteral("1"), 1);
    m_quickGifLoopCombo->addItem(QStringLiteral("2"), 2);
    m_quickGifLoopCombo->addItem(QStringLiteral("3"), 3);
    m_quickGifLoopCombo->addItem(QStringLiteral("5"), 5);
    m_quickGifLoopCombo->addItem(QStringLiteral("10"), 10);
    loopRow->addWidget(m_quickGifLoopCombo);
    drawerLayout->addLayout(loopRow);

    QFrame *line = new QFrame(m_toolSettingsDrawer);
    line->setFrameShape(QFrame::HLine);
    line->setStyleSheet(QStringLiteral("color: rgba(255,255,255,45);"));
    drawerLayout->addWidget(line);
    QLabel *videoTitle = new QLabel(TranslationManager::videoRecordingTitle(), m_toolSettingsDrawer);
    videoTitle->setProperty("section", true);
    drawerLayout->addWidget(videoTitle);

    auto *videoFpsRow = new QHBoxLayout();
    videoFpsRow->addWidget(new QLabel(TranslationManager::recordingFpsLabel().remove(QChar(':')), m_toolSettingsDrawer));
    m_quickVideoFpsSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickVideoFpsSpin->setRange(1, 60);
    m_quickVideoFpsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    videoFpsRow->addWidget(m_quickVideoFpsSpin);
    drawerLayout->addLayout(videoFpsRow);

    auto *videoTimeRow = new QHBoxLayout();
    videoTimeRow->addWidget(new QLabel(TranslationManager::quickMaxSeconds(), m_toolSettingsDrawer));
    m_quickVideoSecondsSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickVideoSecondsSpin->setRange(0, 3600);
    m_quickVideoSecondsSpin->setSpecialValueText(TranslationManager::recordingUnlimited());
    m_quickVideoSecondsSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    videoTimeRow->addWidget(m_quickVideoSecondsSpin);
    drawerLayout->addLayout(videoTimeRow);

    auto *videoCrfRow = new QHBoxLayout();
    videoCrfRow->addWidget(new QLabel(TranslationManager::videoQualityCrf(), m_toolSettingsDrawer));
    m_quickVideoCrfSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickVideoCrfSpin->setRange(18, 32);
    m_quickVideoCrfSpin->setValue(24);
    m_quickVideoCrfSpin->setToolTip(TranslationManager::videoCrfHint());
    m_quickVideoCrfSpin->setButtonSymbols(QAbstractSpinBox::NoButtons);
    videoCrfRow->addWidget(m_quickVideoCrfSpin);
    drawerLayout->addLayout(videoCrfRow);

    QLabel *audioTitle = new QLabel(TranslationManager::audioMode(), m_toolSettingsDrawer);
    audioTitle->setProperty("section", true);
    drawerLayout->addWidget(audioTitle);

    auto addAudioVolumeRow = [&](QCheckBox *&check, QSlider *&slider, QLabel *&valueLabel,
                                 const QString &label, const char *enabledKey, const char *volumeKey) {
        check = new QCheckBox(label, m_toolSettingsDrawer);
        check->setChecked(false);
        drawerLayout->addWidget(check);

        auto *valueRow = new QHBoxLayout();
        valueRow->setContentsMargins(0, 0, 0, 0);
        valueRow->setSpacing(7);
        slider = new QSlider(Qt::Horizontal, m_toolSettingsDrawer);
        slider->setRange(0, 100);
        slider->setValue(80);
        valueLabel = new QLabel(QStringLiteral("80"), m_toolSettingsDrawer);
        valueLabel->setProperty("valueBadge", true);
        valueLabel->setFixedWidth(34);
        valueRow->addWidget(slider, 1);
        valueRow->addWidget(valueLabel);
        drawerLayout->addLayout(valueRow);

        auto updateEnabled = [slider, valueLabel](bool enabled) {
            slider->setEnabled(enabled);
            valueLabel->setEnabled(enabled);
            slider->setGraphicsEffect(nullptr);
            slider->setStyleSheet(enabled ? QString() : QStringLiteral("opacity: 0.45;"));
            valueLabel->setStyleSheet(enabled
                ? QStringLiteral("color: #f4f4f4; background-color: rgba(255, 255, 255, 18); border: 1px solid rgba(255, 255, 255, 38); border-radius: 4px; padding: 1px 5px;")
                : QStringLiteral("color: #777; background-color: rgba(255, 255, 255, 8); border: 1px solid rgba(255, 255, 255, 18); border-radius: 4px; padding: 1px 5px;"));
        };
        updateEnabled(check->isChecked());

        connect(check, &QCheckBox::toggled, this, [enabledKey, updateEnabled](bool enabled) {
            QSettings s("EShot", "EShot");
            s.setValue(QString::fromLatin1(enabledKey), enabled);
            updateEnabled(enabled);
        });
        connect(slider, &QSlider::valueChanged, this, [valueLabel, volumeKey](int value) {
            valueLabel->setText(QString::number(value));
            QSettings s("EShot", "EShot");
            s.setValue(QString::fromLatin1(volumeKey), value);
        });
    };

    addAudioVolumeRow(m_quickDesktopAudioCheck, m_quickDesktopVolumeSlider, m_quickDesktopVolumeLabel,
                      TranslationManager::audioDesktop(), "videoDesktopAudioEnabled", "videoDesktopAudioVolume");
    addAudioVolumeRow(m_quickMicrophoneCheck, m_quickMicrophoneVolumeSlider, m_quickMicrophoneVolumeLabel,
                      TranslationManager::audioMicrophone(), "videoMicrophoneEnabled", "videoMicrophoneVolume");

    const QStringList audioDevices = dshowAudioDevices();
    const QStringList activeInputDevices = windowsAudioInputDevices();
    auto *desktopDeviceRow = new QHBoxLayout();
    desktopDeviceRow->addWidget(new QLabel(TranslationManager::audioSource(), m_toolSettingsDrawer));
    m_quickDesktopAudioDeviceCombo = new QComboBox(m_toolSettingsDrawer);
    bool hasDesktopCandidate = true;
    m_quickDesktopAudioDeviceCombo->addItem(TranslationManager::audioSystemLoopback(), QStringLiteral("__wasapi__"));
    for (const QString &device : audioDevices) {
        const QString lower = device.toLower();
        const bool looksLikeDesktop = lower.contains(QStringLiteral("virtual-audio-capturer")) ||
            lower.contains(QStringLiteral("stereo mix")) ||
            lower.contains(QStringLiteral("what u hear")) ||
            lower.contains(QStringLiteral("loopback")) ||
            lower.contains(QStringLiteral("wave out"));
        if (!looksLikeDesktop)
            continue;
        hasDesktopCandidate = true;
        m_quickDesktopAudioDeviceCombo->addItem(device, device);
    }
    desktopDeviceRow->addWidget(m_quickDesktopAudioDeviceCombo);
    drawerLayout->addLayout(desktopDeviceRow);
    if (m_quickDesktopAudioCheck)
        m_quickDesktopAudioCheck->setEnabled(true);
    m_quickDesktopAudioDeviceCombo->setEnabled(hasDesktopCandidate && m_quickDesktopAudioCheck && m_quickDesktopAudioCheck->isChecked());
    connect(m_quickDesktopAudioCheck, &QCheckBox::toggled,
            this, [this, hasDesktopCandidate](bool enabled) {
                if (m_quickDesktopAudioDeviceCombo)
                    m_quickDesktopAudioDeviceCombo->setEnabled(hasDesktopCandidate && enabled);
            });

    auto *micDeviceRow = new QHBoxLayout();
    micDeviceRow->addWidget(new QLabel(TranslationManager::audioMicrophoneDevice(), m_toolSettingsDrawer));
    m_quickMicrophoneDeviceCombo = new QComboBox(m_toolSettingsDrawer);
    if (activeInputDevices.isEmpty() && audioDevices.isEmpty()) {
        m_quickMicrophoneDeviceCombo->addItem(QStringLiteral("Default"), QStringLiteral("default"));
    } else {
        m_quickMicrophoneDeviceCombo->addItem(QStringLiteral("Default"), activeInputDevices.isEmpty() ? audioDevices.first() : activeInputDevices.first());
    }
    for (const QString &device : activeInputDevices) {
        if (m_quickMicrophoneDeviceCombo->findData(device) >= 0)
            continue;
        m_quickMicrophoneDeviceCombo->addItem(device, device);
    }
    for (const QString &device : audioDevices) {
        if (m_quickMicrophoneDeviceCombo->findData(device) >= 0)
            continue;
        m_quickMicrophoneDeviceCombo->addItem(device, device);
    }
    micDeviceRow->addWidget(m_quickMicrophoneDeviceCombo);
    drawerLayout->addLayout(micDeviceRow);
    m_quickMicrophoneDeviceCombo->setEnabled(m_quickMicrophoneCheck && m_quickMicrophoneCheck->isChecked());
    connect(m_quickMicrophoneCheck, &QCheckBox::toggled,
            m_quickMicrophoneDeviceCombo, &QComboBox::setEnabled);

    connect(m_quickPenWidthSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_quickPenWidthValueLabel)
            m_quickPenWidthValueLabel->setText(QString::number(value));
        if (m_annotationEngine) m_annotationEngine->setPenWidth(value);
    });
    connect(m_quickBlurSlider, &QSlider::valueChanged, this, [this](int value) {
        if (m_quickBlurValueLabel)
            m_quickBlurValueLabel->setText(QString::number(value));
        if (m_annotationEngine) m_annotationEngine->setBlurIntensity(value);
        if (m_toolbar) m_toolbar->setBlurIntensity(value);
        QSettings s("EShot", "EShot");
        s.setValue("blurIntensity", value);
    });
    connect(m_quickGifFpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int value) {
        QSettings s("EShot", "EShot");
        s.setValue("recordingFps", value);
    });
    connect(m_quickGifSecondsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int value) {
        QSettings s("EShot", "EShot");
        s.setValue("recordingMaxSeconds", value);
    });
    connect(m_quickGifLoopCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        QSettings s("EShot", "EShot");
        s.setValue("recordingLoop", m_quickGifLoopCombo->currentData().toInt());
    });
    connect(m_quickVideoFpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int value) {
        QSettings s("EShot", "EShot");
        s.setValue("videoRecordingFps", value);
    });
    connect(m_quickVideoSecondsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int value) {
        QSettings s("EShot", "EShot");
        s.setValue("videoRecordingMaxSeconds", value);
    });
    connect(m_quickVideoCrfSpin, qOverload<int>(&QSpinBox::valueChanged), this, [](int value) {
        QSettings s("EShot", "EShot");
        s.setValue("videoRecordingCrf", value);
    });
    connect(m_quickMicrophoneDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        QSettings s("EShot", "EShot");
        s.setValue("videoMicrophoneDevice", m_quickMicrophoneDeviceCombo->currentData().toString());
    });
    connect(m_quickDesktopAudioDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        QSettings s("EShot", "EShot");
        s.setValue("videoDesktopAudioDevice", m_quickDesktopAudioDeviceCombo->currentData().toString());
    });

    m_toolSettingsDrawer->hide();

    m_toolSettingsAnimation = new QPropertyAnimation(m_toolSettingsDrawer, "geometry", this);
    m_toolSettingsAnimation->setDuration(185);
    m_toolSettingsAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_toolSettingsAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (m_toolSettingsDrawer && m_toolSettingsDrawer->property("closing").toBool()) {
            m_toolSettingsDrawer->hide();
            m_toolSettingsDrawer->setProperty("closing", false);
        }
        if (m_toolSettingsButton)
            m_toolSettingsButton->raise();
    });

    m_toolSettingsButtonAnimation = new QPropertyAnimation(m_toolSettingsButton, "geometry", this);
    m_toolSettingsButtonAnimation->setDuration(185);
    m_toolSettingsButtonAnimation->setEasingCurve(QEasingCurve::OutCubic);
}

void CaptureOverlay::updateToolSettingsDrawerPosition()
{
    if (!m_toolSettingsButton || !m_toolSettingsDrawer || !m_selectionComplete)
        return;

    QRect selRect = normalizedSelectionRect();
    QRect monitorRect = monitorRectAt(selRect.center());
    if (monitorRect.isEmpty())
        monitorRect = rect();

    m_toolSettingsDrawer->adjustSize();
    const bool drawerOpen = m_toolSettingsDrawer->isVisible()
        && !m_toolSettingsDrawer->property("closing").toBool();
    int bx = drawerOpen
        ? monitorRect.left() + m_toolSettingsDrawer->width() - 1
        : monitorRect.left();
    bx = qBound(0, bx, width() - m_toolSettingsButton->width());
    int by = qBound(monitorRect.top() + 32,
                    selRect.center().y() - m_toolSettingsButton->height() / 2,
                    monitorRect.bottom() - m_toolSettingsButton->height() - 32);
    m_toolSettingsButton->move(bx, by);
    m_toolSettingsButton->show();
    m_toolSettingsButton->raise();

    if (!m_toolSettingsDrawer->isVisible())
        return;

    if (m_toolSettingsAnimation && m_toolSettingsAnimation->state() == QAbstractAnimation::Running)
        return;

    int dx = monitorRect.left();
    int dy = m_toolSettingsButton->y() - 26;
    dy = qBound(5, dy, height() - m_toolSettingsDrawer->height() - 5);
    m_toolSettingsDrawer->move(dx, dy);
    m_toolSettingsDrawer->raise();
    m_toolSettingsButton->raise();
}

void CaptureOverlay::setToolSettingsDrawerVisible(bool visible)
{
    if (!m_toolSettingsDrawer || !m_toolSettingsButton)
        return;

    QRect selRect = normalizedSelectionRect();
    QRect monitorRect = monitorRectAt(selRect.center());
    if (monitorRect.isEmpty())
        monitorRect = rect();

    m_toolSettingsDrawer->adjustSize();
    const int drawerW = m_toolSettingsDrawer->width();
    const int drawerH = m_toolSettingsDrawer->height();
    const int drawerVisibleX = monitorRect.left();
    const int drawerHiddenX = monitorRect.left() - drawerW;
    const int buttonClosedX = qBound(0, monitorRect.left(), width() - m_toolSettingsButton->width());
    const int buttonOpenX = qBound(0, monitorRect.left() + drawerW - 1, width() - m_toolSettingsButton->width());
    const int drawerY = qBound(5,
                               m_toolSettingsButton->y() - 26,
                               height() - drawerH - 5);
    const int buttonY = qBound(monitorRect.top() + 32,
                               m_toolSettingsButton->y(),
                               monitorRect.bottom() - m_toolSettingsButton->height() - 32);

    if (m_toolSettingsAnimation)
        m_toolSettingsAnimation->stop();
    if (m_toolSettingsButtonAnimation)
        m_toolSettingsButtonAnimation->stop();

    if (visible) {
        QSettings s("EShot", "EShot");
        if (m_quickPenWidthSlider && m_annotationEngine) {
            QSignalBlocker blocker(m_quickPenWidthSlider);
            const int value = m_annotationEngine->penWidth();
            m_quickPenWidthSlider->setValue(value);
            if (m_quickPenWidthValueLabel)
                m_quickPenWidthValueLabel->setText(QString::number(value));
        }
        if (m_quickBlurSlider && m_annotationEngine) {
            QSignalBlocker blocker(m_quickBlurSlider);
            const int value = m_annotationEngine->blurIntensity();
            m_quickBlurSlider->setValue(value);
            if (m_quickBlurValueLabel)
                m_quickBlurValueLabel->setText(QString::number(value));
        }
        if (m_quickGifFpsSpin) {
            QSignalBlocker blocker(m_quickGifFpsSpin);
            m_quickGifFpsSpin->setValue(s.value("recordingFps", 10).toInt());
        }
        if (m_quickGifSecondsSpin) {
            QSignalBlocker blocker(m_quickGifSecondsSpin);
            m_quickGifSecondsSpin->setValue(s.value("recordingMaxSeconds", 30).toInt());
        }
        if (m_quickGifLoopCombo) {
            QSignalBlocker blocker(m_quickGifLoopCombo);
            int idx = m_quickGifLoopCombo->findData(s.value("recordingLoop", 0).toInt());
            if (idx < 0) idx = 0;
            m_quickGifLoopCombo->setCurrentIndex(idx);
        }
        if (m_quickVideoFpsSpin) {
            QSignalBlocker blocker(m_quickVideoFpsSpin);
            m_quickVideoFpsSpin->setValue(s.value("videoRecordingFps", 30).toInt());
        }
        if (m_quickVideoSecondsSpin) {
            QSignalBlocker blocker(m_quickVideoSecondsSpin);
            m_quickVideoSecondsSpin->setValue(s.value("videoRecordingMaxSeconds", 0).toInt());
        }
        if (m_quickVideoCrfSpin) {
            QSignalBlocker blocker(m_quickVideoCrfSpin);
            m_quickVideoCrfSpin->setValue(s.value("videoRecordingCrf", 24).toInt());
        }
        if (m_quickDesktopAudioCheck)
            m_quickDesktopAudioCheck->setChecked(s.value("videoDesktopAudioEnabled", false).toBool());
        if (m_quickDesktopVolumeSlider) {
            QSignalBlocker blocker(m_quickDesktopVolumeSlider);
            const int value = s.value("videoDesktopAudioVolume", 80).toInt();
            m_quickDesktopVolumeSlider->setValue(value);
            if (m_quickDesktopVolumeLabel)
                m_quickDesktopVolumeLabel->setText(QString::number(value));
        }
        if (m_quickDesktopAudioDeviceCombo) {
            QSignalBlocker blocker(m_quickDesktopAudioDeviceCombo);
            int idx = m_quickDesktopAudioDeviceCombo->findData(s.value("videoDesktopAudioDevice", "__wasapi__").toString());
            if (idx < 0) idx = 0;
            m_quickDesktopAudioDeviceCombo->setCurrentIndex(idx);
            const bool hasDevice = !m_quickDesktopAudioDeviceCombo->currentData().toString().isEmpty();
            if (!hasDevice && m_quickDesktopAudioCheck) {
                QSignalBlocker checkBlocker(m_quickDesktopAudioCheck);
                m_quickDesktopAudioCheck->setChecked(false);
                s.setValue("videoDesktopAudioEnabled", false);
            }
            m_quickDesktopAudioDeviceCombo->setEnabled(hasDevice && m_quickDesktopAudioCheck && m_quickDesktopAudioCheck->isChecked());
        }
        if (m_quickMicrophoneCheck)
            m_quickMicrophoneCheck->setChecked(s.value("videoMicrophoneEnabled", false).toBool());
        if (m_quickMicrophoneVolumeSlider) {
            QSignalBlocker blocker(m_quickMicrophoneVolumeSlider);
            const int value = s.value("videoMicrophoneVolume", 80).toInt();
            m_quickMicrophoneVolumeSlider->setValue(value);
            if (m_quickMicrophoneVolumeLabel)
                m_quickMicrophoneVolumeLabel->setText(QString::number(value));
        }
        if (m_quickMicrophoneDeviceCombo) {
            QSignalBlocker blocker(m_quickMicrophoneDeviceCombo);
            int idx = m_quickMicrophoneDeviceCombo->findData(s.value("videoMicrophoneDevice", "default").toString());
            if (idx < 0) idx = 0;
            m_quickMicrophoneDeviceCombo->setCurrentIndex(idx);
        }
        const QRect startRect = m_toolSettingsDrawer->isVisible()
            ? m_toolSettingsDrawer->geometry()
            : QRect(drawerHiddenX, drawerY, drawerW, drawerH);
        const QRect endRect(drawerVisibleX, drawerY, drawerW, drawerH);
        const QRect buttonStartRect = m_toolSettingsButton->geometry();
        const QRect buttonEndRect(buttonOpenX, buttonY, m_toolSettingsButton->width(), m_toolSettingsButton->height());
        m_toolSettingsDrawer->setProperty("closing", false);
        m_toolSettingsDrawer->setGeometry(startRect);
        m_toolSettingsDrawer->show();
        m_toolSettingsDrawer->raise();
        m_toolSettingsButton->raise();
        if (m_toolSettingsAnimation) {
            m_toolSettingsAnimation->setStartValue(startRect);
            m_toolSettingsAnimation->setEndValue(endRect);
            m_toolSettingsAnimation->start();
        } else {
            m_toolSettingsDrawer->setGeometry(endRect);
        }
        if (m_toolSettingsButtonAnimation) {
            m_toolSettingsButtonAnimation->setStartValue(buttonStartRect);
            m_toolSettingsButtonAnimation->setEndValue(buttonEndRect);
            m_toolSettingsButtonAnimation->start();
        } else {
            m_toolSettingsButton->setGeometry(buttonEndRect);
        }
    } else {
        if (!m_toolSettingsDrawer->isVisible()) {
            m_toolSettingsDrawer->hide();
        } else {
            const QRect startRect = m_toolSettingsDrawer->geometry();
            const QRect endRect(drawerHiddenX, startRect.y(), drawerW, drawerH);
            const QRect buttonStartRect = m_toolSettingsButton->geometry();
            const QRect buttonEndRect(buttonClosedX, buttonY, m_toolSettingsButton->width(), m_toolSettingsButton->height());
            m_toolSettingsDrawer->setProperty("closing", true);
            m_toolSettingsDrawer->raise();
            m_toolSettingsButton->raise();
            if (m_toolSettingsAnimation) {
                m_toolSettingsAnimation->setStartValue(startRect);
                m_toolSettingsAnimation->setEndValue(endRect);
                m_toolSettingsAnimation->start();
            } else {
                m_toolSettingsDrawer->setGeometry(endRect);
                m_toolSettingsDrawer->hide();
                m_toolSettingsDrawer->setProperty("closing", false);
            }
            if (m_toolSettingsButtonAnimation) {
                m_toolSettingsButtonAnimation->setStartValue(buttonStartRect);
                m_toolSettingsButtonAnimation->setEndValue(buttonEndRect);
                m_toolSettingsButtonAnimation->start();
            } else {
                m_toolSettingsButton->setGeometry(buttonEndRect);
            }
        }
    }
}

bool CaptureOverlay::isToolSettingsUiAt(const QPoint &pos) const
{
    if (m_toolSettingsButton && m_toolSettingsButton->isVisible() &&
        m_toolSettingsButton->geometry().contains(pos))
        return true;
    if (m_toolSettingsDrawer && m_toolSettingsDrawer->isVisible() &&
        !m_toolSettingsDrawer->property("closing").toBool() &&
        m_toolSettingsDrawer->geometry().contains(pos))
        return true;
    return false;
}

void CaptureOverlay::refreshUI()
{
    // Refresh toolbar button tooltips
    if (m_toolbar) m_toolbar->refreshToolTips();

    const bool drawerWasVisible = m_toolSettingsDrawer && m_toolSettingsDrawer->isVisible();
    if (m_toolSettingsAnimation) {
        m_toolSettingsAnimation->stop();
        m_toolSettingsAnimation->deleteLater();
        m_toolSettingsAnimation = nullptr;
    }
    if (m_toolSettingsButtonAnimation) {
        m_toolSettingsButtonAnimation->stop();
        m_toolSettingsButtonAnimation->deleteLater();
        m_toolSettingsButtonAnimation = nullptr;
    }
    if (m_toolSettingsButton) {
        m_toolSettingsButton->deleteLater();
        m_toolSettingsButton = nullptr;
    }
    if (m_toolSettingsDrawer) {
        m_toolSettingsDrawer->deleteLater();
        m_toolSettingsDrawer = nullptr;
    }
    setupToolSettingsDrawer();
    updateToolSettingsDrawerPosition();
    if (drawerWasVisible)
        setToolSettingsDrawerVisible(true);

    // Refresh action panel button tooltips
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

void CaptureOverlay::prewarm()
{
    // Force initial paint of overlay + toolbar + action panel at startup
    // so the first user-triggered capture does not stall on first-render
    // composition (~2-3 s on Windows for translucent frameless widgets).
    if (!isVisible()) {
        setGeometry(-10000, -10000, 800, 220);
        show();
        if (m_toolbar) {
            m_toolbar->adjustSize();
            m_toolbar->move(20, 20);
            m_toolbar->show();
        }
        if (m_actionPanel) {
            m_actionPanel->adjustSize();
            m_actionPanel->move(20, 90);
            m_actionPanel->show();
        }
        QCoreApplication::processEvents();
        if (m_toolbar) m_toolbar->grab();
        if (m_actionPanel) m_actionPanel->grab();
        QCoreApplication::processEvents();
        if (m_toolbar) m_toolbar->hide();
        if (m_actionPanel) m_actionPanel->hide();
        hide();
    }
}

void CaptureOverlay::startCapture()
{
    if (m_captureDelayTimer) m_captureDelayTimer->stop();
    m_captureMode = ModeNormal;
    m_isSelecting = false;
    m_selectionComplete = false;
    m_ignoreNextMouseRelease = false;
    m_resizeMode = ResNone;
    m_selectionLocked = false;
    m_selectionStart = QPoint();
    m_selectionEnd = QPoint();
    m_eyedropperActive = false;

    if (m_textEdit) m_textEdit->hide();
    if (m_textEditPanel) m_textEditPanel->hide();
    m_textJustCommitted = false;
    if (m_annotationEngine) m_annotationEngine->clear();
    hideToolbar();

    // Save foreground window before overlay (for %T filename variable)
    m_foregroundHwnd = GetForegroundWindow();

    // Load settings
    QSettings s("EShot", "EShot");
    if (m_toolbar) {
        m_toolbar->refreshTools();
        m_toolbar->setSelectionLocked(false);
        m_toolbar->selectTool(AnnotationEngine::None);
    }
    if (m_annotationEngine) {
        m_annotationEngine->setCurrentTool(AnnotationEngine::None);
        m_annotationEngine->setSelectedIndex(-1);
    }

    m_overlayOpacity = s.value("overlayOpacity", 100).toInt();
    m_crosshairStyle = s.value("crosshairStyle", "dash").toString();
    m_copyAfterCapture = s.value("copyAfterCapture", true).toBool();
    m_closeAfterCopy = s.value("closeAfterCopy", true).toBool();

    // Load blur strength setting
    int blurIntensity = s.value("blurIntensity", 16).toInt();
    if (m_annotationEngine) m_annotationEngine->setBlurIntensity(blurIntensity);
    if (m_toolbar) m_toolbar->setBlurIntensity(blurIntensity);
    if (m_quickBlurSlider) m_quickBlurSlider->setValue(blurIntensity);
    if (m_quickPenWidthSlider && m_annotationEngine) m_quickPenWidthSlider->setValue(m_annotationEngine->penWidth());
    setToolSettingsDrawerVisible(false);

    int delayMs = s.value("captureDelay", 0).toInt();
    if (delayMs > 0)
        m_captureDelayTimer->start(delayMs);
    else
        performCapture();
}

void CaptureOverlay::startCaptureForRecording()
{
    startCapture();
    m_captureMode = ModeRecording;
    setCursor(Qt::CrossCursor);
}

void CaptureOverlay::performCapture()
{
    captureAllScreens();

    // Pass screenshot to annotation engine (for blur effect)
    if (m_annotationEngine) {
        m_annotationEngine->setScreenSnapshot(m_screenSnapshot);
        m_annotationEngine->setSnapshotScale(m_dpr);
    }

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

    QScreen *primary = QGuiApplication::primaryScreen();
    m_dpr = primary ? primary->devicePixelRatio() : 1.0;

    if (vw <= 0 || vh <= 0) {
        if (primary) {
            m_dpr = primary->devicePixelRatio();
            m_virtualDesktopRect = primary->geometry();   // logical
            m_screenSnapshot = primary->grabWindow(0);     // physical pixels
            m_screenSnapshot.setDevicePixelRatio(1.0);
        }
        return;
    }

    // GetSystemMetrics returns physical pixels (the process is per-monitor DPI
    // aware), but the overlay window and all selection math run in logical
    // pixels. Keep the snapshot at full physical resolution and store the
    // virtual desktop in logical coordinates, bridged by m_dpr.
    m_virtualDesktopRect = QRect(qRound(vx / m_dpr), qRound(vy / m_dpr),
                                 qRound(vw / m_dpr), qRound(vh / m_dpr));

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
        // Clean area
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        // selRect is logical (overlay space); the snapshot is physical pixels.
        painter.drawPixmap(selRect, m_screenSnapshot, logicalToSnapshot(selRect));
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        // Annotation
        if (m_annotationEngine && m_selectionComplete) {
            painter.setClipRect(selRect);
            m_annotationEngine->render(&painter, selRect.topLeft());
            if (m_annotationEngine->selectedIndex() >= 0) {
                QRect annotationRect = m_annotationEngine->boundingRectOf(m_annotationEngine->selectedIndex())
                                           .adjusted(-4, -4, 4, 4)
                                           .translated(selRect.topLeft());
                if (!annotationRect.isEmpty()) {
                    QPen selectPen(QColor(255, 205, 70, 220), 1, Qt::DashLine);
                    selectPen.setCosmetic(true);
                    painter.setPen(selectPen);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(annotationRect.adjusted(0, 0, -1, -1), 3, 3);
                }
            }
            painter.setClipping(false);
        }

    // Frame
    painter.setPen(QPen(QColor(0, 120, 215), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(selRect);

    // Corner handles
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

        // Size info — always visible (top-left of selection)
        if (m_isSelecting || m_selectionComplete) {
            QString dim = QString("%1 x %2").arg(selRect.width()).arg(selRect.height());
            QFont f = painter.font(); f.setPointSize(10); f.setBold(true);
            painter.setFont(f);
            QFontMetrics fm(f);
            int tw = fm.horizontalAdvance(dim) + 16;
            int th = fm.height() + 8;
            int lx = selRect.left();
            int ly = selRect.top() - th - 4;
            if (ly < 4)
                ly = selRect.top() + 4;
            if (lx + tw > width() - 4)
                lx = width() - tw - 4;
            if (lx < 4)
                lx = 4;

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

    // Eyedropper: color preview circle
    if (m_eyedropperActive) {
        QPoint cur = mapFromGlobal(QCursor::pos());
        if (rect().contains(cur)) {
            QColor pixelColor;
            const QPoint curDev = logicalToSnapshot(cur);
            if (m_screenSnapshot.rect().contains(curDev)) {
                QImage img = m_screenSnapshot.toImage();
                pixelColor = QColor(img.pixel(curDev));
            }
            if (pixelColor.isValid()) {
                // Circle
                painter.setPen(QPen(Qt::white, 2));
                painter.setBrush(pixelColor);
                painter.drawEllipse(cur, 12, 12);
                // Color code label
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
        if (isToolSettingsUiAt(event->pos())) {
            event->accept();
            return;
        }
        if (m_toolbar && m_toolbar->isVisible() && m_toolbar->geometry().contains(event->pos()))
            return;
        if (m_actionPanel && m_actionPanel->isVisible() && m_actionPanel->geometry().contains(event->pos()))
            return;

        // Eyedropper click — pick color and return to normal mode
        if (m_eyedropperActive) {
            const QPoint pickDev = logicalToSnapshot(event->pos());
            if (m_screenSnapshot.rect().contains(pickDev)) {
                QImage img = m_screenSnapshot.toImage();
                QColor c = QColor(img.pixel(pickDev));
                if (c.isValid()) {
                    if (m_annotationEngine) m_annotationEngine->setColor(c);
                    // Update color button in toolbar
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
            const bool selectionHandleHit = mode != ResNone && mode != ResMove && mode != ResNewSelection;
            
            bool isDrawingTool = (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None);
            
            // Annotation click while no tool is selected → move mode
            if (!selectionHandleHit && m_annotationEngine && selRect.contains(event->pos())) {
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
                // Click in selection area — move current selection
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
            // Selection locked — allow annotation drawing only
            QRect selRect = normalizedSelectionRect();
            if (m_annotationEngine && selRect.contains(event->pos())) {
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
            // Eyedropper cancel
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
    if (isToolSettingsUiAt(event->pos()))
        return;

    selectMonitorAt(event->pos());
    m_ignoreNextMouseRelease = true;
}

void CaptureOverlay::mouseMoveEvent(QMouseEvent *event)
{
    // In Eyedropper mode — only repaint
    if (m_eyedropperActive) {
        update();
        return;
    }

    // Annotation move (with snap-to-edge)
    if (m_isDraggingAnnotation && m_annotationEngine && m_annotationEngine->selectedIndex() >= 0) {
        QRect selRect = normalizedSelectionRect();
        QPoint rel = event->pos() - selRect.topLeft();
        QPoint delta = rel - m_dragAnnotationStart;
        if (!delta.isNull()) {
            // Snap-to-edge: snap if within 8px of edge
            int snapThreshold = 8;
            QRect annBounds = m_annotationEngine->boundingRectOf(m_annotationEngine->selectedIndex());
            if (!annBounds.isEmpty()) {
                QPoint newPos = annBounds.topLeft() + delta;
                // Snap to left edge
                if (qAbs(newPos.x()) < snapThreshold) delta.setX(-annBounds.left());
                // Snap to right edge
                if (qAbs(newPos.x() + annBounds.width() - selRect.width()) < snapThreshold)
                    delta.setX(selRect.width() - annBounds.width() - annBounds.left());
                // Snap to top edge
                if (qAbs(newPos.y()) < snapThreshold) delta.setY(-annBounds.top());
                // Snap to bottom edge
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
            int h = oldSel.height();
            QRect bounds = rect();   // logical overlay bounds
            newTopLeft.setX(qBound(bounds.left(), newTopLeft.x(), bounds.right() - w + 1));
            newTopLeft.setY(qBound(bounds.top(), newTopLeft.y(), bounds.bottom() - h + 1));
            m_selectionStart = newTopLeft;
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

        if (m_selectionComplete && m_toolbar && m_toolbar->isVisible())
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
            QPoint rel = event->pos() - selRect.topLeft();
            if (m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                if ((event->buttons() & Qt::LeftButton) && m_annotationEngine->eraseAnnotationAt(rel))
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

        // Annotation move end
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

                if (m_captureMode == ModeRecording) {
                    finishCapture();
                    return;
                }

                showToolbar();
                updateUndoRedoState();
            }
            update();
        } else if (m_selectionComplete) {
            QRect selRect = normalizedSelectionRect();
            QPoint rel = event->pos() - selRect.topLeft();
            rel.setX(qBound(0, rel.x(), selRect.width()));
            rel.setY(qBound(0, rel.y(), selRect.height()));

            if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                updateUndoRedoState();
            } else if (m_annotationEngine && m_annotationEngine->currentTool() == AnnotationEngine::Text) {
                if (selRect.contains(event->pos())) {
                    beginTextEditAt(event->pos());
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
    // If eyedropper is active, just close it
    if (m_eyedropperActive) {
        m_eyedropperActive = false;
        update();
        return;
    }
    // If text edit is open, just close it
        if (m_textEdit && m_textEdit->isVisible()) {
            cancelTextEdit();
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
    } else if (matchesOverlayShortcut(event, QStringLiteral("actionCopy"), QStringLiteral("Ctrl+C"))) {
        onCopyToClipboard();
    } else if (matchesOverlayShortcut(event, QStringLiteral("actionSave"), QStringLiteral("Ctrl+S"))) {
        onSave();
    } else if (matchesOverlayShortcut(event, QStringLiteral("actionUndo"), QStringLiteral("Ctrl+Z"))) {
        if (m_annotationEngine) { m_annotationEngine->undo(); update(); updateUndoRedoState(); }
    } else if (matchesOverlayShortcut(event, QStringLiteral("actionRedo"), QStringLiteral("Ctrl+Shift+Z"))) {
        if (m_annotationEngine) { m_annotationEngine->redo(); update(); updateUndoRedoState(); }
    } else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        // If text edit is open and Shift is held, insert newline
        if (m_textEdit && m_textEdit->isVisible()) {
            if (event->modifiers() & Qt::ShiftModifier) {
                m_textEdit->insertPlainText("\n");
                return;
            }
            // If text edit was just closed, swallow Enter
            commitText();
            return;
        }
        if (m_textJustCommitted) {
            m_textJustCommitted = false;
            return;
        }
        if (m_selectionComplete) onCopyToClipboard();
    } else if (m_selectionComplete && m_annotationEngine) {
        if (matchesOverlayShortcut(event, QStringLiteral("actionEyedropper"), QStringLiteral("I"))) { onEyedropperRequested(); return; }
        if (matchesOverlayShortcut(event, QStringLiteral("actionLock"), QStringLiteral("K"))) { onSelectionLockToggled(!m_selectionLocked); return; }
        if (matchesOverlayShortcut(event, QStringLiteral("actionPin"), QStringLiteral("Ctrl+P"))) { onPinToDesktop(); return; }
        if (matchesOverlayShortcut(event, QStringLiteral("actionOcr"), QStringLiteral("Ctrl+O"))) { onOcrRequested(); return; }
        if (matchesOverlayShortcut(event, QStringLiteral("actionUpload"), QStringLiteral("Ctrl+U"))) { onUploadRequested(); return; }
        if (matchesOverlayShortcut(event, QStringLiteral("actionGif"), QStringLiteral("Ctrl+G"))) { onGifRequested(); return; }
        if (matchesOverlayShortcut(event, QStringLiteral("actionVideo"), QStringLiteral("Ctrl+Shift+V"))) { onVideoRequested(); return; }

        if (matchesOverlayShortcut(event, QStringLiteral("toolPen"), QStringLiteral("P"))) selectAnnotationTool(AnnotationEngine::Pen);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolArrow"), QStringLiteral("A"))) selectAnnotationTool(AnnotationEngine::Arrow);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolLine"), QStringLiteral("L"))) selectAnnotationTool(AnnotationEngine::Line);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolRectangle"), QStringLiteral("R"))) selectAnnotationTool(AnnotationEngine::Rectangle);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolCircle"), QStringLiteral("C"))) selectAnnotationTool(AnnotationEngine::Circle);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolText"), QStringLiteral("T"))) selectAnnotationTool(AnnotationEngine::Text);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolHighlighter"), QStringLiteral("H"))) selectAnnotationTool(AnnotationEngine::Highlighter);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolSemiRect"), QStringLiteral("D"))) selectAnnotationTool(AnnotationEngine::SemiRect);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolBlur"), QStringLiteral("B"))) selectAnnotationTool(AnnotationEngine::Blur);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolCounter"), QStringLiteral("N"))) selectAnnotationTool(AnnotationEngine::Counter);
        else if (matchesOverlayShortcut(event, QStringLiteral("toolEraser"), QStringLiteral("X"))) selectAnnotationTool(AnnotationEngine::Eraser);
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
    if ((obj == m_textMoveHandle || obj == m_textEditPanel) && m_textEdit && m_textEdit->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_textPanelDragging = true;
                m_textPanelDragOffset = mapFromGlobal(me->globalPosition().toPoint()) - m_textEdit->pos();
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && m_textPanelDragging) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            moveTextEditorTo(mapFromGlobal(me->globalPosition().toPoint()) - m_textPanelDragOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && m_textPanelDragging) {
            m_textPanelDragging = false;
            return true;
        }
    }

    if (obj == m_textEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            // Shift+Enter → newline
            if (ke->modifiers() & Qt::ShiftModifier) {
                m_textEdit->insertPlainText("\n");
                return true;
            }
            // Enter → confirm directly
            commitText();
            return true; // consume event
        }
    }
    return QWidget::eventFilter(obj, event);
}

QPixmap CaptureOverlay::getSelectedPixmap()
{
    QRect selRect = normalizedSelectionRect();
    if (selRect.isEmpty()) return QPixmap();
    // Crop at full physical resolution (selRect is logical).
    QPixmap result = m_screenSnapshot.copy(logicalToSnapshot(selRect));
    if (m_annotationEngine && m_annotationEngine->hasAnnotations()) {
        m_annotationEngine->setSelectionRect(selRect);
        QPainter p(&result);
        p.setRenderHint(QPainter::Antialiasing, true);
        // Annotations are authored in logical coordinates; scale them up to the
        // physical-resolution result.
        p.scale(m_dpr, m_dpr);
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
    // Use saved foreground window (the one active before overlay)
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

    m_toolbar->refreshTools();

    QRect toolbarRect;
    if (m_toolbar->hasVisibleTools()) {
        // --- Bottom toolbar: below the selection, centered ---
        m_toolbar->adjustSize();
        int th = m_toolbar->height();
        int toolbarWidth = m_toolbar->width();

        int tx = selRect.center().x() - toolbarWidth / 2;
        int ty = selRect.bottom() + margin;

        // Check screen bounds
        if (tx < 5) tx = 5;
        if (tx + toolbarWidth > width() - 5) tx = width() - toolbarWidth - 5;
        if (ty + th > height() - 5) {
            // If it does not fit below, try above
            ty = selRect.top() - th - margin;
        }
        if (ty < 5) {
            // If it also does not fit above, dock to bottom inside the frame
            ty = selRect.bottom() - th - 2;
        }

        m_toolbar->setFixedWidth(toolbarWidth);
        m_toolbar->move(tx, ty);
        m_toolbar->show();
        m_toolbar->raise();
        toolbarRect = m_toolbar->geometry();
    } else {
        m_toolbar->hide();
    }

    // --- Right panel: right of selection, vertically centered ---
    if (m_actionPanel) {
        m_actionPanel->adjustSize();
        int pw = m_actionPanel->width();
        int ph = m_actionPanel->height();
        int px = selRect.right() + margin;
        int py = selRect.center().y() - ph / 2;
        bool dockedInside = false;

        if (px + pw > width() - 5) {
            int leftOutside = selRect.left() - margin - pw;
            if (leftOutside >= 5) {
                px = leftOutside;
            } else {
                px = selRect.right() - pw - 2;
                dockedInside = true;
            }
        }

        if (dockedInside && px < selRect.left() + 2)
            px = selRect.left() + 2;
        if (px + pw > width() - 5)
            px = width() - pw - 5;
        if (px < 5)
            px = 5;

        if (dockedInside && py < selRect.top() + 2)
            py = selRect.top() + 2;
        if (dockedInside && py + ph > selRect.bottom() - 2)
            py = selRect.bottom() - ph - 2;
        if (py < 5)
            py = 5;
        if (py + ph > height() - 5)
            py = height() - ph - 5;

        if (toolbarRect.isValid()) {
            auto panelRectAt = [&](int y) {
                return QRect(px, y, pw, ph).adjusted(-4, -4, 4, 4);
            };
            auto inBounds = [&](int y) {
                if (dockedInside)
                    return y >= selRect.top() + 2 && y + ph <= selRect.bottom() - 2;
                return y >= 5 && y + ph <= height() - 5;
            };
            auto tryY = [&](int candidate, int &outY) {
                if (!inBounds(candidate))
                    return false;
                if (panelRectAt(candidate).intersects(toolbarRect.adjusted(-4, -4, 4, 4)))
                    return false;
                outY = candidate;
                return true;
            };

            int adjustedY = py;
            if (panelRectAt(py).intersects(toolbarRect.adjusted(-4, -4, 4, 4))) {
                if (!tryY(toolbarRect.top() - ph - margin, adjustedY)
                    && !tryY(toolbarRect.bottom() + margin, adjustedY)
                    && !tryY(selRect.top() + 2, adjustedY)
                    && !tryY(selRect.bottom() - ph - 2, adjustedY)) {
                    adjustedY = py;
                }
                py = adjustedY;
            }
        }

        m_actionPanel->move(px, py);
        m_actionPanel->show();
        m_actionPanel->raise();
    }

    updateToolSettingsDrawerPosition();
}

void CaptureOverlay::hideToolbar()
{
    if (m_toolbar) m_toolbar->hide();
    if (m_actionPanel) m_actionPanel->hide();
    if (m_toolSettingsButton) m_toolSettingsButton->hide();
    if (m_toolSettingsDrawer) m_toolSettingsDrawer->hide();
    if (m_textEditPanel) m_textEditPanel->hide();
}

void CaptureOverlay::finishCapture()
{
    QRect selRect = normalizedSelectionRect();
    QPixmap result = getSelectedPixmap();
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    hideToolbar();
    cancelTextEdit();

    if (m_captureMode == ModeRecording) {
        m_captureMode = ModeNormal;
        if (!selRect.isEmpty()) emit regionSelected(selRect);
        return;
    }

    // Play sound (if setting is enabled)
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
    hideToolbar();
    cancelTextEdit();
    emit captureCancelled();
}

void CaptureOverlay::beginTextEditAt(const QPoint &pos)
{
    if (!m_textEdit || !m_annotationEngine)
        return;

    {
        QSignalBlocker fontBlocker(m_textInlineFontCombo);
        QSignalBlocker sizeBlocker(m_textInlineSizeSpin);
        if (m_textInlineFontCombo)
            m_textInlineFontCombo->setCurrentFont(QFont(m_annotationEngine->textFontFamily()));
        if (m_textInlineSizeSpin)
            m_textInlineSizeSpin->setValue(m_annotationEngine->textFontSize());
    }

    m_textEdit->clear();
    m_textEdit->setFixedHeight(30);
    updateTextEditorStyle();
    moveTextEditorTo(pos);
    m_textEdit->show();
    if (m_textEditPanel)
        m_textEditPanel->show();
    updateTextEditPanelPosition();
    m_textEdit->setFocus();
}

void CaptureOverlay::moveTextEditorTo(const QPoint &pos)
{
    if (!m_textEdit)
        return;

    QRect selRect = normalizedSelectionRect();
    const int editWidth = qBound(80, selRect.width() - 8, 260);
    const int editHeight = qMax(30, m_textEdit->height());
    const int minX = selRect.left() + 4;
    const int maxX = qMax(minX, selRect.right() - editWidth - 4);
    const int minY = selRect.top() + 4;
    const int maxY = qMax(minY, selRect.bottom() - editHeight - 4);
    int x = qBound(minX, pos.x(), maxX);
    int y = qBound(minY, pos.y(), maxY);
    m_textEditPosition = QPoint(x, y);
    m_textEdit->setGeometry(x, y, editWidth, editHeight);
    updateTextEditPanelPosition();
}

void CaptureOverlay::updateTextEditorStyle()
{
    if (!m_textEdit || !m_annotationEngine)
        return;

    QFont textFont(m_annotationEngine->textFontFamily(), m_annotationEngine->textFontSize());
    textFont.setBold(true);
    m_textEdit->setFont(textFont);
    m_textEdit->setStyleSheet(QString(R"(
        QTextEdit {
            background-color: rgba(0, 0, 0, 180);
            color: %1;
            border: 2px solid #0078D4;
            border-radius: 4px;
            padding: 4px 8px;
        }
    )").arg(m_annotationEngine->color().name()));
}

void CaptureOverlay::updateTextEditPanelPosition()
{
    if (!m_textEditPanel || !m_textEdit || !m_textEdit->isVisible())
        return;

    m_textEditPanel->adjustSize();
    QRect selRect = normalizedSelectionRect();
    int x = m_textEdit->x();
    int y = m_textEdit->y() - m_textEditPanel->height() - 6;
    if (y < selRect.top() + 4)
        y = m_textEdit->geometry().bottom() + 6;
    if (x + m_textEditPanel->width() > selRect.right() - 4)
        x = selRect.right() - m_textEditPanel->width() - 4;
    if (x < selRect.left() + 4)
        x = selRect.left() + 4;
    if (y + m_textEditPanel->height() > selRect.bottom() - 4)
        y = qMax(selRect.top() + 4, m_textEdit->y() - m_textEditPanel->height() - 6);
    m_textEditPanel->move(x, y);
    m_textEditPanel->raise();
    m_textEdit->raise();
}

void CaptureOverlay::commitText()
{
    if (!m_textEdit || m_textEdit->isHidden()) return;
    QString text = m_textEdit->toPlainText().trimmed();
    if (!text.isEmpty() && m_annotationEngine) {
        QRect selRect = normalizedSelectionRect();
        m_textEditPosition = m_textEdit->pos();
        QPoint rel = m_textEditPosition - selRect.topLeft();
        m_annotationEngine->addTextAnnotation(rel, text);
        update();
        updateUndoRedoState();
    }
    m_textEdit->hide();
    if (m_textEditPanel) m_textEditPanel->hide();
    m_textJustCommitted = true;
    setFocus();
}

void CaptureOverlay::cancelTextEdit()
{
    if (m_textEdit) m_textEdit->hide();
    if (m_textEditPanel) m_textEditPanel->hide();
    setFocus();
}

void CaptureOverlay::updateUndoRedoState()
{
    if (m_toolbar) {
        m_toolbar->setUndoEnabled(m_annotationEngine && m_annotationEngine->canUndo());
        m_toolbar->setRedoEnabled(m_annotationEngine && m_annotationEngine->canRedo());
    }
}

bool CaptureOverlay::matchesOverlayShortcut(QKeyEvent *event, const QString &key, const QString &fallback) const
{
    if (!event || event->key() == Qt::Key_unknown)
        return false;
    const Qt::KeyboardModifiers relevantMods =
        event->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    QKeySequence pressed(static_cast<int>(relevantMods) | event->key());
    QSettings s("EShot", "EShot");
    const QString configured = s.value(QStringLiteral("overlayShortcut/%1").arg(key), fallback).toString().trimmed();
    if (configured.isEmpty())
        return false;
    const QKeySequence target(configured);
    return !target.isEmpty() && target.matches(pressed) == QKeySequence::ExactMatch;
}

void CaptureOverlay::selectAnnotationTool(int toolId)
{
    if (!m_annotationEngine)
        return;
    m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(toolId));
    m_annotationEngine->setSelectedIndex(-1);
    if (m_toolbar)
        m_toolbar->selectTool(toolId);
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
    if (m_toolbar)
        m_toolbar->setSelectionLocked(locked);
}

void CaptureOverlay::onBlurIntensityChanged(int intensity)
{
    if (m_annotationEngine) m_annotationEngine->setBlurIntensity(intensity);
    // Save to settings
    QSettings s("EShot", "EShot");
    s.setValue("blurIntensity", intensity);
}

void CaptureOverlay::onOcrRequested()
{
    QPixmap pix = getSelectedPixmap();
    if (pix.isNull()) return;
    QSettings s("EShot", "EShot");
    QString lang = s.value("ocrLanguage", "en-US").toString();
    hide();
    OcrDialog dlg(pix);
    dlg.setLanguageTag(lang);
    dlg.exec();
    show();
    if (m_selectionComplete) showToolbar();
}

void CaptureOverlay::onUploadRequested()
{
    QPixmap pix = getSelectedPixmap();
    if (pix.isNull()) return;
    hide();
    UploadDialog dlg;
    dlg.setImage(pix);
    dlg.exec();
    show();
    if (m_selectionComplete) showToolbar();
}

void CaptureOverlay::onGifRequested()
{
    QRect selRect = normalizedSelectionRect();
    if (selRect.isEmpty()) return;
    QRect screenRect = selRect.translated(m_virtualDesktopRect.topLeft());
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    if (m_toolbar) m_toolbar->hide();
    if (m_actionPanel) m_actionPanel->hide();
    emit gifCaptureRequested(screenRect);
}

void CaptureOverlay::onVideoRequested()
{
    QRect selRect = normalizedSelectionRect();
    if (selRect.isEmpty()) return;
    QRect screenRect = selRect.translated(m_virtualDesktopRect.topLeft());
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    hideToolbar();
    emit videoCaptureRequested(screenRect);
}

QRect CaptureOverlay::normalizedSelectionRect() const
{
    return QRect(m_selectionStart, m_selectionEnd).normalized();
}

QRect CaptureOverlay::monitorRectAt(const QPoint &pos) const
{
#ifdef Q_OS_WIN
    // pos is logical (widget-local); Win32 monitor APIs work in physical pixels.
    POINT nativePoint = {
        (LONG)qRound((pos.x() + m_virtualDesktopRect.x()) * m_dpr),
        (LONG)qRound((pos.y() + m_virtualDesktopRect.y()) * m_dpr)
    };
    HMONITOR monitor = MonitorFromPoint(nativePoint, MONITOR_DEFAULTTONULL);
    if (!monitor) return QRect();

    MONITORINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(monitor, &info)) return QRect();

    // rcMonitor is physical; return a logical, widget-local rect.
    return QRect(
        qRound(info.rcMonitor.left / m_dpr) - m_virtualDesktopRect.x(),
        qRound(info.rcMonitor.top / m_dpr) - m_virtualDesktopRect.y(),
        qRound((info.rcMonitor.right - info.rcMonitor.left) / m_dpr),
        qRound((info.rcMonitor.bottom - info.rcMonitor.top) / m_dpr)
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

    if (m_captureMode == ModeRecording) {
        finishCapture();
        return;
    }

    showToolbar();
    updateUndoRedoState();
    updateCursor(pos);

    update();
}

void CaptureOverlay::onToolSelected(int toolId)
{
    if (m_annotationEngine) {
        m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(toolId));
        m_annotationEngine->setSelectedIndex(-1);
    }
    m_isDraggingAnnotation = false;
    if (m_selectionComplete && m_toolbar && m_toolbar->isVisible())
        showToolbar();
}

void CaptureOverlay::onCopyToClipboard()
{
    QPixmap result = getSelectedPixmap();
    if (result.isNull()) return;
    QGuiApplication::clipboard()->setPixmap(result);

    // Close overlay if closeAfterCopy is enabled
    if (m_closeAfterCopy) {
        finishCapture();
    }
}

void CaptureOverlay::onSave()
{
    QPixmap result = getSelectedPixmap();
    if (result.isNull()) return;

    QSettings s("EShot", "EShot");
    QString cliFullPath = s.value("cliSaveFullPath").toString();
    s.remove("cliSaveFullPath");
    QString path = s.value("savePath", defaultSaveDirectory()).toString();
    if (path.trimmed().isEmpty())
        path = defaultSaveDirectory();
    QString format = s.value("imageFormat", "PNG").toString();
    int quality = s.value("imageQuality", 95).toInt();
    QString pattern = s.value("filenamePattern", "Screenshot_%Y-%M-%D_%h-%m-%s").toString();
    if (pattern.trimmed().isEmpty())
        pattern = QStringLiteral("Screenshot_%Y-%M-%D_%h-%m-%s");
    bool copyPathAfterSave = s.value("copyPathAfterSave", false).toBool();

    QString filename;
    if (!cliFullPath.isEmpty()) {
        filename = cliFullPath;
        QFileInfo fi(filename);
        if (!fi.absoluteDir().exists() && !QDir().mkpath(fi.absolutePath())) {
            QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::errSaveDir() + fi.absolutePath());
            return;
        }
    } else {
        QDir dir(path);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::errSaveDir() + path);
            return;
        }

        QString ext = format.toLower();
        if (ext == "jpeg") ext = "jpg";

        QString baseName = resolveFilenamePattern(pattern);
        filename = QString("%1/%2.%3").arg(path, baseName, ext);

        if (QFile::exists(filename)) {
            int counter = 1;
            while (QFile::exists(QString("%1/%2_%3.%4").arg(path, baseName, QString::number(counter), ext)))
                counter++;
            filename = QString("%1/%2_%3.%4").arg(path, baseName, QString::number(counter), ext);
        }
    }

    bool saved = false;
    if (format == "PNG") saved = result.save(filename, "PNG");
    else if (format == "JPEG") saved = result.save(filename, "JPEG", quality);
    else if (format == "BMP") saved = result.save(filename, "BMP");
    else saved = result.save(filename);

    if (saved) {
        qDebug() << "[CaptureOverlay] Saved:" << filename;
        emit captureSaved(filename);
        if (copyPathAfterSave) {
            QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(filename));
        }
    } else {
        qWarning() << "[CaptureOverlay] Save failed:" << filename;
        QMessageBox::warning(this, TranslationManager::errTitle(), QStringLiteral("Save failed:\n") + QDir::toNativeSeparators(filename));
        return;
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
                setCursor(Qt::CrossCursor); // For drawing
            break;
        default:
            setCursor(Qt::ArrowCursor);
            break;
    }
}
