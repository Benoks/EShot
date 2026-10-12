#include "RecordingFailureMessages.h"

#include "../core/TranslationManager.h"

#include <QDebug>
#include <QRegularExpression>
#include <QStringList>

namespace {

// Text of a translated "...%1..." template before its placeholder, used to
// recognize lines the recorders already localized.
QString translatedPrefix(const char *key)
{
    const QString text = TranslationManager::tr(key);
    const int placeholder = text.indexOf(QStringLiteral("%1"));
    return (placeholder < 0 ? text : text.left(placeholder)).trimmed();
}

bool startsWithTranslatedPrefix(const QString &line, const char *key)
{
    const QString prefix = translatedPrefix(key);
    return !prefix.isEmpty() && line.startsWith(prefix);
}

QString processDisplayName(const QString &process)
{
    const QString lower = process.toLower();
    if (lower == QStringLiteral("ffmpeg") || lower == QStringLiteral("ffmpeg.exe"))
        return QStringLiteral("FFmpeg");
    if (lower == QStringLiteral("gstreamer") || lower == QStringLiteral("gst-launch-1.0"))
        return QStringLiteral("GStreamer");
    return process;
}

// Localizes a known English reason literal; returns an empty string when the
// line is not one of them.
QString knownReason(const QString &line)
{
    struct Literal {
        const char *reason;
        const char *key;
    };
    static const Literal literals[] = {
        {"ffmpeg.exe not found", "videoFfmpegMissing"},
        {"ffmpeg not found", "videoFfmpegMissing"},
        {"gstreamer not found", "videoGstreamerMissing"},
        {"Wayland ScreenCast portal is not available", "videoWaylandPortalMissing"},
        {"Wayland screen recording permission was not granted", "videoWaylandPermissionDenied"},
        {"Wayland recording source does not contain the selected region", "videoWaylandWrongSource"},
        {"cannot start gstreamer", "videoGstreamerStartFailed"},
        {"Wayland PipeWire remote could not be opened", "videoPipeWireRemoteFailed"},
        {"region too small", "recFailRegionTooSmall"},
        {"no frames captured", "recFailNoFrames"},
        {"no video was recorded", "recFailNoVideo"},
        {"cannot create output directory", "recFailOutputDir"},
        {"cannot start ffmpeg", "recFailFfmpegStart"},
        {"failed to mux system audio", "recFailAudioMux"},
        {"No GStreamer AAC encoder is installed", "recFailAacEncoder"},
        {"cannot initialize screen capture", "recFailCaptureInit"},
        {"already recording", "recFailAlreadyRecording"},
        {"failed to convert portal video to GIF", "recFailGifConvert"},
        {"gstreamer pipeline exited during startup", "recFailGstStartup"},
    };
    for (const Literal &literal : literals) {
        if (line == QLatin1String(literal.reason))
            return TranslationManager::tr(literal.key);
    }

    if (line.startsWith(QStringLiteral("cannot open output:")))
        return TranslationManager::tr("recFailOutputFile");

    static const QString missingElements = QStringLiteral("missing gstreamer elements:");
    if (line.startsWith(missingElements, Qt::CaseInsensitive)) {
        const QString list = line.mid(missingElements.size()).trimmed();
        return TranslationManager::tr("recFailMissingGstElements")
            .arg(list.isEmpty() ? QStringLiteral("?") : list);
    }

    static const QRegularExpression exited(
        QStringLiteral("^(\\S+) exited with code (-?\\d+)$"));
    const QRegularExpressionMatch match = exited.match(line);
    if (match.hasMatch()) {
        return TranslationManager::tr("recFailProcessExited")
            .arg(processDisplayName(match.captured(1)), match.captured(2));
    }

    // Lines the recorders already localized (they embed a path or a reason).
    if (startsWithTranslatedPrefix(line, "recPartsNotJoined")
        || line == TranslationManager::tr("gifConversionTimedOut")) {
        return recordingFailureSummaryLine(line, 240);
    }
    return {};
}

bool isPortalCancel(const QString &line)
{
    return line.compare(QStringLiteral("portal canceled"), Qt::CaseInsensitive) == 0
        || line.compare(QStringLiteral("portal cancelled"), Qt::CaseInsensitive) == 0;
}

} // namespace

QString recordingFailureSummaryLine(const QString &text, int maxLength)
{
    QString line;
    for (const QString &candidate : text.split(QLatin1Char('\n'))) {
        line = candidate.trimmed();
        if (!line.isEmpty())
            break;
    }
    maxLength = qMax(2, maxLength);
    if (line.size() > maxLength)
        line = line.left(maxLength - 1).trimmed() + QChar(0x2026);
    return line;
}

RecordingFailureMessage recordingFailureMessage(const QString &reason)
{
    RecordingFailureMessage result;
    QStringList lines;
    for (const QString &line : reason.split(QLatin1Char('\n'))) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            lines.append(trimmed);
    }
    if (lines.isEmpty()) {
        result.unexpected = true;
        result.text = TranslationManager::tr("recFailUnexpected").arg(QStringLiteral("?"));
        return result;
    }

    const QString first = lines.takeFirst();
    if (isPortalCancel(first)) {
        result.silent = true;
        return result;
    }

    QString text = knownReason(first);
    QStringList details;
    if (!text.isEmpty()) {
        // A recognized reason keeps all of its detail lines (kept files, ...).
        details = lines;
    } else {
        // Raw tool output: only show a summary, but never hide where the
        // recorder kept the recorded footage.
        result.unexpected = true;
        text = TranslationManager::tr("recFailUnexpected")
                   .arg(recordingFailureSummaryLine(first));
        for (const QString &line : std::as_const(lines)) {
            if (startsWithTranslatedPrefix(line, "recVideoKeptAt"))
                details.append(line);
        }
    }
    if (!details.isEmpty())
        text += QLatin1Char('\n') + details.join(QLatin1Char('\n'));
    result.text = text;
    return result;
}

QString localizedRecordingFailureReason(const QString &reason)
{
    const RecordingFailureMessage message = recordingFailureMessage(reason);
    if (message.unexpected)
        qWarning().noquote() << "[Recording] recorder failure details:" << reason;
    return message.text;
}
