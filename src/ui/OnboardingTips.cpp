#include "OnboardingTips.h"

#include <QKeySequence>
#include <QSettings>

namespace OnboardingTips {
namespace {
QString key(const char *name)
{
    return QStringLiteral("tips/") + QLatin1String(name);
}
}

bool isSeen(const char *tip)
{
    return QSettings(QStringLiteral("EShot"), QStringLiteral("EShot")).value(key(tip), false).toBool();
}

void markSeen(const char *tip)
{
    QSettings(QStringLiteral("EShot"), QStringLiteral("EShot")).setValue(key(tip), true);
}

int count(const char *counter)
{
    return QSettings(QStringLiteral("EShot"), QStringLiteral("EShot")).value(key(counter), 0).toInt();
}

void increment(const char *counter)
{
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    settings.setValue(key(counter), settings.value(key(counter), 0).toInt() + 1);
}

void resetAll()
{
    QSettings(QStringLiteral("EShot"), QStringLiteral("EShot")).remove(QStringLiteral("tips"));
}

QString overlayShortcutText(const QString &id, const QString &fallback)
{
    const QString configured = QSettings(QStringLiteral("EShot"), QStringLiteral("EShot"))
        .value(QStringLiteral("overlayShortcut/%1").arg(id), fallback).toString().trimmed();
    // An empty value means the user disabled the shortcut. PortableText keeps
    // "Ctrl"/"Shift" like the rest of the UI; NativeText would translate key
    // names with the system locale, not EShot's language.
    return QKeySequence(configured).toString(QKeySequence::PortableText);
}

}
