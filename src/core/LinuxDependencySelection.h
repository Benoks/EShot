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
