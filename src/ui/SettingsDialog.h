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
#include "../core/PlatformHotkey.h"

class QNetworkAccessManager;
class QNetworkReply;
class QGroupBox;
class QFile;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog();

    void setUpdateInfo(bool available, const QString &version, bool busy, const QString &status);
    bool remembersWindowSize() const { return m_rememberSettingsWindowSizeEnabled; }

signals:
    void updateRequested();

private slots:
    void onBrowse();
    void onSave();
    void onReset();
    void onFilenamePatternChanged(const QString &text);
    void onSelectAllTools();
    void onDeselectAllTools();
    void onHotkeyChanged(const QKeySequence &seq);
    void onDisableWindowsPrintScreenSnipping();
    void onRequestLinuxPrintScreenBinding();
    void onExportSettings();
    void onImportSettings();
    void onThemeChanged();
    void onDownloadEssentialOcr();
    void onDeleteSelectedOcr();
    void onTesseractComponentAction();
    void onFfmpegComponentAction();
#ifdef Q_OS_LINUX
    void onOpenLinuxDependencySetup();
#endif

private:
    void done(int result) override;
    void loadSettings();
    void setupUI();
    QWidget* createGeneralTab();
    QWidget* createCaptureTab();
    QWidget* createRecordingTab();
    QWidget* createAppearanceTab();
    QWidget* createInterfaceTab();
    QWidget* createHotkeyTab();
    QWidget* createPackagesTab();

    QString resolvePatternPreview(const QString &pattern) const;
    void updatePrintScreenConflictUi();
    void refreshPackageStatus();
    void downloadOcrLanguage(const QString &code);
    void deleteOcrLanguage(const QString &code);
    void downloadTesseractComponent();
    void downloadFfmpegComponent();
    void downloadReleaseComponent(const QString &componentDir, const QString &exeName, const QString &statusPrefix);
    void downloadComponentArchive(const QString &url, const QString &assetName, qint64 expectedSize,
                                  const QString &componentDir, const QString &exeName, const QString &statusPrefix);
    void extractComponentArchive(const QString &archivePath, const QString &componentDir,
                                 const QString &exeName, const QString &statusPrefix);
    QString tessdataTargetDir() const;

    // Resolve shortcut to Win32 VK + modifier
    static bool keySequenceToWin32(const QKeySequence &seq, UINT &modifiers, UINT &vkey);
    static bool isAutoStartEnabled();

    QSettings *m_settings = nullptr;

    // General
    QLineEdit *m_savePathEdit = nullptr;
    QLineEdit *m_screenshotPathEdit = nullptr;
    QLineEdit *m_gifPathEdit = nullptr;
    QLineEdit *m_videoPathEdit = nullptr;
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
    QGroupBox *m_updateGroup = nullptr;
    QLabel *m_updateStatusLabel = nullptr;
    QPushButton *m_updateButton = nullptr;

    // Packages
    QListWidget *m_packageList = nullptr;
    QLabel *m_packageStatusLabel = nullptr;
    QLabel *m_tesseractStatusLabel = nullptr;
    QLabel *m_ffmpegStatusLabel = nullptr;
    QPushButton *m_tesseractDeleteButton = nullptr;
    QPushButton *m_ffmpegDeleteButton = nullptr;
    QPushButton *m_essentialOcrButton = nullptr;
    QPushButton *m_deleteSelectedOcrButton = nullptr;
    QStringList m_pendingOcrDownloads;
    QString m_activeOcrDownload;
    QString m_packageOperationStatus;
    QNetworkAccessManager *m_packageNetwork = nullptr;
    QNetworkReply *m_packageReply = nullptr;
    QFile *m_packageDownloadFile = nullptr;
    QString m_packageDownloadPath;
    qint64 m_packageExpectedSize = 0;

    // Capture
    QComboBox *m_formatCombo = nullptr;
    QSlider *m_qualitySlider = nullptr;
    QSpinBox *m_qualitySpin = nullptr;
    QSpinBox *m_delaySpin = nullptr;
    QCheckBox *m_copyAfterCaptureCheck = nullptr;
    QCheckBox *m_closeAfterCopyCheck = nullptr;
    QCheckBox *m_instantCopyAfterSelectionCheck = nullptr;
    QCheckBox *m_rememberLastAnnotationToolCheck = nullptr;

    // Appearance
    QCheckBox *m_darkModeCheck = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QLabel *m_opacityValueLabel = nullptr;
    QComboBox *m_crosshairStyleCombo = nullptr;
    QCheckBox *m_captureHintsCheck = nullptr;

    // Accessibility
    QCheckBox *m_highContrastCheck = nullptr;
    QCheckBox *m_blackTrayIconCheck = nullptr;

    // UI - Tool visibility
    QListWidget *m_toolVisibilityList = nullptr;
    QListWidget *m_toolbarControlVisibilityList = nullptr;
    QComboBox *m_visualSearchProviderCombo = nullptr;
    QCheckBox *m_rememberSettingsWindowSizeCheck = nullptr;
    bool m_rememberSettingsWindowSizeEnabled = false;

    // Shortcut
    QKeySequenceEdit *m_hotkeyEdit = nullptr;
    QKeySequenceEdit *m_recordingPauseHotkeyEdit = nullptr;
    QKeySequenceEdit *m_recordingStopHotkeyEdit = nullptr;
    QKeySequenceEdit *m_recordingCancelHotkeyEdit = nullptr;
    QKeySequenceEdit *m_instantCaptureHotkeyEdit = nullptr;
    QKeySequenceEdit *m_gifCaptureHotkeyEdit = nullptr;
    QKeySequenceEdit *m_videoCaptureHotkeyEdit = nullptr;
    QKeySequenceEdit *m_windowCaptureHotkeyEdit = nullptr;
    QMap<QString, QKeySequenceEdit*> m_overlayHotkeyEdits;
    QLabel *m_hotkeyStatusLabel = nullptr;
    QLabel *m_printScreenConflictLabel = nullptr;
    QPushButton *m_printScreenFixButton = nullptr;
    QPushButton *m_linuxPrintScreenBindingButton = nullptr;

    // Recording
    QSpinBox *m_recordingFpsSpin = nullptr;
    QSpinBox *m_recordingMaxSecSpin = nullptr;
    QComboBox *m_recordingLoopCombo = nullptr;
    QComboBox *m_gifSizePresetCombo = nullptr;
    QSpinBox *m_recordingStartDelaySpin = nullptr;
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
