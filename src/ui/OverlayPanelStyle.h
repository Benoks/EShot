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
// The drawer style for the current "highContrast" setting.
QString overlayPanelStyleSheet();
// High contrast: black panels, white borders, yellow focus and hover, and
// point-sized fonts of at least 9pt (12px at 96 DPI) that follow the system
// font scaling.
QString overlayPanelStyleSheet(bool highContrast);
bool overlayHighContrastEnabled();
void configureOverlaySpinBox(QSpinBox *spinBox);

#endif
