#include "AnnotationToolbar.h"
#include "OnboardingTips.h"
#include "annotation/AnnotationEngine.h"
#include "../core/TranslationManager.h"
#include "../core/VisualSearch.h"
#include "../capture/CaptureInteractionPolicy.h"
#include <QColorDialog>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QStyle>
#include <QSettings>
#include <QRegularExpression>
#include <QHBoxLayout>
#include <QFontComboBox>
#include <QSpinBox>

namespace {
QStringList defaultAnnotationTools()
{
    return {"Pen","Arrow","Line","Rectangle","Circle","Text","Highlighter","SemiRect","Blur","Pixelate","Counter","Eraser"};
}

QStringList defaultToolbarControls()
{
    return {"Color","Eyedropper","Lock","Undo","Redo","Ocr","Upload","GoogleLens","Gif","Video"};
}

QStringList normalizedAnnotationTools(QSettings &settings)
{
    QStringList tools = settings.value("visibleTools", defaultAnnotationTools()).toStringList();
    // Pixelate became its own tool next to the smooth Blur; show it to users
    // who customised the toolbar before it existed.
    if (settings.contains("visibleTools")
        && !settings.value("toolsMigratedPixelate", false).toBool()) {
        if (!tools.contains(QStringLiteral("Pixelate"))) {
            const int blurIndex = tools.indexOf(QStringLiteral("Blur"));
            tools.insert(blurIndex >= 0 ? blurIndex + 1 : tools.size(), QStringLiteral("Pixelate"));
        }
        settings.setValue("toolsMigratedPixelate", true);
        settings.setValue("visibleTools", tools);
    }
    return tools;
}

QStringList normalizedToolbarControls(QSettings &settings)
{
    const QStringList defaults = defaultToolbarControls();
    QStringList controls = settings.value("visibleToolbarControls", defaults).toStringList();
    if (settings.contains("visibleToolbarControls") &&
        !settings.value("toolbarControlsMigratedVideo", false).toBool()) {
        if (!controls.contains(QStringLiteral("Video")))
            controls.append(QStringLiteral("Video"));
        settings.setValue("toolbarControlsMigratedVideo", true);
        settings.setValue("visibleToolbarControls", controls);
    }
    if (settings.contains("visibleToolbarControls") &&
        !settings.value("toolbarControlsMigratedGoogleLens", false).toBool()) {
        if (!controls.contains(QStringLiteral("GoogleLens")))
            controls.append(QStringLiteral("GoogleLens"));
        settings.setValue("toolbarControlsMigratedGoogleLens", true);
        settings.setValue("visibleToolbarControls", controls);
    }
    return controls;
}
}

AnnotationToolbar::AnnotationToolbar(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_colorButton(nullptr)
    , m_widthSlider(nullptr)
    , m_currentToolId(-1)
    , m_currentColor(Qt::red)
    , m_eyedropperButton(nullptr)
    , m_lockButton(nullptr)
    , m_selectionLocked(false)
    , m_textOptionsWidget(nullptr)
    , m_textFontCombo(nullptr)
    , m_textSizeSpin(nullptr)
    , m_undoButton(nullptr)
    , m_redoButton(nullptr)
    , m_ocrButton(nullptr)
    , m_uploadButton(nullptr)
    , m_lensButton(nullptr)
{
    if (!parent) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
    }
    setFixedHeight(48);
    setFocusPolicy(Qt::StrongFocus);
    setMinimumWidth(500);

    QSettings s("EShot", "EShot");
    m_visibleTools = normalizedAnnotationTools(s);
    m_visibleControls = normalizedToolbarControls(s);

    setupUI();
    applyStyles();
    refreshToolTips();
}

AnnotationToolbar::~AnnotationToolbar() {}

bool AnnotationToolbar::isToolVisible(const QString &key) const
{
    return m_visibleTools.contains(key);
}

bool AnnotationToolbar::isControlVisible(const QString &key) const
{
    return m_visibleControls.contains(key);
}

void AnnotationToolbar::refreshTools()
{
    QSettings s("EShot", "EShot");
    m_visibleTools = normalizedAnnotationTools(s);
    m_visibleControls = normalizedToolbarControls(s);

    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);

    for (auto btn : m_toolButtons) {
        QString key = btn->property("settingsKey").toString();
        if (!key.isEmpty()) {
            btn->setVisible(isToolVisible(key));
        }
    }
    for (auto it = m_optionalControls.begin(); it != m_optionalControls.end(); ++it) {
        if (it.value())
            it.value()->setVisible(isControlVisible(it.key()));
    }
    updateDynamicOptionVisibility();
    if (m_layout)
        m_layout->activate();
    adjustSize();
    setFixedWidth(sizeHint().width());
    updateGeometry();
}

bool AnnotationToolbar::hasVisibleTools() const
{
    for (auto btn : m_toolButtons) {
        if (btn && !btn->isHidden())
            return true;
    }
    for (auto control : m_optionalControls) {
        if (control && !control->isHidden())
            return true;
    }
    return false;
}

void AnnotationToolbar::updateDynamicOptionVisibility()
{
    if (m_textOptionsWidget) {
        m_textOptionsWidget->setVisible(false);
    }
}

void AnnotationToolbar::selectTool(int toolId)
{
    for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) {
        bool sel = (it.key() == toolId);
        it.value()->setProperty("selected", sel ? "true" : "false");
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }
    m_currentToolId = toolId;

    updateDynamicOptionVisibility();
    adjustSize();
    setFixedWidth(sizeHint().width());
    updateGeometry();
}

void AnnotationToolbar::setUndoEnabled(bool enabled)
{
    if (m_undoButton) {
        // Set the stylesheet once and toggle only enabled state: swapping the
        // whole stylesheet on every call forces a full restyle each time.
        if (m_undoButton->styleSheet().isEmpty()) {
            m_undoButton->setStyleSheet(R"(
                QPushButton {
                    background-color: #3a3a3a;
                    border: 1px solid #505050;
                    border-radius: 8px;
                }
                QPushButton:hover {
                    background-color: #4a4a4a;
                    border-color: #606060;
                }
                QPushButton:pressed {
                    background-color: #333333;
                }
                QPushButton:disabled {
                    background-color: #2d2d2d;
                    border: 1px solid #353535;
                }
            )");
        }
        m_undoButton->setEnabled(enabled);
    }
}

void AnnotationToolbar::setRedoEnabled(bool enabled)
{
    if (m_redoButton) {
        if (m_redoButton->styleSheet().isEmpty()) {
            m_redoButton->setStyleSheet(R"(
                QPushButton {
                    background-color: #3a3a3a;
                    border: 1px solid #505050;
                    border-radius: 8px;
                }
                QPushButton:hover {
                    background-color: #4a4a4a;
                    border-color: #606060;
                }
                QPushButton:pressed {
                    background-color: #333333;
                }
                QPushButton:disabled {
                    background-color: #2d2d2d;
                    border: 1px solid #353535;
                }
            )");
        }
        m_redoButton->setEnabled(enabled);
    }
}

void AnnotationToolbar::setColor(const QColor &color)
{
    m_currentColor = color;
    if (m_colorButton) {
        m_colorButton->setStyleSheet(QString(R"(
            QPushButton { background-color: %1; border: 2px solid #505050; border-radius: 8px; }
            QPushButton:hover { border-color: #707070; }
        )").arg(color.name()));
    }
}

QWidget* AnnotationToolbar::createSeparator()
{
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(13);
    sep->setFixedHeight(26);
    sep->setStyleSheet("color: #505050; margin-left: 6px; margin-right: 6px;");
    return sep;
}

void AnnotationToolbar::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(4);

    // Tool buttons
    m_layout->addWidget(createToolButton(":/icons/pen.svg", TranslationManager::toolPen(), AnnotationEngine::Pen, "Pen"));
    m_layout->addWidget(createToolButton(":/icons/arrow.svg", TranslationManager::toolArrow(), AnnotationEngine::Arrow, "Arrow"));
    m_layout->addWidget(createToolButton(":/icons/line.svg", TranslationManager::toolLine(), AnnotationEngine::Line, "Line"));
    m_layout->addWidget(createToolButton(":/icons/rectangle.svg", TranslationManager::toolRect(), AnnotationEngine::Rectangle, "Rectangle"));
    m_layout->addWidget(createToolButton(":/icons/circle.svg", TranslationManager::toolCircle(), AnnotationEngine::Circle, "Circle"));
    m_layout->addWidget(createToolButton(":/icons/text.svg", TranslationManager::toolText(), AnnotationEngine::Text, "Text"));
    m_layout->addWidget(createToolButton(":/icons/highlighter.svg",
        TranslationManager::toolHighlighter() + QStringLiteral(" · ")
            + TranslationManager::tr("highlighterStraightTooltip"),
        AnnotationEngine::Highlighter, "Highlighter"));
    m_layout->addWidget(createToolButton(":/icons/semirect.svg", TranslationManager::toolSemiRect() + QStringLiteral(" (D)"), AnnotationEngine::SemiRect, "SemiRect"));
    m_layout->addWidget(createToolButton(":/icons/blur.svg", TranslationManager::toolBlur(), AnnotationEngine::Blur, "Blur"));
    m_layout->addWidget(createToolButton(":/icons/pixelate.svg", TranslationManager::toolPixelate(), AnnotationEngine::Pixelate, "Pixelate"));
    m_layout->addWidget(createToolButton(":/icons/counter.svg", TranslationManager::toolCounter(), AnnotationEngine::Counter, "Counter"));
    m_layout->addWidget(createToolButton(":/icons/eraser.svg", TranslationManager::toolEraser(), AnnotationEngine::Eraser, "Eraser"));

    refreshTools();

    m_layout->addWidget(createSeparator());

    // Color button
    m_colorButton = createColorButton(m_currentColor);
    m_optionalControls["Color"] = m_colorButton;
    m_layout->addWidget(m_colorButton);

    // Eyedropper button
    m_eyedropperButton = new QPushButton(this);
    m_eyedropperButton->setToolTip(TranslationManager::toolEyedropper());
    m_eyedropperButton->setCursor(Qt::PointingHandCursor);
    m_eyedropperButton->setFixedSize(34, 34);
    m_eyedropperButton->setIcon(QIcon(":/icons/eyedropper.svg"));
    m_eyedropperButton->setIconSize(QSize(18, 18));
    m_eyedropperButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
        QPushButton:pressed {
            background-color: #333333;
        }
    )");
    connect(m_eyedropperButton, &QPushButton::clicked, this, &AnnotationToolbar::onEyedropperClicked);
    m_optionalControls["Eyedropper"] = m_eyedropperButton;
    m_layout->addWidget(m_eyedropperButton);

    // Selection lock button
    m_lockButton = new QPushButton(this);
    m_lockButton->setToolTip(TranslationManager::actionLock());
    m_lockButton->setCursor(Qt::PointingHandCursor);
    m_lockButton->setFixedSize(34, 34);
    m_lockButton->setIcon(QIcon(":/icons/lock_open.svg"));
    m_lockButton->setIconSize(QSize(18, 18));
    m_lockButton->setCheckable(true);
    m_lockButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
        QPushButton:pressed {
            background-color: #333333;
        }
        QPushButton:checked {
            background-color: #0078D4;
            border: 1px solid #1a8cff;
        }
    )");
    connect(m_lockButton, &QPushButton::clicked, this, &AnnotationToolbar::onLockClicked);
    m_optionalControls["Lock"] = m_lockButton;
    m_layout->addWidget(m_lockButton);

    m_widthSlider = nullptr;

    // Text font controls (hidden by default)
    m_textOptionsWidget = new QWidget(this);
    QHBoxLayout *textLayout = new QHBoxLayout(m_textOptionsWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    m_textFontCombo = new QFontComboBox(m_textOptionsWidget);
    m_textFontCombo->setCurrentFont(QFont("Segoe UI"));
    m_textFontCombo->setFixedWidth(118);
    m_textFontCombo->setToolTip(TranslationManager::toolFont());
    m_textSizeSpin = new QSpinBox(m_textOptionsWidget);
    m_textSizeSpin->setRange(8, 72);
    m_textSizeSpin->setValue(18);
    m_textSizeSpin->setFixedWidth(54);
    m_textSizeSpin->setToolTip(TranslationManager::toolFontSize());
    connect(m_textFontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        emit textFontFamilyChanged(font.family());
    });
    connect(m_textSizeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &AnnotationToolbar::textFontSizeChanged);
    textLayout->addWidget(m_textFontCombo);
    textLayout->addWidget(m_textSizeSpin);
    m_textOptionsWidget->hide();
    m_optionalControls["TextOptions"] = m_textOptionsWidget;
    m_layout->addWidget(m_textOptionsWidget);

    m_layout->addWidget(createSeparator());

    // Undo / Redo
    m_undoButton = createActionButton(":/icons/undo.svg", TranslationManager::toolUndo(), "undo");
    m_redoButton = createActionButton(":/icons/redo.svg", TranslationManager::toolRedo() + QStringLiteral(" (Ctrl+Shift+Z)"), "redo");
    m_optionalControls["Undo"] = m_undoButton;
    m_optionalControls["Redo"] = m_redoButton;
    m_layout->addWidget(m_undoButton);
    m_layout->addWidget(m_redoButton);

    m_layout->addWidget(createSeparator());

    // OCR and upload
    m_ocrButton = new QPushButton(this);
    m_ocrButton->setIcon(QIcon(":/icons/ocr.svg"));
    m_ocrButton->setIconSize(QSize(18, 18));
    m_ocrButton->setFixedSize(34, 34);
    m_ocrButton->setToolTip(TranslationManager::actionOcr());
    m_ocrButton->setCursor(Qt::PointingHandCursor);
    m_ocrButton->setProperty("action", "ocr");
    m_ocrButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
    )");
    connect(m_ocrButton, &QPushButton::clicked, this, &AnnotationToolbar::onActionButtonClicked);
    m_actionButtons["ocr"] = m_ocrButton;
    m_optionalControls["Ocr"] = m_ocrButton;
    m_layout->addWidget(m_ocrButton);

    m_uploadButton = new QPushButton(this);
    m_uploadButton->setIcon(QIcon(":/icons/upload.svg"));
    m_uploadButton->setIconSize(QSize(18, 18));
    m_uploadButton->setFixedSize(34, 34);
    m_uploadButton->setToolTip(TranslationManager::uploadToService());
    m_uploadButton->setCursor(Qt::PointingHandCursor);
    m_uploadButton->setProperty("action", "upload");
    m_uploadButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
    )");
    connect(m_uploadButton, &QPushButton::clicked, this, &AnnotationToolbar::onActionButtonClicked);
    m_actionButtons["upload"] = m_uploadButton;
    m_optionalControls["Upload"] = m_uploadButton;
    m_layout->addWidget(m_uploadButton);

    QSettings visualSearchSettings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    const VisualSearchProvider visualSearchProvider = visualSearchProviderFromSettings(
        visualSearchSettings.value("visualSearchProvider", QStringLiteral("google")).toString());
    m_lensButton = createActionButton(
        visualSearchIconPath(visualSearchProvider),
        visualSearchProvider == VisualSearchProvider::YandexImages
            ? TranslationManager::visualSearchYandexTooltip()
            : TranslationManager::visualSearchGoogleTooltip(),
        "lens");
    m_optionalControls["GoogleLens"] = m_lensButton;
    m_layout->addWidget(m_lensButton);

    m_gifButton = new QPushButton(this);
    m_gifButton->setIcon(QIcon(":/icons/gif.svg"));
    m_gifButton->setIconSize(QSize(22, 22));
    m_gifButton->setFixedSize(34, 34);
    m_gifButton->setToolTip(TranslationManager::recordingStartTitle());
    m_gifButton->setCursor(Qt::PointingHandCursor);
    m_gifButton->setProperty("action", "gif");
    m_gifButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
    )");
    connect(m_gifButton, &QPushButton::clicked, this, &AnnotationToolbar::onActionButtonClicked);
    m_actionButtons["gif"] = m_gifButton;
    m_optionalControls["Gif"] = m_gifButton;
    m_layout->addWidget(m_gifButton);

    m_videoButton = new QPushButton(this);
    m_videoButton->setIcon(QIcon(":/icons/video.svg"));
    m_videoButton->setIconSize(QSize(24, 24));
    m_videoButton->setFixedSize(34, 34);
    m_videoButton->setToolTip(TranslationManager::videoRecordingTitle());
    m_videoButton->setCursor(Qt::PointingHandCursor);
    m_videoButton->setProperty("action", "video");
    m_videoButton->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
    )");
    connect(m_videoButton, &QPushButton::clicked, this, &AnnotationToolbar::onActionButtonClicked);
    m_actionButtons["video"] = m_videoButton;
    m_optionalControls["Video"] = m_videoButton;
    m_layout->addWidget(m_videoButton);

    refreshTools();
    adjustSize();
}

void AnnotationToolbar::applyStyles()
{
    setStyleSheet(R"(
        AnnotationToolbar {
            background-color: #2d2d2d;
            border: 1px solid #404040;
            border-radius: 10px;
        }
        QToolTip {
            color: #ffffff;
            background-color: #3a3a3a;
            border: 1px solid #555555;
            padding: 4px 8px;
            font-size: 12px;
            border-radius: 4px;
        }
    )");

    QGraphicsDropShadowEffect *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0,0,0,160));
    shadow->setOffset(0, 4);
    setGraphicsEffect(shadow);
}

QPushButton* AnnotationToolbar::createToolButton(const QString &iconPath, const QString &tooltip,
                                                  int toolId, const QString &settingsKey)
{
    QPushButton *btn = new QPushButton(this);
    btn->setProperty("settingsKey", settingsKey);
    btn->setToolTip(tooltip);
    btn->setProperty("toolId", toolId);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(34, 34);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(18, 18));
    btn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
        QPushButton:pressed {
            background-color: #333333;
        }
        QPushButton[selected="true"] {
            background-color: #0078D4;
            border: 1px solid #1a8cff;
        }
    )");
    connect(btn, &QPushButton::clicked, this, &AnnotationToolbar::onToolButtonClicked);
    m_toolButtons[toolId] = btn;
    return btn;
}

QPushButton* AnnotationToolbar::createActionButton(const QString &iconPath, const QString &tooltip,
                                                    const QString &action)
{
    QPushButton *btn = new QPushButton(this);
    btn->setToolTip(tooltip);
    btn->setProperty("action", action);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(34, 34);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(18, 18));

    btn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            border: 1px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            background-color: #4a4a4a;
            border-color: #606060;
        }
        QPushButton:pressed {
            background-color: #333333;
        }
    )");

    connect(btn, &QPushButton::clicked, this, &AnnotationToolbar::onActionButtonClicked);
    m_actionButtons[action] = btn;
    return btn;
}

QPushButton* AnnotationToolbar::createColorButton(const QColor &color)
{
    QPushButton *btn = new QPushButton(this);
    btn->setToolTip(TranslationManager::toolColor());
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(34, 34);
    btn->setStyleSheet(QString(R"(
        QPushButton {
            background-color: %1;
            border: 2px solid #505050;
            border-radius: 8px;
        }
        QPushButton:hover {
            border-color: #707070;
        }
    )").arg(color.name()));
    connect(btn, &QPushButton::clicked, this, &AnnotationToolbar::onColorButtonClicked);
    return btn;
}

void AnnotationToolbar::onToolButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    int toolId = btn->property("toolId").toInt();

    if (m_currentToolId == toolId) toolId = AnnotationEngine::None;

    for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) {
        bool sel = (it.key() == toolId);
        it.value()->setProperty("selected", sel ? "true" : "false");
        it.value()->style()->unpolish(it.value());
        it.value()->style()->polish(it.value());
    }

    m_currentToolId = toolId;

    updateDynamicOptionVisibility();
    adjustSize();
    setFixedWidth(sizeHint().width());
    updateGeometry();

    emit toolSelected(toolId);
}

void AnnotationToolbar::onActionButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString action = btn->property("action").toString();

    if (action == "undo") emit undoRequested();
    else if (action == "redo") emit redoRequested();
    else if (action == "ocr") emit ocrRequested();
    else if (action == "upload") emit uploadRequested();
    else if (action == "lens") emit googleLensRequested();
    else if (action == "gif") emit gifRequested();
    else if (action == "video") emit videoRequested();
}

void AnnotationToolbar::onColorButtonClicked()
{
#ifdef Q_OS_LINUX
    const bool detachFromOverlay = shouldDetachModalFromOverlay(
        qEnvironmentVariableIntValue("ESHOT_WAYLAND_XWAYLAND_OVERLAY") == 1);
    if (detachFromOverlay && window())
        window()->hide();
    QColorDialog dlg(m_currentColor, detachFromOverlay ? nullptr : this);
    dlg.setOption(QColorDialog::DontUseNativeDialog, true);
    if (detachFromOverlay) {
        dlg.setWindowFlag(Qt::WindowStaysOnTopHint, true);
        dlg.setWindowModality(Qt::ApplicationModal);
    }
#else
    QColorDialog dlg(m_currentColor, this);
#endif
    dlg.setWindowTitle(TranslationManager::toolColor());
    // A capture keyboard grab would keep typing out of the hex colour field.
    if (QWidget *grabber = QWidget::keyboardGrabber())
        grabber->releaseKeyboard();
    if (dlg.exec() == QDialog::Accepted) {
        QColor c = dlg.selectedColor();
        if (c.isValid()) {
            m_currentColor = c;
            m_colorButton->setStyleSheet(QString(R"(
                QPushButton { background-color: %1; border: 2px solid #555; border-radius: 13px; }
                QPushButton:hover { border-color: #fff; }
            )").arg(c.name()));
            emit colorChanged(c);
        }
    }

    emit modalDialogClosed();
}

void AnnotationToolbar::onEyedropperClicked()
{
    emit eyedropperRequested();
}

void AnnotationToolbar::setSelectionLocked(bool locked)
{
    m_selectionLocked = locked;
    if (m_lockButton) {
        m_lockButton->setIcon(QIcon(m_selectionLocked ? ":/icons/lock.svg" : ":/icons/lock_open.svg"));
        m_lockButton->setChecked(m_selectionLocked);
    }
}

void AnnotationToolbar::onLockClicked()
{
    setSelectionLocked(!m_selectionLocked);
    emit lockToggled(m_selectionLocked);
}

namespace {
// "Name  Key" on the first line and what the control does below it. The key
// is the one configured in Settings, so the tooltip never shows a stale key.
QString richToolTip(const QString &label, const char *shortcutId, const char *fallback,
                    const char *descriptionKey)
{
    static const QRegularExpression keySuffix(QStringLiteral("\\s*\\([^)]*\\)\\s*$"));
    const QString name = QString(label).remove(keySuffix);
    const QString key = shortcutId
        ? OnboardingTips::overlayShortcutText(QLatin1String(shortcutId), QLatin1String(fallback))
        : QString();
    QString html = QStringLiteral("<b>%1</b>").arg(name.toHtmlEscaped());
    if (!key.isEmpty())
        html += QStringLiteral("&nbsp;&nbsp;<span style='color:#a8a8a8'>%1</span>").arg(key.toHtmlEscaped());
    html += QStringLiteral("<br><span style='color:#b8bec8'>%1</span>")
                .arg(TranslationManager::tr(descriptionKey).toHtmlEscaped());
    return html;
}
}

void AnnotationToolbar::refreshToolTips()
{
    using TM = TranslationManager;
    for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) {
        switch (it.key()) {
            case AnnotationEngine::Pen:        it.value()->setToolTip(richToolTip(TM::toolPen(), "toolPen", "P", "descPen")); break;
            case AnnotationEngine::Arrow:      it.value()->setToolTip(richToolTip(TM::toolArrow(), "toolArrow", "A", "descArrow")); break;
            case AnnotationEngine::Line:       it.value()->setToolTip(richToolTip(TM::toolLine(), "toolLine", "L", "descLine")); break;
            case AnnotationEngine::Rectangle:  it.value()->setToolTip(richToolTip(TM::toolRect(), "toolRectangle", "R", "descRect")); break;
            case AnnotationEngine::SemiRect:   it.value()->setToolTip(richToolTip(TM::toolSemiRect(), "toolSemiRect", "D", "descSemiRect")); break;
            case AnnotationEngine::Circle:     it.value()->setToolTip(richToolTip(TM::toolCircle(), "toolCircle", "C", "descCircle")); break;
            case AnnotationEngine::Text:       it.value()->setToolTip(richToolTip(TM::toolText(), "toolText", "T", "descText")); break;
            case AnnotationEngine::Highlighter:it.value()->setToolTip(richToolTip(TM::toolHighlighter(), "toolHighlighter", "H", "descHighlighter")); break;
            case AnnotationEngine::Blur:       it.value()->setToolTip(richToolTip(TM::toolBlur(), "toolBlur", "B", "descBlur")); break;
            case AnnotationEngine::Pixelate:   it.value()->setToolTip(richToolTip(TM::toolPixelate(), "toolPixelate", "M", "descPixelate")); break;
            case AnnotationEngine::Counter:    it.value()->setToolTip(richToolTip(TM::toolCounter(), "toolCounter", "N", "descCounter")); break;
            case AnnotationEngine::Eraser:     it.value()->setToolTip(richToolTip(TM::toolEraser(), "toolEraser", "X", "descEraser")); break;
        }
    }
    if (m_actionButtons.contains("undo")) m_actionButtons["undo"]->setToolTip(richToolTip(TM::toolUndo(), "actionUndo", "Ctrl+Z", "descUndo"));
    if (m_actionButtons.contains("redo")) m_actionButtons["redo"]->setToolTip(richToolTip(TM::toolRedo(), "actionRedo", "Ctrl+Shift+Z", "descRedo"));
    if (m_colorButton) m_colorButton->setToolTip(richToolTip(TM::toolColor(), nullptr, nullptr, "descColor"));
    if (m_eyedropperButton) m_eyedropperButton->setToolTip(richToolTip(TM::toolEyedropper(), "actionEyedropper", "I", "descEyedropper"));
    if (m_lockButton) m_lockButton->setToolTip(richToolTip(TM::actionLock(), "actionLock", "K", "descLock"));
    if (m_textFontCombo) m_textFontCombo->setToolTip(TM::toolFont());
    if (m_textSizeSpin) m_textSizeSpin->setToolTip(TM::toolFontSize());
    if (m_ocrButton) m_ocrButton->setToolTip(richToolTip(TM::actionOcr(), "actionOcr", "Ctrl+O", "descOcr"));
    if (m_uploadButton) m_uploadButton->setToolTip(richToolTip(TM::uploadToService(), "actionUpload", "Ctrl+U", "descUpload"));
    if (m_lensButton) {
        QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
        const VisualSearchProvider provider = visualSearchProviderFromSettings(
            settings.value("visualSearchProvider", QStringLiteral("google")).toString());
        m_lensButton->setIcon(QIcon(visualSearchIconPath(provider)));
        m_lensButton->setToolTip(richToolTip(provider == VisualSearchProvider::YandexImages
                                                 ? TM::visualSearchYandexTooltip()
                                                 : TM::visualSearchGoogleTooltip(),
                                             "actionGoogleLens", "Ctrl+L", "descLens"));
    }
    if (m_gifButton) m_gifButton->setToolTip(richToolTip(TM::recordingStartTitle(), "actionGif", "Ctrl+G", "descGif"));
    if (m_videoButton) m_videoButton->setToolTip(richToolTip(TM::videoRecordingTitle(), "actionVideo", "Ctrl+Shift+V", "descVideo"));
}
