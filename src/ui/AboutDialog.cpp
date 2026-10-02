#include "AboutDialog.h"
#include "../core/TranslationManager.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QApplication>
#include <QPainter>
#include <QIcon>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(TranslationManager::aboutTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setFixedSize(380, 370);
    // Strip context help and maximize button (to prevent ARM64 Snap Layouts crash)
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint);

    setupUI();
}

AboutDialog::~AboutDialog() {}

void AboutDialog::setUpdateStatus(const QString &text, const QString &color)
{
    if (!m_updateStatusLabel) return;
    m_updateStatusLabel->setText(text);
    m_updateStatusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 11px;").arg(color));
}

void AboutDialog::setupUI()
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(6);
    layout->setContentsMargins(24, 20, 24, 20);

    // SVG ikon — high-DPI render
    QLabel *iconLabel = new QLabel();
    QIcon appIcon(":/icons/pen.svg");
    if (!appIcon.isNull()) {
        QPixmap pix = appIcon.pixmap(96, 96);
        iconLabel->setPixmap(pix);
    }
    iconLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(iconLabel);

    // Name
    QLabel *nameLabel = new QLabel("EShot");
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(18);
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    nameLabel->setAlignment(Qt::AlignCenter);
    nameLabel->setStyleSheet("color: #ffffff;");
    layout->addWidget(nameLabel);

    // Version
    QLabel *verLabel = new QLabel(QString("%1 %2").arg(TranslationManager::version(), QApplication::applicationVersion()));
    verLabel->setAlignment(Qt::AlignCenter);
    verLabel->setStyleSheet("color: #999999; font-size: 11px; margin-bottom: 2px;");
    layout->addWidget(verLabel);

    // Description
    QLabel *descLabel = new QLabel(TranslationManager::aboutDesc());
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setStyleSheet("color: #bbbbbb; font-size: 12px;");
    layout->addWidget(descLabel);

    // GitHub link
    QLabel *linkLabel = new QLabel(
        "<a href='https://github.com/Benoks/EShot' style='color: #5B9BD5; text-decoration: none;'>"
        "GitHub</a>"
    );
    linkLabel->setAlignment(Qt::AlignCenter);
    linkLabel->setOpenExternalLinks(true);
    linkLabel->setStyleSheet("font-size: 11px; margin-top: 4px;");
    layout->addWidget(linkLabel);

    layout->addStretch();

    // Update status
    m_updateStatusLabel = new QLabel(
        TranslationManager::tr("installedVersion").arg(QApplication::applicationVersion()));
    m_updateStatusLabel->setAlignment(Qt::AlignCenter);
    m_updateStatusLabel->setWordWrap(true);
    m_updateStatusLabel->setStyleSheet("color: #888888; font-size: 11px;");
    m_updateStatusLabel->setMinimumHeight(34);
    layout->addWidget(m_updateStatusLabel);

    // Update check
    m_checkUpdateBtn = new QPushButton(TranslationManager::checkForUpdates());
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    m_checkUpdateBtn->setFixedHeight(32);
    m_checkUpdateBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #0078D4;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 16px;
            font-weight: bold;
            font-size: 12px;
        }
        QPushButton:hover { background-color: #1a8cff; }
        QPushButton:disabled { background-color: #555; color: #aaa; }
    )");
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &AboutDialog::onCheckForUpdates);
    layout->addWidget(m_checkUpdateBtn);

    // Close
    QPushButton *closeBtn = new QPushButton(TranslationManager::cancel());
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setFixedHeight(32);
    closeBtn->setStyleSheet(R"(
        QPushButton {
            background-color: #3a3a3a;
            color: white;
            border: 1px solid #505050;
            border-radius: 4px;
            padding: 6px 12px;
            font-size: 12px;
        }
        QPushButton:hover { background-color: #4a4a4a; border-color: #666; }
    )");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn);
}

void AboutDialog::onCheckForUpdates()
{
    if (!m_checkUpdateBtn) return;
    if (m_updateAvailable)
        emit updateRequested();
    else
        emit checkForUpdatesRequested();
}

void AboutDialog::setUpdateInfo(bool available, const QString &version, bool busy, const QString &status)
{
    m_updateAvailable = available;
    const QString visibleStatus = status.isEmpty()
        ? (available ? TranslationManager::updateStatusAvailable(version)
                     : TranslationManager::updateStatusIdle())
        : status;
    setUpdateStatus(visibleStatus,
                    available ? QStringLiteral("#ff9800") : QStringLiteral("#888888"));
    if (!m_checkUpdateBtn) return;
    m_checkUpdateBtn->setEnabled(!busy);
    m_checkUpdateBtn->setText(available ? TranslationManager::updateNow()
                                       : TranslationManager::checkForUpdates());
}
