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
#include <QMap>

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

protected:
    void showEvent(QShowEvent *event) override;

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
    bool m_loadedAutoStart = false;
    QCheckBox *m_showNotificationsCheck = nullptr;
    QWidget *m_notificationOptionsWidget = nullptr;
    QCheckBox *m_notifyCopyCheck = nullptr;
    QCheckBox *m_notifySaveCheck = nullptr;
    QCheckBox *m_notifyGifCheck = nullptr;
    QCheckBox *m_notifyVideoCheck = nullptr;
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
    QListWidget *m_toolbarControlVisibilityList = nullptr;

    // Shortcut
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QKeySequenceEdit *m_recordingPauseHotkeyEdit = nullptr;
    QKeySequenceEdit *m_recordingStopHotkeyEdit = nullptr;
    QKeySequenceEdit *m_recordingCancelHotkeyEdit = nullptr;
    QMap<QString, QKeySequenceEdit*> m_overlayHotkeyEdits;
    QLabel *m_hotkeyStatusLabel = nullptr;
    QLabel *m_printScreenConflictLabel = nullptr;
    QPushButton *m_printScreenFixButton = nullptr;

    // Recording
    QSpinBox *m_recordingFpsSpin = nullptr;
    QSpinBox *m_recordingMaxSecSpin = nullptr;
    QComboBox *m_recordingLoopCombo = nullptr;
    QSpinBox *m_videoFpsSpin = nullptr;
    QSpinBox *m_videoMaxSecSpin = nullptr;
    QSpinBox *m_videoCrfSpin = nullptr;
    QCheckBox *m_videoDesktopAudioCheck = nullptr;
    QSlider *m_videoDesktopVolumeSlider = nullptr;
    QSpinBox *m_videoDesktopVolumeSpin = nullptr;
    QCheckBox *m_videoMicrophoneCheck = nullptr;
    QSlider *m_videoMicrophoneVolumeSlider = nullptr;
    QSpinBox *m_videoMicrophoneVolumeSpin = nullptr;
    QComboBox *m_videoMicrophoneDeviceCombo = nullptr;

    // Language
    QComboBox *m_langCombo = nullptr;
};

#endif
