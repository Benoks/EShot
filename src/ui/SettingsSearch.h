#pragma once

#include <QHash>
#include <QString>
#include <QStringList>

// Text matching for the search field in Settings. Plain string functions,
// so they are tested without any widgets.
namespace SettingsSearch {

// Case- and accent-insensitive form of a text: compatibility-decomposed
// (full-width letters become plain ones), combining marks dropped, case
// folded, and the Turkish dotless ı folded to i, so "İŞĞÜÖÇ" == "isguoc".
QString fold(const QString &text);

// The folded words of a query. A setting matches when it has every word.
QStringList terms(const QString &query);

// True when every term occurs in the folded text. No terms match nothing.
bool matches(const QString &foldedText, const QStringList &terms);

// Maps the UI texts of one language to the English text of the same
// translation key (both folded), so "hotkey" also finds settings in a
// Turkish UI. Empty for English.
QHash<QString, QString> englishIndex(int language);
// The folded English text of a UI string such as "Gecikme:", or "".
QString englishFor(const QHash<QString, QString> &index, const QString &text);

}
