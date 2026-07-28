#pragma once

#include <QProcess>

bool videoRecordingProcessSucceeded(bool usesGStreamer, bool expectedStop,
                                    QProcess::ExitStatus status, int exitCode,
                                    qint64 outputSize);
