#ifndef RECORDINGDRAWERPOLICY_H
#define RECORDINGDRAWERPOLICY_H

#include <QPoint>
#include <QRect>
#include <QSize>
#include <QStringList>

enum class RecordingDrawerMode {
    None,
    Gif,
    Video
};

QStringList recordingDrawerFields(RecordingDrawerMode mode);
QPoint recordingDrawerPosition(const QRect &availableBounds, const QSize &drawerSize);

#endif
