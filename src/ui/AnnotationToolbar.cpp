#include "AnnotationToolbar.h"
#include "annotation/AnnotationEngine.h"
#include "../core/TranslationManager.h"
#include <QColorDialog>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QStyle>
#include <QSettings>
#include <QHBoxLayout>
#include <QFontComboBox>
#include <QSpinBox>

AnnotationToolbar::AnnotationToolbar(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_colorButton(nullptr)
    , m_currentToolId(-1)
    , m_currentColor(Qt::red)
    , m_eyedropperButton(nullptr)
    , m_lockButton(nullptr)
    , m_selectionLocked(false)
    , m_blurIntensitySlider(nullptr)
    , m_blurIntensityLabel(nullptr)
    , m_blurIntensityWidget(nullptr)
    , m_textOptionsWidget(nullptr)
    , m_textFontCombo(nullptr)
    , m_textSizeSpin(nullptr)
    , m_undoButton(nullptr)
    , m_redoButton(nullptr)
    , m_ocrButton(nullptr)
    , m_uploadButton(nullptr)
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
    m_visibleTools = s.value("visibleTools",
        QStringList{"Pen","Arrow","Line","Rectangle","Circle","Text","Highlighter","SemiRect","Blur","Counter","Eraser"})
        .toStringList();

    setupUI();
    applyStyles();
}

AnnotationToolbar::~AnnotationToolbar() {}

bool AnnotationToolbar::isToolVisible(const QString &key) const
{
    return m_visibleTools.contains(key);
}

void AnnotationToolbar::refreshTools()
{
    QSettings s("EShot", "EShot");
    m_visibleTools = s.value("visibleTools",
        QStringList{"Pen","Arrow","Line","Rectangle","Circle","Text","Highlighter","SemiRect","Blur","Counter","Eraser"})
        .toStringList();

    for (auto btn : m_toolButtons) {
        QString key = btn->property("settingsKey").toString();
        if (!key.isEmpty()) {
            btn->setVisible(isToolVisible(key));
        }
    }
    adjustSize();
}

bool AnnotationToolbar::hasVisibleTools() const
{
    for (auto btn : m_toolButtons) {
        if (btn && btn->isVisible())
            return true;
    }
    return false;
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

    // Show/hide blur widget
    if (m_blurIntensityWidget) {
        bool showBlur = (toolId == AnnotationEngine::Blur);
        m_blurIntensityWidget->setVisible(showBlur);
    }
    if (m_textOptionsWidget) {
        m_textOptionsWidget->setVisible(toolId == AnnotationEngine::Text);
    }
    adjustSize();
    setFixedWidth(sizeHint().width());
    updateGeometry();
}

void AnnotationToolbar::setUndoEnabled(bool enabled)
{
    if (m_undoButton) {
        m_undoButton->setEnabled(enabled);
        m_undoButton->setStyleSheet(enabled ?
            R"(
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
            )" :
            R"(
                QPushButton {
                    background-color: #2d2d2d;
                    border: 1px solid #353535;
                    border-radius: 8px;
                }
            )");
    }
}

void AnnotationToolbar::setRedoEnabled(bool enabled)
{
    if (m_redoButton) {
        m_redoButton->setEnabled(enabled);
        m_redoButton->setStyleSheet(enabled ?
            R"(
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
            )" :
            R"(
                QPushButton {
                    background-color: #2d2d2d;
                    border: 1px solid #353535;
                    border-radius: 8px;
                }
            )");
    }
}

void AnnotationToolbar::setBlurIntensity(int intensity)
{
    if (m_blurIntensitySlider) {
        m_blurIntensitySlider->blockSignals(true);
        m_blurIntensitySlider->setValue(intensity);
        m_blurIntensitySlider->blockSignals(false);
    }
    if (m_blurIntensityLabel) {
        m_blurIntensityLabel->setText(QString::number(intensity));
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
    sep->setFixedWidth(1);
    sep->setFixedHeight(26);
    sep->setStyleSheet("background-color: #505050;");
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
    m_layout->addWidget(createToolButton(":/icons/highlighter.svg", TranslationManager::toolHighlighter(), AnnotationEngine::Highlighter, "Highlighter"));
    m_layout->addWidget(createToolButton(":/icons/semirect.svg", TranslationManager::toolSemiRect(), AnnotationEngine::SemiRect, "SemiRect"));
    m_layout->addWidget(createToolButton(":/icons/blur.svg", TranslationManager::toolBlur(), AnnotationEngine::Blur, "Blur"));
    m_layout->addWidget(createToolButton(":/icons/counter.svg", TranslationManager::toolCounter(), AnnotationEngine::Counter, "Counter"));
    m_layout->addWidget(createToolButton(":/icons/eraser.svg", TranslationManager::toolEraser(), AnnotationEngine::Eraser, "Eraser"));

    refreshTools();

    m_layout->addWidget(createSeparator());

    // Color button
    m_colorButton = createColorButton(m_currentColor);
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
    m_layout->addWidget(m_lockButton);

    m_layout->addWidget(createSeparator());

    // Pen width slider
    QSlider *slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(1, 20);
    slider->setValue(3);
    slider->setFixedWidth(80);
    slider->setToolTip(TranslationManager::toolWidth());
    slider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #404040;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #0078D4;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #1a8cff;
        }
    )");
    connect(slider, &QSlider::valueChanged, this, &AnnotationToolbar::onWidthSliderChanged);
    m_layout->addWidget(slider);

    // Text font controls (hidden by default)
    m_textOptionsWidget = new QWidget(this);
    QHBoxLayout *textLayout = new QHBoxLayout(m_textOptionsWidget);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);
    m_textFontCombo = new QFontComboBox(m_textOptionsWidget);
    m_textFontCombo->setCurrentFont(QFont("Segoe UI"));
    m_textFontCombo->setFixedWidth(118);
    m_textFontCombo->setToolTip("Font");
    m_textSizeSpin = new QSpinBox(m_textOptionsWidget);
    m_textSizeSpin->setRange(8, 72);
    m_textSizeSpin->setValue(18);
    m_textSizeSpin->setFixedWidth(54);
    m_textSizeSpin->setToolTip("Font size");
    connect(m_textFontCombo, &QFontComboBox::currentFontChanged, this, [this](const QFont &font) {
        emit textFontFamilyChanged(font.family());
    });
    connect(m_textSizeSpin, qOverload<int>(&QSpinBox::valueChanged),
            this, &AnnotationToolbar::textFontSizeChanged);
    textLayout->addWidget(m_textFontCombo);
    textLayout->addWidget(m_textSizeSpin);
    m_textOptionsWidget->hide();
    m_layout->addWidget(m_textOptionsWidget);

    // Blur strength widget (hidden by default)
    m_blurIntensityWidget = new QWidget(this);
    QHBoxLayout *blurLayout = new QHBoxLayout(m_blurIntensityWidget);
    blurLayout->setContentsMargins(0, 0, 0, 0);
    blurLayout->setSpacing(4);
    m_blurIntensityLabel = new QLabel("16", m_blurIntensityWidget);
    m_blurIntensityLabel->setStyleSheet("color: #aaa; font-size: 11px;");
    m_blurIntensityLabel->setFixedWidth(20);
    m_blurIntensitySlider = new QSlider(Qt::Horizontal, m_blurIntensityWidget);
    m_blurIntensitySlider->setRange(4, 64);
    m_blurIntensitySlider->setValue(16);
    m_blurIntensitySlider->setFixedWidth(80);
    m_blurIntensitySlider->setToolTip(TranslationManager::toolBlurIntensity());
    m_blurIntensitySlider->setStyleSheet(R"(
        QSlider::groove:horizontal {
            background: #404040;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #ff6b6b;
            width: 14px;
            height: 14px;
            margin: -5px 0;
            border-radius: 7px;
        }
        QSlider::handle:horizontal:hover {
            background: #ff8888;
        }
    )");
    connect(m_blurIntensitySlider, &QSlider::valueChanged, this, &AnnotationToolbar::onBlurIntensityChanged);
    blurLayout->addWidget(m_blurIntensitySlider);
    blurLayout->addWidget(m_blurIntensityLabel);
    m_blurIntensityWidget->hide();
    m_layout->addWidget(m_blurIntensityWidget);

    m_layout->addWidget(createSeparator());

    // Undo / Redo
    m_undoButton = createActionButton(":/icons/undo.svg", TranslationManager::toolUndo(), "undo");
    m_redoButton = createActionButton(":/icons/redo.svg", TranslationManager::toolRedo(), "redo");
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
    m_layout->addWidget(m_uploadButton);

    m_gifButton = new QPushButton(this);
    m_gifButton->setIcon(QIcon(":/icons/record.svg"));
    m_gifButton->setIconSize(QSize(18, 18));
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
    m_layout->addWidget(m_gifButton);

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

    // Show/hide blur widget
    if (m_blurIntensityWidget) {
        bool showBlur = (toolId == AnnotationEngine::Blur);
        m_blurIntensityWidget->setVisible(showBlur);
    }
    if (m_textOptionsWidget) {
        m_textOptionsWidget->setVisible(toolId == AnnotationEngine::Text);
    }
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
    else if (action == "gif") emit gifRequested();
}

void AnnotationToolbar::onColorButtonClicked()
{
    QColorDialog dlg(m_currentColor, this);
    dlg.setWindowTitle(TranslationManager::toolColor());
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
}

void AnnotationToolbar::onWidthSliderChanged(int value) { emit penWidthChanged(value); }

void AnnotationToolbar::onBlurIntensityChanged(int value)
{
    if (m_blurIntensityLabel)
        m_blurIntensityLabel->setText(QString::number(value));
    emit blurIntensityChanged(value);
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

void AnnotationToolbar::refreshToolTips()
{
    for (auto it = m_toolButtons.begin(); it != m_toolButtons.end(); ++it) {
        int id = it.key();
        switch (id) {
            case AnnotationEngine::Pen:        it.value()->setToolTip(TranslationManager::toolPen()); break;
            case AnnotationEngine::Arrow:      it.value()->setToolTip(TranslationManager::toolArrow()); break;
            case AnnotationEngine::Line:       it.value()->setToolTip(TranslationManager::toolLine()); break;
            case AnnotationEngine::Rectangle:  it.value()->setToolTip(TranslationManager::toolRect()); break;
            case AnnotationEngine::SemiRect:   it.value()->setToolTip(TranslationManager::toolSemiRect()); break;
            case AnnotationEngine::Circle:     it.value()->setToolTip(TranslationManager::toolCircle()); break;
            case AnnotationEngine::Text:       it.value()->setToolTip(TranslationManager::toolText()); break;
            case AnnotationEngine::Highlighter:it.value()->setToolTip(TranslationManager::toolHighlighter()); break;
            case AnnotationEngine::Blur:       it.value()->setToolTip(TranslationManager::toolBlur()); break;
            case AnnotationEngine::Counter:    it.value()->setToolTip(TranslationManager::toolCounter()); break;
            case AnnotationEngine::Eraser:     it.value()->setToolTip(TranslationManager::toolEraser()); break;
        }
    }
    if (m_actionButtons.contains("undo")) m_actionButtons["undo"]->setToolTip(TranslationManager::toolUndo());
    if (m_actionButtons.contains("redo")) m_actionButtons["redo"]->setToolTip(TranslationManager::toolRedo());
    if (m_colorButton) m_colorButton->setToolTip(TranslationManager::toolColor());
    if (m_eyedropperButton) m_eyedropperButton->setToolTip(TranslationManager::toolEyedropper());
    if (m_lockButton) m_lockButton->setToolTip(TranslationManager::actionLock());
    if (m_ocrButton) m_ocrButton->setToolTip(TranslationManager::actionOcr());
    if (m_uploadButton) m_uploadButton->setToolTip(TranslationManager::uploadToService());
    if (m_gifButton) m_gifButton->setToolTip(TranslationManager::recordingStartTitle());
}
