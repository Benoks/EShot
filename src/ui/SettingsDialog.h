#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QSlider>
#include <QSettings>
#include <QTabWidget>
#include <QLabel>
#include <QListWidget>
#include <QKeySequenceEdit>
#include <QPushButton>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

private slots:
    void onBrowse();
    void onSave();
    void onReset();
    void onFilenamePatternChanged(const QString &text);
    void onSelectAllTools();
    void onDeselectAllTools();
    void onHotkeyChanged(const QKeySequence &seq);
    void onDisableWindowsPrintScreenSnipping();
    void onExportSettings();
    void onImportSettings();
    void onThemeChanged();

private:
    void loadSettings();
    void setupUI();
    QWidget* createGeneralTab();
    QWidget* createCaptureTab();
    QWidget* createRecordingTab();
    QWidget* createAppearanceTab();
    QWidget* createInterfaceTab();
    QWidget* createHotkeyTab();

    QString resolvePatternPreview(const QString &pattern) const;
    void updatePrintScreenConflictUi();

    // Resolve shortcut to Win32 VK + modifier
    static bool keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey);
    static bool isAutoStartEnabled();

    QSettings *m_settings = nullptr;

    // General
    QLineEdit *m_savePathEdit = nullptr;
    QLineEdit *m_filenamePatternEdit = nullptr;
    QLabel *m_patternPreviewLabel = nullptr;
    QCheckBox *m_autoStartCheck = nullptr;
    QCheckBox *m_showNotificationsCheck = nullptr;
    QCheckBox *m_playSoundCheck = nullptr;
    QCheckBox *m_copyPathAfterSaveCheck = nullptr;

    // Capture
    QComboBox *m_formatCombo = nullptr;
    QSlider *m_qualitySlider = nullptr;
    QSpinBox *m_qualitySpin = nullptr;
    QSpinBox *m_delaySpin = nullptr;
    QCheckBox *m_copyAfterCaptureCheck = nullptr;
    QCheckBox *m_closeAfterCopyCheck = nullptr;

    // Appearance
    QCheckBox *m_darkModeCheck = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityValueLabel = nullptr;
    QComboBox *m_crosshairStyleCombo = nullptr;

    // Accessibility
    QCheckBox *m_highContrastCheck = nullptr;

    // UI - Tool visibility
    QListWidget *m_toolVisibilityList = nullptr;

    // Shortcut
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QLabel *m_hotkeyStatusLabel = nullptr;
    QLabel *m_printScreenConflictLabel = nullptr;
    QPushButton *m_printScreenFixButton = nullptr;

    // Recording
    QSpinBox *m_recordingFpsSpin = nullptr;
    QSpinBox *m_recordingMaxSecSpin = nullptr;
    QComboBox *m_recordingLoopCombo = nullptr;

    // Language
    QComboBox *m_langCombo = nullptr;
};

#endif
