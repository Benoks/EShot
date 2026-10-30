#include "ControlCenterDialog.h"

#include "../core/TranslationManager.h"
#include "ThemeColors.h"

#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

// The action icons are white line art; draw them dark on the light theme.
QIcon themedIcon(const QIcon &icon)
{
    if (ThemeColors::currentTheme() != ThemeColors::Theme::Light || icon.isNull())
        return icon;
    QPixmap pixmap = icon.pixmap(QSize(36, 36));
    QPainter painter(&pixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), QColor(0x20, 0x20, 0x20));
    painter.end();
    return QIcon(pixmap);
}

QPushButton *createActionButton(const QIcon &icon, const QString &text, QWidget *parent,
                                bool followTheme = true)
{
    auto *button = new QPushButton(followTheme ? themedIcon(icon) : icon, text, parent);
    button->setCursor(Qt::PointingHandCursor);
    button->setIconSize(QSize(18, 18));
    button->setFixedHeight(38);
    return button;
}

}

ControlCenterDialog::ControlCenterDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TranslationManager::appTitle());
    setWindowIcon(QIcon(QStringLiteral(":/icons/pen.svg")));
    setModal(true);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint
                   & ~Qt::WindowMaximizeButtonHint);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(8);
    layout->setSizeConstraint(QLayout::SetFixedSize);

    auto *title = new QLabel(QStringLiteral("EShot"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    title->setMinimumWidth(264);
    layout->addWidget(title);
    layout->addSpacing(4);

    auto *capture = createActionButton(QIcon(QStringLiteral(":/icons/copy.svg")),
                                       TranslationManager::trayCapture(), this);
    // The GIF badge has its own dark background and stays as drawn.
    auto *recordGif = createActionButton(QIcon(QStringLiteral(":/icons/gif.svg")),
                                         TranslationManager::tr("trayRecordGif"), this, false);
    auto *recordVideo = createActionButton(QIcon(QStringLiteral(":/icons/video.svg")),
                                           TranslationManager::tr("trayRecordVideo"), this);
    auto *openFolder = createActionButton(QIcon(QStringLiteral(":/icons/save.svg")),
                                          TranslationManager::tr("trayOpenScreenshotsFolder"), this);
    auto *settings = createActionButton(QIcon(QStringLiteral(":/icons/gear.svg")),
                                        TranslationManager::traySettings(), this);
    auto *about = createActionButton(QIcon(QStringLiteral(":/icons/pen.svg")),
                                     TranslationManager::trayAbout(), this);
    auto *quit = createActionButton(QIcon(QStringLiteral(":/icons/close.svg")),
                                    TranslationManager::trayQuit(), this);

    connect(capture, &QPushButton::clicked, this, [this]() { choose(Action::Capture); });
    connect(recordGif, &QPushButton::clicked, this, [this]() { choose(Action::RecordGif); });
    connect(recordVideo, &QPushButton::clicked, this, [this]() { choose(Action::RecordVideo); });
    connect(openFolder, &QPushButton::clicked, this, [this]() { choose(Action::OpenFolder); });
    connect(settings, &QPushButton::clicked, this, [this]() { choose(Action::Settings); });
    connect(about, &QPushButton::clicked, this, [this]() { choose(Action::About); });
    connect(quit, &QPushButton::clicked, this, [this]() { choose(Action::Quit); });

    layout->addWidget(capture);
    layout->addWidget(recordGif);
    layout->addWidget(recordVideo);
    layout->addWidget(openFolder);
    layout->addSpacing(4);
    layout->addWidget(settings);
    layout->addWidget(about);
    layout->addWidget(quit);
}

ControlCenterDialog::Action ControlCenterDialog::selectedAction() const
{
    return m_selectedAction;
}

void ControlCenterDialog::choose(Action action)
{
    m_selectedAction = action;
    accept();
}
