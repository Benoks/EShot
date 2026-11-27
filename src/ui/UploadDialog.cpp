#include "UploadDialog.h"
#include "core/ImageUploader.h"
#include "core/CatboxUploader.h"
#include "core/TranslationManager.h"
#include "ThemeColors.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QSettings>
#include <QDesktopServices>
#include <QUrl>
#include <QGroupBox>
#include <QPointer>
#include <QShowEvent>
#include <QStackedWidget>
#include <QListWidget>
#include <QMessageBox>
#include <QBuffer>
#include <QLocale>

UploadDialog::UploadDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(TranslationManager::uploadTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    resize(540, 360);

    auto *outer = new QVBoxLayout(this);
    m_pages = new QStackedWidget(this);
    auto *uploadPage = new QWidget(this);
    auto *layout = new QVBoxLayout(uploadPage);
    layout->setContentsMargins(0, 0, 0, 0);

    auto *providerRow = new QHBoxLayout();
    QLabel *providerLabel = new QLabel(TranslationManager::uploadProvider(), this);
    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItem(QStringLiteral("Catbox.moe"),
                              static_cast<int>(ImageUploader::Provider::Catbox));
    m_providerCombo->addItem(TranslationManager::tr("uploadProviderUguu"),
                              static_cast<int>(ImageUploader::Provider::Uguu));
    m_providerCombo->addItem(TranslationManager::tr("uploadProviderTmpFiles"),
                              static_cast<int>(ImageUploader::Provider::TmpFiles));
    m_providerCombo->addItem(TranslationManager::tr("uploadProviderTempSh"),
                              static_cast<int>(ImageUploader::Provider::TempSh));
    m_providerCombo->addItem(QStringLiteral("Allwebs"),
                              static_cast<int>(ImageUploader::Provider::Allwebs));
    // Radikal Cloud is no longer listed: its domain stopped resolving in 2026.
    m_providerCombo->addItem(QStringLiteral("Yandex Disk"),
                              static_cast<int>(ImageUploader::Provider::YandexDisk));
    m_providerCombo->addItem(QStringLiteral("Google Drive"),
                              static_cast<int>(ImageUploader::Provider::GoogleDrive));
    providerRow->addWidget(providerLabel);
    providerRow->addWidget(m_providerCombo, 1);
    layout->addLayout(providerRow);

    m_authBox = new QGroupBox(this);
    auto *authForm = new QFormLayout(m_authBox);
    m_authEdit = new QLineEdit(this);
    m_authEdit->setEchoMode(QLineEdit::Password);
    m_authHelpLabel = new QLabel(this);
    m_authHelpLabel->setWordWrap(true);
    m_authHelpLabel->setOpenExternalLinks(true);
    m_authHelpLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    ThemeColors::setStyleSheet(m_authHelpLabel, QStringLiteral("color: @hint; font-size: 12px;"));
    m_saveAuthBtn = new QPushButton(TranslationManager::save(), this);
    authForm->addRow(QString(), m_authEdit);
    authForm->addRow(QString(), m_authHelpLabel);
    authForm->addRow(QString(), m_saveAuthBtn);
    layout->addWidget(m_authBox);

    auto *btnRow = new QHBoxLayout();
    m_uploadBtn = new QPushButton(TranslationManager::upload(), this);
    m_statusLabel = new QLabel(this);
    btnRow->addWidget(m_uploadBtn);
    btnRow->addWidget(m_statusLabel, 1);
    layout->addLayout(btnRow);

    m_linkEdit = new QLineEdit(this);
    m_linkEdit->setReadOnly(true);
    m_linkEdit->setPlaceholderText(TranslationManager::uploadLinkPlaceholder());
    layout->addWidget(m_linkEdit);

    m_deleteEdit = new QLineEdit(this);
    m_deleteEdit->setReadOnly(true);
    m_deleteEdit->setPlaceholderText(TranslationManager::uploadDeletePlaceholder());
    layout->addWidget(m_deleteEdit);

    auto *linkRow = new QHBoxLayout();
    m_copyLinkBtn = new QPushButton(TranslationManager::copy(), this);
    m_openLinkBtn = new QPushButton(TranslationManager::uploadOpen(), this);
    m_copyLinkBtn->setEnabled(false);
    m_openLinkBtn->setEnabled(false);
    linkRow->addWidget(m_copyLinkBtn);
    linkRow->addWidget(m_openLinkBtn);
    linkRow->addStretch();
    layout->addLayout(linkRow);
    layout->addStretch();
    m_pages->addWidget(uploadPage);

    auto *historyPage = new QWidget(this);
    auto *historyLayout = new QVBoxLayout(historyPage);
    historyLayout->setContentsMargins(0, 0, 0, 0);
    auto *expiryNote = new QLabel(TranslationManager::tr("uploadHistoryExpiryNote"), historyPage);
    expiryNote->setWordWrap(true);
    ThemeColors::setStyleSheet(expiryNote, QStringLiteral("color: @hint; font-size: 12px;"));
    historyLayout->addWidget(expiryNote);
    m_historyList = new QListWidget(historyPage);
    m_historyList->setIconSize(QSize(48, 48));
    historyLayout->addWidget(m_historyList, 1);
    auto *historyRow = new QHBoxLayout();
    m_historyCopyBtn = new QPushButton(TranslationManager::tr("uploadCopyLink"), historyPage);
    m_historyOpenBtn = new QPushButton(TranslationManager::uploadOpen(), historyPage);
    m_historyCopyDeleteBtn = new QPushButton(TranslationManager::tr("uploadCopyDeleteLink"), historyPage);
    m_historyRemoveBtn = new QPushButton(TranslationManager::tr("uploadHistoryRemove"), historyPage);
    historyRow->addWidget(m_historyCopyBtn);
    historyRow->addWidget(m_historyOpenBtn);
    historyRow->addWidget(m_historyCopyDeleteBtn);
    historyRow->addStretch();
    historyRow->addWidget(m_historyRemoveBtn);
    historyLayout->addLayout(historyRow);
    m_pages->addWidget(historyPage);
    outer->addWidget(m_pages, 1);

    auto *bottomRow = new QHBoxLayout();
    m_historyBtn = new QPushButton(TranslationManager::tr("uploadHistory"), this);
    m_clearHistoryBtn = new QPushButton(TranslationManager::tr("uploadHistoryClear"), this);
    m_clearHistoryBtn->setVisible(false);
    m_historyStatusLabel = new QLabel(this);
    m_closeBtn = new QPushButton(TranslationManager::ocrClose(), this);
    bottomRow->addWidget(m_historyBtn);
    bottomRow->addWidget(m_clearHistoryBtn);
    bottomRow->addWidget(m_historyStatusLabel, 1);
    bottomRow->addWidget(m_closeBtn);
    outer->addLayout(bottomRow);

    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UploadDialog::onProviderChanged);
    connect(m_uploadBtn, &QPushButton::clicked, this, &UploadDialog::onUploadClicked);
    connect(m_copyLinkBtn, &QPushButton::clicked, this, &UploadDialog::onCopyLink);
    connect(m_openLinkBtn, &QPushButton::clicked, this, &UploadDialog::onOpenLink);
    connect(m_saveAuthBtn, &QPushButton::clicked, this, &UploadDialog::onSaveAuth);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_historyBtn, &QPushButton::clicked, this, &UploadDialog::onToggleHistory);
    connect(m_clearHistoryBtn, &QPushButton::clicked, this, &UploadDialog::onClearHistory);
    connect(m_historyList, &QListWidget::currentRowChanged, this, &UploadDialog::updateHistoryButtons);
    auto copyFromHistory = [this](bool deleteLink) {
        const UploadHistory::Entry *e = currentHistoryEntry();
        if (!e) return;
        QGuiApplication::clipboard()->setText(deleteLink ? e->deleteUrl : e->url);
        m_historyStatusLabel->setText(deleteLink ? TranslationManager::tr("uploadDeleteLinkCopied")
                                                 : TranslationManager::uploadLinkCopied());
    };
    connect(m_historyCopyBtn, &QPushButton::clicked, this, [copyFromHistory] { copyFromHistory(false); });
    connect(m_historyCopyDeleteBtn, &QPushButton::clicked, this, [copyFromHistory] { copyFromHistory(true); });
    auto openFromHistory = [this] {
        if (const UploadHistory::Entry *e = currentHistoryEntry())
            QDesktopServices::openUrl(QUrl(e->url));
    };
    connect(m_historyOpenBtn, &QPushButton::clicked, this, openFromHistory);
    connect(m_historyList, &QListWidget::itemActivated, this, openFromHistory);
    connect(m_historyRemoveBtn, &QPushButton::clicked, this, [this] {
        if (!currentHistoryEntry()) return;
        const int row = m_historyList->currentRow();
        m_history.remove(row);
        refreshHistory();
        m_historyList->setCurrentRow(qMin(row, int(m_history.entries().size()) - 1));
    });

    QSettings s("EShot", "EShot");
    int provIdx = m_providerCombo->findData(s.value("uploadProvider", 0).toInt());
    if (provIdx < 0) provIdx = 0;
    m_providerCombo->setCurrentIndex(provIdx);
    rebuildUploader();
}

UploadDialog::~UploadDialog()
{
    if (m_uploader) {
        m_uploader->cancel();
    }
}

void UploadDialog::setImage(const QPixmap &pixmap)
{
    m_pixmap = pixmap;
    if (m_uploader) m_uploader->setImage(pixmap);
}

void UploadDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    if (m_autoUploadTried) return;
    m_autoUploadTried = true;
    // Start right away unless the service still needs a credential typed in.
    if (m_uploader && !m_pixmap.isNull()
        && (!m_uploader->authRequired() || !m_uploader->authValue().isEmpty()))
        onUploadClicked();
}

void UploadDialog::setBusy(bool busy)
{
    m_uploadBtn->setEnabled(!busy);
    m_saveAuthBtn->setEnabled(!busy);
    m_providerCombo->setEnabled(!busy);
}

void UploadDialog::onProviderChanged(int index)
{
    QSettings s("EShot", "EShot");
    s.setValue("uploadProvider", m_providerCombo->itemData(index));
    rebuildUploader();
}

void UploadDialog::rebuildUploader()
{
    if (m_uploader) {
        m_uploader->cancel();
        m_uploader->deleteLater();
        m_uploader = nullptr;
    }

    auto provider = static_cast<ImageUploader::Provider>(m_providerCombo->currentData().toInt());
    m_uploader = ImageUploader::create(provider, this);
    if (!m_uploader) {
        m_statusLabel->setText(TranslationManager::uploadFailed());
        return;
    }

    QPointer<UploadDialog> self(this);
    connect(m_uploader, &ImageUploader::uploading, this, &UploadDialog::onUploading);
    connect(m_uploader, &ImageUploader::succeeded, this,
            [self](const QString &url, const QString &del) {
        if (self) self->onSucceeded(url, del);
    });
    connect(m_uploader, &ImageUploader::failed, this,
            [self](const QString &reason) {
        if (self) self->onFailed(reason);
    });

    if (!m_pixmap.isNull()) {
        m_uploader->setImage(m_pixmap);
    }
    rebuildAuthSection();
}

void UploadDialog::rebuildAuthSection()
{
    if (!m_uploader) return;
    m_authBox->setVisible(m_uploader->needsAuth());
    if (m_uploader->needsAuth()) {
        m_authEdit->setVisible(true);
        m_saveAuthBtn->setVisible(true);
        m_authEdit->setText(m_uploader->authValue());
        m_authEdit->setPlaceholderText(m_uploader->authPlaceholder());
        updateAuthHelp();
    } else {
        m_authEdit->clear();
        m_authEdit->setVisible(false);
        m_authHelpLabel->clear();
        m_authHelpLabel->setVisible(false);
        m_saveAuthBtn->setVisible(false);
    }
}

void UploadDialog::updateAuthHelp()
{
    if (!m_uploader || !m_authHelpLabel) return;

    QString text;
    switch (m_uploader->provider()) {
    case ImageUploader::Provider::YandexDisk:
        text = TranslationManager::uploadAuthHelpYandex();
        break;
    case ImageUploader::Provider::GoogleDrive:
        text = TranslationManager::uploadAuthHelpGoogleDrive();
        break;
    case ImageUploader::Provider::Allwebs:
    case ImageUploader::Provider::RadikalCloud:
        text = TranslationManager::uploadAuthHelpApiKey(m_uploader->providerDisplayName());
        break;
    case ImageUploader::Provider::Catbox:
        text = TranslationManager::uploadAuthHelpCatbox();
        break;
    default:
        break;
    }

    m_authHelpLabel->setText(text);
    m_authHelpLabel->setVisible(!text.isEmpty());
}

void UploadDialog::onSaveAuth()
{
    if (!m_uploader) return;
    const bool stored = m_uploader->setAuthValue(m_authEdit->text());
    m_authEdit->setText(m_uploader->authValue());
    // Without a working keyring the value only lasts for this session.
    m_statusLabel->setText(stored ? TranslationManager::uploadAuthSaved()
                                  : TranslationManager::uploadAuthSaveFailed());
}

void UploadDialog::onUploadClicked()
{
    if (!m_uploader) return;
    if (m_pixmap.isNull()) {
        m_statusLabel->setText(TranslationManager::uploadFailed());
        return;
    }
    m_linkEdit->clear();
    m_deleteEdit->clear();
    m_copyLinkBtn->setEnabled(false);
    m_openLinkBtn->setEnabled(false);
    if (m_uploader->needsAuth()) {
        m_uploader->setAuthValue(m_authEdit->text());
        m_authEdit->setText(m_uploader->authValue());
    }
    m_uploader->setImage(m_pixmap);
    m_uploader->upload();
}

void UploadDialog::onUploading()
{
    setBusy(true);
    m_statusLabel->setText(TranslationManager::uploadUploading());
}

void UploadDialog::onSucceeded(const QString &url, const QString &deleteUrl)
{
    setBusy(false);
    m_linkEdit->setText(url);
    m_deleteEdit->setText(deleteUrl);
    m_deleteEdit->setVisible(!deleteUrl.isEmpty());
    m_copyLinkBtn->setEnabled(true);
    m_openLinkBtn->setEnabled(true);
    onCopyLink();

    UploadHistory::Entry entry;
    entry.time = QDateTime::currentDateTimeUtc();
    entry.provider = static_cast<int>(m_uploader->provider());
    entry.providerName = m_uploader->providerDisplayName();
    entry.url = url;
    entry.deleteUrl = deleteUrl;
    entry.imageSize = m_pixmap.size();
    QBuffer thumbnail(&entry.thumbnailPng);
    thumbnail.open(QIODevice::WriteOnly);
    m_pixmap.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation).save(&thumbnail, "PNG");
    m_history.add(entry);
    refreshHistory();
}

void UploadDialog::onFailed(const QString &reason)
{
    setBusy(false);
    m_statusLabel->setText(TranslationManager::uploadFailed() + QStringLiteral(" - ") + reason);
}

void UploadDialog::onCopyLink()
{
    QString link = m_linkEdit->text();
    if (link.isEmpty()) return;
    QGuiApplication::clipboard()->setText(link);
    m_statusLabel->setText(TranslationManager::uploadLinkCopied());
}

void UploadDialog::onOpenLink()
{
    QString link = m_linkEdit->text();
    if (link.isEmpty()) return;
    QDesktopServices::openUrl(QUrl(link));
}

void UploadDialog::onToggleHistory()
{
    const bool showHistory = m_pages->currentIndex() == 0;
    m_pages->setCurrentIndex(showHistory ? 1 : 0);
    m_historyBtn->setText(showHistory ? TranslationManager::tr("uploadHistoryBack")
                                      : TranslationManager::tr("uploadHistory"));
    m_clearHistoryBtn->setVisible(showHistory);
    m_historyStatusLabel->clear();
    if (showHistory) {
        refreshHistory();
        m_historyList->setCurrentRow(m_history.entries().isEmpty() ? -1 : 0);
    }
}

void UploadDialog::onClearHistory()
{
    if (QMessageBox::question(this, TranslationManager::tr("uploadHistoryClear"),
                              TranslationManager::tr("uploadHistoryClearConfirm"),
                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes)
        return;
    m_history.clear();
    refreshHistory();
}

void UploadDialog::refreshHistory()
{
    const QSignalBlocker blocker(m_historyList);
    m_historyList->clear();
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const UploadHistory::Entry &e : m_history.entries()) {
        // The combo text is translated and names the expiry, e.g. "Uguu.se (3 hours)".
        const int comboIndex = m_providerCombo->findData(e.provider);
        QString text = QStringLiteral("%1 · %2 · %3×%4\n%5")
                           .arg(QLocale().toString(e.time.toLocalTime(), QLocale::ShortFormat),
                                comboIndex >= 0 ? m_providerCombo->itemText(comboIndex) : e.providerName)
                           .arg(e.imageSize.width())
                           .arg(e.imageSize.height())
                           .arg(e.url);
        if (UploadHistory::likelyExpired(e, now))
            text += QStringLiteral("\n") + TranslationManager::tr("uploadHistoryExpired");
        auto *item = new QListWidgetItem(text, m_historyList);
        QPixmap thumbnail;
        if (thumbnail.loadFromData(e.thumbnailPng, "PNG"))
            item->setIcon(QIcon(thumbnail));
        item->setToolTip(e.url);
    }
    if (m_history.entries().isEmpty()) {
        auto *item = new QListWidgetItem(TranslationManager::tr("uploadHistoryEmpty"), m_historyList);
        item->setFlags(Qt::NoItemFlags);
    }
    updateHistoryButtons();
}

const UploadHistory::Entry *UploadDialog::currentHistoryEntry() const
{
    const int row = m_historyList->currentRow();
    if (row < 0 || row >= m_history.entries().size()) return nullptr;
    return &m_history.entries().at(row);
}

void UploadDialog::updateHistoryButtons()
{
    const UploadHistory::Entry *e = currentHistoryEntry();
    m_historyCopyBtn->setEnabled(e);
    m_historyOpenBtn->setEnabled(e);
    m_historyCopyDeleteBtn->setEnabled(e && !e->deleteUrl.isEmpty());
    m_historyRemoveBtn->setEnabled(e);
    m_clearHistoryBtn->setEnabled(!m_history.entries().isEmpty());
}
