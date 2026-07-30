#ifndef OVERLAYPANELSTYLE_H
#define OVERLAYPANELSTYLE_H

#include <QString>

class QSpinBox;

struct OverlayPanelMetrics {
    int panelRadius;
    int controlRadius;
    int actionRadius;
    int controlHeight;
    int contentMargin;
    int rowSpacing;
    int sectionSpacing;
    int drawerAnimationDurationMs;
};

OverlayPanelMetrics overlayPanelMetrics();
QString overlayPanelStyleSheet();
void configureOverlaySpinBox(QSpinBox *spinBox);

#endif
