#include "SettingsSearch.h"

#include "../core/TranslationManager.h"

namespace {

// Lookup key of a UI text: folded, without the colon of a form label or the
// symbol in front of a tool name.
QString lookupKey(const QString &text)
{
    QString key = SettingsSearch::fold(text);
    int start = 0;
    int end = key.size();
    while (start < end && !key.at(start).isLetterOrNumber())
        ++start;
    while (end > start && !key.at(end - 1).isLetterOrNumber())
        --end;
    return key.mid(start, end - start);
}

}

QString SettingsSearch::fold(const QString &text)
{
    const QString decomposed = text.normalized(QString::NormalizationForm_KD);
    QString folded;
    folded.reserve(decomposed.size());
    for (const QChar c : decomposed) {
        switch (c.category()) {
        case QChar::Mark_NonSpacing:
        case QChar::Mark_SpacingCombining:
        case QChar::Mark_Enclosing:
            continue;
        default:
            break;
        }
        folded += c == QChar(0x0131) ? QChar(u'i') : c;
    }
    return folded.toCaseFolded();
}

QStringList SettingsSearch::terms(const QString &query)
{
    return fold(query).simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

bool SettingsSearch::matches(const QString &foldedText, const QStringList &terms)
{
    if (terms.isEmpty())
        return false;
    for (const QString &term : terms) {
        if (!foldedText.contains(term))
            return false;
    }
    return true;
}

QHash<QString, QString> SettingsSearch::englishIndex(int language)
{
    QHash<QString, QString> index;
    if (language == TranslationManager::English || language < 0 || language >= TranslationManager::LangCount)
        return index;
    index.reserve(TranslationManager::translationCount());
    for (int i = 0; i < TranslationManager::translationCount(); ++i) {
        const auto &row = TranslationManager::translationAt(i);
        const QString key = lookupKey(QString::fromUtf8(row.vals[language]));
        if (key.isEmpty())
            continue;
        const QString english = fold(QString::fromUtf8(row.vals[TranslationManager::English]));
        QString &entry = index[key];
        // Two keys can share a translation ("Kaydet" is save and Save).
        if (!entry.contains(english))
            entry += entry.isEmpty() ? english : QLatin1Char('\n') + english;
    }
    return index;
}

QString SettingsSearch::englishFor(const QHash<QString, QString> &index, const QString &text)
{
    return index.isEmpty() ? QString() : index.value(lookupKey(text));
}
