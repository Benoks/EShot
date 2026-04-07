#include "AnnotationToolbar.h"
#include "annotation/AnnotationEngine.h"
#include <QColorDialog>
#include <QSlider>
#include <QLabel>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QStyle>
#include <QSettings>

AnnotationToolbar::AnnotationToolbar(QWidget *parent)
    : QWidget(parent)
    , m_layout(nullptr)
    , m_colorButton(nullptr)
    , m_currentToolId(-1)
    , m_currentColor(Qt::red)
{
    if (!parent) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground);
    } else {
        setAttribute(Qt::WA_TranslucentBackground, false);
    }
    setFixedHeight(48);

    // Görünür araçları yükle
    QSettings s("EShot", "EShot");
    m_visibleTools = s.value("visibleTools",
        QStringList{"Pen","Arrow","Rectangle","Circle","Text","Highlighter","Blur","Counter"})
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
        QStringList{"Pen","Arrow","Rectangle","Circle","Text","Highlighter","Blur","Counter"})
        .toStringList();

    for (auto btn : m_toolButtons) {
        QString key = btn->property("settingsKey").toString();
        if (!key.isEmpty()) {
            btn->setVisible(isToolVisible(key));
        }
    }
    adjustSize();
}

QWidget* AnnotationToolbar::createSeparator()
{
    QFrame *sep = new QFrame(this);
    sep->setFrameShape(QFrame::VLine);
    sep->setFixedWidth(1);
    sep->setFixedHeight(24);
    sep->setStyleSheet("background-color: #555;");
    return sep;
}

void AnnotationToolbar::setupUI()
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(8, 6, 8, 6);
    m_layout->setSpacing(4);

    // Araçlar — sadece görünür olanlar
    // Araçlar — hepsini oluştur, görünürlüğü refreshTools yönetecek
    m_layout->addWidget(createToolButton(":/icons/pen.svg", "Kalem (P)", AnnotationEngine::Pen, "Pen"));
    m_layout->addWidget(createToolButton(":/icons/arrow.svg", "Ok (A)", AnnotationEngine::Arrow, "Arrow"));
    m_layout->addWidget(createToolButton(":/icons/rectangle.svg", "Dikdörtgen (R)", AnnotationEngine::Rectangle, "Rectangle"));
    m_layout->addWidget(createToolButton(":/icons/circle.svg", "Çember (C) [Shift=Tam Daire]", AnnotationEngine::Circle, "Circle"));
    m_layout->addWidget(createToolButton(":/icons/text.svg", "Metin (T)", AnnotationEngine::Text, "Text"));
    m_layout->addWidget(createToolButton(":/icons/highlighter.svg", "Vurgulayıcı (H)", AnnotationEngine::Highlighter, "Highlighter"));
    m_layout->addWidget(createToolButton(":/icons/blur.svg", "Bulanıklaştır (B)", AnnotationEngine::Blur, "Blur"));
    m_layout->addWidget(createToolButton(":/icons/counter.svg", "Numara (#)", AnnotationEngine::Counter, "Counter"));

    refreshTools(); // İlk görünürlük ayarını yap

    m_layout->addWidget(createSeparator());

    // Renk
    m_colorButton = createColorButton(m_currentColor);
    m_layout->addWidget(m_colorButton);

    m_layout->addWidget(createSeparator());

    // Kalınlık
    QSlider *slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(1, 20);
    slider->setValue(3);
    slider->setFixedWidth(70);
    slider->setToolTip("Çizgi Kalınlığı");
    slider->setStyleSheet(R"(
        QSlider::groove:horizontal { background: #444; height: 4px; border-radius: 2px; }
        QSlider::handle:horizontal { background: #0078D4; width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; }
        QSlider::handle:horizontal:hover { background: #1a8cff; }
    )");
    connect(slider, &QSlider::valueChanged, this, &AnnotationToolbar::onWidthSliderChanged);
    m_layout->addWidget(slider);

    m_layout->addWidget(createSeparator());

    // Undo/Redo
    m_layout->addWidget(createActionButton(":/icons/undo.svg", "Geri Al (Ctrl+Z)", "undo"));
    m_layout->addWidget(createActionButton(":/icons/redo.svg", "İleri Al (Ctrl+Y)", "redo"));

    m_layout->addWidget(createSeparator());
    m_layout->addStretch();

    // Pin
    m_layout->addWidget(createActionButton(":/icons/pin.svg", "Ekrana Sabitle 📌", "pin"));

    m_layout->addWidget(createSeparator());

    // Ana eylemler
    m_layout->addWidget(createActionButton(":/icons/copy.svg", "Kopyala (Ctrl+C / Enter)", "copy"));
    m_layout->addWidget(createActionButton(":/icons/save.svg", "Kaydet (Ctrl+S)", "save"));
    m_layout->addWidget(createActionButton(":/icons/close.svg", "Kapat (Esc)", "close"));

    adjustSize();
}

void AnnotationToolbar::applyStyles()
{
    setStyleSheet(R"(
        AnnotationToolbar {
            background-color: #1a1a1a;
            border: 1px solid #3d3d3d;
            border-radius: 10px;
        }
        QToolTip {
            color: #ffffff;
            background-color: #333333;
            border: 1px solid #555555;
            padding: 4px;
            font-size: 12px;
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
    btn->setFixedSize(32, 32);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(18, 18));
    btn->setStyleSheet(R"(
        QPushButton { background-color: transparent; border: none; border-radius: 6px; }
        QPushButton:hover { background-color: #3d3d3d; }
        QPushButton:pressed { background-color: #2d2d2d; }
        QPushButton[selected="true"] { background-color: #0078D4; border: none; }
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
    btn->setFixedSize(32, 32);
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(QSize(18, 18));

    QString bg = "transparent", hv = "#3d3d3d";
    if (action == "copy") { bg = "#0078D4"; hv = "#1084D8"; }
    else if (action == "save") { bg = "#107C10"; hv = "#1a8c1a"; }
    else if (action == "close") { bg = "#C42B1C"; hv = "#d43c2d"; }
    else if (action == "pin") { bg = "#6B4C9A"; hv = "#7D5CB5"; }

    btn->setStyleSheet(QString(R"(
        QPushButton { background-color: %1; border: none; border-radius: 6px; }
        QPushButton:hover { background-color: %2; }
        QPushButton:pressed { background-color: %1; }
    )").arg(bg, hv));

    connect(btn, &QPushButton::clicked, this, &AnnotationToolbar::onActionButtonClicked);
    return btn;
}

QPushButton* AnnotationToolbar::createColorButton(const QColor &color)
{
    QPushButton *btn = new QPushButton(this);
    btn->setToolTip("Renk Seç");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(26, 26);
    btn->setStyleSheet(QString(R"(
        QPushButton { background-color: %1; border: 2px solid #555; border-radius: 13px; }
        QPushButton:hover { border-color: #fff; }
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
    emit toolSelected(toolId);
}

void AnnotationToolbar::onActionButtonClicked()
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;
    QString action = btn->property("action").toString();

    if (action == "copy") emit copyRequested();
    else if (action == "save") emit saveRequested();
    else if (action == "close") emit closeRequested();
    else if (action == "pin") emit pinRequested();
    else if (action == "undo") emit undoRequested();
    else if (action == "redo") emit redoRequested();
}

void AnnotationToolbar::onColorButtonClicked()
{
    QColorDialog dlg(m_currentColor, this);
    dlg.setWindowTitle("Renk Seç");
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