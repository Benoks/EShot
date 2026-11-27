#pragma once

// EShot's colours for the capture overlay (toolbar, panels, shortcut sheet)
// and the graphite application theme. Change a value here, not in a style
// sheet. High contrast keeps its own black/white/yellow values.
namespace DesignTokens {

inline constexpr const char *Panel = "#2a2a2c";          // toolbar, panels, windows
inline constexpr const char *PanelBorder = "#3f3f42";
inline constexpr const char *Field = "#202022";          // text fields, segmented track
inline constexpr const char *Control = "#353538";        // buttons, combo boxes
inline constexpr const char *ControlHover = "#3e3e41";
inline constexpr const char *ControlPressed = "#2f2f31";
inline constexpr const char *ControlBorder = "#48484c";
inline constexpr const char *ControlBorderHover = "#545458";
inline constexpr const char *Selected = "#444448";       // checked segment, pressed toggle
inline constexpr const char *Separator = "#343437";
inline constexpr const char *Text = "#ececec";
inline constexpr const char *TextMuted = "#adadb0";      // 6.4:1 on Panel
inline constexpr const char *TextDisabled = "#87878a";
inline constexpr const char *Accent = "#0078d7";         // selection frame, primary button
inline constexpr const char *AccentHover = "#1a86dd";
inline constexpr const char *AccentPressed = "#006abf";
inline constexpr const char *Link = "#7dbdf7";
inline constexpr const char *Record = "#d44848";

}
