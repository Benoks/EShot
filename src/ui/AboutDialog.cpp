#include "AboutDialog.h"
#include "../core/TranslationManager.h"
#include "../core/BugReport.h"
#include "ThemeColors.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QApplication>
#include <QPainter>
#include <QIcon>
#include <QDesktopServices>

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(TranslationManager::aboutTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    // Strip context help and maximize button (to prevent ARM64 Snap Layouts crash)
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint & ~Qt::WindowMaximizeButtonHint);

    setupUI();
    // Grows with large fonts, display scaling and long localized status text
    // instead of clipping them.
    setMinimumSize(380, 370);
    adjustSize();
}

AboutDialog::~AboutDialog() {}

void AboutDialog::setUpdateStatus(const QString &text, bool highlighted)
{
    if (!m_updateStatusLabel) return;
    m_updateStatusLabel->setText(text);
    ThemeColors::setStyleSheet(m_updateStatusLabel,
                               highlighted ? QStringLiteral("color: @warning; font-size: 12px;")
                                           : QStringLiteral("color: @hint; font-size: 12px;"));
    if (isVisible())
        adjustSize();
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
        QPixmap pix = ThemeColors::iconForTheme(appIcon, QSize(96, 96)).pixmap(96, 96);
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
    layout->addWidget(nameLabel);

    // Version
    QLabel *verLabel = new QLabel(QString("%1 %2").arg(TranslationManager::version(), QApplication::applicationVersion()));
    verLabel->setAlignment(Qt::AlignCenter);
    ThemeColors::setStyleSheet(verLabel, "color: @hint; font-size: 12px; margin-bottom: 2px;");
    layout->addWidget(verLabel);

    // Description
    QLabel *descLabel = new QLabel(TranslationManager::aboutDesc());
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    ThemeColors::setStyleSheet(descLabel, "color: @secondary; font-size: 12px;");
    layout->addWidget(descLabel);

    // GitHub link
    QLabel *linkLabel = new QLabel(
        QStringLiteral("<a href='https://github.com/Benoks/EShot' style='color: %1; text-decoration: none;'>"
                       "GitHub</a>").arg(ThemeColors::css(ThemeColors::Role::Link)));
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
    ThemeColors::setStyleSheet(m_updateStatusLabel, "color: @hint; font-size: 12px;");
    m_updateStatusLabel->setMinimumHeight(34);
    layout->addWidget(m_updateStatusLabel);

    // Update check
    m_checkUpdateBtn = new QPushButton(TranslationManager::checkForUpdates());
    m_checkUpdateBtn->setCursor(Qt::PointingHandCursor);
    m_checkUpdateBtn->setMinimumHeight(32);
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
        QPushButton:disabled { background-color: #555; color: #c8c8c8; }
    )");
    connect(m_checkUpdateBtn, &QPushButton::clicked, this, &AboutDialog::onCheckForUpdates);
    layout->addWidget(m_checkUpdateBtn);

    QPushButton *reportBtn = new QPushButton(TranslationManager::reportProblem());
    reportBtn->setCursor(Qt::PointingHandCursor);
    reportBtn->setMinimumHeight(32);
    connect(reportBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(bugReportUrl(currentBugReportInfo()));
    });
    layout->addWidget(reportBtn);

    // Close: a plain dialog button, so it follows the dark/light/high-contrast palette.
    QPushButton *closeBtn = new QPushButton(TranslationManager::ocrClose());
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setMinimumHeight(32);
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
    setUpdateStatus(visibleStatus, available);
    if (!m_checkUpdateBtn) return;
    m_checkUpdateBtn->setEnabled(!busy);
    m_checkUpdateBtn->setText(available ? TranslationManager::updateNow()
                                       : TranslationManager::checkForUpdates());
}
