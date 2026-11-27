#pragma once

// EShot's colours for the capture overlay (toolbar, panels, shortcut sheet)
// and the graphite application theme. Change a value here, not in a style
// sheet. High contrast keeps its own black/white/yellow values.
namespace DesignTokens {

inline constexpr const char *Panel = "#2c2b29";          // toolbar, panels, windows
inline constexpr const char *PanelBorder = "#42403d";
inline constexpr const char *Field = "#302e2c";          // text fields, segmented track: just above Panel, the border marks them
inline constexpr const char *TabInactive = "#252422";    // settings tabs behind the selected one
inline constexpr const char *Control = "#3a3836";        // buttons, combo boxes
inline constexpr const char *ControlHover = "#45433f";
inline constexpr const char *ControlPressed = "#33312f";
inline constexpr const char *ControlBorder = "#57534f";
inline constexpr const char *ControlBorderHover = "#66625d";
inline constexpr const char *Selected = "#4a4743";       // checked segment, pressed toggle
inline constexpr const char *Separator = "#383633";
inline constexpr const char *Text = "#edebe8";
inline constexpr const char *TextMuted = "#b1ada8";      // 6.3:1 on Panel
inline constexpr const char *TextDisabled = "#8a8681";
inline constexpr const char *Accent = "#e6e3de";         // primary button, checkbox, focus (light warm grey)
inline constexpr const char *AccentHover = "#f1efeb";
inline constexpr const char *AccentPressed = "#d6d2cc";
inline constexpr const char *OnAccent = "#1c1a17";        // text and icons on Accent
inline constexpr const char *ToolActive = "#625d57";      // selected toolbar tool, white icon on it (warm grey)
inline constexpr const char *ToolActiveBorder = "#7a746d";
inline constexpr const char *SelectionFrame = "#0078d7";  // capture selection frame, stays vivid on screenshots
inline constexpr const char *Link = "#e0dcd6";
inline constexpr const char *Record = "#d44848";

}
