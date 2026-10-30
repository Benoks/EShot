#ifndef LINUXUPDATESCRIPT_H
#define LINUXUPDATESCRIPT_H

#include <QString>

// When expectedSha256 (64 hex digits) is given, the staged copy is checked
// against it right before it replaces the current AppImage.
QString buildLinuxUpdateScript(qint64 processId,
                               const QString &currentAppImage,
                               const QString &downloadedAppImage,
                               const QString &expectedSha256 = QString());

#endif
