#include "UploadDialog.h"
#include "core/ImageUploader.h"
#include "core/CatboxUploader.h"
#include "core/TranslationManager.h"
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

UploadDialog::UploadDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(TranslationManager::uploadTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    resize(540, 360);

    auto *layout = new QVBoxLayout(this);

    auto *providerRow = new QHBoxLayout();
    QLabel *providerLabel = new QLabel(TranslationManager::uploadProvider(), this);
    m_providerCombo = new QComboBox(this);
    m_providerCombo->addItem(QStringLiteral("Catbox.moe"),
                              static_cast<int>(ImageUploader::Provider::Catbox));
    m_providerCombo->addItem(QStringLiteral("Uguu.se (3 hours)"),
                              static_cast<int>(ImageUploader::Provider::Uguu));
    providerRow->addWidget(providerLabel);
    providerRow->addWidget(m_providerCombo, 1);
    layout->addLayout(providerRow);

    auto *authBox = new QGroupBox(this);
    auto *authForm = new QFormLayout(authBox);
    m_authEdit = new QLineEdit(this);
    m_authEdit->setEchoMode(QLineEdit::Password);
    m_saveAuthBtn = new QPushButton(TranslationManager::save(), this);
    authForm->addRow(QString(), m_authEdit);
    authForm->addRow(QString(), m_saveAuthBtn);
    layout->addWidget(authBox);

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
    m_closeBtn = new QPushButton(TranslationManager::ocrClose(), this);
    m_copyLinkBtn->setEnabled(false);
    m_openLinkBtn->setEnabled(false);
    linkRow->addWidget(m_copyLinkBtn);
    linkRow->addWidget(m_openLinkBtn);
    linkRow->addStretch();
    linkRow->addWidget(m_closeBtn);
    layout->addLayout(linkRow);

    connect(m_providerCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, &UploadDialog::onProviderChanged);
    connect(m_uploadBtn, &QPushButton::clicked, this, &UploadDialog::onUploadClicked);
    connect(m_copyLinkBtn, &QPushButton::clicked, this, &UploadDialog::onCopyLink);
    connect(m_openLinkBtn, &QPushButton::clicked, this, &UploadDialog::onOpenLink);
    connect(m_saveAuthBtn, &QPushButton::clicked, this, &UploadDialog::onSaveAuth);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

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
    if (m_uploader->needsAuth()) {
        m_authEdit->setVisible(true);
        m_saveAuthBtn->setVisible(true);
        if (auto *catbox = qobject_cast<CatboxUploader *>(m_uploader)) {
            QSettings s("EShot", "EShot");
            m_authEdit->setText(s.value("catboxUserHash").toString());
            m_authEdit->setPlaceholderText(TranslationManager::catboxUserHashDesc());
        }
    } else {
        m_authEdit->setVisible(false);
        m_saveAuthBtn->setVisible(false);
    }
}

void UploadDialog::onSaveAuth()
{
    if (!m_uploader) return;
    if (auto *catbox = qobject_cast<CatboxUploader *>(m_uploader)) {
        catbox->setUserHash(m_authEdit->text());
    }
    m_statusLabel->setText(TranslationManager::exportSuccess());
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
    m_statusLabel->setText(TranslationManager::uploadSuccess());
}

void UploadDialog::onFailed(const QString &reason)
{
    setBusy(false);
    m_statusLabel->setText(TranslationManager::uploadFailed() + QStringLiteral(" — ") + reason);
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
