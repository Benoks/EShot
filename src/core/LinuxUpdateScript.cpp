#include "LinuxUpdateScript.h"

namespace {
QString shellSingleQuote(QString value)
{
    value.replace(QChar(0x27), QStringLiteral("'\"'\"'"));
    return QStringLiteral("'") + value + QStringLiteral("'");
}
}

QString buildLinuxUpdateScript(qint64 processId,
                               const QString &currentAppImage,
                               const QString &downloadedAppImage)
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
    script += QStringLiteral("cp -f -- \"$current\" \"$current.bak\"\n");
    script += QStringLiteral("mv -f -- \"$staged\" \"$current\"\n");
    script += QStringLiteral("chmod 0755 -- \"$current\"\n");
    script += QStringLiteral("sync\n");
    script += QStringLiteral("\"$current\" --silent >/dev/null 2>&1 &\n");
    script += QStringLiteral("rm -f -- \"$download\" \"$0\"\n");
    return script;
}
