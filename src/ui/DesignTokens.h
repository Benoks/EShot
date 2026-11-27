#pragma once

// EShot's colours for the capture overlay (toolbar, panels, shortcut sheet)
// and the graphite application theme. Change a value here, not in a style
// sheet. High contrast keeps its own black/white/yellow values.
namespace DesignTokens {

inline constexpr const char *Panel = "#2a2a2c";          // toolbar, panels, windows
inline constexpr const char *PanelBorder = "#3f3f42";
inline constexpr const char *Field = "#202022";          // text fields, segmented track
inline constexpr const char *Control = "#3a3a3e";        // buttons, combo boxes
inline constexpr const char *ControlHover = "#444449";
inline constexpr const char *ControlPressed = "#323236";
inline constexpr const char *ControlBorder = "#56565b";
inline constexpr const char *ControlBorderHover = "#646469";
inline constexpr const char *Selected = "#444448";       // checked segment, pressed toggle
inline constexpr const char *Separator = "#343437";
inline constexpr const char *Text = "#ececec";
inline constexpr const char *TextMuted = "#adadb0";      // 6.4:1 on Panel
inline constexpr const char *TextDisabled = "#87878a";
inline constexpr const char *Accent = "#8ab4f8";         // primary button, checkbox, focus (soft blue)
inline constexpr const char *AccentHover = "#a1c3fa";
inline constexpr const char *AccentPressed = "#76a2ec";
inline constexpr const char *OnAccent = "#0f1f38";        // text and icons on Accent
inline constexpr const char *ToolActive = "#3d74c5";      // selected toolbar tool, white icon on it
inline constexpr const char *ToolActiveBorder = "#4b82d1";
inline constexpr const char *SelectionFrame = "#0078d7";  // capture selection frame, stays vivid on screenshots
inline constexpr const char *Link = "#7dbdf7";
inline constexpr const char *Record = "#d44848";

}
