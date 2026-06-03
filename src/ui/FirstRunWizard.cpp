#include "FirstRunWizard.h"
#include "../core/HotkeyManager.h"
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
#include <QGroupBox>
#include <QFont>
#include <QDir>
#include <QMessageBox>

FirstRunWizard::FirstRunWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(TranslationManager::wizardTitle());
    setWindowIcon(QIcon(":/icons/pen.svg"));
    setFixedSize(560, 430);
    setModal(true);

    setupUi();
    loadDefaults();
}

FirstRunWizard::~FirstRunWizard() {}

bool FirstRunWizard::shouldShow()
{
    QSettings s("EShot", "EShot");
    return !s.value("wizardCompleted", false).toBool();
}

static QKeySequence win32ToKeySequence(UINT modifiers, UINT vkey)
{
    Qt::KeyboardModifiers qtMod;
#ifdef Q_OS_WIN
    if (modifiers & MOD_SHIFT)   qtMod |= Qt::ShiftModifier;
    if (modifiers & MOD_CONTROL) qtMod |= Qt::ControlModifier;
    if (modifiers & MOD_ALT)     qtMod |= Qt::AltModifier;
    if (modifiers & MOD_WIN)     qtMod |= Qt::MetaModifier;

    struct { UINT win; Qt::Key qt; } mapping[] = {
        { VK_SNAPSHOT, Qt::Key_Print },
        { VK_F1,  Qt::Key_F1  }, { VK_F2,  Qt::Key_F2  }, { VK_F3,  Qt::Key_F3  },
        { VK_F4,  Qt::Key_F4  }, { VK_F5,  Qt::Key_F5  }, { VK_F6,  Qt::Key_F6  },
        { VK_F7,  Qt::Key_F7  }, { VK_F8,  Qt::Key_F8  }, { VK_F9,  Qt::Key_F9  },
        { VK_F10, Qt::Key_F10 }, { VK_F11, Qt::Key_F11 }, { VK_F12, Qt::Key_F12 },
        { VK_HOME,   Qt::Key_Home   }, { VK_END,    Qt::Key_End      },
        { VK_PRIOR,  Qt::Key_PageUp }, { VK_NEXT,   Qt::Key_PageDown  },
        { VK_INSERT, Qt::Key_Insert }, { VK_DELETE, Qt::Key_Delete    },
        { VK_LEFT,   Qt::Key_Left   }, { VK_RIGHT,  Qt::Key_Right     },
        { VK_UP,     Qt::Key_Up     }, { VK_DOWN,   Qt::Key_Down      },
        { VK_SPACE,  Qt::Key_Space  }, { VK_RETURN, Qt::Key_Return    },
        { VK_ESCAPE, Qt::Key_Escape }, { VK_TAB,    Qt::Key_Tab       },
        { VK_BACK,   Qt::Key_Backspace },
    };
    for (auto &m : mapping) {
        if (vkey == m.win) return QKeySequence(QKeyCombination(qtMod, m.qt));
    }
    if (vkey >= 'A' && vkey <= 'Z')
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_A + (vkey - 'A'))));
    if (vkey >= '0' && vkey <= '9')
        return QKeySequence(QKeyCombination(qtMod, static_cast<Qt::Key>(Qt::Key_0 + (vkey - '0'))));
#else
    Q_UNUSED(modifiers); Q_UNUSED(vkey);
#endif
    return QKeySequence(Qt::Key_Print);
}

bool FirstRunWizard::keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey)
{
#ifdef Q_OS_WIN
    if (seq.isEmpty()) return false;

    QKeyCombination combo = seq[0];
    Qt::KeyboardModifiers qtMod = combo.keyboardModifiers();
    Qt::Key qtKey = combo.key();

    if (qtKey == Qt::Key_unknown ||
        qtKey == Qt::Key_Shift ||
        qtKey == Qt::Key_Control ||
        qtKey == Qt::Key_Alt ||
        qtKey == Qt::Key_Meta)
        return false;

    modifiers = 0;
    if (qtMod & Qt::ShiftModifier)   modifiers |= MOD_SHIFT;
    if (qtMod & Qt::ControlModifier) modifiers |= MOD_CONTROL;
    if (qtMod & Qt::AltModifier)     modifiers |= MOD_ALT;
    if (qtMod & Qt::MetaModifier)    modifiers |= MOD_WIN;

    struct { Qt::Key qt; UINT win; } mapping[] = {
        { Qt::Key_Print,      VK_SNAPSHOT },
        { Qt::Key_F1,         VK_F1 }, { Qt::Key_F2,  VK_F2  }, { Qt::Key_F3,  VK_F3  },
        { Qt::Key_F4,         VK_F4 }, { Qt::Key_F5,  VK_F5  }, { Qt::Key_F6,  VK_F6  },
        { Qt::Key_F7,         VK_F7 }, { Qt::Key_F8,  VK_F8  }, { Qt::Key_F9,  VK_F9  },
        { Qt::Key_F10,        VK_F10}, { Qt::Key_F11, VK_F11 }, { Qt::Key_F12, VK_F12 },
        { Qt::Key_Home,       VK_HOME },  { Qt::Key_End,    VK_END    },
        { Qt::Key_PageUp,     VK_PRIOR }, { Qt::Key_PageDown,VK_NEXT  },
        { Qt::Key_Insert,     VK_INSERT },{ Qt::Key_Delete,  VK_DELETE },
        { Qt::Key_Left,       VK_LEFT },  { Qt::Key_Right,   VK_RIGHT },
        { Qt::Key_Up,         VK_UP   },  { Qt::Key_Down,    VK_DOWN  },
        { Qt::Key_Space,      VK_SPACE }, { Qt::Key_Return,  VK_RETURN},
        { Qt::Key_Escape,     VK_ESCAPE },{ Qt::Key_Tab,     VK_TAB   },
        { Qt::Key_Backspace,  VK_BACK  },
    };
    for (auto &m : mapping) {
        if (qtKey == m.qt) { vkey = m.win; return true; }
    }

    if (qtKey >= Qt::Key_A && qtKey <= Qt::Key_Z) {
        if (qtMod == Qt::NoModifier) return false;
        vkey = 'A' + (qtKey - Qt::Key_A);
        return true;
    }
    if (qtKey >= Qt::Key_0 && qtKey <= Qt::Key_9) {
        if (qtMod == Qt::NoModifier) return false;
        vkey = '0' + (qtKey - Qt::Key_0);
        return true;
    }
    return false;
#else
    Q_UNUSED(seq); Q_UNUSED(modifiers); Q_UNUSED(vkey);
    return false;
#endif
}

void FirstRunWizard::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);

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

    QGroupBox *langGroup = new QGroupBox(TranslationManager::language());
    QVBoxLayout *langLayout = new QVBoxLayout(langGroup);
    m_langCombo = new QComboBox();
    m_langCombo->addItem(QString::fromUtf8("Türkçe"), "tr");
    m_langCombo->addItem("English", "en");
    m_langCombo->addItem("Deutsch", "de");
    m_langCombo->addItem(QString::fromUtf8("Français"), "fr");
    m_langCombo->addItem(QString::fromUtf8("Español"), "es");
    m_langCombo->addItem(QString::fromUtf8("日本語"), "ja");
    m_langCombo->addItem(QString::fromUtf8("中文"), "zh");
    m_langCombo->addItem(QString::fromUtf8("Русский"), "ru");
    langLayout->addWidget(m_langCombo);
    mainLayout->addWidget(langGroup);

    QGroupBox *hkGroup = new QGroupBox(TranslationManager::hotkeyTitle());
    QVBoxLayout *hkLayout = new QVBoxLayout(hkGroup);
    QLabel *hkDesc = new QLabel(TranslationManager::wizardHotkeyDesc());
    hkDesc->setWordWrap(true);
    hkLayout->addWidget(hkDesc);
    m_hotkeyEdit = new QKeySequenceEdit();
    m_hotkeyEdit->setMinimumHeight(38);
    connect(m_hotkeyEdit, &QKeySequenceEdit::keySequenceChanged,
            this, &FirstRunWizard::onHotkeyChanged);
    hkLayout->addWidget(m_hotkeyEdit);

    m_hotkeyStatusLabel = new QLabel();
    m_hotkeyStatusLabel->setStyleSheet("font-size: 12px;");
    hkLayout->addWidget(m_hotkeyStatusLabel);

    m_printScreenConflictLabel = new QLabel(
        TranslationManager::currentLanguage() == TranslationManager::Turkish
            ? QString::fromUtf8("Windows, Print Screen tuşunu Snipping Tool için kullanıyor. EShot'un Print Screen ile çalışması için bu Windows ayarını kapatın.")
            : QStringLiteral("Windows is using Print Screen for Snipping Tool. Disable this Windows setting to let EShot use Print Screen."));
    m_printScreenConflictLabel->setWordWrap(true);
    m_printScreenConflictLabel->setStyleSheet(
        "background: rgba(255, 193, 7, 0.14); color: #ffd166; "
        "border: 1px solid rgba(255, 193, 7, 0.45); border-radius: 6px; "
        "padding: 7px; font-size: 12px;");
    hkLayout->addWidget(m_printScreenConflictLabel);

    m_printScreenFixButton = new QPushButton(
        TranslationManager::currentLanguage() == TranslationManager::Turkish
            ? QString::fromUtf8("Windows Print Screen ayarını kapat")
            : QStringLiteral("Disable Windows Print Screen shortcut"));
    connect(m_printScreenFixButton, &QPushButton::clicked,
            this, &FirstRunWizard::onDisableWindowsPrintScreenSnipping);
    hkLayout->addWidget(m_printScreenFixButton);
    mainLayout->addWidget(hkGroup);

    QGroupBox *pathGroup = new QGroupBox(TranslationManager::saveDir());
    QHBoxLayout *pathLayout = new QHBoxLayout(pathGroup);
    m_savePathEdit = new QLineEdit();
    QPushButton *browseBtn = new QPushButton(TranslationManager::browse());
    connect(browseBtn, &QPushButton::clicked, this, &FirstRunWizard::onBrowse);
    pathLayout->addWidget(m_savePathEdit);
    pathLayout->addWidget(browseBtn);
    mainLayout->addWidget(pathGroup);

    mainLayout->addStretch();

    QHBoxLayout *btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    QPushButton *finishBtn = new QPushButton(TranslationManager::wizardFinish());
    finishBtn->setDefault(true);
    finishBtn->setStyleSheet(R"(
        QPushButton { background-color: #0078D4; color: white; border: none;
                      padding: 8px 24px; border-radius: 4px; font-weight: bold; }
        QPushButton:hover { background-color: #1a8cff; }
    )");
    connect(finishBtn, &QPushButton::clicked, this, &FirstRunWizard::onFinish);
    btnLayout->addWidget(finishBtn);
    mainLayout->addLayout(btnLayout);
}

void FirstRunWizard::loadDefaults()
{
    QSettings s("EShot", "EShot");

    int langInt = s.value("language", static_cast<int>(TranslationManager::currentLanguage())).toInt();
    static const char* langCodes[] = {"tr","en","de","fr","es","ja","zh","ru"};
    QString lang = (langInt >= 0 && langInt < TranslationManager::LangCount) ? langCodes[langInt] : "en";
    int li = m_langCombo->findData(lang);
    if (li >= 0) m_langCombo->setCurrentIndex(li);

    UINT savedMod = static_cast<UINT>(s.value("hotkeyModifiers", 0).toUInt());
    UINT savedVKey = static_cast<UINT>(s.value("hotkeyVKey", VK_SNAPSHOT).toUInt());
    m_hotkeyEdit->setKeySequence(win32ToKeySequence(savedMod, savedVKey));

    QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!defaultPath.isEmpty())
        defaultPath = QDir(defaultPath).filePath("EShot");
    m_savePathEdit->setText(s.value("savePath", defaultPath).toString());

    onHotkeyChanged();
}

void FirstRunWizard::onBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, TranslationManager::saveDir(), m_savePathEdit->text());
    if (!dir.isEmpty()) m_savePathEdit->setText(dir);
}

void FirstRunWizard::onHotkeyChanged()
{
    UINT mod = 0, vk = 0;
    const bool ok = keySequenceToWin32(m_hotkeyEdit->keySequence(), mod, vk);
    if (ok) {
        m_hotkeyStatusLabel->setText(QStringLiteral("OK: %1").arg(m_hotkeyEdit->keySequence().toString(QKeySequence::NativeText)));
        m_hotkeyStatusLabel->setStyleSheet("color: #4caf50; font-size: 12px;");
    } else {
        m_hotkeyStatusLabel->setText(TranslationManager::hotkeyInvalid());
        m_hotkeyStatusLabel->setStyleSheet("color: #ff9800; font-size: 12px;");
    }
    updatePrintScreenConflictUi();
}

void FirstRunWizard::updatePrintScreenConflictUi()
{
    UINT mod = 0, vk = 0;
    const bool hotkeyOk = keySequenceToWin32(m_hotkeyEdit->keySequence(), mod, vk);
    const bool showWarning = hotkeyOk
        && HotkeyManager::isPlainPrintScreen(mod, vk)
        && HotkeyManager::isWindowsPrintScreenSnippingEnabled();

    m_printScreenConflictLabel->setVisible(showWarning);
    m_printScreenFixButton->setVisible(showWarning);
}

void FirstRunWizard::onDisableWindowsPrintScreenSnipping()
{
    if (!HotkeyManager::setWindowsPrintScreenSnippingEnabled(false)) {
        QMessageBox::warning(this, QStringLiteral("EShot"), TranslationManager::errTitle());
        return;
    }
    HotkeyManager::instance().reRegisterCaptureHotkey(
        HotkeyManager::instance().captureModifiers(),
        HotkeyManager::instance().captureVirtualKey());
    updatePrintScreenConflictUi();
}

void FirstRunWizard::onFinish()
{
    QString savePath = m_savePathEdit->text().trimmed();
    if (savePath.isEmpty())
        savePath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!savePath.isEmpty()) {
        QDir dir(savePath);
        if (!dir.exists() && !dir.mkpath(".")) {
            QMessageBox::warning(this, TranslationManager::errTitle(), TranslationManager::errSaveDir() + savePath);
            return;
        }
    }

    UINT modifiers = 0, vkey = 0;
    if (!keySequenceToWin32(m_hotkeyEdit->keySequence(), modifiers, vkey)) {
        QMessageBox::warning(this, TranslationManager::errInvalidHotkeyTitle(), TranslationManager::errInvalidHotkey());
        return;
    }

    if (!HotkeyManager::instance().reRegisterCaptureHotkey(modifiers, vkey)) {
        QMessageBox::warning(
            this,
            TranslationManager::errInvalidHotkeyTitle(),
            TranslationManager::currentLanguage() == TranslationManager::Turkish
                ? QString::fromUtf8("Bu kısayol başka bir uygulama tarafından kullanılıyor olabilir. Lütfen farklı bir kombinasyon seçin.")
                : QStringLiteral("This hotkey may already be used by another app. Please choose a different combination."));
        return;
    }

    QString lang = m_langCombo->currentData().toString();
    TranslationManager::Language langEnum = TranslationManager::English;
    if (lang == "tr") langEnum = TranslationManager::Turkish;
    else if (lang == "de") langEnum = TranslationManager::German;
    else if (lang == "fr") langEnum = TranslationManager::French;
    else if (lang == "es") langEnum = TranslationManager::Spanish;
    else if (lang == "ja") langEnum = TranslationManager::Japanese;
    else if (lang == "zh") langEnum = TranslationManager::Chinese;
    else if (lang == "ru") langEnum = TranslationManager::Russian;

    TranslationManager::setLanguage(langEnum);

    QSettings s("EShot", "EShot");
    s.setValue("language", static_cast<int>(langEnum));
    s.setValue("hotkeyModifiers", modifiers);
    s.setValue("hotkeyVKey", vkey);
    s.setValue("savePath", savePath);
    s.setValue("wizardCompleted", true);
    s.sync();

    accept();
}
