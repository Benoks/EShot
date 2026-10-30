#ifndef APPLICATIONTHEME_H
#define APPLICATIONTHEME_H

#include <QPalette>
#include <QString>

class QApplication;

QPalette eshotApplicationPalette(bool dark, bool highContrast);
void applyEShotApplicationTheme(QApplication &application, bool dark, bool highContrast);
// The tray menu style for the same dark/light/high-contrast setting.
QString eshotTrayMenuStyleSheet(bool dark, bool highContrast);

#endif
