#include <QtTest>
#include "core/LinuxDependencySelection.h"

class LinuxDependencySelectionTests : public QObject {
    Q_OBJECT
private slots:
    void defaultsIncludeEnglishAndSupportedSystemLanguage() {
        QCOMPARE(defaultOcrLanguageCodes("tr_TR"), QStringList({"eng", "tur"}));
        QCOMPARE(defaultOcrLanguageCodes("en_US"), QStringList({"eng"}));
        QCOMPARE(defaultOcrLanguageCodes("xx_YY"), QStringList({"eng"}));
    }
    void argumentsReflectSelectionsAndFilterLanguages() {
        const auto args = linuxDependencyArguments(true, true, {"eng", "tur", "bogus"}, true);
        QVERIFY(args.contains("--ffmpeg"));
        QVERIFY(args.contains("--ocr"));
        QVERIFY(args.contains("--desktop"));
        QCOMPARE(args.at(args.indexOf("--ocr-languages") + 1), QString("eng,tur"));
        QCOMPARE(linuxDependencyArguments(false, false, {}, false), QStringList());
    }
    void everySupportedLanguageHasADisplayName() {
        const auto names = ocrLanguageDisplayNames();
        for (const QString &code : supportedOcrLanguageCodes()) {
            QVERIFY2(names.contains(code), qPrintable(code));
            QVERIFY2(!names.value(code).isEmpty(), qPrintable(code));
        }
    }
    void everySupportedLocaleCanBeDefaultSelected() {
        const QMap<QString, QString> locales{{"en_US", "eng"}, {"tr_TR", "tur"}, {"de_DE", "deu"},
            {"fr_FR", "fra"}, {"es_ES", "spa"}, {"it_IT", "ita"}, {"pt_BR", "por"},
            {"ru_RU", "rus"}, {"uk_UA", "ukr"}, {"ar_SA", "ara"}, {"zh_CN", "chi_sim"},
            {"zh_TW", "chi_tra"}, {"ja_JP", "jpn"}, {"ko_KR", "kor"}, {"nl_NL", "nld"}, {"pl_PL", "pol"}};
        for (auto it = locales.cbegin(); it != locales.cend(); ++it) {
            const auto defaults = defaultOcrLanguageCodes(it.key());
            QVERIFY2(defaults.contains(it.value()), qPrintable(it.key()));
            QVERIFY(defaults.contains("eng"));
        }
    }
    void legacyWizardCompletionDoesNotSkipLinuxSetup() {
        QVERIFY(linuxSetupShouldShow(false, false));
        QVERIFY(linuxSetupShouldShow(false, true));
        QVERIFY(linuxSetupShouldShow(true, false));
        QVERIFY(!linuxSetupShouldShow(true, true));
    }
    void removesOnlyPlainPrintFromKdeShortcutList() {
        const int plainPrint = QKeyCombination(Qt::NoModifier, Qt::Key_Print).toCombined();
        const int alternate = QKeyCombination(Qt::MetaModifier | Qt::ShiftModifier,
                                              Qt::Key_S).toCombined();
        QCOMPARE(kdeShortcutsWithoutPlainPrint({alternate, plainPrint}), QList<int>({alternate}));
        QCOMPARE(kdeShortcutsWithoutPlainPrint({plainPrint}), QList<int>());
    }
    void selectsPortalPackagesByDefaultOnWayland() {
        QVERIFY(defaultLinuxPortalSelection("wayland"));
        QVERIFY(defaultLinuxPortalSelection("WAYLAND"));
        QVERIFY(!defaultLinuxPortalSelection("x11"));
        QVERIFY(!defaultLinuxPortalSelection(""));
    }
    void offersIntegrationOnlyForUserManagedAppImages() {
        QVERIFY(!shouldOfferAppImageIntegration(QString()));
        QVERIFY(shouldOfferAppImageIntegration(
            QStringLiteral("/home/user/Downloads/EShot.AppImage")));
        QVERIFY(shouldOfferAppImageIntegration(
            QStringLiteral("/home/user/.local/opt/EShot/EShot.AppImage")));
        QVERIFY(!shouldOfferAppImageIntegration(
            QStringLiteral("/opt/eshot/EShot.AppImage")));
        QVERIFY(!shouldOfferAppImageIntegration(
            QStringLiteral("/usr/lib/eshot/EShot.AppImage")));
    }
};
QTEST_MAIN(LinuxDependencySelectionTests)
#include "LinuxDependencySelectionTests.moc"
