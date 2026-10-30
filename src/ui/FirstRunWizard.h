#ifndef FIRSTRUNWIZARD_H
#define FIRSTRUNWIZARD_H

#include <QDialog>
#include <QLabel>
#include <QList>
#include <functional>
#include "../core/TranslationManager.h"
#include "../core/PlatformHotkey.h"

class QComboBox;
class QKeySequenceEdit;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QProcess;

class FirstRunWizard : public QDialog {
    Q_OBJECT

public:
    explicit FirstRunWizard(QWidget *parent = nullptr);
    ~FirstRunWizard();

    static bool shouldShow();
#ifdef Q_OS_LINUX
    static void showLinuxDependencySetup(QWidget *parent = nullptr);
#endif

public slots:
    // Ignored while the Linux dependency installer runs (Esc, the window
    // close button or another caller); closing would kill the install.
    void reject() override;

private slots:
    void onBrowse();
    void onFinish();
    void onHotkeyChanged();
    void onLanguageChanged();
    void onDisableWindowsPrintScreenSnipping();
#ifdef Q_OS_LINUX
    void onActivateLinuxPrintScreen();
#endif

private:
    void setupUi();
    void retranslateUi();
    // Applies a text now and again whenever the wizard language changes.
    void addTranslatedText(std::function<void()> apply);
    static TranslationManager::Language languageFromCode(const QString &code);
    void loadDefaults();
    void updatePrintScreenConflictUi();
#ifdef Q_OS_LINUX
    void startLinuxDependencyInstaller();
#endif

    QList<std::function<void()>> m_retranslations;
    QComboBox *m_langCombo = nullptr;
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QLabel *m_hotkeyStatusLabel = nullptr;
    QLabel *m_printScreenConflictLabel = nullptr;
    QPushButton *m_printScreenFixButton = nullptr;
    QLineEdit *m_savePathEdit = nullptr;
#ifdef Q_OS_LINUX
    QCheckBox *m_linuxFfmpegCheck = nullptr;
    QCheckBox *m_linuxOcrCheck = nullptr;
    QCheckBox *m_linuxDesktopCheck = nullptr;
    QCheckBox *m_linuxAppImageIntegrationCheck = nullptr;
    QList<QCheckBox *> m_linuxLanguageChecks;
    QLabel *m_linuxInstallStatus = nullptr;
    QPushButton *m_finishButton = nullptr;
    QPushButton *m_skipDepsButton = nullptr;
    QProcess *m_linuxInstallerProcess = nullptr;
    bool m_linuxExplicitSkip = false;
#endif
};

#endif
