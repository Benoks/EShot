#include <QtTest>

#include "core/TranslationManager.h"
#include "ui/SettingsSearch.h"

class SettingsSearchTests : public QObject {
    Q_OBJECT
private slots:
    void foldsCaseAndAccents()
    {
        using SettingsSearch::fold;
        QCOMPARE(fold(QStringLiteral("Sistemle Başlat")), QStringLiteral("sistemle baslat"));
        QCOMPARE(fold(QString::fromUtf8("ŞĞÜÖÇ şğüöç")), QStringLiteral("sguoc sguoc"));
        QCOMPARE(fold(QString::fromUtf8("Éléphant Ñandú")), QStringLiteral("elephant nandu"));
        QCOMPARE(fold(QString::fromUtf8("ＥＳｈｏｔ")), QStringLiteral("eshot"));
    }

    void foldsBothTurkishIs()
    {
        using SettingsSearch::fold;
        // Dotted İ and dotless ı both become a plain i.
        QCOMPARE(fold(QString::fromUtf8("İSTANBUL")), QStringLiteral("istanbul"));
        QCOMPARE(fold(QString::fromUtf8("ılık ISI")), QStringLiteral("ilik isi"));
        QCOMPARE(fold(QString::fromUtf8("Kısayol")), fold(QStringLiteral("KISAYOL")));
    }

    void everyWordOfTheQueryMustMatch()
    {
        const QString text = SettingsSearch::fold(QString::fromUtf8("Seçimi otomatik kilitle"));
        QVERIFY(SettingsSearch::matches(text, SettingsSearch::terms(QStringLiteral("  KILIT   secim "))));
        QVERIFY(SettingsSearch::matches(text, SettingsSearch::terms(QString::fromUtf8("otomatik"))));
        QVERIFY(!SettingsSearch::matches(text, SettingsSearch::terms(QStringLiteral("kilit video"))));
        QVERIFY(!SettingsSearch::matches(text, SettingsSearch::terms(QStringLiteral("   "))));
        QCOMPARE(SettingsSearch::terms(QString::fromUtf8(" Kısa  TUŞ ")),
                 QStringList({QStringLiteral("kisa"), QStringLiteral("tus")}));
    }

    void findsTheEnglishTextOfATranslatedLabel()
    {
        const auto index = SettingsSearch::englishIndex(TranslationManager::Turkish);
        const auto turkish = [](const char *key) {
            for (int i = 0; i < TranslationManager::translationCount(); ++i) {
                const auto &row = TranslationManager::translationAt(i);
                if (qstrcmp(row.key, key) == 0)
                    return QString::fromUtf8(row.vals[TranslationManager::Turkish]);
            }
            return QString();
        };
        QVERIFY(SettingsSearch::englishFor(index, turkish("tabHotkey")).contains(QStringLiteral("hotkey")));
        // Form labels carry a colon; the lookup ignores it.
        QVERIFY(SettingsSearch::englishFor(index, turkish("delay")).contains(QStringLiteral("delay")));
        QVERIFY(SettingsSearch::englishFor(index, turkish("delay").chopped(1)).contains(QStringLiteral("delay")));
        QVERIFY(SettingsSearch::englishFor(index, QStringLiteral("Xyzzy:")).isEmpty());
        QVERIFY(SettingsSearch::englishIndex(TranslationManager::English).isEmpty());
    }
};

QTEST_APPLESS_MAIN(SettingsSearchTests)
#include "SettingsSearchTests.moc"
