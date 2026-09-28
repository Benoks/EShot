#pragma once

#include <QString>

// One-time hints for new users, remembered in QSettings under "tips/".
namespace OnboardingTips {

inline constexpr const char *QuickSettingsTip = "quickSettings";
inline constexpr const char *TextToolTip = "textTool";
inline constexpr const char *CtrlMoveTip = "ctrlMove";
inline constexpr const char *DrawerPeek = "drawerPeek";
inline constexpr const char *TrayWelcome = "trayWelcome";
// Counters for hints shown during the first few captures.
inline constexpr const char *SelectionHintCount = "selectionHintCount";
inline constexpr const char *TabGlowCount = "tabGlowCount";

bool isSeen(const char *tip);
void markSeen(const char *tip);
int count(const char *counter);
void increment(const char *counter);
// "Show tips again" in Settings.
void resetAll();

// Overlay shortcut as the user configured it in Settings, for display.
QString overlayShortcutText(const QString &id, const QString &fallback);

}
