#include <QtTest>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

#include "core/UpdateManager.h"

namespace {
class ReleaseReply : public QNetworkReply
{
public:
    ReleaseReply(const QNetworkRequest &request, QByteArray body,
                 NetworkError error, QObject *parent)
        : QNetworkReply(parent), m_body(std::move(body))
    {
        setRequest(request);
        setUrl(request.url());
        open(QIODevice::ReadOnly);
        if (error != NoError)
            setError(error, QStringLiteral("Release request failed"));
        QTimer::singleShot(0, this, [this]() {
            setFinished(true);
            emit finished();
        });
    }

    void abort() override {}
    qint64 bytesAvailable() const override
    {
        return m_body.size() - m_position + QNetworkReply::bytesAvailable();
    }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        const qint64 size = qMin(maxSize, m_body.size() - m_position);
        if (size <= 0)
            return -1;
        memcpy(data, m_body.constData() + m_position, size);
        m_position += size;
        return size;
    }

private:
    QByteArray m_body;
    qint64 m_position = 0;
};

class ReleaseNetwork : public QNetworkAccessManager
{
public:
    QByteArray body;
    QNetworkReply::NetworkError error = QNetworkReply::NoError;

protected:
    QNetworkReply *createRequest(Operation, const QNetworkRequest &request,
                                 QIODevice *) override
    {
        return new ReleaseReply(request, body, error, this);
    }
};
}

class UpdateManagerTests : public QObject
{
    Q_OBJECT

private slots:
    void failedChecksRespectUserIntent_data()
    {
        QTest::addColumn<QByteArray>("body");
        QTest::addColumn<int>("error");
        QTest::addColumn<bool>("manual");
        for (bool manual : {false, true}) {
            const QByteArray suffix = manual ? "-manual" : "-automatic";
            QTest::newRow(("network" + suffix).constData())
                << QByteArray() << int(QNetworkReply::HostNotFoundError) << manual;
            QTest::newRow(("forbidden" + suffix).constData())
                << QByteArray(R"({"message":"API rate limit exceeded"})")
                << int(QNetworkReply::ContentAccessDenied) << manual;
            QTest::newRow(("invalid-json" + suffix).constData())
                << QByteArray("<html>Service unavailable</html>") << int(QNetworkReply::NoError) << manual;
            QTest::newRow(("missing-tag" + suffix).constData())
                << QByteArray(R"({"message":"Service unavailable"})") << int(QNetworkReply::NoError) << manual;
            QTest::newRow(("empty-version" + suffix).constData())
                << QByteArray(R"({"tag_name":"v"})") << int(QNetworkReply::NoError) << manual;
        }
    }

    void failedChecksRespectUserIntent()
    {
        QFETCH(QByteArray, body);
        QFETCH(int, error);
        QFETCH(bool, manual);
        UpdateManager manager;
        delete manager.m_network;
        auto *network = new ReleaseNetwork;
        network->setParent(&manager);
        network->body = body;
        network->error = QNetworkReply::NetworkError(error);
        manager.m_network = network;
        QSignalSpy failures(&manager, &UpdateManager::failed);
        QSignalSpy checks(&manager, &UpdateManager::updateCheckFinished);

        manager.checkForUpdates(manual);
        QVERIFY(manager.isBusy());
        QTRY_VERIFY(!manager.isBusy());
        QCOMPARE(failures.count(), manual ? 1 : 0);
        QCOMPARE(checks.count(), 0);
        QVERIFY(!manager.updateAvailable());
        QVERIFY(!manager.statusText().isEmpty());
    }

    void validReleaseStillCompletes_data()
    {
        QTest::addColumn<bool>("manual");
        QTest::newRow("automatic") << false;
        QTest::newRow("manual") << true;
    }

    void validReleaseStillCompletes()
    {
        QFETCH(bool, manual);
        QCoreApplication::setApplicationVersion(QStringLiteral("4.7.0"));
        UpdateManager manager;
        delete manager.m_network;
        auto *network = new ReleaseNetwork;
        network->setParent(&manager);
        network->body = R"({"tag_name":"v4.7.0","html_url":"https://github.com/Benoks/EShot/releases/tag/v4.7.0","assets":[]})";
        manager.m_network = network;
        QSignalSpy failures(&manager, &UpdateManager::failed);
        QSignalSpy checks(&manager, &UpdateManager::updateCheckFinished);

        manager.checkForUpdates(manual);
        QTRY_COMPARE(checks.count(), 1);
        QCOMPARE(failures.count(), 0);
        QCOMPARE(manager.latestVersion(), QStringLiteral("4.7.0"));
        QVERIFY(!manager.updateAvailable());
        QVERIFY(!manager.isBusy());
    }
};

QTEST_GUILESS_MAIN(UpdateManagerTests)
#include "UpdateManagerTests.moc"
