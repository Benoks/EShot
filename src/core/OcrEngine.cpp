#include "OcrEngine.h"
#include "OcrLanguageSelector.h"
#include <QProcess>
#include <QProcessEnvironment>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QHash>
#include <QPointer>
#include <QDateTime>
#include <QDebug>
#include <QPixmap>
#include <atomic>

QString OcrEngine::tesseractPath() {
    QStringList candidates = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract/tesseract.exe"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract.exe"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract/tesseract"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract"),
#ifdef Q_OS_WIN
        QStringLiteral("C:/Program Files/Tesseract-OCR/tesseract.exe"),
        QStringLiteral("C:/Program Files (x86)/Tesseract-OCR/tesseract.exe"),
#endif
    };
    for (const QString &p : candidates) {
        if (QFileInfo::exists(p)) return p;
    }
    QString envPath = QProcessEnvironment::systemEnvironment().value(QStringLiteral("TESSERACT_PATH"));
    if (!envPath.isEmpty() && QFileInfo::exists(envPath)) return envPath;
    const QString systemTesseract = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
    if (!systemTesseract.isEmpty()) return systemTesseract;
    return QString();
}

QString OcrEngine::tessdataDir() {
    const QString exe = tesseractPath();
    if (exe.isEmpty()) return QString();

    const QString environmentPath = QProcessEnvironment::systemEnvironment().value(
        QStringLiteral("TESSDATA_PREFIX"));
    if (!environmentPath.isEmpty()) {
        const QString direct = QDir::cleanPath(environmentPath);
        if (QFileInfo::exists(QDir(direct).filePath(QStringLiteral("eng.traineddata"))))
            return direct;
        const QString nested = QDir(direct).filePath(QStringLiteral("tessdata"));
        if (QFileInfo::exists(QDir(nested).filePath(QStringLiteral("eng.traineddata"))))
            return nested;
    }

    QFileInfo fi(exe);
    QString dir = fi.absoluteDir().absoluteFilePath(QStringLiteral("tessdata"));
    if (QFileInfo::exists(dir)) return dir;
    QStringList fallbacks = {
        QCoreApplication::applicationDirPath() + QStringLiteral("/tessdata"),
        QCoreApplication::applicationDirPath() + QStringLiteral("/tesseract/tessdata"),
    };
    for (const QString &p : fallbacks) {
        if (QFileInfo::exists(p)) return p;
    }

    const QString located = QStandardPaths::locate(
        QStandardPaths::GenericDataLocation, QStringLiteral("tessdata"),
        QStandardPaths::LocateDirectory);
    if (!located.isEmpty()) return located;

    const QStringList systemFallbacks = {
        QStringLiteral("/usr/share/tesseract-ocr/5/tessdata"),
        QStringLiteral("/usr/share/tesseract-ocr/4.00/tessdata"),
        QStringLiteral("/usr/local/share/tessdata")
    };
    for (const QString &path : systemFallbacks) {
        if (QFileInfo::exists(path)) return path;
    }
    return QString();
}

QString OcrEngine::mapLanguageTag(const QString &bcp47) {
    static const QHash<QString, QString> kMap = {
        {QStringLiteral("en"),  QStringLiteral("eng")},
        {QStringLiteral("en-US"), QStringLiteral("eng")},
        {QStringLiteral("en-GB"), QStringLiteral("eng")},
        {QStringLiteral("tr"),  QStringLiteral("tur")},
        {QStringLiteral("tr-TR"), QStringLiteral("tur")},
        {QStringLiteral("de"),  QStringLiteral("deu")},
        {QStringLiteral("de-DE"), QStringLiteral("deu")},
        {QStringLiteral("fr"),  QStringLiteral("fra")},
        {QStringLiteral("fr-FR"), QStringLiteral("fra")},
        {QStringLiteral("es"),  QStringLiteral("spa")},
        {QStringLiteral("es-ES"), QStringLiteral("spa")},
        {QStringLiteral("it"),  QStringLiteral("ita")},
        {QStringLiteral("it-IT"), QStringLiteral("ita")},
        {QStringLiteral("ru"),  QStringLiteral("rus")},
        {QStringLiteral("ru-RU"), QStringLiteral("rus")},
        {QStringLiteral("ja"),  QStringLiteral("jpn")},
        {QStringLiteral("ja-JP"), QStringLiteral("jpn")},
        {QStringLiteral("zh"),  QStringLiteral("chi_sim")},
        {QStringLiteral("zh-CN"), QStringLiteral("chi_sim")},
        {QStringLiteral("zh-Hant"), QStringLiteral("chi_tra")},
        {QStringLiteral("zh-TW"), QStringLiteral("chi_tra")},
        {QStringLiteral("ko"),  QStringLiteral("kor")},
        {QStringLiteral("ko-KR"), QStringLiteral("kor")},
        {QStringLiteral("pt"),  QStringLiteral("por")},
        {QStringLiteral("pt-BR"), QStringLiteral("por")},
        {QStringLiteral("pt-PT"), QStringLiteral("por")},
        {QStringLiteral("pl"),  QStringLiteral("pol")},
        {QStringLiteral("pl-PL"), QStringLiteral("pol")},
        {QStringLiteral("nl"),  QStringLiteral("nld")},
        {QStringLiteral("nl-NL"), QStringLiteral("nld")},
    };
    if (bcp47.isEmpty()) return QStringLiteral("eng");
    if (kMap.contains(bcp47)) return kMap.value(bcp47);
    QString lower = bcp47.toLower();
    if (kMap.contains(lower)) return kMap.value(lower);
    QString primary = bcp47.split(QStringLiteral("-")).first().toLower();
    if (kMap.contains(primary)) return kMap.value(primary);
    return bcp47;
}

OcrEngine::OcrEngine(QObject *parent) : QObject(parent) {}

OcrEngine::~OcrEngine()
{
    if (m_proc) {
        disconnect(m_proc, nullptr, this, nullptr);
        m_proc->kill();
        m_proc->waitForFinished(2000);
    }
    for (const QString &p : std::as_const(m_pendingFiles)) {
        if (!p.isEmpty() && QFile::exists(p)) QFile::remove(p);
    }
    m_pendingFiles.clear();
}

void OcrEngine::recognize(const QPixmap &pixmap, const QString &languageTag,
                          const QString &preferredLanguageTag) {
    if (pixmap.isNull()) {
        emit failed(QStringLiteral("empty image"));
        return;
    }

    if (m_proc && m_proc->state() != QProcess::NotRunning) {
        emit failed(QStringLiteral("OCR already running"));
        return;
    }

    const QString exe = tesseractPath();
    if (exe.isEmpty() || !QFileInfo::exists(exe)) {
        emit failed(QStringLiteral("Tesseract OCR engine not found. Please install Tesseract-OCR or place Tesseract in the app folder."));
        return;
    }

    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty()) tempDir = QDir::tempPath();
    QDir().mkpath(tempDir);

    const QString unique = QStringLiteral("%1_%2")
        .arg(QCoreApplication::applicationPid())
        .arg(QDateTime::currentMSecsSinceEpoch());
    const QString baseName = QStringLiteral("eshot_ocr_%1").arg(unique);
    const QString imagePath = QDir(tempDir).filePath(baseName + QStringLiteral(".png"));
    if (!pixmap.save(imagePath, "PNG")) {
        emit failed(QStringLiteral("cannot save temp image"));
        return;
    }
    m_pendingFiles.insert(imagePath);

    const QString td = tessdataDir();
    if (languageTag.compare(QStringLiteral("auto"), Qt::CaseInsensitive) == 0) {
        QString preferred = mapLanguageTag(preferredLanguageTag);
        if (preferredLanguageTag.trimmed().isEmpty())
            preferred = QStringLiteral("eng");
        startAutomaticRecognition(imagePath, td, preferred);
    } else {
        startRecognitionProcess(imagePath, td, mapLanguageTag(languageTag));
    }
}

void OcrEngine::startAutomaticRecognition(const QString &imagePath,
                                          const QString &tessdataDirectory,
                                          const QString &preferredLanguage)
{
    const QStringList installed = installedOcrLanguageCodes(tessdataDirectory);
    if (installed.isEmpty()) {
        failAndRemoveImage(imagePath, QStringLiteral("No OCR language packs are installed"));
        return;
    }

    auto continueWithScript = [this, imagePath, tessdataDirectory, installed,
                               preferredLanguage](const QString &script) {
        const QString languages = automaticOcrLanguageArgument(
            installed, script, preferredLanguage);
        qInfo() << "[EShot] OCR automatic selection script=" << script
                << "languages=" << languages;
        startRecognitionProcess(imagePath, tessdataDirectory, languages);
    };

    if (!ocrScriptDetectionAvailable(tessdataDirectory)) {
        qInfo() << "[EShot] OCR script detection data is unavailable; using preferred fallback";
        continueWithScript(QString());
        return;
    }

    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_proc, &QProcess::errorOccurred, this, [](QProcess::ProcessError error) {
        qWarning() << "[EShot] OCR script detection process error:" << error;
    });
    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, continueWithScript](int exitCode, QProcess::ExitStatus status) {
        const QString output = QString::fromUtf8(m_proc->readAllStandardOutput())
            + QLatin1Char('\n')
            + QString::fromUtf8(m_proc->readAllStandardError());
        const QString script = ocrScriptFromOsdOutput(output);
        qInfo() << "[EShot] OCR script detection finished exit=" << exitCode
                << "status=" << status << "script=" << script;
        m_proc->deleteLater();
        m_proc = nullptr;
        continueWithScript(script);
    });

    QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
    if (!tessdataDirectory.isEmpty())
        environment.insert(QStringLiteral("TESSDATA_PREFIX"), tessdataDirectory);
    m_proc->setProcessEnvironment(environment);

    QStringList arguments{
        QDir::toNativeSeparators(imagePath), QStringLiteral("stdout"),
        QStringLiteral("-l"), QStringLiteral("osd"),
        QStringLiteral("--psm"), QStringLiteral("0")
    };
    if (!tessdataDirectory.isEmpty())
        arguments << QStringLiteral("--tessdata-dir")
                  << QDir::toNativeSeparators(tessdataDirectory);
    m_proc->start(tesseractPath(), arguments);
    // Starting Tesseract is normally immediate, but cold AppImages and slow
    // network-mounted home directories can legitimately take longer. This is
    // an error path only, so retain the reliable startup allowance.
    if (!m_proc->waitForStarted(10000)) {
        qWarning() << "[EShot] OCR script detection could not start:" << m_proc->errorString();
        if (m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
        m_proc->deleteLater();
        m_proc = nullptr;
        continueWithScript(QString());
    }
}

void OcrEngine::startRecognitionProcess(const QString &imagePath,
                                        const QString &tessdataDirectory,
                                        const QString &languageArgument)
{
    if (languageArgument.trimmed().isEmpty()) {
        failAndRemoveImage(imagePath, QStringLiteral("No usable OCR language pack is installed"));
        return;
    }

    emit languageResolved(languageArgument);
    m_proc = new QProcess(this);
    m_proc->setProcessChannelMode(QProcess::SeparateChannels);

    QPointer<OcrEngine> self(this);
    connect(m_proc, &QProcess::errorOccurred, this, [self](QProcess::ProcessError err) {
        if (!self) return;
        qWarning() << "[EShot] OCR process error:" << err;
    });

    connect(m_proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, self, imagePath](int exitCode, QProcess::ExitStatus status) {
        if (!self) {
            if (QFile::exists(imagePath)) QFile::remove(imagePath);
            return;
        }
        const QString outText = QString::fromUtf8(m_proc->readAllStandardOutput()).trimmed();
        const QString errText = QString::fromUtf8(m_proc->readAllStandardError()).trimmed();
        QFile::remove(imagePath);
        m_pendingFiles.remove(imagePath);
        m_proc->deleteLater();
        m_proc = nullptr;

        if (status != QProcess::NormalExit || exitCode != 0) {
            QString detail = !errText.isEmpty() ? errText : outText;
            if (detail.isEmpty()) detail = QStringLiteral("tesseract exit %1").arg(exitCode);
            qWarning() << "[EShot] OCR tesseract failed:" << detail << "exit=" << exitCode;
            emit failed(QStringLiteral("Tesseract: ") + detail.left(400));
            return;
        }
        if (outText.isEmpty()) {
            emit failed(QStringLiteral("No text recognized"));
            return;
        }
        emit textReady(outText);
    });

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    if (!tessdataDirectory.isEmpty())
        env.insert(QStringLiteral("TESSDATA_PREFIX"), tessdataDirectory);
    m_proc->setProcessEnvironment(env);

    QStringList args;
    args << QDir::toNativeSeparators(imagePath)
         << QStringLiteral("stdout")
         << QStringLiteral("-l") << languageArgument
         << QStringLiteral("--psm") << QStringLiteral("6");
    if (!tessdataDirectory.isEmpty()) {
        args << QStringLiteral("--tessdata-dir")
             << QDir::toNativeSeparators(tessdataDirectory);
    }

    m_proc->start(tesseractPath(), args);
    if (!m_proc->waitForStarted(10000)) {
        QString errStr = m_proc->errorString();
        if (m_proc->state() != QProcess::NotRunning)
            m_proc->kill();
        m_proc->deleteLater();
        m_proc = nullptr;
        failAndRemoveImage(imagePath, QStringLiteral("Cannot start Tesseract: ") + errStr);
    }
}

void OcrEngine::failAndRemoveImage(const QString &imagePath, const QString &reason)
{
    QFile::remove(imagePath);
    m_pendingFiles.remove(imagePath);
    emit failed(reason);
}
