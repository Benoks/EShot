#include "CaptureOverlay.h"
#include "CaptureInteractionPolicy.h"
#include "WindowSnapPolicy.h"
#include "WindowsWindowProvider.h"
#include "PinnedWindow.h"
#include "annotation/AnnotationEngine.h"
#include "annotation/AnnotationRotationGeometry.h"
#include "ui/AnnotationToolbar.h"
#include "ui/OcrDialog.h"
#include "ui/UploadDialog.h"
#include "core/ImageUploader.h"
#include "core/HotkeyManager.h"
#include "core/LinuxDesktopIntegration.h"
#include "core/LinuxPortalScreenshot.h"
#include "core/LinuxScreenshotPolicy.h"
#include "core/VisualSearch.h"
#include "recording/LinuxRecordingSupport.h"
#include "recording/RecordingSettingsPolicy.h"
#include "recording/RecordingDrawerPolicy.h"

#include "../core/TranslationManager.h"

#include <QPainter>
#include <QPainterPath>
#include <QLinearGradient>
#include <QScreen>
#include <QApplication>
#include <QGuiApplication>
#include <QCursor>
#include <QWindow>
#include <QClipboard>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDateTime>
#include <QSettings>
#include <QStandardPaths>
#include <functional>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
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
#include <QVariantAnimation>
#include <QAbstractAnimation>
#include <QEasingCurve>
#include <QDebug>
#include <QDir>
#include <QPointer>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QTemporaryFile>
#include <QProcess>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QHash>

#ifdef Q_OS_WIN
#include <windows.h>
#ifdef __MINGW32__
#include <initguid.h>
#endif
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <propsys.h>
#endif

namespace {
void drawCaptureHintShortcut(QPainter &painter, int &x, int y,
                             const QString &key, const QString &label,
                             bool compact)
{
    QFont keyFont = painter.font();
    keyFont.setPointSize(9);
    keyFont.setWeight(QFont::DemiBold);
    QFontMetrics keyMetrics(keyFont);
    const int keyWidth = keyMetrics.horizontalAdvance(key) + 14;
    const QRect keyRect(x, y, keyWidth, 24);

    painter.setPen(QPen(QColor(255, 255, 255, 55), 1));
    painter.setBrush(QColor(255, 255, 255, 24));
    painter.drawRoundedRect(keyRect, 5, 5);
    painter.setFont(keyFont);
    painter.setPen(QColor(246, 248, 251, 235));
    painter.drawText(keyRect, Qt::AlignCenter, key);
    x = keyRect.right() + 1;

    if (!compact) {
        QFont labelFont = painter.font();
        labelFont.setPointSize(9);
        labelFont.setWeight(QFont::Normal);
        painter.setFont(labelFont);
        painter.setPen(QColor(220, 224, 230, 220));
        const int labelWidth = QFontMetrics(labelFont).horizontalAdvance(label);
        painter.drawText(QRect(x + 7, y, labelWidth, 24), Qt::AlignVCenter, label);
        x += labelWidth + 7;
    }
}

void drawCaptureHints(QPainter &painter, const QRect &monitorRect, bool recordingMode)
{
    QFont primaryFont = painter.font();
    primaryFont.setPointSize(11);
    primaryFont.setWeight(QFont::DemiBold);
    const QString primaryText = QStringLiteral("%1  •  %2").arg(
        recordingMode ? TranslationManager::captureHintRecording()
                      : TranslationManager::captureHintDrag(),
        TranslationManager::captureHintScreen());
    const int primaryWidth = QFontMetrics(primaryFont).horizontalAdvance(primaryText) + 36;
    QFont secondaryFont = primaryFont;
    secondaryFont.setPointSize(9);
    secondaryFont.setWeight(QFont::Normal);
    const QString quickSettingsText = recordingMode
        ? QString()
        : TranslationManager::captureHintQuickSettings();
    const QString stopShortcut = HotkeyManager::instance().recordingStopShortcutText();
    const QString stopLabel = TranslationManager::recordingStop();
    const int secondaryWidth = quickSettingsText.isEmpty()
        ? 0 : QFontMetrics(secondaryFont).horizontalAdvance(quickSettingsText) + 36;
    const int stopHintWidth = recordingMode ? 0
        : QFontMetrics(secondaryFont).horizontalAdvance(stopShortcut + stopLabel) + 64;
    const int preferredWidth = recordingMode
        ? qBound(620, qMax(primaryWidth, secondaryWidth), 840)
        : qBound(440, qMax(qMax(primaryWidth, secondaryWidth), stopHintWidth), 840);
    const QRect hintRect = captureHintRect(
        monitorRect, QSize(preferredWidth, recordingMode ? 76 : 132));
    if (!hintRect.isValid())
        return;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(255, 255, 255, 42), 1));
    painter.setBrush(QColor(20, 20, 22, 178));
    painter.drawRoundedRect(hintRect, 10, 10);

    painter.setFont(primaryFont);
    painter.setPen(QColor(246, 248, 251, 238));
    const QRect primaryRect(hintRect.left() + 16, hintRect.top() + 8,
                            hintRect.width() - 32, 22);
    const QString visiblePrimary = QFontMetrics(primaryFont).elidedText(
        primaryText, Qt::ElideRight, primaryRect.width());
    painter.drawText(primaryRect, Qt::AlignCenter, visiblePrimary);

    const bool compact = hintRect.width() < 390;
    struct HintShortcut { QString key; QString label; };
    const QList<HintShortcut> shortcuts = recordingMode
        ? QList<HintShortcut>{
              {HotkeyManager::instance().recordingPauseShortcutText(),
               TranslationManager::recordingPauseResume()},
              {HotkeyManager::instance().recordingStopShortcutText(),
               TranslationManager::recordingStopShort()},
              {HotkeyManager::instance().recordingCancelShortcutText(),
               TranslationManager::recordingCancel()}}
        : QList<HintShortcut>{
              {QStringLiteral("Ctrl+C"), TranslationManager::captureHintCopy()},
              {QStringLiteral("Ctrl+S"), TranslationManager::captureHintSave()},
              {QStringLiteral("Esc"), TranslationManager::captureHintCancel()}};

    QFont keyFont = painter.font();
    keyFont.setPointSize(9);
    keyFont.setWeight(QFont::DemiBold);
    QFont labelFont = keyFont;
    labelFont.setWeight(QFont::Normal);
    int shortcutsWidth = 0;
    for (int i = 0; i < shortcuts.size(); ++i) {
        shortcutsWidth += QFontMetrics(keyFont).horizontalAdvance(shortcuts[i].key) + 14;
        if (!compact)
            shortcutsWidth += QFontMetrics(labelFont).horizontalAdvance(shortcuts[i].label) + 7;
        if (i + 1 < shortcuts.size())
            shortcutsWidth += compact ? 8 : 16;
    }

    int x = hintRect.left() + qMax(10, (hintRect.width() - shortcutsWidth) / 2);
    const int y = hintRect.top() + (recordingMode ? 42 : 35);
    for (int i = 0; i < shortcuts.size(); ++i) {
        drawCaptureHintShortcut(painter, x, y, shortcuts[i].key, shortcuts[i].label, compact);
        if (i + 1 < shortcuts.size())
            x += compact ? 8 : 16;
    }
    if (!quickSettingsText.isEmpty()) {
        painter.setFont(secondaryFont);
        painter.setPen(QColor(190, 196, 205, 205));
        const QRect secondaryRect(hintRect.left() + 16, hintRect.top() + 64,
                                  hintRect.width() - 32, 20);
        const QString visibleSecondary = QFontMetrics(secondaryFont).elidedText(
            quickSettingsText, Qt::ElideRight, secondaryRect.width());
        painter.drawText(secondaryRect, Qt::AlignCenter, visibleSecondary);

        QFont stopKeyFont = secondaryFont;
        stopKeyFont.setWeight(QFont::DemiBold);
        QFont stopLabelFont = secondaryFont;
        const int stopRowWidth = QFontMetrics(stopKeyFont).horizontalAdvance(stopShortcut) + 14
            + 7 + QFontMetrics(stopLabelFont).horizontalAdvance(stopLabel);
        int stopX = hintRect.left() + qMax(10, (hintRect.width() - stopRowWidth) / 2);
        drawCaptureHintShortcut(painter, stopX, hintRect.top() + 94,
                                stopShortcut, stopLabel, compact);
    }
    painter.restore();
}

#ifdef Q_OS_WIN
BOOL CALLBACK collectPhysicalMonitorGeometries(HMONITOR monitor, HDC, LPRECT, LPARAM data)
{
    auto *geometries = reinterpret_cast<QHash<QString, QRect> *>(data);
    MONITORINFOEXW info = {};
    info.cbSize = sizeof(info);
    if (GetMonitorInfoW(monitor, &info)) {
        const RECT &rect = info.rcMonitor;
        geometries->insert(QString::fromWCharArray(info.szDevice),
                           QRect(rect.left, rect.top,
                                 rect.right - rect.left,
                                 rect.bottom - rect.top));
    }
    return TRUE;
}
#endif

class SideTabButton : public QPushButton
{
public:
    explicit SideTabButton(const QString &text, QWidget *parent = nullptr)
        : QPushButton(text, parent)
    {
        setFixedWidth(28);
        fitToAvailableHeight(10000);
    }

    void fitToAvailableHeight(int availableHeight)
    {
        setFixedHeight(quickSettingsTabHeight(
            QFontMetrics(labelFont()).horizontalAdvance(text()), availableHeight));
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

        painter.setFont(labelFont());
        painter.setPen(QColor(245, 245, 245));
        painter.translate(width() / 2.0, height() / 2.0);
        painter.rotate(-90);
        painter.drawText(QRect(-height() / 2, -width() / 2, height(), width()),
                         Qt::AlignCenter,
                         text());
    }

private:
    QFont labelFont() const
    {
        QFont result = font();
        result.setPointSize(8);
        result.setBold(true);
        result.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
        return result;
    }
};

QString defaultSaveDirectory()
{
    QString picturesPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (picturesPath.trimmed().isEmpty())
        picturesPath = QDir::homePath();
    return QDir(picturesPath).filePath(QStringLiteral("EShot"));
}

QStringList dshowAudioDevices();

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
    QStringList candidates = {
        QDir(appDir).filePath(QStringLiteral("ffmpeg/ffmpeg.exe")),
        QDir(appDir).filePath(QStringLiteral("ffmpeg.exe")),
#ifndef Q_OS_WIN
        QDir(appDir).filePath(QStringLiteral("ffmpeg/ffmpeg")),
        QDir(appDir).filePath(QStringLiteral("ffmpeg")),
#endif
        QDir(appDir).filePath(QStringLiteral("../third_party/ffmpeg/bin/ffmpeg.exe")),
#ifndef Q_OS_WIN
        QDir(appDir).filePath(QStringLiteral("../third_party/ffmpeg/bin/ffmpeg")),
#endif
        QDir::current().filePath(QStringLiteral("third_party/ffmpeg/bin/ffmpeg.exe"))
    };
#ifndef Q_OS_WIN
    candidates << QDir::current().filePath(QStringLiteral("third_party/ffmpeg/bin/ffmpeg"));
#endif
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            return QFileInfo(path).absoluteFilePath();
    }
    return QStandardPaths::findExecutable(QStringLiteral("ffmpeg"));
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

QString defaultDesktopAudioDevice()
{
#ifdef Q_OS_WIN
    return QStringLiteral("__wasapi__");
#else
    return QStringLiteral("@DEFAULT_SINK@.monitor");
#endif
}

QStringList desktopAudioDevices()
{
#ifdef Q_OS_WIN
    return dshowAudioDevices();
#else
    return {QStringLiteral("@DEFAULT_SINK@.monitor")};
#endif
}

QList<QPair<QString, QString>> microphoneAudioDevices()
{
#ifdef Q_OS_WIN
    QList<QPair<QString, QString>> devices;
    for (const QString &name : windowsAudioInputDevices()) devices.append(qMakePair(name, name));
    return devices;
#else
    return discoverLinuxMicrophoneDevices();
#endif
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
    , m_instantCopyAfterSelection(false)
    , m_showCaptureHints(true)
    , m_resizeMode(ResNone)
    , m_foregroundHwnd(nullptr)
    , m_isDraggingAnnotation(false)
    , m_textJustCommitted(false)
    , m_textEditing(false)
    , m_eyedropperActive(false)
    , m_selectionLocked(false)
{
    if (qEnvironmentVariableIntValue("ESHOT_WAYLAND_XWAYLAND_OVERLAY") == 1)
        setWindowFlag(Qt::X11BypassWindowManagerHint, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setAttribute(Qt::WA_DeleteOnClose, false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);

#ifdef Q_OS_LINUX
    if (qEnvironmentVariableIntValue("ESHOT_WAYLAND_XWAYLAND_OVERLAY") == 1) {
        // The capture canvas stays unmanaged so it can span the XWayland
        // virtual desktop. A tiny managed editor receives real KWin keyboard
        // focus and mirrors its input into the visible editor over the canvas.
        m_textFocusProxy = new QTextEdit(nullptr);
        m_textFocusProxy->setWindowFlags(
            Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
        m_textFocusProxy->setAttribute(Qt::WA_DeleteOnClose, false);
        m_textFocusProxy->setWindowOpacity(0.01);
        m_textFocusProxy->setFixedSize(24, 24);
        m_textFocusProxy->setAcceptRichText(false);
        m_textFocusProxy->setFocusPolicy(Qt::StrongFocus);
        m_textFocusProxy->setWindowTitle(QStringLiteral("EShot Text Input"));
        m_textFocusProxy->hide();
        m_textFocusProxy->installEventFilter(this);
    }
#endif

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
    if (m_textFocusProxy) {
        connect(m_textFocusProxy, &QTextEdit::textChanged, this, [this]() {
            if (!m_textEdit || !m_textFocusProxy || !m_textFocusProxy->isVisible())
                return;
            m_textEdit->setPlainText(m_textFocusProxy->toPlainText());
            QTextCursor cursor = m_textEdit->textCursor();
            cursor.movePosition(QTextCursor::End);
            m_textEdit->setTextCursor(cursor);
        });
    }
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
            background-color: rgba(31, 32, 35, 245);
            border: 1px solid #56585d;
            border-radius: 8px;
        }
        QToolButton {
            background-color: #34363a;
            color: white;
            border: 1px solid #505258;
            border-radius: 6px;
        }
        QToolButton:hover { background-color: #45484e; border-color: #6f7279; }
        QToolButton:pressed { background-color: #25272a; }
        QToolButton#textCommitButton {
            background-color: #0878c9;
            border-color: #1593e6;
        }
        QToolButton#textCommitButton:hover { background-color: #0b8de8; }
        QFontComboBox, QSpinBox {
            background: #292b2f;
            color: white;
            border: 1px solid #55585f;
            border-radius: 6px;
            min-height: 0px;
        }
        QFontComboBox { padding: 0px 8px; }
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
            width: 28px;
            border-left: 1px solid #4b4d52;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: #34363a;
        }
        QFontComboBox::down-arrow {
            image: url(:/icons/chevron_down.svg);
            width: 14px;
            height: 14px;
        }
        QSpinBox {
            padding: 0px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border;
            width: 22px;
            background: #34363a;
            border-left: 1px solid #4b4d52;
        }
        QSpinBox::up-button {
            subcontrol-position: top right;
            border-top-right-radius: 6px;
        }
        QSpinBox::down-button {
            subcontrol-position: bottom right;
            border-bottom-right-radius: 6px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background: #3a3a3a;
        }
        QSpinBox::up-arrow {
            image: url(:/icons/chevron_up.svg);
            width: 11px;
            height: 11px;
        }
        QSpinBox::down-arrow {
            image: url(:/icons/chevron_down.svg);
            width: 11px;
            height: 11px;
        }
    )");
    QHBoxLayout *textPanelLayout = new QHBoxLayout(m_textEditPanel);
    textPanelLayout->setContentsMargins(6, 6, 6, 6);
    textPanelLayout->setSpacing(6);
    m_textMoveHandle = new QToolButton(m_textEditPanel);
    m_textMoveHandle->setIcon(QIcon(QStringLiteral(":/icons/drag.svg")));
    m_textMoveHandle->setIconSize(QSize(17, 17));
    m_textMoveHandle->setFixedSize(30, 30);
    m_textMoveHandle->setToolTip(TranslationManager::toolMove());
    m_textMoveHandle->setCursor(Qt::SizeAllCursor);
    m_textMoveHandle->setFocusPolicy(Qt::NoFocus);
    m_textMoveHandle->installEventFilter(this);
    m_textEditPanel->installEventFilter(this);
    m_textInlineFontCombo = new QFontComboBox(m_textEditPanel);
    m_textInlineFontCombo->setFixedSize(158, 32);
    m_textInlineSizeSpin = new QSpinBox(m_textEditPanel);
    m_textInlineSizeSpin->setRange(8, 72);
    m_textInlineSizeSpin->setAlignment(Qt::AlignCenter);
    m_textInlineSizeSpin->setFixedSize(68, 32);
    m_textCommitButton = new QToolButton(m_textEditPanel);
    m_textCommitButton->setObjectName(QStringLiteral("textCommitButton"));
    m_textCommitButton->setIcon(QIcon(QStringLiteral(":/icons/check.svg")));
    m_textCommitButton->setIconSize(QSize(18, 18));
    m_textCommitButton->setFixedSize(30, 30);
    m_textCommitButton->setToolTip(TranslationManager::save() + QStringLiteral(" (Enter)"));
    m_textCommitButton->setFocusPolicy(Qt::NoFocus);
    m_textCancelButton = new QToolButton(m_textEditPanel);
    m_textCancelButton->setIcon(QIcon(QStringLiteral(":/icons/close.svg")));
    m_textCancelButton->setIconSize(QSize(16, 16));
    m_textCancelButton->setFixedSize(30, 30);
    m_textCancelButton->setToolTip(TranslationManager::cancel() + QStringLiteral(" (Esc)"));
    m_textCancelButton->setFocusPolicy(Qt::NoFocus);
    textPanelLayout->addWidget(m_textMoveHandle);
    textPanelLayout->addWidget(m_textInlineFontCombo);
    textPanelLayout->addWidget(m_textInlineSizeSpin);
    textPanelLayout->addWidget(m_textCommitButton);
    textPanelLayout->addWidget(m_textCancelButton);
    m_textEditPanel->hide();
    connect(m_textCommitButton, &QToolButton::clicked, this, &CaptureOverlay::commitText);
    connect(m_textCancelButton, &QToolButton::clicked, this, &CaptureOverlay::cancelTextEdit);
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
    connect(m_toolbar, &AnnotationToolbar::modalDialogClosed,
            this, &CaptureOverlay::restoreAfterModalDialog);
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
    connect(m_toolbar, &AnnotationToolbar::googleLensRequested, this, &CaptureOverlay::onGoogleLensRequested);
    connect(m_toolbar, &AnnotationToolbar::gifRequested, this, &CaptureOverlay::onGifRequested);
    connect(m_toolbar, &AnnotationToolbar::videoRequested, this, &CaptureOverlay::onVideoRequested);
    connect(m_annotationEngine, &AnnotationEngine::annotationAdded, this, &CaptureOverlay::updateUndoRedoState);

    setupToolSettingsDrawer();
    setupRecordingDrawer();

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

    m_windowSnapAnimation = new QVariantAnimation(this);
    m_windowSnapAnimation->setDuration(windowSnapAnimationDurationMs());
    m_windowSnapAnimation->setStartValue(0.0);
    m_windowSnapAnimation->setEndValue(1.0);
    connect(m_windowSnapAnimation, &QVariantAnimation::valueChanged,
            this, [this](const QVariant &value) {
                m_animatedWindowRect = windowSnapTransitionRect(
                    m_windowSnapAnimationStart, m_hoveredWindowRect, value.toReal());
                update();
            });
    connect(m_windowSnapAnimation, &QVariantAnimation::finished, this, [this]() {
        m_animatedWindowRect = m_hoveredWindowRect;
        update();
    });


}

CaptureOverlay::~CaptureOverlay()
{
    delete m_textFocusProxy;
    m_textFocusProxy = nullptr;
    if (m_googleLensUploader) {
        m_googleLensUploader->disconnect(this);
        m_googleLensUploader->cancel();
    }
    if (!m_visualSearchImagePath.isEmpty())
        QFile::remove(m_visualSearchImagePath);
}

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
    fpsRow->addWidget(new QLabel(TranslationManager::gifFpsLabel().remove(QChar(':')), m_toolSettingsDrawer));
    m_quickGifFpsSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickGifFpsSpin->setRange(1, gifRecordingFpsLimit());
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
    videoFpsRow->addWidget(new QLabel(TranslationManager::videoFpsLabel().remove(QChar(':')), m_toolSettingsDrawer));
    m_quickVideoFpsSpin = new QSpinBox(m_toolSettingsDrawer);
    m_quickVideoFpsSpin->setRange(1, videoRecordingFpsLimit());
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

    const QStringList audioDevices = desktopAudioDevices();
    const auto activeInputDevices = microphoneAudioDevices();
    auto *desktopDeviceRow = new QHBoxLayout();
    auto *desktopDeviceLabel = new QLabel(TranslationManager::audioSource(), m_toolSettingsDrawer);
    desktopDeviceRow->addWidget(desktopDeviceLabel);
    m_quickDesktopAudioDeviceCombo = new QComboBox(m_toolSettingsDrawer);
    bool hasDesktopCandidate = true;
    m_quickDesktopAudioDeviceCombo->addItem(TranslationManager::audioSystemLoopback(), defaultDesktopAudioDevice());
#ifdef Q_OS_WIN
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
#else
    Q_UNUSED(hasDesktopCandidate);
#endif
    desktopDeviceRow->addWidget(m_quickDesktopAudioDeviceCombo);
    drawerLayout->addLayout(desktopDeviceRow);
    if (m_quickDesktopAudioCheck)
        m_quickDesktopAudioCheck->setEnabled(true);
    const bool desktopAudioEnabled = hasDesktopCandidate && m_quickDesktopAudioCheck
        && m_quickDesktopAudioCheck->isChecked();
    desktopDeviceLabel->setEnabled(desktopAudioEnabled);
    m_quickDesktopAudioDeviceCombo->setEnabled(desktopAudioEnabled);
    connect(m_quickDesktopAudioCheck, &QCheckBox::toggled,
            this, [this, hasDesktopCandidate, desktopDeviceLabel](bool enabled) {
                desktopDeviceLabel->setEnabled(hasDesktopCandidate && enabled);
                if (m_quickDesktopAudioDeviceCombo)
                    m_quickDesktopAudioDeviceCombo->setEnabled(hasDesktopCandidate && enabled);
            });

    auto *micDeviceRow = new QHBoxLayout();
    auto *microphoneDeviceLabel = new QLabel(TranslationManager::audioMicrophoneDevice(), m_toolSettingsDrawer);
    micDeviceRow->addWidget(microphoneDeviceLabel);
    m_quickMicrophoneDeviceCombo = new QComboBox(m_toolSettingsDrawer);
    if (activeInputDevices.isEmpty() && audioDevices.isEmpty()) {
        m_quickMicrophoneDeviceCombo->addItem(TranslationManager::tr("defaultAudioDevice"), QStringLiteral("default"));
    } else {
        m_quickMicrophoneDeviceCombo->addItem(TranslationManager::tr("defaultAudioDevice"), activeInputDevices.isEmpty() ? QStringLiteral("default") : activeInputDevices.first().second);
    }
    for (const auto &device : activeInputDevices) {
        if (m_quickMicrophoneDeviceCombo->findData(device.second) >= 0)
            continue;
        m_quickMicrophoneDeviceCombo->addItem(device.first, device.second);
    }
#ifdef Q_OS_WIN
    for (const QString &device : audioDevices) {
        if (m_quickMicrophoneDeviceCombo->findData(device) >= 0)
            continue;
        m_quickMicrophoneDeviceCombo->addItem(device, device);
    }
#endif
    micDeviceRow->addWidget(m_quickMicrophoneDeviceCombo);
    drawerLayout->addLayout(micDeviceRow);
    const bool microphoneEnabled = m_quickMicrophoneCheck && m_quickMicrophoneCheck->isChecked();
    microphoneDeviceLabel->setEnabled(microphoneEnabled);
    m_quickMicrophoneDeviceCombo->setEnabled(microphoneEnabled);
    connect(m_quickMicrophoneCheck, &QCheckBox::toggled,
            this, [this, microphoneDeviceLabel](bool enabled) {
                microphoneDeviceLabel->setEnabled(enabled);
                if (m_quickMicrophoneDeviceCombo)
                    m_quickMicrophoneDeviceCombo->setEnabled(enabled);
            });

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

void CaptureOverlay::setupRecordingDrawer()
{
    m_recordingDrawer = new QWidget(this);
    m_recordingDrawer->setObjectName(QStringLiteral("recordingDrawer"));
    m_recordingDrawer->setFixedWidth(264);
    m_recordingDrawer->setStyleSheet(R"(
        QWidget#recordingDrawer { background: rgba(37, 37, 37, 250); border: 1px solid rgba(255,255,255,55); border-radius: 10px; }
        QLabel { color: #d6d6d6; font-size: 11px; }
        QLabel#recordingDrawerTitle { color: #f5f5f5; font-size: 13px; font-weight: 700; }
        QSpinBox, QComboBox { min-height: 32px; background: #303030; border: 1px solid #505050; border-radius: 6px; color: #f2f2f2; }
        QComboBox { padding: 0 7px; }
        QSpinBox { padding: 0; }
        QSpinBox:hover, QComboBox:hover { border-color: #6a6a6a; background: #383838; }
        QSpinBox:focus, QComboBox:focus { border-color: #0078d4; }
        QComboBox::drop-down {
            subcontrol-origin: padding;
            subcontrol-position: top right;
            width: 28px;
            border-left: 1px solid #4b4d52;
            border-top-right-radius: 6px;
            border-bottom-right-radius: 6px;
            background: #34363a;
        }
        QComboBox::down-arrow {
            image: url(:/icons/chevron_down.svg);
            width: 14px;
            height: 14px;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            subcontrol-origin: border;
            width: 22px;
            background: #34363a;
            border-left: 1px solid #4b4d52;
        }
        QSpinBox::up-button { subcontrol-position: top right; border-top-right-radius: 6px; }
        QSpinBox::down-button { subcontrol-position: bottom right; border-bottom-right-radius: 6px; }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover { background: #3a3a3a; }
        QSpinBox::up-arrow { image: url(:/icons/chevron_up.svg); width: 11px; height: 11px; }
        QSpinBox::down-arrow { image: url(:/icons/chevron_down.svg); width: 11px; height: 11px; }
        QCheckBox { color: #e4e4e4; spacing: 7px; }
        QPushButton#recordingStartButton { min-height: 32px; background: #0078d4; border: 1px solid #2497eb; border-radius: 6px; color: white; font-weight: 700; }
        QPushButton#recordingStartButton:hover { background: #1686dc; }
        QPushButton#recordingCancelButton { min-height: 28px; background: transparent; border: 1px solid #505050; border-radius: 6px; color: #d6d6d6; }
    )");

    auto *layout = new QVBoxLayout(m_recordingDrawer);
    layout->setContentsMargins(13, 12, 13, 12);
    layout->setSpacing(9);
    m_recordingDrawerTitle = new QLabel(m_recordingDrawer);
    m_recordingDrawerTitle->setObjectName(QStringLiteral("recordingDrawerTitle"));
    layout->addWidget(m_recordingDrawerTitle);

    m_recordingGifOptions = new QWidget(m_recordingDrawer);
    auto *gifForm = new QFormLayout(m_recordingGifOptions);
    gifForm->setContentsMargins(0, 0, 0, 0);
    gifForm->setSpacing(7);
    m_recordingGifFpsSpin = new QSpinBox(m_recordingGifOptions);
    m_recordingGifFpsSpin->setRange(1, gifRecordingFpsLimit());
    m_recordingGifFpsSpin->setAlignment(Qt::AlignCenter);
    m_recordingGifSecondsSpin = new QSpinBox(m_recordingGifOptions);
    m_recordingGifSecondsSpin->setRange(0, 600);
    m_recordingGifSecondsSpin->setSpecialValueText(TranslationManager::recordingUnlimited());
    m_recordingGifSecondsSpin->setAlignment(Qt::AlignCenter);
    m_recordingGifLoopCombo = new QComboBox(m_recordingGifOptions);
    m_recordingGifLoopCombo->addItem(TranslationManager::recordingLoopInfinite(), 0);
    for (int loops : {1, 2, 3, 5, 10})
        m_recordingGifLoopCombo->addItem(QString::number(loops), loops);
    gifForm->addRow(TranslationManager::gifFpsLabel().remove(QChar(':')), m_recordingGifFpsSpin);
    gifForm->addRow(TranslationManager::quickMaxSeconds(), m_recordingGifSecondsSpin);
    gifForm->addRow(TranslationManager::recordingLoop().remove(QChar(':')), m_recordingGifLoopCombo);
    layout->addWidget(m_recordingGifOptions);

    m_recordingVideoOptions = new QWidget(m_recordingDrawer);
    auto *videoForm = new QFormLayout(m_recordingVideoOptions);
    videoForm->setContentsMargins(0, 0, 0, 0);
    videoForm->setSpacing(7);
    m_recordingVideoFpsSpin = new QSpinBox(m_recordingVideoOptions);
    m_recordingVideoFpsSpin->setRange(1, videoRecordingFpsLimit());
    m_recordingVideoFpsSpin->setAlignment(Qt::AlignCenter);
    m_recordingVideoSecondsSpin = new QSpinBox(m_recordingVideoOptions);
    m_recordingVideoSecondsSpin->setRange(0, 3600);
    m_recordingVideoSecondsSpin->setSpecialValueText(TranslationManager::recordingUnlimited());
    m_recordingVideoSecondsSpin->setAlignment(Qt::AlignCenter);
    m_recordingVideoCrfSpin = new QSpinBox(m_recordingVideoOptions);
    m_recordingVideoCrfSpin->setRange(18, 32);
    m_recordingVideoCrfSpin->setAlignment(Qt::AlignCenter);
    m_recordingDesktopAudioCheck = new QCheckBox(TranslationManager::audioDesktop(), m_recordingVideoOptions);
    m_recordingMicrophoneCheck = new QCheckBox(TranslationManager::audioMicrophone(), m_recordingVideoOptions);
    m_recordingMicrophoneDeviceCombo = new QComboBox(m_recordingVideoOptions);
    const auto inputDevices = microphoneAudioDevices();
    m_recordingMicrophoneDeviceCombo->addItem(
        TranslationManager::tr("defaultAudioDevice"),
        inputDevices.isEmpty() ? QStringLiteral("default") : inputDevices.first().second);
    for (const auto &device : inputDevices) {
        if (m_recordingMicrophoneDeviceCombo->findData(device.second) < 0)
            m_recordingMicrophoneDeviceCombo->addItem(device.first, device.second);
    }
#ifdef Q_OS_WIN
    for (const QString &device : desktopAudioDevices()) {
        if (m_recordingMicrophoneDeviceCombo->findData(device) < 0)
            m_recordingMicrophoneDeviceCombo->addItem(device, device);
    }
#endif
    videoForm->addRow(TranslationManager::videoFpsLabel().remove(QChar(':')), m_recordingVideoFpsSpin);
    videoForm->addRow(TranslationManager::quickMaxSeconds(), m_recordingVideoSecondsSpin);
    videoForm->addRow(TranslationManager::videoQualityCrf(), m_recordingVideoCrfSpin);
    videoForm->addRow(m_recordingDesktopAudioCheck);
    videoForm->addRow(m_recordingMicrophoneCheck);
    videoForm->addRow(TranslationManager::audioMicrophoneDevice(), m_recordingMicrophoneDeviceCombo);
    layout->addWidget(m_recordingVideoOptions);

    m_recordingStartButton = new QPushButton(m_recordingDrawer);
    m_recordingStartButton->setObjectName(QStringLiteral("recordingStartButton"));
    m_recordingCancelButton = new QPushButton(TranslationManager::recordingCancel(), m_recordingDrawer);
    m_recordingCancelButton->setObjectName(QStringLiteral("recordingCancelButton"));
    layout->addWidget(m_recordingStartButton);
    layout->addWidget(m_recordingCancelButton);

    auto persist = [](const char *key, int value) { QSettings("EShot", "EShot").setValue(key, value); };
    connect(m_recordingGifFpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [persist](int value) { persist("recordingFps", value); });
    connect(m_recordingGifSecondsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [persist](int value) { persist("recordingMaxSeconds", value); });
    connect(m_recordingGifLoopCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) { QSettings("EShot", "EShot").setValue("recordingLoop", m_recordingGifLoopCombo->currentData()); });
    connect(m_recordingVideoFpsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [persist](int value) { persist("videoRecordingFps", value); });
    connect(m_recordingVideoSecondsSpin, qOverload<int>(&QSpinBox::valueChanged), this, [persist](int value) { persist("videoRecordingMaxSeconds", value); });
    connect(m_recordingVideoCrfSpin, qOverload<int>(&QSpinBox::valueChanged), this, [persist](int value) { persist("videoRecordingCrf", value); });
    connect(m_recordingDesktopAudioCheck, &QCheckBox::toggled, this, [](bool enabled) { QSettings("EShot", "EShot").setValue("videoDesktopAudioEnabled", enabled); });
    connect(m_recordingMicrophoneCheck, &QCheckBox::toggled, this, [this](bool enabled) {
        QSettings("EShot", "EShot").setValue("videoMicrophoneEnabled", enabled);
        if (m_recordingMicrophoneDeviceCombo)
            m_recordingMicrophoneDeviceCombo->setEnabled(enabled);
    });
    connect(m_recordingMicrophoneDeviceCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, [this](int) {
        QSettings("EShot", "EShot").setValue(
            "videoMicrophoneDevice", m_recordingMicrophoneDeviceCombo->currentData().toString());
    });
    connect(m_recordingStartButton, &QPushButton::clicked, this, &CaptureOverlay::startRecordingFromDrawer);
    connect(m_recordingCancelButton, &QPushButton::clicked, this, [this] { hideRecordingDrawer(); });
    m_recordingDrawer->hide();
}

void CaptureOverlay::showRecordingDrawer(RecordingDrawerMode mode)
{
    if (!m_recordingDrawer || !m_selectionComplete)
        return;

    m_recordingDrawerMode = mode;
    const QStringList fields = recordingDrawerFields(mode);
    m_recordingGifOptions->setVisible(fields.contains(QStringLiteral("gifFps")));
    m_recordingVideoOptions->setVisible(fields.contains(QStringLiteral("videoFps")));
    const bool isGif = mode == RecordingDrawerMode::Gif;
    m_recordingDrawerTitle->setText(isGif ? TranslationManager::quickGifRecording() : TranslationManager::videoRecordingTitle());
    m_recordingStartButton->setText(TranslationManager::recordingStart());

    QSettings settings("EShot", "EShot");
    {
        QSignalBlocker gifFpsBlocker(m_recordingGifFpsSpin);
        QSignalBlocker gifTimeBlocker(m_recordingGifSecondsSpin);
        QSignalBlocker gifLoopBlocker(m_recordingGifLoopCombo);
        QSignalBlocker videoFpsBlocker(m_recordingVideoFpsSpin);
        QSignalBlocker videoTimeBlocker(m_recordingVideoSecondsSpin);
        QSignalBlocker videoCrfBlocker(m_recordingVideoCrfSpin);
        QSignalBlocker desktopAudioBlocker(m_recordingDesktopAudioCheck);
        QSignalBlocker microphoneBlocker(m_recordingMicrophoneCheck);
        QSignalBlocker microphoneDeviceBlocker(m_recordingMicrophoneDeviceCombo);
        m_recordingGifFpsSpin->setValue(settings.value("recordingFps", 10).toInt());
        m_recordingGifSecondsSpin->setValue(settings.value("recordingMaxSeconds", 30).toInt());
        int loopIndex = m_recordingGifLoopCombo->findData(settings.value("recordingLoop", 0).toInt());
        m_recordingGifLoopCombo->setCurrentIndex(loopIndex < 0 ? 0 : loopIndex);
        m_recordingVideoFpsSpin->setValue(settings.value("videoRecordingFps", 30).toInt());
        m_recordingVideoSecondsSpin->setValue(settings.value("videoRecordingMaxSeconds", 0).toInt());
        m_recordingVideoCrfSpin->setValue(settings.value("videoRecordingCrf", 24).toInt());
        m_recordingDesktopAudioCheck->setChecked(loadRecordingAudioEnabled(settings, RecordingAudioSource::Desktop));
        m_recordingMicrophoneCheck->setChecked(loadRecordingAudioEnabled(settings, RecordingAudioSource::Microphone));
        const int microphoneDeviceIndex = m_recordingMicrophoneDeviceCombo->findData(
            settings.value("videoMicrophoneDevice", "default").toString());
        m_recordingMicrophoneDeviceCombo->setCurrentIndex(microphoneDeviceIndex < 0 ? 0 : microphoneDeviceIndex);
        m_recordingMicrophoneDeviceCombo->setEnabled(m_recordingMicrophoneCheck->isChecked());
    }

    setToolSettingsDrawerVisible(false);
    if (m_toolbar)
        m_toolbar->hide();
    if (m_actionPanel)
        m_actionPanel->hide();
    if (m_toolSettingsButton)
        m_toolSettingsButton->hide();
    if (m_toolSettingsDrawer)
        m_toolSettingsDrawer->hide();
    m_recordingDrawer->adjustSize();
    QRect bounds = m_selectionAnchorScreenRect.intersected(rect());
    if (bounds.isEmpty())
        bounds = rect();
    bounds.adjust(12, 12, -12, -12);
    m_recordingDrawer->move(recordingDrawerPosition(bounds, m_recordingDrawer->size()));
    m_recordingDrawer->show();
    m_recordingDrawer->raise();
}

void CaptureOverlay::hideRecordingDrawer(bool restoreToolbar)
{
    if (m_recordingDrawer)
        m_recordingDrawer->hide();
    m_recordingDrawerMode = RecordingDrawerMode::None;
    if (restoreToolbar && m_selectionComplete) {
        showToolbar();
        updateUndoRedoState();
        update();
    }
}

void CaptureOverlay::startRecordingFromDrawer()
{
    const RecordingDrawerMode mode = m_recordingDrawerMode;
    if (mode == RecordingDrawerMode::None || !m_selectionComplete)
        return;
    const QRect captureRect = selectedCaptureRect();
    const QRect displayRect = selectedDisplayRect();
    hideRecordingDrawer(false);
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    hideToolbar();
    if (mode == RecordingDrawerMode::Gif)
        emit gifCaptureRequested(captureRect, displayRect);
    else
        emit videoCaptureRequested(captureRect, displayRect);
}

void CaptureOverlay::updateToolSettingsDrawerPosition()
{
    if (!m_toolSettingsButton || !m_toolSettingsDrawer || !m_selectionComplete)
        return;

    QRect selRect = normalizedSelectionRect();
    QRect monitorRect = m_selectionAnchorScreenRect.isValid()
        ? m_selectionAnchorScreenRect
        : monitorRectAt(selRect.center());
    if (monitorRect.isEmpty())
        monitorRect = rect();
    monitorRect = monitorRect.intersected(rect());
    if (monitorRect.isEmpty())
        monitorRect = rect();

    static_cast<SideTabButton *>(m_toolSettingsButton)
        ->fitToAvailableHeight(monitorRect.height());
    m_toolSettingsDrawer->adjustSize();
    const bool drawerOpen = m_toolSettingsDrawer->isVisible()
        && !m_toolSettingsDrawer->property("closing").toBool();
    int bx = drawerOpen
        ? monitorRect.left() + m_toolSettingsDrawer->width() - 1
        : monitorRect.left();
    const int buttonMaxX = qMax(monitorRect.left(), monitorRect.right() + 1 - m_toolSettingsButton->width());
    const int buttonMaxY = qMax(monitorRect.top() + 32, monitorRect.bottom() - m_toolSettingsButton->height() - 32);
    bx = qBound(monitorRect.left(), bx, buttonMaxX);
    int by = qBound(monitorRect.top() + 32,
                    selRect.center().y() - m_toolSettingsButton->height() / 2,
                    buttonMaxY);
    m_toolSettingsButton->move(bx, by);
    m_toolSettingsButton->show();
    m_toolSettingsButton->raise();

    if (!m_toolSettingsDrawer->isVisible())
        return;

    if (m_toolSettingsAnimation && m_toolSettingsAnimation->state() == QAbstractAnimation::Running)
        return;

    int dx = monitorRect.left();
    int dy = m_toolSettingsButton->y() - 26;
    const int drawerMaxY = qMax(monitorRect.top() + 5, monitorRect.bottom() + 1 - m_toolSettingsDrawer->height() - 5);
    dy = qBound(monitorRect.top() + 5, dy, drawerMaxY);
    m_toolSettingsDrawer->move(dx, dy);
    m_toolSettingsDrawer->raise();
    m_toolSettingsButton->raise();
}

void CaptureOverlay::setToolSettingsDrawerVisible(bool visible)
{
    if (!m_toolSettingsDrawer || !m_toolSettingsButton)
        return;

    QRect selRect = normalizedSelectionRect();
    QRect monitorRect = m_selectionAnchorScreenRect.isValid()
        ? m_selectionAnchorScreenRect
        : monitorRectAt(selRect.center());
    if (monitorRect.isEmpty())
        monitorRect = rect();
    monitorRect = monitorRect.intersected(rect());
    if (monitorRect.isEmpty())
        monitorRect = rect();

    static_cast<SideTabButton *>(m_toolSettingsButton)
        ->fitToAvailableHeight(monitorRect.height());
    m_toolSettingsDrawer->adjustSize();
    const int drawerW = m_toolSettingsDrawer->width();
    const int drawerH = m_toolSettingsDrawer->height();
    const int drawerVisibleX = monitorRect.left();
    const int drawerHiddenX = monitorRect.left() - drawerW;
    const int buttonMaxX = qMax(monitorRect.left(), monitorRect.right() + 1 - m_toolSettingsButton->width());
    const int buttonMaxY = qMax(monitorRect.top() + 32, monitorRect.bottom() - m_toolSettingsButton->height() - 32);
    const int drawerMaxY = qMax(monitorRect.top() + 5, monitorRect.bottom() + 1 - drawerH - 5);
    const int buttonClosedX = qBound(monitorRect.left(), monitorRect.left(), buttonMaxX);
    const int buttonOpenX = qBound(monitorRect.left(), monitorRect.left() + drawerW - 1, buttonMaxX);
    const int drawerY = qBound(monitorRect.top() + 5,
                               m_toolSettingsButton->y() - 26,
                               drawerMaxY);
    const int buttonY = qBound(monitorRect.top() + 32,
                               m_toolSettingsButton->y(),
                               buttonMaxY);

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
            m_quickDesktopAudioCheck->setChecked(loadRecordingAudioEnabled(
                s, RecordingAudioSource::Desktop));
        if (m_quickDesktopVolumeSlider) {
            QSignalBlocker blocker(m_quickDesktopVolumeSlider);
            const int value = s.value("videoDesktopAudioVolume", 80).toInt();
            m_quickDesktopVolumeSlider->setValue(value);
            if (m_quickDesktopVolumeLabel)
                m_quickDesktopVolumeLabel->setText(QString::number(value));
        }
        if (m_quickDesktopAudioDeviceCombo) {
            QSignalBlocker blocker(m_quickDesktopAudioDeviceCombo);
            int idx = m_quickDesktopAudioDeviceCombo->findData(s.value("videoDesktopAudioDevice", defaultDesktopAudioDevice()).toString());
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
            m_quickMicrophoneCheck->setChecked(loadRecordingAudioEnabled(
                s, RecordingAudioSource::Microphone));
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
    if (m_recordingDrawer && m_recordingDrawer->isVisible()
        && m_recordingDrawer->geometry().contains(pos))
        return true;
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
    startCaptureInternal(CaptureSelectionMode::FreeRegion, false);
}

void CaptureOverlay::startWindowCapture()
{
    startCaptureInternal(CaptureSelectionMode::Window, false);
}

void CaptureOverlay::startCaptureInternal(CaptureSelectionMode selectionMode, bool recordingMode)
{
    m_captureLatencyTimer.start();
    m_logNextCapturePaint = true;
    if (m_captureDelayTimer) m_captureDelayTimer->stop();
    m_captureMode = recordingMode ? ModeRecording : ModeNormal;
    m_selectionMode = selectionMode;
    m_isSelecting = false;
    m_selectionComplete = false;
    m_ignoreNextMouseRelease = false;
    m_resizeMode = ResNone;
    m_selectionLocked = false;
    m_selectionStart = QPoint();
    m_selectionEnd = QPoint();
    m_selectionAnchorScreenRect = QRect();
    m_windowSnapCandidates.clear();
    setHoveredWindowRect(QRect());
    m_pressedWindowRect = QRect();
    m_windowSnapClickPending = false;
    m_eyedropperActive = false;

    if (m_textEdit) m_textEdit->hide();
    if (m_textEditPanel) m_textEditPanel->hide();
    if (m_textFocusProxy) m_textFocusProxy->hide();
    m_textJustCommitted = false;
    if (m_annotationEngine) m_annotationEngine->clear();
    hideToolbar();

    // Save foreground window before overlay (for %T filename variable)
#ifdef Q_OS_WIN
    m_foregroundHwnd = GetForegroundWindow();
#else
    m_foregroundHwnd = nullptr;
#endif

    // Load settings
    QSettings s("EShot", "EShot");
    const int initialTool = initialAnnotationTool(
        s.value("rememberLastAnnotationTool", false).toBool(),
        s.value("lastAnnotationTool", AnnotationEngine::None).toInt(),
        AnnotationEngine::None);
    if (m_toolbar) {
        m_toolbar->refreshTools();
        m_toolbar->setSelectionLocked(false);
        m_toolbar->selectTool(initialTool);
    }
    if (m_annotationEngine) {
        m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(initialTool));
        m_annotationEngine->setSelectedIndex(-1);
    }

    m_overlayOpacity = s.value("overlayOpacity", 100).toInt();
    m_crosshairStyle = s.value("crosshairStyle", "dash").toString();
    m_copyAfterCapture = s.value("copyAfterCapture", true).toBool();
    m_closeAfterCopy = s.value("closeAfterCopy", true).toBool();
    m_instantCopyAfterSelection = s.value("instantCopyAfterSelection", false).toBool();
    m_showCaptureHints = s.value("showCaptureHints", true).toBool();

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

void CaptureOverlay::startInstantCapture()
{
    startCapture();
    m_instantCopyAfterSelection = true;
}

void CaptureOverlay::startCaptureForRecording()
{
    startCaptureInternal(CaptureSelectionMode::FreeRegion, true);
    setCursor(Qt::CrossCursor);
}

void CaptureOverlay::performCapture()
{
    captureAllScreens();
    if (!LinuxScreenshotPolicy::canPresentCapture(!m_screenSnapshot.isNull())) {
        qCritical() << "[CaptureOverlay] capture aborted: no usable cursor-free snapshot";
        cancelCapture();
        return;
    }

    // Pass screenshot to annotation engine (for blur effect)
    if (m_annotationEngine) {
        m_annotationEngine->setScreenSnapshot(m_screenSnapshot);
        m_annotationEngine->setSnapshotScale(m_dpr);
    }

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
    const bool isWayland = QGuiApplication::platformName().contains(
        QStringLiteral("wayland"), Qt::CaseInsensitive);
    if (isWayland && m_captureScreen) {
        winId();
        if (QWindow *window = windowHandle())
            window->setScreen(m_captureScreen);
        setGeometry(m_captureScreen->geometry());
        showFullScreen();
    } else {
        setGeometry(m_virtualDesktopRect);
        show();
    }
#else
    setGeometry(m_virtualDesktopRect);
    show();
#endif

#ifdef Q_OS_WIN
    // Force the overlay to the very top of the z-order, even above elevated windows.
    // This works because EShot now runs with administrator privileges (see EShot.manifest).
    HWND hwnd = reinterpret_cast<HWND>(winId());
    ::SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    // Bring it to the very front and give it keyboard focus
    ::SetForegroundWindow(hwnd);
    ::BringWindowToTop(hwnd);

    if (m_selectionMode == CaptureSelectionMode::Window) {
        m_windowSnapCandidates = windowsForCaptureOverlay(
            reinterpret_cast<quintptr>(hwnd), m_captureMonitors, m_virtualDesktopRect);
    }
    setHoveredWindowRect(windowSnapTargetForMode(
        m_selectionMode, m_windowSnapCandidates, mapFromGlobal(QCursor::pos()), rect()));
#endif

    activateWindow();
    setFocus(Qt::ActiveWindowFocusReason);
    raise();
}

void CaptureOverlay::captureAllScreens()
{
    m_captureScreen = nullptr;
    m_screenSnapshot = QPixmap();
#ifdef Q_OS_WIN
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    m_physicalVirtualDesktopTopLeft = QPoint(vx, vy);

    QScreen *primary = QGuiApplication::primaryScreen();
    m_dpr = primary ? primary->devicePixelRatio() : 1.0;

    QHash<QString, QRect> physicalByName;
    EnumDisplayMonitors(nullptr, nullptr, collectPhysicalMonitorGeometries,
                        reinterpret_cast<LPARAM>(&physicalByName));
    m_captureMonitors.clear();
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect logical = screen->geometry();
        const qreal scale = screen->devicePixelRatio();
        QRect physical = physicalByName.value(screen->name());
        if (!physical.isValid()) {
            physical = QRect(logical.topLeft(),
                             QSize(qRound(logical.width() * scale),
                                   qRound(logical.height() * scale)));
        }
        m_captureMonitors.append({logical, physical, scale});
    }

    if (vw <= 0 || vh <= 0) {
        if (primary) {
            m_dpr = primary->devicePixelRatio();
            m_virtualDesktopRect = primary->geometry();   // logical
            m_physicalVirtualDesktopTopLeft = QPoint(
                qRound(m_virtualDesktopRect.x() * m_dpr),
                qRound(m_virtualDesktopRect.y() * m_dpr));
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
    const QString currentDesktop = qEnvironmentVariable("XDG_CURRENT_DESKTOP");
    const QString sessionDesktop = qEnvironmentVariable("XDG_SESSION_DESKTOP");
    const QString sessionType = qEnvironmentVariable("XDG_SESSION_TYPE");
    const LinuxDesktopEnvironment desktop = LinuxDesktopIntegration::detect(
        currentDesktop, sessionDesktop);
    const bool kdeWaylandSession = LinuxScreenshotPolicy::isKdeWaylandSession(
        currentDesktop, sessionDesktop, sessionType);
    const bool useXwaylandVirtualOverlay =
        qEnvironmentVariableIntValue("ESHOT_WAYLAND_XWAYLAND_OVERLAY") == 1
        && QGuiApplication::platformName().contains(QStringLiteral("xcb"), Qt::CaseInsensitive);
    if (useXwaylandVirtualOverlay) {
        QRect logicalRect;
        QRect physicalRect;
        m_captureMonitors.clear();
        for (QScreen *screen : QGuiApplication::screens()) {
            const QRect logical = screen->geometry();
            const qreal scale = screen->devicePixelRatio();
            const QRect physical = physicalRectFromLogical(logical, scale);
            logicalRect = logicalRect.united(logical);
            physicalRect = physicalRect.united(physical);
            m_captureMonitors.append({logical, physical, scale});
        }

        if (desktop != LinuxDesktopEnvironment::Kde) {
            QPixmap portalSnapshot = LinuxPortalScreenshot::grab(this);
            if (!portalSnapshot.isNull()) {
                portalSnapshot.setDevicePixelRatio(1.0);
                m_screenSnapshot = portalSnapshot;
                m_virtualDesktopRect = logicalRect;
                m_physicalVirtualDesktopTopLeft = physicalRect.topLeft();
                m_dpr = logicalRect.width() > 0
                    ? portalSnapshot.width() / static_cast<qreal>(logicalRect.width())
                    : 1.0;
                qInfo() << "[CaptureOverlay] selected capture backend=portal-workspace desktop="
                        << LinuxDesktopIntegration::displayName(desktop);
            } else {
                qCritical() << "[CaptureOverlay] portal workspace capture failed for"
                            << LinuxDesktopIntegration::displayName(desktop);
            }
            return;
        }

        QPixmap workspaceSnapshot = LinuxPortalScreenshot::grabWorkspace(this);
        if (!workspaceSnapshot.isNull()) {
            workspaceSnapshot.setDevicePixelRatio(1.0);
            m_screenSnapshot = workspaceSnapshot;
            m_virtualDesktopRect = logicalRect;
            m_physicalVirtualDesktopTopLeft = physicalRect.topLeft();
            m_dpr = logicalRect.width() > 0
                ? workspaceSnapshot.width() / static_cast<qreal>(logicalRect.width()) : 1.0;
            return;
        }

        QPixmap spectacleSnapshot = LinuxPortalScreenshot::grabSpectacleWorkspace(this);
        if (!spectacleSnapshot.isNull()) {
            spectacleSnapshot.setDevicePixelRatio(1.0);
            m_screenSnapshot = spectacleSnapshot;
            m_virtualDesktopRect = logicalRect;
            m_physicalVirtualDesktopTopLeft = physicalRect.topLeft();
            m_dpr = logicalRect.width() > 0
                ? spectacleSnapshot.width() / static_cast<qreal>(logicalRect.width()) : 1.0;
            qInfo() << "[CaptureOverlay] selected capture backend=spectacle-workspace";
            return;
        }

        if (!LinuxScreenshotPolicy::allowCursorBearingFallback(kdeWaylandSession)) {
            m_screenSnapshot = QPixmap();
            qCritical() << "[CaptureOverlay] no cursor-free screenshot backend is available;"
                           " refusing KDE Wayland portal/QScreen fallback";
            return;
        }

        QPixmap kwinSnapshot(physicalRect.size());
        kwinSnapshot.setDevicePixelRatio(1.0);
        kwinSnapshot.fill(Qt::black);
        QPainter kwinPainter(&kwinSnapshot);
        bool capturedAllScreens = !m_captureMonitors.isEmpty();
        for (QScreen *screen : QGuiApplication::screens()) {
            QPixmap screenGrab = LinuxPortalScreenshot::grabScreen(screen, this);
            if (screenGrab.isNull()) {
                capturedAllScreens = false;
                break;
            }
            const QRect destination = physicalRectFromLogical(screen->geometry(), screen->devicePixelRatio())
                                          .translated(-physicalRect.topLeft());
            kwinPainter.drawPixmap(destination, screenGrab, screenGrab.rect());
        }
        kwinPainter.end();
        if (capturedAllScreens) {
            m_screenSnapshot = kwinSnapshot;
            m_virtualDesktopRect = logicalRect;
            m_physicalVirtualDesktopTopLeft = physicalRect.topLeft();
            m_dpr = logicalRect.width() > 0
                ? kwinSnapshot.width() / static_cast<qreal>(logicalRect.width())
                : 1.0;
            return;
        }

        QPixmap portalSnapshot = LinuxPortalScreenshot::grab(this);
        if (!portalSnapshot.isNull()) {
            portalSnapshot.setDevicePixelRatio(1.0);
            m_screenSnapshot = portalSnapshot;
            m_virtualDesktopRect = logicalRect;
            m_physicalVirtualDesktopTopLeft = physicalRect.topLeft();
            m_dpr = logicalRect.width() > 0
                ? portalSnapshot.width() / static_cast<qreal>(logicalRect.width())
                : 1.0;
            return;
        }
    }

    const bool isWayland = QGuiApplication::platformName().contains(QStringLiteral("wayland"), Qt::CaseInsensitive);
    if (isWayland) {
        QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
        if (!screen)
            screen = QGuiApplication::primaryScreen();
        if (!screen)
            return;
        m_captureScreen = screen;

        const QRect logical = screen->geometry();
        const qreal scale = screen->devicePixelRatio();
        const QRect physical = physicalRectFromLogical(logical, scale);
        m_dpr = scale;
        m_virtualDesktopRect = logical;
        m_physicalVirtualDesktopTopLeft = physical.topLeft();
        m_captureMonitors = {{logical, physical, scale}};

        if (desktop != LinuxDesktopEnvironment::Kde) {
            QPixmap portalSnapshot = LinuxPortalScreenshot::grab(this);
            if (!portalSnapshot.isNull()) {
                QRect virtualPhysical;
                for (QScreen *candidate : QGuiApplication::screens()) {
                    virtualPhysical = virtualPhysical.united(
                        physicalRectFromLogical(candidate->geometry(),
                                                candidate->devicePixelRatio()));
                }
                const QRect crop = portalCropRect(physical, virtualPhysical);
                if (portalSnapshot.rect().contains(crop))
                    portalSnapshot = portalSnapshot.copy(crop);
                portalSnapshot.setDevicePixelRatio(1.0);
                m_screenSnapshot = portalSnapshot;
                qInfo() << "[CaptureOverlay] selected capture backend=portal-screen desktop="
                        << LinuxDesktopIntegration::displayName(desktop);
            }
            return;
        }

        m_screenSnapshot = LinuxPortalScreenshot::grabScreen(screen, this);
        m_screenSnapshot.setDevicePixelRatio(1.0);
        if (!m_screenSnapshot.isNull()) {
            qInfo() << "[CaptureOverlay] selected capture backend=kwin-screen";
            return;
        }

        QPixmap spectacleSnapshot = LinuxPortalScreenshot::grabSpectacleWorkspace(this);
        if (!spectacleSnapshot.isNull()) {
            QRect virtualPhysical;
            for (QScreen *candidate : QGuiApplication::screens())
                virtualPhysical = virtualPhysical.united(
                    physicalRectFromLogical(candidate->geometry(), candidate->devicePixelRatio()));
            const QRect crop = portalCropRect(physical, virtualPhysical);
            if (spectacleSnapshot.rect().contains(crop))
                spectacleSnapshot = spectacleSnapshot.copy(crop);
            spectacleSnapshot.setDevicePixelRatio(1.0);
            m_screenSnapshot = spectacleSnapshot;
            qInfo() << "[CaptureOverlay] selected capture backend=spectacle-screen-crop";
            return;
        }

        if (!LinuxScreenshotPolicy::allowCursorBearingFallback(kdeWaylandSession)) {
            m_screenSnapshot = QPixmap();
            qCritical() << "[CaptureOverlay] no cursor-free screenshot backend is available;"
                           " refusing KDE Wayland portal/QScreen fallback";
            return;
        }

        QPixmap portalSnapshot = LinuxPortalScreenshot::grab(this);
        if (!portalSnapshot.isNull()) {
            QRect virtualPhysical;
            for (QScreen *candidate : QGuiApplication::screens())
                virtualPhysical = virtualPhysical.united(
                    physicalRectFromLogical(candidate->geometry(), candidate->devicePixelRatio()));
            const QRect crop = portalCropRect(physical, virtualPhysical);
            if (portalSnapshot.rect().contains(crop))
                portalSnapshot = portalSnapshot.copy(crop);
            portalSnapshot.setDevicePixelRatio(1.0);
            m_screenSnapshot = portalSnapshot;
        }
        return;
    }

    QRect logicalRect;
    QRect physicalRect;
    auto screens = QGuiApplication::screens();
    m_dpr = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->devicePixelRatio() : 1.0;
    for (auto *s : screens) {
        auto g = s->geometry();
        const qreal d = s->devicePixelRatio();
        logicalRect = logicalRect.united(g);
        physicalRect = physicalRect.united(QRect(qRound(g.x() * d),
                                                 qRound(g.y() * d),
                                                 qRound(g.width() * d),
                                                 qRound(g.height() * d)));
    }
    m_virtualDesktopRect = logicalRect;
    m_physicalVirtualDesktopTopLeft = physicalRect.topLeft();
    m_screenSnapshot = QPixmap(physicalRect.size());
    m_screenSnapshot.setDevicePixelRatio(1.0);
    m_screenSnapshot.fill(Qt::black);
    QPainter p(&m_screenSnapshot);
    bool grabbedAnyScreen = false;
    for (auto *s : screens) {
        auto grab = s->grabWindow(0);
        if (grab.isNull())
            continue;
        grabbedAnyScreen = true;
        auto g = s->geometry();
        const qreal d = s->devicePixelRatio();
        p.drawPixmap(qRound(g.x() * d) - physicalRect.x(),
                     qRound(g.y() * d) - physicalRect.y(),
                     grab);
    }
    p.end();
    if (!grabbedAnyScreen || m_screenSnapshot.isNull()) {
        QPixmap portalSnapshot = LinuxPortalScreenshot::grab(this);
        if (!portalSnapshot.isNull()) {
            portalSnapshot.setDevicePixelRatio(1.0);
            m_screenSnapshot = portalSnapshot;
            m_virtualDesktopRect = QRect(QPoint(0, 0), portalSnapshot.size());
            m_physicalVirtualDesktopTopLeft = QPoint(0, 0);
            m_dpr = 1.0;
        }
    }
#endif
}

void CaptureOverlay::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    painter.drawPixmap(0, 0, width(), height(), m_screenSnapshot);
    // Draw once beneath the dim layer so annotations outside the selection
    // remain visible with the same darkening as the desktop background.
    if (m_annotationEngine && m_selectionComplete)
        m_annotationEngine->render(&painter, QPoint());
    painter.fillRect(rect(), QColor(0, 0, 0, m_overlayOpacity));

    const bool windowPreview = !m_isSelecting && !m_selectionComplete
        && !m_hoveredWindowRect.isEmpty();
    QRect selRect = windowPreview
        ? (m_animatedWindowRect.isValid() ? m_animatedWindowRect : m_hoveredWindowRect)
        : normalizedSelectionRect();

    if (!selRect.isEmpty()) {
        // Clean area
        painter.save();
        painter.setClipRect(selRect);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        // Keep the preview aligned to the full canvas. Cropping and rescaling
        // the selection itself causes subpixel shimmer at fractional DPI.
        painter.drawPixmap(rect(), m_screenSnapshot, m_screenSnapshot.rect());
        painter.restore();

        // Annotation
        if (m_annotationEngine && m_selectionComplete) {
            painter.setClipRect(selRect);
            m_annotationEngine->render(&painter, QPoint());
            if (m_annotationEngine->selectedIndex() >= 0) {
                const int selectedIndex = m_annotationEngine->selectedIndex();
                QRect annotationRect = m_annotationEngine->boundingRectOf(selectedIndex)
                                           .adjusted(-4, -4, 4, 4);
                if (!annotationRect.isEmpty()) {
                    QPen selectPen(QColor(179, 102, 255, 235), 1, Qt::DashLine);
                    selectPen.setCosmetic(true);
                    painter.setPen(selectPen);
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRoundedRect(annotationRect.adjusted(0, 0, -1, -1), 3, 3);

                    if (m_annotationEngine->isRotatable(selectedIndex)) {
                        const QPointF handleCenter = AnnotationRotationGeometry::handleCenter(annotationRect);
                        const QPointF connectorStart(annotationRect.center().x(), annotationRect.bottom());
                        painter.drawLine(connectorStart, handleCenter);
                        const QRectF handleRect(handleCenter.x() - 12, handleCenter.y() - 12, 24, 24);
                        painter.setBrush(QColor(250, 250, 252));
                        painter.setPen(QPen(QColor(179, 102, 255), 1));
                        painter.drawEllipse(handleRect);
                        QFont handleFont = painter.font();
                        handleFont.setPointSize(13);
                        handleFont.setBold(true);
                        painter.setFont(handleFont);
                        painter.setPen(QColor(58, 34, 84));
                        painter.drawText(handleRect, Qt::AlignCenter, QString::fromUtf8("↻"));
                    }
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

        if (!windowPreview) {
            drawHandle(selRect.topLeft());
            drawHandle(selRect.topRight());
            drawHandle(selRect.bottomLeft());
            drawHandle(selRect.bottomRight());
        }

        // Size info — always visible (top-left of selection)
        if (m_isSelecting || m_selectionComplete) {
            const QSize pixelSize = logicalToSnapshot(selRect).size();
            QString dim = QString("%1 x %2").arg(pixelSize.width()).arg(pixelSize.height());
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
    if (!m_isSelecting && !m_selectionComplete && m_hoveredWindowRect.isEmpty()
        && m_crosshairStyle != "none" && !m_eyedropperActive) {
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

    if (shouldShowCaptureHints(m_showCaptureHints, m_isSelecting,
                               m_selectionComplete, m_eyedropperActive)) {
        const QPoint cursorPos = mapFromGlobal(QCursor::pos());
        drawCaptureHints(painter, monitorRectAt(cursorPos), m_captureMode == ModeRecording);
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

    if (m_logNextCapturePaint) {
        m_logNextCapturePaint = false;
        qInfo() << "[CaptureOverlay] first frame painted after"
                << m_captureLatencyTimer.elapsed() << "ms";
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

        if (m_selectionComplete && m_annotationEngine
            && m_annotationEngine->selectedIndex() >= 0) {
            const int selectedIndex = m_annotationEngine->selectedIndex();
            if (m_annotationEngine->isRotatable(selectedIndex)) {
                const QRect annotationRect = m_annotationEngine->boundingRectOf(selectedIndex)
                                                 .adjusted(-4, -4, 4, 4);
                const QPointF handleCenter = AnnotationRotationGeometry::handleCenter(annotationRect);
                if (QLineF(event->pos(), handleCenter).length() <= 14.0) {
                    m_isRotatingAnnotation = true;
                    m_rotationCenter = m_annotationEngine->rotatedBoundingRectOf(selectedIndex).center();
                    m_rotationDragStartAngle = AnnotationRotationGeometry::angleAt(m_rotationCenter, event->pos());
                    m_rotationStartDegrees = m_annotationEngine->rotationDegreesOf(selectedIndex);
                    setCursor(Qt::CrossCursor);
                    event->accept();
                    return;
                }
            }
        }

        if (m_selectionComplete && !m_selectionLocked) {
            QRect selRect = normalizedSelectionRect();

            ResizeMode mode = getResizeMode(event->pos());
            const bool selectionHandleHit = mode != ResNone && mode != ResMove && mode != ResNewSelection;
            if (m_annotationEngine && shouldReleaseToolForResize(selectionHandleHit,
                    m_annotationEngine->currentTool(), AnnotationEngine::None)) {
                selectAnnotationTool(AnnotationEngine::None);
            }
            
            bool isDrawingTool = (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None);
            if (isDrawingTool && selRect.contains(event->pos())) {
                QPoint rel = event->pos();
                if (m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                    if (m_annotationEngine->eraseAnnotationAt(rel)) {
                        update();
                        updateUndoRedoState();
                    }
                } else {
                    const int existingAnnotation = m_annotationEngine->findAnnotationAt(rel);
                    if (existingAnnotation >= 0) {
                        m_annotationEngine->setCurrentTool(AnnotationEngine::None);
                        m_annotationEngine->setSelectedIndex(existingAnnotation);
                        if (m_toolbar)
                            m_toolbar->selectTool(AnnotationEngine::None);
                        update();
                        return;
                    }
                    m_annotationEngine->beginDraw(rel);
                    update();
                }
                return;
            }

            // Annotation click while no tool is selected → move mode
            if (!isDrawingTool && !selectionHandleHit && m_annotationEngine && selRect.contains(event->pos())) {
                QPoint rel = event->pos();
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
                m_selectionAnchorScreenRect = monitorRectAt(event->pos());
                hideToolbar();
                if (m_annotationEngine) m_annotationEngine->clear();
                update();
            }
        } else if (m_selectionComplete && m_selectionLocked) {
            // Selection locked — allow annotation drawing only
            QRect selRect = normalizedSelectionRect();
            bool isDrawingTool = (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None);
            if (!isDrawingTool && m_annotationEngine && selRect.contains(event->pos())) {
                QPoint rel = event->pos();
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
            if (isDrawingTool && selRect.contains(event->pos())) {
                QPoint rel = event->pos();
                if (m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                    if (m_annotationEngine->eraseAnnotationAt(rel)) {
                        update();
                        updateUndoRedoState();
                    }
                } else {
                    m_annotationEngine->beginDraw(rel);
                    update();
                }
            }
        } else {
            if (!m_hoveredWindowRect.isEmpty()) {
                m_windowSnapPressPosition = event->pos();
                m_pressedWindowRect = m_hoveredWindowRect;
                m_windowSnapClickPending = true;
                event->accept();
                return;
            }
            if (!allowsManualSelection(m_selectionMode)) {
                event->accept();
                return;
            }
            m_isSelecting = true;
            m_selectionStart = event->pos();
            m_selectionEnd = event->pos();
            m_selectionAnchorScreenRect = monitorRectAt(event->pos());
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
            m_windowSnapClickPending = false;
            m_pressedWindowRect = QRect();
            m_selectionLocked = false;
            m_selectionStart = m_selectionEnd = QPoint();
            m_selectionAnchorScreenRect = QRect();
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
    if (!allowsManualSelection(m_selectionMode))
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

    if (m_windowSnapClickPending) {
        if (!isWindowSnapClick(m_windowSnapPressPosition, event->pos(),
                               QApplication::startDragDistance())) {
            m_windowSnapClickPending = false;
            m_pressedWindowRect = QRect();
            if (allowsManualSelection(m_selectionMode)) {
                setHoveredWindowRect(QRect());
                m_isSelecting = true;
                m_selectionStart = m_windowSnapPressPosition;
                m_selectionEnd = event->pos();
                m_selectionAnchorScreenRect = monitorRectAt(m_windowSnapPressPosition);
                hideToolbar();
            } else {
                setHoveredWindowRect(windowSnapTargetForMode(
                    m_selectionMode, m_windowSnapCandidates, event->pos(), rect()));
            }
        }
        update();
        return;
    }

    // Annotation rotation
    if (m_isRotatingAnnotation && m_annotationEngine && m_annotationEngine->selectedIndex() >= 0) {
        const qreal currentAngle = AnnotationRotationGeometry::angleAt(m_rotationCenter, event->pos());
        qreal rotation = m_rotationStartDegrees + currentAngle - m_rotationDragStartAngle;
        rotation = AnnotationRotationGeometry::snappedAngle(
            rotation, QApplication::keyboardModifiers().testFlag(Qt::ShiftModifier));
        m_annotationEngine->rotateAnnotation(m_annotationEngine->selectedIndex(), rotation);
        update();
        return;
    }

    // Annotation move
    if (m_isDraggingAnnotation && m_annotationEngine && m_annotationEngine->selectedIndex() >= 0) {
        QRect selRect = normalizedSelectionRect();
        QPoint rel = event->pos();
        QPoint delta = rel - m_dragAnnotationStart;
        if (!delta.isNull()) {
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
            QPoint rel = event->pos();
            if (m_annotationEngine->currentTool() == AnnotationEngine::Eraser) {
                if ((event->buttons() & Qt::LeftButton) && m_annotationEngine->eraseAnnotationAt(rel))
                    update();
            } else {
                m_annotationEngine->continueDraw(rel);
                update();
            }
        }
    } else if (!m_selectionComplete) {
        const QRect hovered = windowSnapTargetForMode(
            m_selectionMode, m_windowSnapCandidates, event->pos(), rect());
        if (hovered != m_hoveredWindowRect) {
            setHoveredWindowRect(hovered);
        } else {
            update(); // crosshair
        }
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

        if (m_windowSnapClickPending) {
            const QRect selectedWindow = m_pressedWindowRect;
            m_windowSnapClickPending = false;
            m_pressedWindowRect = QRect();
            if (isWindowSnapClick(m_windowSnapPressPosition, event->pos(),
                                  QApplication::startDragDistance())) {
                completeSelection(selectedWindow);
            }
            return;
        }

        // Annotation rotation end
        if (m_isRotatingAnnotation) {
            m_isRotatingAnnotation = false;
            setCursor(Qt::CrossCursor);
            updateUndoRedoState();
            update();
            return;
        }

        // Annotation move end
        if (m_isDraggingAnnotation) {
            m_isDraggingAnnotation = false;
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
                completeSelection(selRect);
                return;
            }
            update();
        } else if (m_selectionComplete) {
            QRect selRect = normalizedSelectionRect();
            QPoint rel(qBound(selRect.left(), event->pos().x(), selRect.right()),
                       qBound(selRect.top(), event->pos().y(), selRect.bottom()));

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
            m_selectionAnchorScreenRect = QRect();
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
        if (matchesOverlayShortcut(event, QStringLiteral("actionGoogleLens"), QStringLiteral("Ctrl+L"))) { onGoogleLensRequested(); return; }
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
    if (obj == m_textFocusProxy
        && shouldForwardCaptureKeyFromManagedProxy(m_textEdit && m_textEdit->isVisible())) {
        if (event->type() == QEvent::KeyPress) {
            keyPressEvent(static_cast<QKeyEvent *>(event));
            return true;
        }
        if (event->type() == QEvent::KeyRelease) {
            keyReleaseEvent(static_cast<QKeyEvent *>(event));
            return true;
        }
    }

    if ((obj == m_textMoveHandle || obj == m_textEditPanel) && m_textEdit && m_textEdit->isVisible()) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                m_textPanelDragging = true;
                m_textPanelDragOffset = mapFromGlobal(me->globalPosition().toPoint()) - m_textEditPosition;
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

    if ((obj == m_textEdit || obj == m_textFocusProxy) && event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Escape) {
            cancelTextEdit();
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            // Shift+Enter → newline
            if (ke->modifiers() & Qt::ShiftModifier) {
                if (auto *edit = qobject_cast<QTextEdit *>(obj))
                    edit->insertPlainText("\n");
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
        m_annotationEngine->render(&p, -selRect.topLeft());
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
    QRect toolbarBounds = m_selectionAnchorScreenRect.isValid()
        ? m_selectionAnchorScreenRect
        : monitorRectAt(selRect.center());
    if (!toolbarBounds.isValid())
        toolbarBounds = rect();
    toolbarBounds = toolbarBounds.intersected(rect()).adjusted(5, 5, -5, -5);
    if (!toolbarBounds.isValid())
        toolbarBounds = rect().adjusted(5, 5, -5, -5);

    m_toolbar->refreshTools();

    QRect toolbarRect;
    if (m_toolbar->hasVisibleTools()) {
        // --- Bottom toolbar: below the selection, centered ---
        m_toolbar->adjustSize();
        int th = m_toolbar->height();
        int toolbarWidth = m_toolbar->width();

        int tx = selRect.center().x() - toolbarWidth / 2;
        int ty = selRect.bottom() + margin;

        if (toolbarWidth <= toolbarBounds.width()) {
            if (tx < toolbarBounds.left()) tx = toolbarBounds.left();
            if (tx + toolbarWidth > toolbarBounds.right() + 1)
                tx = toolbarBounds.right() + 1 - toolbarWidth;
        } else {
            tx = toolbarBounds.left();
        }
        if (ty + th > toolbarBounds.bottom() + 1) {
            // If it does not fit below, try above
            ty = selRect.top() - th - margin;
        }
        if (ty < toolbarBounds.top()) {
            // If it also does not fit above, dock to bottom inside the frame
            ty = selRect.bottom() - th - 2;
        }
        if (ty + th > toolbarBounds.bottom() + 1)
            ty = toolbarBounds.bottom() + 1 - th;
        if (ty < toolbarBounds.top())
            ty = toolbarBounds.top();

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

        if (px + pw > toolbarBounds.right() + 1) {
            int leftOutside = selRect.left() - margin - pw;
            if (leftOutside >= toolbarBounds.left()) {
                px = leftOutside;
            } else {
                px = selRect.right() - pw - 2;
                dockedInside = true;
            }
        }

        if (dockedInside && px < selRect.left() + 2)
            px = selRect.left() + 2;
        if (px + pw > toolbarBounds.right() + 1)
            px = toolbarBounds.right() + 1 - pw;
        if (px < toolbarBounds.left())
            px = toolbarBounds.left();

        if (dockedInside && py < selRect.top() + 2)
            py = selRect.top() + 2;
        if (dockedInside && py + ph > selRect.bottom() - 2)
            py = selRect.bottom() - ph - 2;
        if (py < toolbarBounds.top())
            py = toolbarBounds.top();
        if (py + ph > toolbarBounds.bottom() + 1)
            py = toolbarBounds.bottom() + 1 - ph;

        if (toolbarRect.isValid()) {
            auto panelRectAt = [&](int y) {
                return QRect(px, y, pw, ph).adjusted(-4, -4, 4, 4);
            };
            auto inBounds = [&](int y) {
                if (dockedInside)
                    return y >= selRect.top() + 2 && y + ph <= selRect.bottom() - 2;
                return y >= toolbarBounds.top() && y + ph <= toolbarBounds.bottom() + 1;
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
    if (m_recordingDrawer) m_recordingDrawer->hide();
    m_recordingDrawerMode = RecordingDrawerMode::None;
    if (m_textEditPanel) m_textEditPanel->hide();
    if (m_textFocusProxy) m_textFocusProxy->hide();
}

void CaptureOverlay::finishCapture()
{
    QRect selRect = normalizedSelectionRect();
    QPixmap result = getSelectedPixmap();
    hide();
    m_selectionComplete = false;
    m_isSelecting = false;
    m_selectionAnchorScreenRect = QRect();
    hideToolbar();
    cancelTextEdit();

    if (m_captureMode == ModeRecording) {
        m_captureMode = ModeNormal;
        if (!selRect.isEmpty())
            emit regionSelected(selectedCaptureRect(), selectedDisplayRect());
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
    m_selectionAnchorScreenRect = QRect();
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
    if (m_textFocusProxy)
        m_textFocusProxy->clear();
    m_textEdit->setFixedHeight(30);
    updateTextEditorStyle();
    moveTextEditorTo(pos);
    m_textEdit->show();
    if (m_textEditPanel)
        m_textEditPanel->show();
    updateTextEditPanelPosition();
    acquireTextKeyboardFocus();
}

void CaptureOverlay::acquireTextKeyboardFocus()
{
    if (!m_textEdit)
        return;

    if (!m_textFocusProxy) {
        m_textEdit->setFocus(Qt::MouseFocusReason);
        return;
    }

    const QPoint proxyPosition = mapToGlobal(m_textEditPosition + QPoint(4, 4));
    m_textFocusProxy->move(proxyPosition);
    m_textFocusProxy->show();
    m_textFocusProxy->raise();
    if (QWindow *window = m_textFocusProxy->windowHandle())
        window->requestActivate();
    m_textFocusProxy->activateWindow();
    m_textFocusProxy->setFocus(Qt::MouseFocusReason);
    qInfo() << "[CaptureOverlay] requested managed text input focus; activeWindow="
            << QApplication::activeWindow()
            << "focusWidget=" << QApplication::focusWidget();
}

void CaptureOverlay::acquireCaptureKeyboardFocus()
{
    if (!m_textFocusProxy) {
        activateWindow();
        if (windowHandle())
            windowHandle()->requestActivate();
        setFocus(Qt::ActiveWindowFocusReason);
        return;
    }

    m_textFocusProxy->move(mapToGlobal(QPoint(4, 4)));
    m_textFocusProxy->show();
    m_textFocusProxy->raise();
    if (QWindow *window = m_textFocusProxy->windowHandle())
        window->requestActivate();
    m_textFocusProxy->activateWindow();
    m_textFocusProxy->setFocus(Qt::ActiveWindowFocusReason);
    qInfo() << "[CaptureOverlay] managed capture focus requested; activeWindow="
            << QApplication::activeWindow()
            << "focusWidget=" << QApplication::focusWidget();
}

void CaptureOverlay::releaseTextKeyboardFocus()
{
    if (QWidget::keyboardGrabber() == m_textEdit)
        m_textEdit->releaseKeyboard();
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
        m_annotationEngine->addTextAnnotation(m_textEditPosition, text);
        update();
        updateUndoRedoState();
    }
    releaseTextKeyboardFocus();
    m_textEdit->hide();
    if (m_textEditPanel) m_textEditPanel->hide();
    if (m_textFocusProxy) m_textFocusProxy->hide();
    m_textJustCommitted = true;
    setFocus();
}

void CaptureOverlay::cancelTextEdit()
{
    if (m_textEdit) {
        releaseTextKeyboardFocus();
        m_textEdit->hide();
    }
    if (m_textEditPanel) m_textEditPanel->hide();
    if (m_textFocusProxy) m_textFocusProxy->hide();
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
    restoreAfterModalDialog();
}

void CaptureOverlay::onUploadRequested()
{
    QPixmap pix = getSelectedPixmap();
    if (pix.isNull()) return;
    hide();
    UploadDialog dlg;
    dlg.setImage(pix);
    dlg.exec();
    restoreAfterModalDialog();
}

void CaptureOverlay::restoreAfterModalDialog()
{
    show();
    if (m_selectionComplete)
        showToolbar();

    QPointer<CaptureOverlay> self(this);
    QTimer::singleShot(0, this, [self]() {
        if (!self)
            return;
        self->raise();
        self->acquireCaptureKeyboardFocus();
    });
}

void CaptureOverlay::onGoogleLensRequested()
{
    QPixmap pix = getSelectedPixmap();
    if (pix.isNull()) return;

    if (m_googleLensUploader) {
        m_googleLensUploader->disconnect(this);
        m_googleLensUploader->cancel();
        m_googleLensUploader->deleteLater();
        m_googleLensUploader = nullptr;
    }
    if (!m_visualSearchImagePath.isEmpty()) {
        QFile::remove(m_visualSearchImagePath);
        m_visualSearchImagePath.clear();
    }

    const quint64 generation = m_visualSearchOperations.begin();
    m_visualSearchUploadFallbacks = VisualSearchUploadFallbackState();
    QImage uploadImage = pix.toImage();
    const QSize uploadSize = visualSearchUploadSize(uploadImage.size());
    if (uploadSize != uploadImage.size())
        uploadImage = uploadImage.scaled(uploadSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    QTemporaryFile uploadFile(QDir::tempPath() + QStringLiteral("/eshot_visual_search_XXXXXX.jpg"));
    uploadFile.setAutoRemove(false);
    if (!uploadFile.open()) {
        QMessageBox::warning(this, TranslationManager::visualSearchFailed(),
                             TranslationManager::visualSearchTempCreateError());
        return;
    }
    m_visualSearchImagePath = uploadFile.fileName();
    uploadFile.close();
    if (!uploadImage.save(m_visualSearchImagePath, "JPEG", 88)) {
        QFile::remove(m_visualSearchImagePath);
        m_visualSearchImagePath.clear();
        QMessageBox::warning(this, TranslationManager::visualSearchFailed(),
                             TranslationManager::visualSearchTempPrepareError());
        return;
    }

    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    const VisualSearchProvider provider = visualSearchProviderFromSettings(
        settings.value("visualSearchProvider", QStringLiteral("google")).toString());
    hide();
    hideToolbar();
    startNextVisualSearchUpload(generation, provider);
}

void CaptureOverlay::startNextVisualSearchUpload(quint64 generation, VisualSearchProvider provider)
{
    if (!m_visualSearchOperations.isCurrent(generation))
        return;

    VisualSearchUploadProvider fallbackProvider;
    if (!m_visualSearchUploadFallbacks.takeNext(&fallbackProvider)) {
        const QString details = m_visualSearchUploadFallbacks.failureSummary();
        clearVisualSearchUpload();
        QMessageBox::warning(this, TranslationManager::uploadFailed(),
                             TranslationManager::visualSearchUploadUnavailable(details));
        restoreAfterModalDialog();
        return;
    }

    ImageUploader::Provider imageProvider = ImageUploader::Provider::Catbox;
    switch (fallbackProvider) {
    case VisualSearchUploadProvider::Catbox:
        imageProvider = ImageUploader::Provider::Catbox;
        break;
    case VisualSearchUploadProvider::Uguu:
        imageProvider = ImageUploader::Provider::Uguu;
        break;
    case VisualSearchUploadProvider::TmpFiles:
        imageProvider = ImageUploader::Provider::TmpFiles;
        break;
    }

    m_googleLensUploader = ImageUploader::create(imageProvider, this);
    if (!m_googleLensUploader) {
        m_visualSearchUploadFallbacks.recordFailure(
            QStringLiteral("EShot"), TranslationManager::visualSearchUploaderCreateError());
        startNextVisualSearchUpload(generation, provider);
        return;
    }

    ImageUploader *uploader = m_googleLensUploader;
    const QString uploaderName = uploader->providerDisplayName();
    QPointer<CaptureOverlay> self(this);
    connect(uploader, &ImageUploader::succeeded, this,
            [this, self, uploader, generation, provider](const QString &url, const QString &) {
        if (!self || !m_visualSearchOperations.isCurrent(generation) || m_googleLensUploader != uploader)
            return;
        const bool browserOpened = QDesktopServices::openUrl(visualSearchResultUrl(provider, QUrl(url)));
        uploader->deleteLater();
        m_googleLensUploader = nullptr;
        clearVisualSearchUpload();
        if (!browserOpened) {
            QMessageBox::warning(this, TranslationManager::visualSearchBrowserLaunchTitle(),
                                 TranslationManager::visualSearchBrowserLaunchError());
            restoreAfterModalDialog();
        }
    });
    connect(uploader, &ImageUploader::failed, this,
            [this, self, uploader, uploaderName, generation, provider](const QString &reason) {
        if (!self || !m_visualSearchOperations.isCurrent(generation) || m_googleLensUploader != uploader)
            return;
        m_visualSearchUploadFallbacks.recordFailure(uploaderName, reason);
        uploader->deleteLater();
        m_googleLensUploader = nullptr;
        QTimer::singleShot(0, this, [this, self, generation, provider]() {
            if (self)
                startNextVisualSearchUpload(generation, provider);
        });
    });
    uploader->setImagePath(m_visualSearchImagePath);
    uploader->upload();
}

void CaptureOverlay::clearVisualSearchUpload()
{
    if (!m_visualSearchImagePath.isEmpty()) {
        QFile::remove(m_visualSearchImagePath);
        m_visualSearchImagePath.clear();
    }
}

void CaptureOverlay::onGifRequested()
{
    if (normalizedSelectionRect().isEmpty())
        return;
    showRecordingDrawer(RecordingDrawerMode::Gif);
}

void CaptureOverlay::onVideoRequested()
{
    if (normalizedSelectionRect().isEmpty())
        return;
    showRecordingDrawer(RecordingDrawerMode::Video);
}

QRect CaptureOverlay::normalizedSelectionRect() const
{
    return QRect(m_selectionStart, m_selectionEnd).normalized();
}

QRect CaptureOverlay::selectedCaptureRect() const
{
    QRect captureRect = logicalToSnapshot(normalizedSelectionRect());
    return captureRect.translated(m_physicalVirtualDesktopTopLeft);
}

QRect CaptureOverlay::selectedDisplayRect() const
{
#ifdef Q_OS_WIN
    const QRect displayRect = displayRectFromPhysical(selectedCaptureRect(), m_captureMonitors);
    if (displayRect.isValid())
        return displayRect;
#endif
    return normalizedSelectionRect().translated(m_virtualDesktopRect.topLeft());
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
        QRect localRect(
            geometry.x() - m_virtualDesktopRect.x(),
            geometry.y() - m_virtualDesktopRect.y(),
            geometry.width(),
            geometry.height()
        );
        if (localRect.contains(pos))
            return localRect.intersected(rect());
    }
    return QRect();
#endif
}

void CaptureOverlay::setHoveredWindowRect(const QRect &targetRect)
{
    const QRect currentRect = m_animatedWindowRect.isValid()
        ? m_animatedWindowRect : m_hoveredWindowRect;
    if (m_windowSnapAnimation)
        m_windowSnapAnimation->stop();

    if (targetRect == m_hoveredWindowRect) {
        if (targetRect.isEmpty())
            m_animatedWindowRect = QRect();
        return;
    }

    m_hoveredWindowRect = targetRect;
    if (targetRect.isEmpty()) {
        m_animatedWindowRect = QRect();
        update();
        return;
    }
    if (currentRect.isEmpty() || currentRect == targetRect || !m_windowSnapAnimation) {
        m_animatedWindowRect = targetRect;
        update();
        return;
    }

    m_windowSnapAnimationStart = currentRect;
    m_windowSnapAnimation->setDuration(windowSnapAnimationDurationMs());
    m_windowSnapAnimation->setStartValue(0.0);
    m_windowSnapAnimation->setEndValue(1.0);
    m_windowSnapAnimation->start();
}

void CaptureOverlay::selectMonitorAt(const QPoint &pos)
{
    QRect monitorRect = monitorRectAt(pos);
    if (monitorRect.isEmpty()) return;

    if (m_textEdit) m_textEdit->hide();
    if (m_textFocusProxy) m_textFocusProxy->hide();
    if (m_annotationEngine) m_annotationEngine->clear();

    m_isSelecting = false;
    m_selectionComplete = true;
    m_windowSnapClickPending = false;
    setHoveredWindowRect(QRect());
    m_pressedWindowRect = QRect();
    m_isDraggingAnnotation = false;
    m_resizeMode = ResNone;
    m_selectionStart = monitorRect.topLeft();
    m_selectionEnd = monitorRect.bottomRight();
    m_selectionAnchorScreenRect = monitorRect;

    if (m_captureMode == ModeRecording) {
        finishCapture();
        return;
    }

    showToolbar();
    updateUndoRedoState();
    updateCursor(pos);

    update();
}

void CaptureOverlay::completeSelection(const QRect &selectionRect)
{
    const QRect bounded = selectionRect.intersected(rect());
    if (bounded.width() <= 10 || bounded.height() <= 10)
        return;

    m_isSelecting = false;
    m_selectionComplete = true;
    m_windowSnapClickPending = false;
    setHoveredWindowRect(QRect());
    m_pressedWindowRect = QRect();
    m_selectionStart = bounded.topLeft();
    m_selectionEnd = bounded.bottomRight();
    m_selectionAnchorScreenRect = monitorRectAt(bounded.center());

    if (m_captureMode == ModeRecording) {
        finishCapture();
        return;
    }

    if (m_instantCopyAfterSelection) {
        onCopyToClipboard();
        if (isVisible())
            finishCapture();
        return;
    }

    showToolbar();
    updateUndoRedoState();
    updateCursor(bounded.center());
    update();
}

void CaptureOverlay::onToolSelected(int toolId)
{
    if (m_annotationEngine) {
        m_annotationEngine->setCurrentTool(static_cast<AnnotationEngine::Tool>(toolId));
        m_annotationEngine->setSelectedIndex(-1);
    }
    m_isDraggingAnnotation = false;
    QSettings settings("EShot", "EShot");
    if (settings.value("rememberLastAnnotationTool", false).toBool())
        settings.setValue("lastAnnotationTool", toolId);
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
    QString path = s.contains("screenshotSavePath")
        ? s.value("screenshotSavePath").toString().trimmed()
        : QDir(defaultSaveDirectory()).filePath(QStringLiteral("Screenshots"));
    if (path.isEmpty())
        path = s.value("savePath", defaultSaveDirectory()).toString();
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

    if (m_annotationEngine && m_annotationEngine->currentTool() != AnnotationEngine::None) {
        QRect sel = normalizedSelectionRect();
        setCursor(sel.contains(pos) ? Qt::CrossCursor : Qt::ArrowCursor);
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
