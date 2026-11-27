#pragma once

// EShot's colours for the capture overlay (toolbar, panels, shortcut sheet)
// and the graphite application theme. Change a value here, not in a style
// sheet. High contrast keeps its own black/white/yellow values.
namespace DesignTokens {

inline constexpr const char *Panel = "#3b3d42";          // toolbar, panels, windows
inline constexpr const char *PanelBorder = "#505257";
inline constexpr const char *Field = "#303237";          // text fields, segmented track
inline constexpr const char *Control = "#46484e";        // buttons, combo boxes
inline constexpr const char *ControlHover = "#4f5157";
inline constexpr const char *ControlPressed = "#404247";
inline constexpr const char *ControlBorder = "#5a5c62";
inline constexpr const char *ControlBorderHover = "#64666c";
inline constexpr const char *Selected = "#55575d";       // checked segment, pressed toggle
inline constexpr const char *Separator = "#44464b";
inline constexpr const char *Text = "#ededee";
inline constexpr const char *TextMuted = "#b3b5ba";      // 5.3:1 on Panel
inline constexpr const char *TextDisabled = "#8a8c91";
inline constexpr const char *Accent = "#0078d7";         // selection frame, primary button
inline constexpr const char *AccentHover = "#1a86dd";
inline constexpr const char *AccentPressed = "#006abf";
inline constexpr const char *Link = "#7dbdf7";
inline constexpr const char *Record = "#d44848";

}
