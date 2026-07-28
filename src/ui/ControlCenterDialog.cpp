#include "ControlCenterDialog.h"

#include "../core/TranslationManager.h"

#include <QIcon>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

QPushButton *createActionButton(const QIcon &icon, const QString &text, QWidget *parent)
{
    auto *button = new QPushButton(icon, text, parent);
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
    setFixedWidth(300);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint
                   & ~Qt::WindowMaximizeButtonHint);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(8);

    auto *title = new QLabel(QStringLiteral("EShot"), this);
    QFont titleFont = title->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);
    layout->addSpacing(4);

    auto *capture = createActionButton(QIcon(QStringLiteral(":/icons/copy.svg")),
                                       TranslationManager::trayCapture(), this);
    auto *settings = createActionButton(QIcon(QStringLiteral(":/icons/gear.svg")),
                                        TranslationManager::traySettings(), this);
    auto *about = createActionButton(QIcon(QStringLiteral(":/icons/pen.svg")),
                                     TranslationManager::trayAbout(), this);
    auto *quit = createActionButton(QIcon(QStringLiteral(":/icons/close.svg")),
                                    TranslationManager::trayQuit(), this);

    connect(capture, &QPushButton::clicked, this, [this]() { choose(Action::Capture); });
    connect(settings, &QPushButton::clicked, this, [this]() { choose(Action::Settings); });
    connect(about, &QPushButton::clicked, this, [this]() { choose(Action::About); });
    connect(quit, &QPushButton::clicked, this, [this]() { choose(Action::Quit); });

    layout->addWidget(capture);
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
