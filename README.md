# EShot

Fast, lightweight screenshot and annotation tool for Windows.

[![Version](https://img.shields.io/badge/version-2.4.3-blue.svg)](https://github.com/Benoks/EShot/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey.svg)](#)
[![Qt](https://img.shields.io/badge/Qt-6.x-green.svg)](https://www.qt.io/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

<p align="center">
  <img src="preview.png" alt="EShot preview" width="760">
</p>

EShot is built for quick captures, clean annotations, OCR, uploads, pinned screenshots, and GIF recording without turning into a heavy background app.

## Highlights

- Region capture with multi-monitor and high-DPI support
- Annotation tools: pen, arrow, line, rectangle, ellipse, text, blur, crop, eyedropper, and semi-transparent highlight
- GIF recording for selected screen areas
- OCR with bundled Tesseract support and optional language packs
- Upload support for Catbox and Uguu
- Pin screenshots on top of the desktop
- Custom global hotkeys
- Start with Windows through Task Scheduler
- Update check through GitHub releases

## What's New in v2.4.0

- Added GIF recording with configurable FPS, duration, and loop options
- Added eyedropper tool for picking colors directly from the screen
- Added semi-transparent rectangle tool for filled highlights
- Added selection lock to prevent accidental movement while annotating
- Added adjustable blur intensity
- Added multiline text support
- Improved toolbar placement around the selected area
- Improved undo and redo button states
- Improved update notification behavior through the tray icon
- Fixed selection, toolbar, startup, and installer regressions found during the v2.4.0 cycle

## Features

### Capture

- Capture a selected region from the screen
- Move and resize the selected area before saving or copying
- Keep the dimension label anchored to the selected area
- Copy, save, upload, pin, annotate, or run OCR from the capture toolbar

### Annotation

EShot includes the common tools needed for fast screenshot markup:

- Freehand pen
- Arrow and line
- Rectangle and ellipse
- Semi-transparent rectangle
- Text and multiline text
- Blur with adjustable intensity
- Crop
- Eyedropper color picker
- Undo and redo

Annotations snap to selection edges where useful, and toolbars reposition automatically when there is not enough screen space outside the selected region.

### GIF Recording

Record a selected part of the screen as a GIF. Recording settings include FPS, maximum duration, and loop behavior.

### OCR

EShot can extract text from a selected area using Tesseract OCR. The installer can include the main OCR engine and selectable language packs, including English, Turkish, Russian, German, French, Spanish, and Italian.

### Upload

Upload screenshots directly from the capture toolbar. Supported services:

- Catbox
- Uguu

Catbox and Uguu work without an API key.

### Pin to Desktop

Pinned screenshots stay above other windows, making them useful for reference images, snippets, notes, UI comparisons, and quick visual memory.

### Settings

The settings window includes:

- General behavior
- Capture behavior
- Recording options
- Appearance options
- Interface options
- Hotkey configuration

Settings can be exported and imported, and hotkey combinations are validated before being saved.

## Keyboard Shortcuts

Default shortcuts can be changed in Settings.

| Shortcut | Action |
| --- | --- |
| `Print Screen` | Start capture |
| `Enter` | Confirm/copy selected capture |
| `Ctrl+C` | Copy selected capture |
| `Esc` | Cancel capture or close active overlay |
| `Shift+Enter` | Add a new line while editing text |
| `I` | Eyedropper tool |
| `D` | Semi-transparent rectangle tool |

## Install

1. Download the latest installer from the [Releases page](https://github.com/Benoks/EShot/releases/latest).
2. Run the setup file.
3. Select optional OCR language packs if you want OCR support for more languages.
4. Start EShot from the Start menu, desktop shortcut, or tray icon.

If "Start with Windows" is enabled, EShot registers itself through Windows Task Scheduler.

## Build from Source

Requirements:

- Windows 10 or Windows 11
- Qt 6
- CMake
- A C++17 compatible compiler
- Inno Setup, only when building the installer

Basic build:

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Installer builds use the included Inno Setup script:

```powershell
iscc EShot_Setup.iss
```

## Command Line

```powershell
EShot.exe --capture
EShot.exe --save "C:\path\to\capture.png"
EShot.exe --silent
```

## Project Structure

```text
EShot/
+-- resources/             # Icons and application resources
+-- tesseract/             # OCR runtime files used by the installer
+-- CMakeLists.txt         # CMake build configuration
+-- EShot_Setup.iss        # Inno Setup installer script
+-- main.cpp               # Application entry point
+-- MainWindow.*           # Main tray/window behavior
+-- ScreenshotOverlay.*    # Capture and annotation overlay
+-- GifRecorder.*          # GIF recording support
+-- SettingsDialog.*       # Settings UI and persistence
`-- UpdateChecker.*        # GitHub release update checks
```

## License

This project is released under the MIT License. See [LICENSE](LICENSE) for details.

EShot uses Tesseract OCR for text recognition. Tesseract OCR and the official tessdata language files are licensed under the Apache License 2.0. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for third-party notices.
