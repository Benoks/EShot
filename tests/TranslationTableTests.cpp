#include <QtTest>

#include "core/TranslationManager.h"

#include <QDirIterator>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

#ifndef ESHOT_SOURCE_DIR
#error "ESHOT_SOURCE_DIR must point at the repository root"
#endif

class TranslationTableTests : public QObject {
    Q_OBJECT

private:
    static QSet<QString> placeholders(const QString &text)
    {
        static const QRegularExpression placeholder(QStringLiteral("%(\\d+)"));
        QSet<QString> found;
        auto it = placeholder.globalMatch(text);
        while (it.hasNext())
            found.insert(it.next().captured(0));
        return found;
    }

    static QSet<QString> tableKeys()
    {
        QSet<QString> keys;
        for (int i = 0; i < TranslationManager::translationCount(); ++i)
            keys.insert(QString::fromUtf8(TranslationManager::translationAt(i).key));
        return keys;
    }

private slots:
    void everyRowHasEightValuesWithMatchingPlaceholders()
    {
        QVERIFY(TranslationManager::translationCount() > 0);
        for (int i = 0; i < TranslationManager::translationCount(); ++i) {
            const auto &row = TranslationManager::translationAt(i);
            const QString key = QString::fromUtf8(row.key);
            QVERIFY2(!key.isEmpty(), qPrintable(QStringLiteral("Row %1 has no key").arg(i)));
            QSet<QString> english;
            for (int language = 0; language < TranslationManager::LangCount; ++language) {
                const char *value = row.vals[language];
                QVERIFY2(value && *value,
                         qPrintable(QStringLiteral("%1 has no value for language %2")
                                        .arg(key).arg(language)));
                if (language == TranslationManager::English)
                    english = placeholders(QString::fromUtf8(value));
            }
            for (int language = 0; language < TranslationManager::LangCount; ++language) {
                const QSet<QString> found = placeholders(QString::fromUtf8(row.vals[language]));
                QVERIFY2(found == english,
                         qPrintable(QStringLiteral("%1: placeholders of language %2 differ from English")
                                        .arg(key).arg(language)));
            }
        }
    }

    void keysAreUnique()
    {
        QSet<QString> seen;
        for (int i = 0; i < TranslationManager::translationCount(); ++i) {
            const QString key = QString::fromUtf8(TranslationManager::translationAt(i).key);
            QVERIFY2(!seen.contains(key), qPrintable(QStringLiteral("Duplicate key %1").arg(key)));
            seen.insert(key);
        }
    }

    // Every tr("key") in the sources must resolve; a missing row would show
    // the raw key to the user.
    void everyKeyUsedInTheSourcesExists()
    {
        const QSet<QString> keys = tableKeys();
        static const QRegularExpression call(QStringLiteral("\\btr\\(\"([A-Za-z][A-Za-z0-9_]*)\"\\)"));
        QStringList missing;
        int files = 0;
        QDirIterator it(QStringLiteral(ESHOT_SOURCE_DIR "/src"),
                        {QStringLiteral("*.cpp"), QStringLiteral("*.h")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QFile file(it.next());
            QVERIFY(file.open(QIODevice::ReadOnly));
            ++files;
            const QString text = QString::fromUtf8(file.readAll());
            auto match = call.globalMatch(text);
            while (match.hasNext()) {
                const QString key = match.next().captured(1);
                if (!keys.contains(key))
                    missing << QStringLiteral("%1 (%2)").arg(key, QFileInfo(file.fileName()).fileName());
            }
        }
        QVERIFY(files > 10);
        missing.removeDuplicates();
        QVERIFY2(missing.isEmpty(), qPrintable(missing.join(QStringLiteral(", "))));
    }
};

QTEST_APPLESS_MAIN(TranslationTableTests)
#include "TranslationTableTests.moc"
