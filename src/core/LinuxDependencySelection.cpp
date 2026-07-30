#include "LinuxDependencySelection.h"

#include <QDir>
#include <QHash>

QStringList supportedOcrLanguageCodes()
{
    return {"eng", "tur", "deu", "fra", "spa", "ita", "por", "rus", "ukr",
            "ara", "chi_sim", "chi_tra", "jpn", "kor", "nld", "pol"};
}

QMap<QString, QString> ocrLanguageDisplayNames()
{
    return {{"eng", "English"}, {"tur", QString::fromUtf8("Türkçe")}, {"deu", "Deutsch"},
            {"fra", QString::fromUtf8("Français")}, {"spa", QString::fromUtf8("Español")},
            {"ita", "Italiano"}, {"por", QString::fromUtf8("Português")},
            {"rus", QString::fromUtf8("Русский")}, {"ukr", QString::fromUtf8("Українська")},
            {"ara", QString::fromUtf8("العربية")}, {"chi_sim", QString::fromUtf8("中文（简体）")},
            {"chi_tra", QString::fromUtf8("中文（繁體）")}, {"jpn", QString::fromUtf8("日本語")},
            {"kor", QString::fromUtf8("한국어")}, {"nld", "Nederlands"}, {"pol", "Polski"}};
}

QStringList defaultOcrLanguageCodes(const QString &localeName)
{
    static const QHash<QString, QString> localeMap = {
        {"tr", "tur"}, {"de", "deu"}, {"fr", "fra"}, {"es", "spa"},
        {"it", "ita"}, {"pt", "por"}, {"ru", "rus"}, {"uk", "ukr"},
        {"ar", "ara"}, {"zh", "chi_sim"}, {"ja", "jpn"}, {"ko", "kor"},
        {"nl", "nld"}, {"pl", "pol"}
    };
    QStringList result{"eng"};
    const QString language = localeName.left(2).toLower();
    QString code = localeMap.value(language);
    if (language == "zh") {
        const QString normalized = localeName.toLower();
        if (normalized.contains("_tw") || normalized.contains("_hk") || normalized.contains("_mo") || normalized.contains("hant")) code = "chi_tra";
    }
    if (!code.isEmpty() && code != "eng") result.append(code);
    return result;
}

QStringList linuxDependencyArguments(bool ffmpeg, bool ocr,
                                     const QStringList &languages, bool desktop)
{
    QStringList args;
    if (ffmpeg) args << "--ffmpeg";
    if (ocr) {
        args << "--ocr";
        QStringList filtered;
        const auto supported = supportedOcrLanguageCodes();
        for (const QString &language : languages) {
            if (supported.contains(language) && !filtered.contains(language)) filtered << language;
        }
        if (!filtered.contains("eng")) filtered.prepend("eng");
        args << "--ocr-languages" << filtered.join(',');
    }
    if (desktop) args << "--desktop";
    return args;
}

bool linuxSetupShouldShow(bool completionKeyExists, bool completed)
{
    return !completionKeyExists || !completed;
}

QList<int> kdeShortcutsWithoutPlainPrint(const QList<int> &shortcuts)
{
    const int plainPrint = QKeyCombination(Qt::NoModifier, Qt::Key_Print).toCombined();
    QList<int> filtered = shortcuts;
    filtered.removeAll(plainPrint);
    return filtered;
}

bool defaultLinuxPortalSelection(const QString &sessionType)
{
    return sessionType.compare(QStringLiteral("wayland"), Qt::CaseInsensitive) == 0;
}

bool shouldOfferAppImageIntegration(const QString &appImagePath)
{
    const QString path = QDir::cleanPath(appImagePath.trimmed());
    if (path.isEmpty() || path == QStringLiteral("."))
        return false;
    return path != QStringLiteral("/opt")
        && !path.startsWith(QStringLiteral("/opt/"))
        && path != QStringLiteral("/usr")
        && !path.startsWith(QStringLiteral("/usr/"));
}
