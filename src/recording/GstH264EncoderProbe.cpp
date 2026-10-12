#include "GstH264EncoderProbe.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcessEnvironment>
#include <QSettings>
#include <QStandardPaths>
#include <QTimer>

namespace {
const char *kGroup = "recordingH264Probe";

qint64 mtimeOf(const QString &path)
{
    const QFileInfo info(path);
    return info.exists() ? info.lastModified().toMSecsSinceEpoch() : 0;
}

QList<qint64> registryMtimes()
{
    QList<qint64> mtimes;
    const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (const char *name : {"GST_REGISTRY_1_0", "GST_REGISTRY"}) {
        const QString path = env.value(QString::fromLatin1(name));
        if (!path.isEmpty())
            mtimes << mtimeOf(path);
    }
    if (!mtimes.isEmpty())
        return mtimes;
    QString cache = env.value(QStringLiteral("XDG_CACHE_HOME"));
    if (cache.isEmpty())
        cache = QDir::home().filePath(QStringLiteral(".cache"));
    const QDir dir(QDir(cache).filePath(QStringLiteral("gstreamer-1.0")));
    const QFileInfoList registries = dir.entryInfoList({QStringLiteral("registry.*.bin")},
                                                       QDir::Files, QDir::Name);
    for (const QFileInfo &info : registries)
        mtimes << info.lastModified().toMSecsSinceEpoch();
    return mtimes;
}

QString driverVersions()
{
    QStringList parts;
    QFile nvidia(QStringLiteral("/proc/driver/nvidia/version"));
    if (nvidia.open(QIODevice::ReadOnly))
        parts << QString::fromLocal8Bit(nvidia.readLine()).trimmed();
    // VA-API drivers (Mesa, intel-media-driver): their mtimes change on update.
    QStringList vaDirs = QProcessEnvironment::systemEnvironment()
        .value(QStringLiteral("LIBVA_DRIVERS_PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
    vaDirs << QStringLiteral("/usr/lib/dri") << QStringLiteral("/usr/lib64/dri")
           << QStringLiteral("/usr/lib/x86_64-linux-gnu/dri")
           << QStringLiteral("/usr/lib/aarch64-linux-gnu/dri");
    for (const QString &path : std::as_const(vaDirs)) {
        const QFileInfoList drivers = QDir(path).entryInfoList({QStringLiteral("*_drv_video.so")},
                                                               QDir::Files, QDir::Name);
        for (const QFileInfo &info : drivers)
            parts << QStringLiteral("%1@%2").arg(info.fileName())
                         .arg(info.lastModified().toMSecsSinceEpoch());
    }
    return parts.join(QLatin1Char(';'));
}
}

GstH264EncoderProbe *GstH264EncoderProbe::instance()
{
    static QPointer<GstH264EncoderProbe> probe;
    if (!probe)
        probe = new GstH264EncoderProbe(QCoreApplication::instance());
    return probe;
}

GstH264EncoderProbe::GstH264EncoderProbe(QObject *parent)
    : QObject(parent)
{
}

QString GstH264EncoderProbe::hardwareEncodingSettingsKey()
{
    return QStringLiteral("videoHardwareEncoding");
}

bool GstH264EncoderProbe::hardwareEncodingEnabled()
{
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    return settings.value(hardwareEncodingSettingsKey(), true).toBool();
}

QString GstH264EncoderProbe::currentSignature(const QString &gstLaunchPath) const
{
    return gstH264ProbeSignature(gstLaunchPath, mtimeOf(gstLaunchPath), registryMtimes(),
                                 driverVersions());
}

void GstH264EncoderProbe::loadState()
{
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    settings.beginGroup(QString::fromLatin1(kGroup));
    GstH264ProbeState stored;
    stored.signature = settings.value(QStringLiteral("signature")).toString();
    stored.working = settings.value(QStringLiteral("working")).toStringList();
    stored.failed = settings.value(QStringLiteral("failed")).toStringList();
    settings.endGroup();
    const GstH264ProbeState current = gstH264ProbeStateForSignature(
        stored, currentSignature(m_gstLaunchPath));
    const bool changed = current.signature != stored.signature;
    m_state = current;
    if (changed)
        saveState();
}

void GstH264EncoderProbe::saveState() const
{
    QSettings settings(QStringLiteral("EShot"), QStringLiteral("EShot"));
    settings.beginGroup(QString::fromLatin1(kGroup));
    settings.setValue(QStringLiteral("signature"), m_state.signature);
    settings.setValue(QStringLiteral("working"), m_state.working);
    settings.setValue(QStringLiteral("failed"), m_state.failed);
    settings.endGroup();
}

void GstH264EncoderProbe::ensureStarted(const QString &gstLaunchPath)
{
    if (gstLaunchPath.isEmpty() || m_process)
        return;
    if (!m_loaded || gstLaunchPath != m_gstLaunchPath) {
        m_gstLaunchPath = gstLaunchPath;
        loadState();
        m_loaded = true;
    }
    probeNext();
}

QStringList GstH264EncoderProbe::workingHardwareEncoders(bool *finished) const
{
    if (finished)
        *finished = m_loaded && !m_process && nextGstH264EncoderToProbe(m_state).isEmpty();
    return m_state.working;
}

void GstH264EncoderProbe::markFailed(const QString &encoder)
{
    if (!isHardwareGstH264Encoder(encoder))
        return;
    m_state.working.removeAll(encoder);
    if (!m_state.failed.contains(encoder))
        m_state.failed << encoder;
    saveState();
    qWarning() << "EShot: hardware encoder" << encoder << "failed in a recording; not using it again";
    if (!m_gstLaunchPath.isEmpty() && !m_process)
        probeNext();
}

void GstH264EncoderProbe::probeNext()
{
    const QString encoder = nextGstH264EncoderToProbe(m_state);
    if (encoder.isEmpty())
        return;

    auto *process = new QProcess(this);
    m_process = process;
    m_probingEncoder = encoder;
    process->setProgram(m_gstLaunchPath);
    process->setArguments(gstH264EncoderProbeArguments(encoder));
    process->setProcessChannelMode(QProcess::MergedChannels);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LANGUAGE"), QStringLiteral("en"));
    env.remove(QStringLiteral("GST_DEBUG"));
    process->setProcessEnvironment(env);

    auto *timeout = new QTimer(process);
    timeout->setSingleShot(true);
    connect(timeout, &QTimer::timeout, process, [process]() {
        process->kill();
    });
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart && process == m_process)
            finishProbe(m_probingEncoder, false, process->errorString());
    });
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus status) {
        if (process != m_process)
            return;
        const QString output = QString::fromLocal8Bit(process->readAll());
        const bool worked = status == QProcess::NormalExit && exitCode == 0;
        finishProbe(m_probingEncoder, worked,
                    worked ? QString() : gstFailureReason(output));
    });
    process->start();
    timeout->start(gstH264EncoderProbeTimeoutMs());
}

void GstH264EncoderProbe::finishProbe(const QString &probedEncoder, bool worked, const QString &detail)
{
    // Copy first: callers pass m_probingEncoder, which is cleared below.
    const QString encoder = probedEncoder;
    if (encoder.isEmpty())
        return;
    if (m_process) {
        m_process->disconnect(this);
        m_process->deleteLater();
        m_process = nullptr;
    }
    m_probingEncoder.clear();
    if (worked) {
        if (!m_state.working.contains(encoder))
            m_state.working << encoder;
        qInfo() << "EShot: hardware H.264 encoder available:" << encoder;
    } else {
        if (!m_state.failed.contains(encoder))
            m_state.failed << encoder;
        qInfo().noquote() << "EShot: hardware H.264 encoder" << encoder << "not usable:"
                          << (detail.isEmpty() ? QStringLiteral("probe failed") : detail);
    }
    saveState();
    probeNext();
}
