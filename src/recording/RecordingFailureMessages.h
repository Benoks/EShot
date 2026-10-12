#ifndef RECORDINGFAILUREMESSAGES_H
#define RECORDINGFAILUREMESSAGES_H

#include <QString>

// What to tell the user about a recordingFailed(reason) signal.
//
// Recorders report "reason\ndetail": the first line is a short English
// reason literal (or raw tool output), the following lines are details such
// as where a kept recording is.
struct RecordingFailureMessage {
    // Localized text for the notification; empty when silent.
    QString text;
    // Nothing should be shown, e.g. the user closed the portal dialog.
    bool silent = false;
    // The reason was not recognized (raw tool output); the full text belongs
    // in the log, the notification only shows a short summary.
    bool unexpected = false;
};

RecordingFailureMessage recordingFailureMessage(const QString &reason);

// recordingFailureMessage(reason).text; also logs unexpected reasons.
QString localizedRecordingFailureReason(const QString &reason);

// First non-empty line of text, shortened to maxLength characters.
QString recordingFailureSummaryLine(const QString &text, int maxLength = 120);

#endif
