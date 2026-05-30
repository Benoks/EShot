# EShot

A fast, lightweight screenshot tool for Windows with annotation tools, pin-to-desktop, and multi-monitor support.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Windows%2010%2F11-blue.svg)]()
[![Qt](https://img.shields.io/badge/Qt-6.8-green.svg)]()
[![Version](https://img.shields.io/badge/Version-2.0.0-orange.svg)]()

<!-- ![EShot Preview](preview.png) -->

## Features

- **8 Annotation Tools** — Pen, Arrow, Rectangle, Circle, Text, Highlighter, Mosaic Blur, Counter
- **Pin to Desktop** — Pin any capture as an always-on-top window
- **Multi-Monitor** — Pixel-perfect capture across all monitors (DPI-aware)
- **Mosaic Blur** — Pixelate sensitive areas with real screen content
- **Bilingual** — Turkish & English (auto-detects system language)
- **Dark Mode** — Clean, modern dark UI
- **Customizable Hotkey** — Change the capture key from Print Screen to anything
- **Smart Saving** — Filename templates with date/time variables (`%Y-%m-%d_%h-%m-%s`)
- **Lightweight** — Minimal resource usage, runs in system tray

## Quick Start

Download the latest release from [Releases](https://github.com/Benoks/EShot/releases) and run the installer.

| Key | Action |
|-----|--------|
| `Print Screen` | Start capture |
| `Left Click + Drag` | Select area |
| `Enter` / `Ctrl+C` | Copy to clipboard |
| `Ctrl+S` | Save to file |
| `Esc` | Cancel |
| `Ctrl+Z` / `Ctrl+Y` | Undo / Redo |
| `Shift` + Draw | Perfect square / circle |

## Build from Source

### Requirements

- [Visual Studio Build Tools 2022](https://visualstudio.microsoft.com/visual-cpp-build-tools/) (MSVC)
- [Qt 6.8](https://www.qt.io/download-qt-installer) (Core, Gui, Widgets)
- [CMake 3.16+](https://cmake.org/download/)
- [Inno Setup 6](https://jrsoftware.org/isinfo.php) (optional, for installer)

### Build

```bash
git clone https://github.com/Benoks/EShot.git
cd EShot
cmake -S . -B build -G "Visual Studio 17 2022" -DCMAKE_PREFIX_PATH="C:/Qt/6.8.0/msvc2022_64"
cmake --build build --config Release
```

The executable will be at `build/bin/Release/EShot.exe`.

### Create Installer

```bash
"C:\Program Files (x86)\Inno Setup 6\ISCC.exe" EShot_Setup.iss
```

Output: `installer_output/EShot_Setup_v2.0.0.exe`

## Tech Stack

- **Language:** C++17
- **UI Framework:** Qt 6.8 (Widgets)
- **Build System:** CMake
- **Platform API:** Win32 (`BitBlt`, `RegisterHotKey`, `GetSystemMetrics`)
- **Packaging:** Inno Setup 6

## Project Structure

```
EShot/
├── src/
│   ├── main.cpp                  # App entry, tray icon, hotkey setup
│   ├── core/
│   │   ├── HotkeyManager.cpp     # Global hotkey via Win32 API
│   │   └── TranslationManager.h  # Bilingual support (TR/EN)
│   ├── capture/
│   │   ├── CaptureOverlay.cpp    # Full-screen overlay, selection, toolbar
│   │   └── PinnedWindow.cpp      # Pin-to-desktop window
│   ├── annotation/
│   │   └── AnnotationEngine.cpp  # 8 drawing tools + undo/redo
│   └── ui/
│       ├── AnnotationToolbar.cpp # Floating toolbar with tools
│       ├── SettingsDialog.cpp    # Settings (5 tabs)
│       └── AboutDialog.cpp       # About window
├── icons/                        # SVG icons for tools & UI
├── resources/                    # Qt resource files
└── EShot_Setup.iss               # Inno Setup installer script
```

## Contributing

Contributions are welcome! Please open an issue first to discuss what you'd like to change.

## License

[MIT](LICENSE)
