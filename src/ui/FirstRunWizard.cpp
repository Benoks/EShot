#include "FirstRunWizard.h"
#include "../core/TranslationManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QLineEdit>
#include <QFileDialog>
#include <QStandardPaths>
#include <QSettings>
#include <QStackedWidget>
#include <QGroupBox>
#include <QFont>

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QDialog(parent)
    , m_currentPage(0)
{
    setWindowTitle(TranslationManager::wizardTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setFixedSize(500, 400);
    setModal(true);

    setupPages();
    showPage(0);
}

FirstRunWizard::~FirstRunWizard() {}

bool FirstRunWizard::shouldShow()
{
    QSettings s("EShot", "EShot");
    return !s.value("wizardCompleted", false).toBool();
}

void FirstRunWizard::setupPages()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);

    // Başlık
    QFont titleFont;
    titleFont.setPointSize(16);
    titleFont.setBold(true);

    QLabel *titleLabel = new QLabel(TranslationManager::wizardTitle());
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    QLabel *descLabel = new QLabel(TranslationManager::wizardDesc());
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #aaa;");
    mainLayout->addWidget(descLabel);

    // Sayfalar
    m_stack = new QStackedWidget(this);

    // Sayfa 1: Dil
    QWidget *page1 = new QWidget();
    QVBoxLayout *p1Layout = new QVBoxLayout(page1);
    QGroupBox *langGroup = new QGroupBox(TranslationManager::language());
    QVBoxLayout *langLayout = new QVBoxLayout(langGroup);
    m_langCombo = new QComboBox();
    m_langCombo->addItem("Türkçe", "tr");
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Deutsch", "de");
    m_langCombo->addItem("Français", "fr");
    m_langCombo->addItem("Español", "es");
    m_langCombo->addItem("日本語", "ja");
    m_langCombo->addItem("中文", "zh");
    langLayout->addWidget(m_langCombo);
    p1Layout->addWidget(langGroup);
    p1Layout->addStretch();
    m_stack->addWidget(page1);

    // Sayfa 2: Kısayol
    QWidget *page2 = new QWidget();
    QVBoxLayout *p2Layout = new QVBoxLayout(page2);
    QGroupBox *hkGroup = new QGroupBox(TranslationManager::hotkeyTitle());
    QVBoxLayout *hkLayout = new QVBoxLayout(hkGroup);
    QLabel *hkDesc = new QLabel(TranslationManager::wizardHotkeyDesc());
    hkDesc->setWordWrap(true);
    hkLayout->addWidget(hkDesc);
    m_hotkeyEdit = new QKeySequenceEdit();
    m_hotkeyEdit->setKeySequence(QKeySequence(Qt::Key_Print));
    m_hotkeyEdit->setMinimumHeight(40);
    hkLayout->addWidget(m_hotkeyEdit);
    p2Layout->addWidget(hkGroup);
    p2Layout->addStretch();
    m_stack->addWidget(page2);

    // Sayfa 3: Kayıt yolu
    QWidget *page3 = new QWidget();
    QVBoxLayout *p3Layout = new QVBoxLayout(page3);
    QGroupBox *pathGroup = new QGroupBox(TranslationManager::saveDir());
    QVBoxLayout *pathLayout = new QVBoxLayout(pathGroup);
    m_savePathEdit = new QLineEdit();
    m_savePathEdit->setText(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation) + "/EShot");
    QPushButton *browseBtn = new QPushButton(TranslationManager::browse());
    connect(browseBtn, &QPushButton::clicked, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, TranslationManager::saveDir());
        if (!dir.isEmpty()) m_savePathEdit->setText(dir);
    });
    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_savePathEdit);
    pathRow->addWidget(browseBtn);
    pathLayout->addLayout(pathRow);
    p3Layout->addWidget(pathGroup);
    p3Layout->addStretch();
    m_stack->addWidget(page3);

    mainLayout->addWidget(m_stack);

    // Butonlar
    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();

    QPushButton *backBtn = new QPushButton(TranslationManager::wizardBack());
    connect(backBtn, &QPushButton::clicked, this, &FirstRunWizard::onBack);
    btnLayout->addWidget(backBtn);

    QPushButton *nextBtn = new QPushButton(TranslationManager::wizardNext());
    nextBtn->setDefault(true);
    nextBtn->setStyleSheet(R"(
        QPushButton { background-color: #0078D4; color: white; border: none;
                      padding: 8px 24px; border-radius: 4px; font-weight: bold; }
        QPushButton:hover { background-color: #1a8cff; }
    )");
    connect(nextBtn, &QPushButton::clicked, this, &FirstRunWizard::onNext);
    btnLayout->addWidget(nextBtn);

    mainLayout->addLayout(btnLayout);

    showPage(0);
}

void FirstRunWizard::showPage(int index)
{
    m_currentPage = index;
    m_stack->setCurrentIndex(index);
}

void FirstRunWizard::onNext()
{
    if (m_currentPage < 2) {
        showPage(m_currentPage + 1);
    } else {
        onFinish();
    }
}

void FirstRunWizard::onBack()
{
    if (m_currentPage > 0) {
        showPage(m_currentPage - 1);
    }
}

void FirstRunWizard::onFinish()
{
    QSettings s("EShot", "EShot");

    // Dil kaydet
    QString lang = m_langCombo->currentData().toString();
    TranslationManager::Language langEnum = TranslationManager::English;
    if (lang == "tr") langEnum = TranslationManager::Turkish;
    else if (lang == "de") langEnum = TranslationManager::German;
    else if (lang == "fr") langEnum = TranslationManager::French;
    else if (lang == "es") langEnum = TranslationManager::Spanish;
    else if (lang == "ja") langEnum = TranslationManager::Japanese;
    else if (lang == "zh") langEnum = TranslationManager::Chinese;
    TranslationManager::setLanguage(langEnum);
    s.setValue("language", static_cast<int>(langEnum));
    s.setValue("wizardCompleted", true);

    // Kısayol kaydet
    s.setValue("hotkeyModifiers", 0);
    s.setValue("hotkeyVKey", 0x2C); // VK_SNAPSHOT

    // Kayıt yolu kaydet
    s.setValue("savePath", m_savePathEdit->text());

    s.setValue("wizardCompleted", true);
    s.sync();

    accept();
}
