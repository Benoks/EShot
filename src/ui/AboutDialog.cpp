#include "AboutDialog.h"
#include "../core/TranslationManager.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QPixmap>
#include <QApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPainter>
#include <QIcon>
#include <QVersionNumber>
#include <QNetworkRequest>
#include <QUrl>
#include <QDateTime>
#include <QList>
#include <algorithm>

static bool isNewerVersion(const QString &latest, const QString &current)
{
    QVersionNumber latestVersion = QVersionNumber::fromString(latest.trimmed());
    QVersionNumber currentVersion = QVersionNumber::fromString(current.trimmed());
    if (latestVersion.isNull() || currentVersion.isNull())
        return latest.trimmed() != current.trimmed();
    return QVersionNumber::compare(latestVersion, currentVersion) > 0;
}

AboutDialog::AboutDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(TranslationManager::aboutTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setFixedSize(380, 370);
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

    setupUI();
}

AboutDialog::~AboutDialog() {}

static QString updateStatusText(const QString &currentVersion, const QString &latestVersion, bool newer)
{
    if (TranslationManager::currentLanguage() == TranslationManager::Turkish) {
        return newer
            ? QString::fromUtf8("Yeni sürüm var: v%1 (yüklü: v%2). İndirmek için GitHub bağlantısını kullanın.")
                  .arg(latestVersion, currentVersion)
            : QString::fromUtf8("Güncel: v%1 (son sürüm: v%2)").arg(currentVersion, latestVersion);
    }

    return newer
        ? QStringLiteral("New version available: v%1 (installed: v%2). Use the GitHub link to download.")
              .arg(latestVersion, currentVersion)
        : QStringLiteral("Up to date: v%1 (latest: v%2)").arg(currentVersion, latestVersion);
}

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
        TranslationManager::currentLanguage() == TranslationManager::Turkish
            ? QString::fromUtf8("Yüklü sürüm: v%1").arg(QApplication::applicationVersion())
            : QStringLiteral("Installed version: v%1").arg(QApplication::applicationVersion()));
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

    static QList<QDateTime> recentChecks;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    recentChecks.erase(std::remove_if(recentChecks.begin(), recentChecks.end(),
        [now](const QDateTime &dt) { return dt.secsTo(now) > 600; }), recentChecks.end());
    if (recentChecks.size() >= 3) {
        setUpdateStatus(
            TranslationManager::currentLanguage() == TranslationManager::Turkish
                ? QString::fromUtf8("GÃ¼ncelleme kontrolÃ¼ sÄ±nÄ±rÄ±na ulaÅŸÄ±ldÄ±. BirkaÃ§ dakika sonra tekrar deneyin.")
                : QStringLiteral("Update check limit reached. Try again in a few minutes."),
            QStringLiteral("#ff9800"));
        return;
    }
    recentChecks.append(now);

    m_checkUpdateBtn->setEnabled(false);
    m_checkUpdateBtn->setText(QStringLiteral("..."));
    setUpdateStatus(
        TranslationManager::currentLanguage() == TranslationManager::Turkish
            ? QString::fromUtf8("Güncelleme kontrol ediliyor...")
            : QStringLiteral("Checking for updates..."),
        QStringLiteral("#888888"));

    if (!m_updateManager) {
        m_updateManager = new QNetworkAccessManager(this);
        connect(m_updateManager, &QNetworkAccessManager::finished,
                this, &AboutDialog::onUpdateReplyFinished);
    }

    QNetworkRequest request(QUrl(QStringLiteral("https://api.github.com/repos/Benoks/EShot/releases/latest")));
    request.setRawHeader("Accept", "application/vnd.github+json");
    m_updateManager->get(request);
}

void AboutDialog::onUpdateReplyFinished(QNetworkReply *reply)
{
    if (!reply) return;

    const QString currentVersion = QApplication::applicationVersion();
    QString statusColor = QStringLiteral("#ff5252");
    QString statusText;

    if (reply->error() == QNetworkReply::NoError) {
        QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
        QString latestTag;
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            latestTag = obj.value(QStringLiteral("tag_name")).toString();
        }

        if (latestTag.startsWith("v", Qt::CaseInsensitive))
            latestTag = latestTag.mid(1);

        if (!latestTag.isEmpty()) {
            const bool newer = isNewerVersion(latestTag, currentVersion);
            statusText = updateStatusText(currentVersion, latestTag, newer);
            statusColor = newer ? QStringLiteral("#ff9800") : QStringLiteral("#4caf50");
        } else {
            statusText = TranslationManager::currentLanguage() == TranslationManager::Turkish
                ? QString::fromUtf8("Son sürüm bilgisi okunamadı. GitHub bağlantısını kontrol edin.")
                : QStringLiteral("Could not read latest version. Please check the GitHub link.");
        }
    } else {
        statusText = TranslationManager::currentLanguage() == TranslationManager::Turkish
            ? QString::fromUtf8("Güncelleme kontrol edilemedi. GitHub bağlantısını kontrol edin.")
            : QStringLiteral("Could not check for updates. Please check the GitHub link.");
    }

    setUpdateStatus(statusText, statusColor);

    if (m_checkUpdateBtn) {
        m_checkUpdateBtn->setEnabled(true);
        m_checkUpdateBtn->setText(TranslationManager::checkForUpdates());
    }
    reply->deleteLater();
}
