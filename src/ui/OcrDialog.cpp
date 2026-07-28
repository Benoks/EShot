#include "OcrDialog.h"
#include "core/OcrEngine.h"
#include "core/OcrLanguageSelector.h"
#include "core/TranslationManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGuiApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QUrlQuery>
#include <QMessageBox>
#include <QDebug>
#include <QSettings>
#include <QDir>
#include <QFileInfo>
#include <QStandardItemModel>
#include <QStandardItem>
#include <QColor>

namespace {
constexpr int LanguageInstalledRole = Qt::UserRole + 1;
}

OcrDialog::OcrDialog(const QPixmap &pixmap, QWidget *parent)
    : QDialog(parent), m_pixmap(pixmap)
{
    setWindowTitle(TranslationManager::ocrTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    resize(520, 420);

    m_engine = new OcrEngine(this);
    connect(m_engine, &OcrEngine::textReady, this, &OcrDialog::onTextReady);
    connect(m_engine, &OcrEngine::failed, this, &OcrDialog::onOcrFailed);
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    m_languageTag = settings.value(QStringLiteral("ocrLanguage"), m_languageTag).toString();
    m_preferredLanguageTag = settings.value(
        QStringLiteral("ocrPreferredLanguage"), TranslationManager::langCode()).toString();
    connect(m_engine, &OcrEngine::languageResolved, this, [this](const QString &languages) {
        if (m_languageTag != QStringLiteral("auto"))
            return;
        const int automaticIndex = m_langCombo ? m_langCombo->findData(QStringLiteral("auto")) : -1;
        if (automaticIndex >= 0) {
            m_langCombo->setItemText(
                automaticIndex,
                QStringLiteral("%1 (%2)").arg(TranslationManager::ocrAutomatic(), languages));
        }
    });

    auto *layout = new QVBoxLayout(this);

    auto *langRow = new QHBoxLayout();
    QLabel *langLabel = new QLabel(QString("%1:").arg(TranslationManager::language()), this);
    m_langCombo = new QComboBox(this);
    populateLanguages();
    langRow->addWidget(langLabel);
    langRow->addWidget(m_langCombo);
    langRow->addStretch();
    layout->addLayout(langRow);

    int idx = m_langCombo->findData(m_languageTag);
    if (idx < 0 || !m_langCombo->itemData(idx, LanguageInstalledRole).toBool()) {
        idx = firstInstalledLanguageIndex();
    }
    if (idx >= 0) {
        m_langCombo->setCurrentIndex(idx);
        m_languageTag = m_langCombo->currentData().toString();
    }
    connect(m_langCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OcrDialog::onLanguageChanged);

    m_statusLabel = new QLabel(TranslationManager::ocrProcessing(), this);
    layout->addWidget(m_statusLabel);

    m_textEdit = new QTextEdit(this);
    m_textEdit->setReadOnly(true);
    m_textEdit->setPlaceholderText(TranslationManager::ocrEmpty());
    layout->addWidget(m_textEdit, 1);

    auto *btnRow = new QHBoxLayout();
    m_copyBtn = new QPushButton(TranslationManager::ocrCopy(), this);
    m_copyBtn->setEnabled(false);
    m_translateBtn = new QPushButton(TranslationManager::tr("ocrTranslate"), this);
    m_translateBtn->setIcon(QIcon(QStringLiteral(":/icons/external_link.svg")));
    m_translateBtn->setIconSize(QSize(16, 16));
    m_translateBtn->setToolTip(TranslationManager::tr("ocrTranslate"));
    m_translateBtn->setEnabled(false);
    m_retryBtn = new QPushButton(TranslationManager::ocrRetry(), this);
    m_retryBtn->setEnabled(false);
    m_closeBtn = new QPushButton(TranslationManager::ocrClose(), this);
    const QList<QPushButton *> actionButtons = {m_copyBtn, m_translateBtn, m_retryBtn, m_closeBtn};
    for (QPushButton *button : actionButtons) {
        button->setFixedHeight(30);
        button->setStyleSheet(QStringLiteral("QPushButton { padding: 0 10px; }"));
    }

    btnRow->addWidget(m_copyBtn);
    btnRow->addWidget(m_translateBtn);
    btnRow->addWidget(m_retryBtn);
    btnRow->addStretch();
    btnRow->addWidget(m_closeBtn);
    layout->addLayout(btnRow);

    connect(m_copyBtn, &QPushButton::clicked, this, &OcrDialog::onCopyClicked);
    connect(m_translateBtn, &QPushButton::clicked, this, &OcrDialog::onTranslateClicked);
    connect(m_retryBtn, &QPushButton::clicked, this, &OcrDialog::onRetryClicked);
    connect(m_closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    setBusy(true);
    m_engine->recognize(m_pixmap, m_languageTag, m_preferredLanguageTag);
}

OcrDialog::~OcrDialog() = default;

void OcrDialog::setLanguageTag(const QString &tag)
{
    if (tag.isEmpty()) return;
    if (m_langCombo) {
        const int idx = m_langCombo->findData(tag);
        if (idx >= 0 && m_langCombo->itemData(idx, LanguageInstalledRole).toBool()) {
            m_languageTag = tag;
            m_langCombo->setCurrentIndex(idx);
        }
    }
}

void OcrDialog::populateLanguages()
{
    struct LanguageItem {
        const char *label;
        const char *tag;
    };

    const LanguageItem languages[] = {
        {"English", "en-US"},
        {"T\303\274rk\303\247e", "tr-TR"},
        {"\320\240\321\203\321\201\321\201\320\272\320\270\320\271", "ru-RU"},
        {"Deutsch", "de-DE"},
        {"Fran\303\247ais", "fr-FR"},
        {"Espa\303\261ol", "es-ES"},
        {"Italiano", "it-IT"},
        {"Portugu\303\252s", "pt-BR"},
        {"Polski", "pl-PL"},
        {"Nederlands", "nl-NL"},
        {"\346\227\245\346\234\254\350\252\236", "ja-JP"},
        {"\355\225\234\352\265\255\354\226\264", "ko-KR"},
        {"\347\256\200\344\275\223\344\270\255\346\226\207", "zh-CN"},
    };

    const QString missingTip = TranslationManager::ocrLanguagePackMissing();

    const bool automaticAvailable = !installedOcrLanguageCodes(OcrEngine::tessdataDir()).isEmpty();
    m_langCombo->addItem(TranslationManager::ocrAutomatic(), QStringLiteral("auto"));
    m_langCombo->setItemData(0, automaticAvailable, LanguageInstalledRole);
    if (!automaticAvailable) {
        auto *model = qobject_cast<QStandardItemModel *>(m_langCombo->model());
        if (QStandardItem *item = model ? model->item(0) : nullptr) {
            item->setEnabled(false);
            item->setForeground(QColor(145, 145, 145));
            item->setToolTip(missingTip);
        }
    }

    for (const auto &language : languages) {
        const QString tag = QString::fromLatin1(language.tag);
        const bool installed = isLanguageInstalled(tag);
        m_langCombo->addItem(QString::fromUtf8(language.label), tag);
        const int row = m_langCombo->count() - 1;
        m_langCombo->setItemData(row, installed, LanguageInstalledRole);

        auto *model = qobject_cast<QStandardItemModel *>(m_langCombo->model());
        QStandardItem *item = model ? model->item(row) : nullptr;
        if (!installed && item) {
            item->setEnabled(false);
            item->setForeground(QColor(145, 145, 145));
            item->setToolTip(missingTip);
        }
    }
}

bool OcrDialog::isLanguageInstalled(const QString &tag) const
{
    const QString mapped = OcrEngine::mapLanguageTag(tag);
    if (mapped.isEmpty()) {
        return false;
    }
    return QFileInfo::exists(QDir(OcrEngine::tessdataDir()).filePath(mapped + QStringLiteral(".traineddata")));
}

int OcrDialog::firstInstalledLanguageIndex() const
{
    for (int i = 0; i < m_langCombo->count(); ++i) {
        if (m_langCombo->itemData(i, LanguageInstalledRole).toBool()) {
            return i;
        }
    }
    return -1;
}

void OcrDialog::translateUi()
{
    setWindowTitle(TranslationManager::ocrTitle());
    m_copyBtn->setText(TranslationManager::ocrCopy());
    m_translateBtn->setText(TranslationManager::tr("ocrTranslate"));
    m_closeBtn->setText(TranslationManager::ocrClose());
}

void OcrDialog::setBusy(bool busy)
{
    m_retryBtn->setEnabled(!busy);
    m_langCombo->setEnabled(!busy);
    if (busy) {
        m_statusLabel->setText(TranslationManager::ocrProcessing());
    }
}

void OcrDialog::runOcr()
{
    setBusy(true);
    m_textEdit->clear();
    m_copyBtn->setEnabled(false);
    m_translateBtn->setEnabled(false);
    m_engine->recognize(m_pixmap, m_languageTag, m_preferredLanguageTag);
}

void OcrDialog::onLanguageChanged(int index)
{
    if (index < 0 || !m_langCombo->itemData(index, LanguageInstalledRole).toBool()) {
        return;
    }
    QString tag = m_langCombo->currentData().toString();
    if (!tag.isEmpty() && tag != m_languageTag) {
        m_languageTag = tag;
        QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
        settings.setValue(QStringLiteral("ocrLanguage"), tag);
        if (tag != QStringLiteral("auto")) {
            m_preferredLanguageTag = tag;
            settings.setValue(QStringLiteral("ocrPreferredLanguage"), tag);
        }
        if (!m_textEdit->toPlainText().isEmpty()) {
            runOcr();
        }
    }
}

void OcrDialog::onTextReady(const QString &text)
{
    setBusy(false);
    if (text.trimmed().isEmpty()) {
        m_statusLabel->setText(TranslationManager::ocrEmpty());
        m_textEdit->clear();
        m_copyBtn->setEnabled(false);
        m_translateBtn->setEnabled(false);
    } else {
        m_statusLabel->setText(TranslationManager::ocrTitle());
        m_textEdit->setPlainText(text);
        m_copyBtn->setEnabled(true);
        m_translateBtn->setEnabled(true);
    }
}

void OcrDialog::onOcrFailed(const QString &reason)
{
    setBusy(false);
    m_statusLabel->setText(TranslationManager::ocrFailed() + QStringLiteral(" - ") + reason);
    m_textEdit->clear();
    m_copyBtn->setEnabled(false);
    m_translateBtn->setEnabled(false);
    m_retryBtn->setEnabled(true);
    qWarning() << "[EShot] OCR failed:" << reason;
}

void OcrDialog::onCopyClicked()
{
    QGuiApplication::clipboard()->setText(m_textEdit->toPlainText());
    m_statusLabel->setText(TranslationManager::ocrCopied());
}

void OcrDialog::onTranslateClicked()
{
    const QString text = m_textEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    QUrl url(QStringLiteral("https://translate.google.com/"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("sl"), QStringLiteral("auto"));
    query.addQueryItem(QStringLiteral("tl"), TranslationManager::langCode());
    query.addQueryItem(QStringLiteral("text"), text);
    query.addQueryItem(QStringLiteral("op"), QStringLiteral("translate"));
    url.setQuery(query);
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox::warning(this, TranslationManager::tr("visualSearchBrowserLaunchTitle"),
                             TranslationManager::tr("ocrTranslateBrowserError"));
    }
}

void OcrDialog::onRetryClicked()
{
    runOcr();
}
