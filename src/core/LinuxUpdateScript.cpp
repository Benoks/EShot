#include "LinuxUpdateScript.h"

#include <QRegularExpression>

namespace {
QString shellSingleQuote(QString value)
{
    value.replace(QChar(0x27), QStringLiteral("'\"'\"'"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}

// Lowercase hex digest, accepting GitHub's "sha256:" prefix; empty if invalid.
QString plainSha256(const QString &value)
{
    QString digest = value.trimmed().toLower();
    if (digest.startsWith(QStringLiteral("sha256:")))
        digest = digest.mid(7);
    static const QRegularExpression hex(QStringLiteral("^[0-9a-f]{64}$"));
    return hex.match(digest).hasMatch() ? digest : QString();
}
}

QString buildLinuxUpdateScript(qint64 processId,
                               const QString &currentAppImage,
                               const QString &downloadedAppImage,
                               const QString &expectedSha256)
{
    if (processId <= 0 || currentAppImage.isEmpty() || downloadedAppImage.isEmpty())
        return QString();

    QString script;
    script += QStringLiteral("#!/bin/sh\nset -eu\n");
    script += QStringLiteral("pid=%1\n").arg(processId);
    script += QStringLiteral("current=%1\n").arg(shellSingleQuote(currentAppImage));
    script += QStringLiteral("download=%1\n").arg(shellSingleQuote(downloadedAppImage));
    script += QStringLiteral("staged=\"${current}.new\"\n");
    script += QStringLiteral("cp -f -- \"$download\" \"$staged\"\n");
    script += QStringLiteral("chmod 0755 -- \"$staged\"\n");
    script += QStringLiteral("if [ ! -s \"$staged\" ] || [ ! -x \"$staged\" ]; then rm -f -- \"$staged\"; exit 1; fi\n");
    script += QStringLiteral("sync\n");
    script += QStringLiteral("attempts=0\n");
    script += QStringLiteral("while kill -0 \"$pid\" 2>/dev/null && [ \"$attempts\" -lt 300 ]; do\n");
    script += QStringLiteral("  sleep 0.2\n");
    script += QStringLiteral("  attempts=$((attempts + 1))\n");
    script += QStringLiteral("done\n");
    script += QStringLiteral("if kill -0 \"$pid\" 2>/dev/null; then rm -f -- \"$staged\"; exit 1; fi\n");
    const QString digest = plainSha256(expectedSha256);
    if (!digest.isEmpty()) {
        // The download sat in a user-writable cache; never install a file
        // that no longer matches the release digest.
        script += QStringLiteral("expected=%1\n").arg(digest);
        script += QStringLiteral("if command -v sha256sum >/dev/null 2>&1; then\n");
        script += QStringLiteral("  actual=$(sha256sum -- \"$staged\" | cut -d' ' -f1)\n");
        script += QStringLiteral("  if [ \"$actual\" != \"$expected\" ]; then rm -f -- \"$staged\"; \"$current\" --silent >/dev/null 2>&1 & exit 1; fi\n");
        script += QStringLiteral("fi\n");
    }
    script += QStringLiteral("cp -f -- \"$current\" \"$current.bak\"\n");
    script += QStringLiteral("mv -f -- \"$staged\" \"$current\"\n");
    script += QStringLiteral("chmod 0755 -- \"$current\"\n");
    script += QStringLiteral("sync\n");
    // The backup is only needed until the new file is in place.
    script += QStringLiteral("rm -f -- \"$current.bak\"\n");
    // Refresh the menu entry and icons of an integrated copy; a release may
    // change them. Never integrate a copy the user keeps elsewhere.
    script += QStringLiteral("if [ \"$current\" = \"${HOME:-}/.local/opt/EShot/EShot.AppImage\" ]; then\n");
    script += QStringLiteral("  \"$current\" --integrate-only >/dev/null 2>&1 || true\n");
    script += QStringLiteral("fi\n");
    script += QStringLiteral("\"$current\" --silent >/dev/null 2>&1 &\n");
    script += QStringLiteral("rm -f -- \"$download\" \"$0\"\n");
    return script;
}
