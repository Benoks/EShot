#include <QtTest>
#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QScopeGuard>
#include <QStyleFactory>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include "core/TranslationManager.h"
#include "ui/FirstRunWizard.h"

class FirstRunWizardLayoutTests : public QObject
{
    Q_OBJECT
private slots:
    void onboardingKeepsItsEnglishDefaultDespiteSavedLanguage()
    {
        QSettings settings("EShot", "EShot");
        const bool hadLanguage = settings.contains("language");
        const QVariant language = settings.value("language");
        const bool hadSetup = settings.contains("linuxSetupCompleted");
        const QVariant setup = settings.value("linuxSetupCompleted");
        const auto restore = qScopeGuard([&] {
            if (hadLanguage) settings.setValue("language", language);
            else settings.remove("language");
            if (hadSetup) settings.setValue("linuxSetupCompleted", setup);
            else settings.remove("linuxSetupCompleted");
        });
        settings.setValue("language", static_cast<int>(TranslationManager::Turkish));
        settings.setValue("linuxSetupCompleted", true);
        // The startup flow requests English without overwriting saved preferences.
        TranslationManager::setLanguage(TranslationManager::English, false);
        FirstRunWizard wizard;
        QCOMPARE(wizard.windowTitle(), QStringLiteral("Welcome to EShot!"));
        auto *languageCombo = wizard.findChild<QComboBox *>();
        QVERIFY(languageCombo);
        QCOMPARE(languageCombo->currentData().toString(), QStringLiteral("en"));
        QCOMPARE(settings.value("language").toInt(), static_cast<int>(TranslationManager::Turkish));
    }

    void compactContent_data()
    {
        QTest::addColumn<QString>("style");
        QTest::addColumn<int>("language");
        for (const QString &style : {QStringLiteral("Fusion"), QStringLiteral("Windows")}) {
            for (int language = 0; language < TranslationManager::LangCount; ++language)
                QTest::newRow(qPrintable(style + QString::number(language))) << style << language;
        }
    }

    void compactContent()
    {
        QFETCH(QString, style);
        QFETCH(int, language);
        QApplication::setStyle(QStyleFactory::create(style));
        TranslationManager::setLanguage(static_cast<TranslationManager::Language>(language), false);
        FirstRunWizard wizard;
        // A tall viewport must leave spare space below the form, not inside its rows.
        wizard.resize(640, 1800);
        wizard.show();
        QCoreApplication::processEvents();
        auto *content = wizard.findChild<QWidget *>(QStringLiteral("firstRunScrollContent"));
        QVERIFY(content);
        auto *layout = qobject_cast<QVBoxLayout *>(content->layout());
        QVERIFY(layout);
        auto *title = qobject_cast<QLabel *>(layout->itemAt(0)->widget());
        auto *description = qobject_cast<QLabel *>(layout->itemAt(1)->widget());
        QVERIFY(title);
        QVERIFY(description);
        QVERIFY2(title->height() <= title->sizeHint().height() + 2, "Title absorbs spare viewport height");
        QVERIFY2(description->height() <= description->heightForWidth(description->width()) + 2,
                 "Description absorbs spare viewport height");
        for (auto *group : content->findChildren<QGroupBox *>(QString(), Qt::FindDirectChildrenOnly))
            QVERIFY2(group->height() <= group->sizeHint().height() + 2,
                     qPrintable(group->title() + QStringLiteral(" absorbs spare viewport height")));
    }

    void smallWindowKeepsFinishVisibleAndFormScrollable()
    {
        QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
        TranslationManager::setLanguage(TranslationManager::English, false);
        FirstRunWizard wizard;
        wizard.resize(480, 320);
        wizard.show();
        QCoreApplication::processEvents();
        QCOMPARE(wizard.size(), QSize(480, 320));
        auto *scroll = wizard.findChild<QScrollArea *>(QStringLiteral("firstRunScrollArea"));
        QVERIFY(scroll);
        QVERIFY(scroll->verticalScrollBar()->maximum() > 0);
        QCOMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        QPushButton *finish = nullptr;
        for (auto *button : wizard.findChildren<QPushButton *>())
            if (button->isDefault()) finish = button;
        QVERIFY(finish);
        QVERIFY(finish->isVisible());
        QVERIFY(wizard.rect().contains(QRect(finish->mapTo(&wizard, QPoint()), finish->size())));
        scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
        QCoreApplication::processEvents();
        QVERIFY(finish->isVisible());
    }

    void initialSizeFitsAvailableScreen()
    {
        FirstRunWizard wizard;
        const QSize available = wizard.screen()->availableGeometry().size();
        QVERIFY(wizard.width() <= available.width());
        QVERIFY(wizard.height() <= available.height() - 32);
        if (!qEnvironmentVariableIsEmpty("ESHOT_WIZARD_SNAPSHOT")) {
            wizard.show();
            QCoreApplication::processEvents();
            QVERIFY(wizard.grab().save(qEnvironmentVariable("ESHOT_WIZARD_SNAPSHOT")));
        }
    }
};

int main(int argc, char **argv)
{
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, config.path());
    QApplication app(argc, argv);
    FirstRunWizardLayoutTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "FirstRunWizardLayoutTests.moc"
