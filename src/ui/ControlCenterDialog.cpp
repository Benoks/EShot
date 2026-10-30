#include "ControlCenterDialog.h"

#include "../core/TranslationManager.h"
#include "ThemeColors.h"

#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QPushButton *createActionButton(const QIcon &icon, const QString &text, QWidget *parent,
                                bool followTheme = true)
{
    auto *button = new QPushButton(followTheme ? ThemeColors::iconForTheme(icon, QSize(36, 36)) : icon, text, parent);
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
    auto *reportProblem = createActionButton(QIcon(QStringLiteral(":/icons/bug.svg")),
                                             TranslationManager::reportProblem(), this);
    auto *quit = createActionButton(QIcon(QStringLiteral(":/icons/close.svg")),
                                    TranslationManager::trayQuit(), this);

    connect(capture, &QPushButton::clicked, this, [this]() { choose(Action::Capture); });
    connect(recordGif, &QPushButton::clicked, this, [this]() { choose(Action::RecordGif); });
    connect(recordVideo, &QPushButton::clicked, this, [this]() { choose(Action::RecordVideo); });
    connect(openFolder, &QPushButton::clicked, this, [this]() { choose(Action::OpenFolder); });
    connect(settings, &QPushButton::clicked, this, [this]() { choose(Action::Settings); });
    connect(about, &QPushButton::clicked, this, [this]() { choose(Action::About); });
    connect(reportProblem, &QPushButton::clicked, this, [this]() { choose(Action::ReportProblem); });
    connect(quit, &QPushButton::clicked, this, [this]() { choose(Action::Quit); });

    layout->addWidget(capture);
    layout->addWidget(recordGif);
    layout->addWidget(recordVideo);
    layout->addWidget(openFolder);
    layout->addWidget(settings);
    layout->addWidget(about);
    layout->addWidget(reportProblem);
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
