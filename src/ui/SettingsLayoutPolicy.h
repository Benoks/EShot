#pragma once

#include <QList>
#include <QSize>

int settingsDialogWidthForTabs(int tabBarWidth, int horizontalMargins, int baseWidth);
QSize settingsDialogActionButtonSize(const QList<QSize> &sizeHints,
                                     int minimumWidth = 104,
                                     int minimumHeight = 34);
QSize settingsDialogRestoredSize(bool rememberSize, const QSize &savedSize,
                                 const QSize &minimumSize, const QSize &maximumSize);
bool settingsDialogUsesAdaptiveSize(bool rememberSize);
