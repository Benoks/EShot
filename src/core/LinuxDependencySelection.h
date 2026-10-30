#pragma once

#include <QStringList>
#include <QMap>

QStringList supportedOcrLanguageCodes();
QMap<QString, QString> ocrLanguageDisplayNames();
QStringList defaultOcrLanguageCodes(const QString &localeName);
QStringList linuxDependencyArguments(bool ffmpeg, bool ocr,
                                     const QStringList &languages, bool desktop);
bool linuxSetupShouldShow(bool completionKeyExists, bool completed);
QList<int> kdeShortcutsWithoutPlainPrint(const QList<int> &shortcuts);
QList<int> kdeShortcutsAfterEshotPrintScreenRegistration(const QList<int> &originalShortcuts,
                                                          bool eshotRegistered);
bool defaultLinuxPortalSelection(const QString &sessionType);
bool shouldOfferAppImageIntegration(const QString &appImagePath);
// True when the AppImage runs from a folder used by AppImage managers such as
// Gear Lever (~/AppImages) or AppImageLauncher (~/Applications). EShot sets
// itself up, so those copies add a second menu entry and miss its updates.
bool isAppImageManagedExternally(const QString &appImagePath, const QString &homePath);
// The dependency installer dialog must stay open while its install process
// runs: closing it would kill bash half way and orphan pkexec or the package
// manager. `processRunning` is true from start() until finished().
bool linuxDependencyDialogMayClose(bool processRunning);
