#include "UpdateManager.h"
#include "LinuxUpdateScript.h"
#include "TranslationManager.h"
#include "UpdateAssetSelector.h"
#include "UpdatePolicy.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QSettings>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVersionNumber>

namespace {
bool isNewerVersion(const QString &latest, const QString &current)
{
    QVersionNumber latestVersion = QVersionNumber::fromString(latest.trimmed());
    QVersionNumber currentVersion = QVersionNumber::fromString(current.trimmed());
    if (latestVersion.isNull())
        return false;
    if (currentVersion.isNull())
        return latest.trimmed() != current.trimmed();
    return QVersionNumber::compare(latestVersion, currentVersion) > 0;
}

QString psSingleQuote(QString value)
{
    value.replace('\'', "''");
    return QStringLiteral("'") + value + QStringLiteral("'");
}
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
    , m_network(new QNetworkAccessManager(this))
{
    setStatus(TranslationManager::updateStatusIdle());
}

UpdateManager::~UpdateManager()
{
    if (m_checkReply) {
        disconnect(m_checkReply, nullptr, this, nullptr);
        m_checkReply->abort();
    }
    if (m_downloadReply) {
        disconnect(m_downloadReply, nullptr, this, nullptr);
        m_downloadReply->abort();
    }
    if (m_releaseListReply) {
        disconnect(m_releaseListReply, nullptr, this, nullptr);
        m_releaseListReply->abort();
    }
    if (m_downloadFile) {
        m_downloadFile->close();
        delete m_downloadFile;
    }
}

void UpdateManager::setStatus(const QString &status)
{
    if (m_statusText == status)
        return;
    m_statusText = status;
    emit statusChanged();
}

void UpdateManager::checkForUpdates(bool manual)
{
    if (isBusy())
        return;

    m_checking = true;
    setStatus(TranslationManager::updateStatusChecking());

    QUrl url(QStringLiteral("https://api.github.com/repos/Benoks/EShot/releases/latest"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("_t"), QString::number(QDateTime::currentMSecsSinceEpoch()));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "EShot-Updater");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    m_checkReply = m_network->get(request);
    connect(m_checkReply, &QNetworkReply::finished, this, [this, manual]() {
        QNetworkReply *reply = m_checkReply;
        m_checkReply = nullptr;
        m_checking = false;

        if (reply->error() != QNetworkReply::NoError) {
            const QString msg = reply->errorString();
            reply->deleteLater();
            m_installAfterCheck = false;
            setStatus(TranslationManager::updateStatusFailed(msg));
            emit failed(msg);
            return;
        }

        const QByteArray data = reply->readAll();
        reply->deleteLater();
        parseRelease(data, manual);
    });
}

void UpdateManager::parseRelease(const QByteArray &data, bool manual)
{
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        const QString msg = TranslationManager::updateInvalidResponse();
        m_installAfterCheck = false;
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    const QJsonObject obj = doc.object();
    QString latestTag = obj.value(QStringLiteral("tag_name")).toString();
    if (latestTag.startsWith('v', Qt::CaseInsensitive))
        latestTag = latestTag.mid(1);

    m_latestVersion = latestTag;
    m_releaseUrl = obj.value(QStringLiteral("html_url")).toString();
    const UpdatePlatform platform =
#ifdef Q_OS_WIN
        UpdatePlatform::Windows;
#else
        UpdatePlatform::Linux;
#endif
    const UpdateAsset asset = selectUpdateAsset(
        obj.value(QStringLiteral("assets")).toArray(),
        platform,
        QSysInfo::currentCpuArchitecture());
    m_installerUrl = asset.url;
    m_installerName = asset.name;
    m_installerSize = asset.size;
    m_installerSha256 = asset.sha256;

    const QString currentVersion = QCoreApplication::applicationVersion();
    m_updateAvailable = !latestTag.isEmpty() && isNewerVersion(latestTag, currentVersion);

    if (m_updateAvailable) {
        setStatus(TranslationManager::updateStatusAvailable(latestTag));
    } else {
        if (manual)
            setStatus(TranslationManager::updateStatusUpToDate());
        else
            setStatus(TranslationManager::updateStatusIdle());
    }

    emit updateCheckFinished(m_updateAvailable, m_latestVersion);
    emit statusChanged();

    if (m_installAfterCheck) {
        m_installAfterCheck = false;
        if (m_updateAvailable)
            QTimer::singleShot(0, this, [this]() { installUpdate(); });
    }

    if (!manual && m_updateAvailable)
        checkSilentUpdateEligibility();
}

void UpdateManager::checkSilentUpdateEligibility()
{
    if (m_releaseListReply || !isSelfManagedInstall())
        return;

    QUrl url(QStringLiteral("https://api.github.com/repos/Benoks/EShot/releases"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
    url.setQuery(query);

    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "EShot-Updater");
    request.setTransferTimeout(30000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_releaseListReply = m_network->get(request);
    connect(m_releaseListReply, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = m_releaseListReply;
        m_releaseListReply = nullptr;
        if (!reply || reply->error() != QNetworkReply::NoError) {
            if (reply) reply->deleteLater();
            return;
        }
        const QJsonDocument document = QJsonDocument::fromJson(reply->readAll());
        reply->deleteLater();
        if (document.isArray() && shouldSilentlyInstallUpdate(
                countNewerStableReleases(document.array(), QCoreApplication::applicationVersion()),
                isSelfManagedInstall())) {
            installUpdate(true);
        }
    });
}

bool UpdateManager::isSelfManagedInstall() const
{
#ifdef Q_OS_WIN
    const QString appDirectory = QDir::cleanPath(QCoreApplication::applicationDirPath());
    const QString uninstallKey = QStringLiteral(
        "Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\{E5H0T-SCAP-2024-GUID-000000000001}_is1");
    for (const QString &root : {QStringLiteral("HKEY_CURRENT_USER\\"),
                                QStringLiteral("HKEY_LOCAL_MACHINE\\")}) {
        QSettings install(root + uninstallKey, QSettings::NativeFormat);
        const QString installDirectory = QDir::cleanPath(install.value(QStringLiteral("InstallLocation")).toString());
        if (!installDirectory.isEmpty() && installDirectory == appDirectory)
            return true;
    }
    return false;
#elif defined(Q_OS_LINUX)
    const QFileInfo appImage(qEnvironmentVariable("APPIMAGE"));
    const QString integratedPath = QDir::home().filePath(
        QStringLiteral(".local/opt/EShot/EShot.AppImage"));
    return appImage.isFile() && appImage.isWritable()
        && QDir::cleanPath(appImage.absoluteFilePath()) == QDir::cleanPath(integratedPath);
#else
    return false;
#endif
}

QString UpdateManager::updateCacheDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (dir.trimmed().isEmpty())
        dir = QDir::tempPath() + QStringLiteral("/EShot");
    dir = QDir(dir).filePath(QStringLiteral("updates"));
    QDir().mkpath(dir);
    return dir;
}

void UpdateManager::installUpdate(bool silent)
{
    if (isBusy())
        return;
    m_silentUpdate = silent;
    if (!m_updateAvailable) {
        m_installAfterCheck = true;
        checkForUpdates(true);
        return;
    }
    if (m_installerUrl.isEmpty()) {
        const QString msg = TranslationManager::updateNoInstaller();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }
#if defined(Q_OS_LINUX)
    const QString appImagePath = qEnvironmentVariable("APPIMAGE");
    if (appImagePath.isEmpty() || !QFileInfo::exists(appImagePath)) {
        const QString msg = TranslationManager::updateNoInstaller();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }
#elif !defined(Q_OS_WIN)
    const QString msg = TranslationManager::updateNoInstaller();
    setStatus(TranslationManager::updateStatusFailed(msg));
    emit failed(msg);
    return;
#endif
    downloadInstaller();
}

void UpdateManager::downloadInstaller()
{
    m_downloading = true;
    setStatus(TranslationManager::updateStatusDownloading());

    const QString fileName = m_installerName.isEmpty()
#ifdef Q_OS_WIN
        ? QStringLiteral("EShot_Update_%1.exe").arg(m_latestVersion)
#else
        ? QStringLiteral("EShot_Update_%1.AppImage").arg(m_latestVersion)
#endif
        : QFileInfo(m_installerName).fileName();
    if (fileName.isEmpty() || fileName == QLatin1String(".") || fileName == QLatin1String("..")) {
        const QString msg = TranslationManager::updateNoInstaller();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }
    const QString path = QDir(updateCacheDir()).filePath(fileName);
    m_downloadFile = new QFile(path);
    if (!m_downloadFile->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString msg = m_downloadFile->errorString();
        delete m_downloadFile;
        m_downloadFile = nullptr;
        m_downloading = false;
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    QNetworkRequest request{QUrl(m_installerUrl)};
    request.setRawHeader("User-Agent", "EShot-Updater");
    request.setTransferTimeout(600000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    m_downloadReply = m_network->get(request);

    connect(m_downloadReply, &QNetworkReply::readyRead, this, [this]() {
        if (m_downloadFile && m_downloadReply)
            m_downloadFile->write(m_downloadReply->readAll());
    });
    connect(m_downloadReply, &QNetworkReply::downloadProgress, this, &UpdateManager::downloadProgress);
    connect(m_downloadReply, &QNetworkReply::finished, this, &UpdateManager::finishDownload);
}

void UpdateManager::finishDownload()
{
    QNetworkReply *reply = m_downloadReply;
    m_downloadReply = nullptr;
    m_downloading = false;

    if (m_downloadFile && reply)
        m_downloadFile->write(reply->readAll());

    const QString path = m_downloadFile ? m_downloadFile->fileName() : QString();
    if (m_downloadFile) {
        m_downloadFile->flush();
        m_downloadFile->close();
    }

    if (!reply || reply->error() != QNetworkReply::NoError) {
        const QString msg = reply ? reply->errorString() : TranslationManager::updateInvalidResponse();
        if (m_downloadFile) {
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        if (!path.isEmpty())
            QFile::remove(path);
        if (reply) reply->deleteLater();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }
    reply->deleteLater();

    const qint64 size = QFileInfo(path).size();
    if (size <= 0 || (m_installerSize > 0 && size != m_installerSize)) {
        if (m_downloadFile) {
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        QFile::remove(path);
        const QString msg = TranslationManager::updateInvalidDownload();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    // GitHub exposes a digest for AppImage assets, which is mandatory for a
    // self-replacing Linux update. Older Windows releases do not always carry
    // the optional digest field, so keep accepting their size-validated setup
    // executables while verifying them whenever a digest is present.
#ifdef Q_OS_WIN
    const bool digestRequired = false;
#else
    const bool digestRequired = true;
#endif
    const bool digestValid = downloadedAssetDigestIsValid(
        path, m_installerSha256, digestRequired);
    if (!digestValid) {
        if (m_downloadFile) {
            delete m_downloadFile;
            m_downloadFile = nullptr;
        }
        QFile::remove(path);
        const QString msg = TranslationManager::updateInvalidDownload();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    delete m_downloadFile;
    m_downloadFile = nullptr;
    launchInstaller(path);
}

void UpdateManager::launchInstaller(const QString &installerPath)
{
    m_installing = true;
    setStatus(TranslationManager::updateStatusInstalling());

#ifdef Q_OS_WIN
    const QString exePath = QCoreApplication::applicationFilePath();
    const QString scriptPath = QDir(updateCacheDir()).filePath(
        QStringLiteral("install_update_%1.ps1").arg(QDateTime::currentMSecsSinceEpoch()));
    const QString logPath = QDir(updateCacheDir()).filePath(QStringLiteral("install_update.log"));

    QString script;
    script += QStringLiteral("$ErrorActionPreference = 'SilentlyContinue'\r\n");
    script += QStringLiteral("$pidToWait = %1\r\n").arg(QCoreApplication::applicationPid());
    script += QStringLiteral("$installer = %1\r\n").arg(psSingleQuote(QDir::toNativeSeparators(installerPath)));
    script += QStringLiteral("$exe = %1\r\n").arg(psSingleQuote(QDir::toNativeSeparators(exePath)));
    script += QStringLiteral("$log = %1\r\n").arg(psSingleQuote(QDir::toNativeSeparators(logPath)));
    script += QStringLiteral("try { Wait-Process -Id $pidToWait -Timeout 45 } catch {}\r\n");
    script += QStringLiteral("$args = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',('/LOG=' + $log))\r\n");
    script += QStringLiteral("$p = Start-Process -FilePath $installer -ArgumentList $args -Wait -PassThru\r\n");
    script += QStringLiteral("if ($p.ExitCode -eq 0) { Start-Sleep -Milliseconds 800; if (Test-Path $exe) { Start-Process -FilePath $exe -ArgumentList '--silent' } }\r\n");
    script += QStringLiteral("Remove-Item -LiteralPath $installer -Force\r\n");
    script += QStringLiteral("Remove-Item -LiteralPath $PSCommandPath -Force\r\n");

    QFile file(scriptPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        const QString msg = file.errorString();
        m_installing = false;
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }
    file.write(QByteArrayLiteral("\xEF\xBB\xBF"));
    file.write(script.toUtf8());
    file.close();

    const QString program = QStringLiteral("powershell.exe");
    const QStringList args = {
        QStringLiteral("-NoProfile"),
        QStringLiteral("-ExecutionPolicy"), QStringLiteral("Bypass"),
        QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
        QStringLiteral("-File"), scriptPath
    };

    if (!QProcess::startDetached(program, args)) {
        m_installing = false;
        const QString msg = TranslationManager::updateCannotLaunchInstaller();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    setStatus(TranslationManager::updateStatusRestarting());
    emit installerLaunched();
    QTimer::singleShot(500, qApp, &QCoreApplication::quit);
#elif defined(Q_OS_LINUX)
    const QString currentPath = QFileInfo(qEnvironmentVariable("APPIMAGE")).absoluteFilePath();
    const QFileInfo currentInfo(currentPath);
    if (!currentInfo.exists() || !currentInfo.isFile() || !currentInfo.isWritable()) {
        m_installing = false;
        const QString msg = TranslationManager::updateCannotLaunchInstaller();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    const QString scriptPath = QDir(updateCacheDir()).filePath(
        QStringLiteral("install_update_%1.sh").arg(QDateTime::currentMSecsSinceEpoch()));
    const QString script = buildLinuxUpdateScript(
        QCoreApplication::applicationPid(), currentPath, installerPath);
    QFile file(scriptPath);
    if (script.isEmpty() || !file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        m_installing = false;
        const QString msg = file.errorString().isEmpty()
            ? TranslationManager::updateCannotLaunchInstaller()
            : file.errorString();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }
    file.write(script.toUtf8());
    file.close();
    QFile::setPermissions(scriptPath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);

    if (!QProcess::startDetached(QStringLiteral("/bin/sh"), {scriptPath})) {
        m_installing = false;
        QFile::remove(scriptPath);
        const QString msg = TranslationManager::updateCannotLaunchInstaller();
        setStatus(TranslationManager::updateStatusFailed(msg));
        emit failed(msg);
        return;
    }

    setStatus(TranslationManager::updateStatusRestarting());
    emit installerLaunched();
    QTimer::singleShot(500, qApp, &QCoreApplication::quit);
#else
    Q_UNUSED(installerPath);
    m_installing = false;
    const QString msg = TranslationManager::updateNoInstaller();
    setStatus(TranslationManager::updateStatusFailed(msg));
    emit failed(msg);
#endif
}
