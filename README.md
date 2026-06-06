# EShot

Fast, lightweight Windows screenshot tool with annotations, OCR, GIF recording, video recording, uploads, and pinned captures.

[![Version](https://img.shields.io/badge/version-3.0.2-blue.svg)](https://github.com/Benoks/EShot/releases)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-lightgrey.svg)](#)
[![Qt](https://img.shields.io/badge/Qt-6.x-green.svg)](https://www.qt.io/)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)

<p align="center">
  <img src="preview.png" alt="EShot preview" width="760">
</p>

EShot is built for people who want a quick screenshot workflow without a heavy desktop app running in the background. It focuses on region capture, clean annotation, OCR, uploads, pinned screenshots, GIFs, and MP4 screen recording from one compact tray app.

## Highlights

- Region capture with multi-monitor and high-DPI support
- Annotation tools: pen, arrow, line, rectangle, ellipse, text, highlighter, blur, counter, eraser, eyedropper, and semi-transparent rectangle
- GIF recording for selected screen areas
- MP4 video recording powered by FFmpeg
- Desktop audio and microphone support for video recording
- OCR powered by Tesseract with selectable language packs
- Catbox and Uguu upload support
- Pin screenshots above the desktop
- Quick Settings drawer inside the capture screen
- Customizable global and in-capture shortcuts
- Start with Windows through Task Scheduler
- GitHub release update check

## What's New in v3.0.2

- Added MP4 video recording for selected areas
- Added FFmpeg integration as an optional installer component
- Added desktop audio and microphone recording controls
- Added Quick Settings drawer for capture, GIF, and video settings
- Added fully customizable screenshot-screen shortcuts for tools and toolbar actions
- Added more OCR language support: Japanese, Korean, Simplified Chinese, Portuguese, Polish, and Dutch
- Improved OCR language picker: unavailable languages are shown disabled instead of silently disappearing
- Improved setup experience for existing installs and already-installed OCR/FFmpeg components
- Improved first-capture startup performance with overlay pre-warming
- Improved text annotation controls and movable text editing panel
- Improved toolbar grouping, spacing, tool visibility options, and annotation selection behavior
- Improved notifications for saved images, GIFs, and videos

## Features

### Capture

- Select any region of the screen
- Move, resize, annotate, copy, save, upload, pin, OCR, record GIF, or record video from the selected area
- Dimension label stays anchored to the selection
- Toolbar and side actions reposition when screen space is tight

### Annotation

EShot includes the common tools needed for quick markup:

- Freehand pen
- Arrow and line
- Rectangle and ellipse
- Semi-transparent rectangle
- Multiline text
- Mosaic blur with adjustable intensity
- Counter
- Eraser
- Eyedropper color picker
- Undo and redo

### GIF Recording

Record a selected area as a GIF. You can configure FPS, maximum duration, and loop behavior.

### Video Recording

Record a selected area as an MP4 video. Video recording uses FFmpeg and supports:

- Configurable FPS
- Configurable CRF quality
- Optional duration limit
- Desktop/system audio
- Microphone audio
- Pause, stop, and cancel controls

### OCR

EShot can extract text from a selected area using Tesseract OCR. The installer can include the OCR engine and selectable language packs.

Supported language options include English, Turkish, Russian, German, French, Spanish, Italian, Portuguese, Polish, Dutch, Japanese, Korean, and Simplified Chinese. Missing language packs are shown disabled in the OCR dialog.

### Upload

Upload screenshots directly from the capture toolbar.

Supported services:

- Catbox
- Uguu

### Pin to Desktop

Pinned screenshots stay above other windows, useful for reference images, snippets, UI comparison, notes, and quick visual memory.

### Settings

The settings window includes:

- General behavior
- Capture behavior
- GIF and video recording settings
- Appearance options
- Interface and toolbar visibility
- Global capture hotkey
- Recording hotkeys
- Screenshot-screen tool/action shortcuts
- Settings export and import

## Default Shortcuts

Most shortcuts can be changed in Settings.

| Shortcut | Action |
| --- | --- |
| `Print Screen` | Start capture |
| `Enter` | Copy selected capture |
| `Ctrl+C` | Copy selected capture |
| `Ctrl+S` | Save selected capture |
| `Esc` | Cancel or close overlay |
| `Shift+Enter` | New line while editing text |
| `P` | Pen |
| `A` | Arrow |
| `L` | Line |
| `R` | Rectangle |
| `C` | Circle |
| `T` | Text |
| `H` | Highlighter |
| `B` | Blur |
| `N` | Counter |
| `X` | Eraser |
| `D` | Semi-transparent rectangle |
| `I` | Eyedropper |

## Install

1. Download the latest installer from the [Releases page](https://github.com/Benoks/EShot/releases/latest).
2. Run the setup file.
3. Select optional FFmpeg and OCR components if needed.
4. Start EShot from the Start menu, desktop shortcut, or tray icon.

If "Start with Windows" is enabled, EShot registers itself through Windows Task Scheduler.

## Build from Source

Requirements:

- Windows 10 or Windows 11
- Qt 6
- CMake
- C++17 compatible compiler
- Inno Setup, only for installer builds

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

Installer:

```powershell
iscc EShot_Setup.iss
```

## Command Line

```powershell
EShot.exe --capture
EShot.exe --save "C:\path\to\capture.png"
EShot.exe --silent
```

## Third-Party Components

EShot uses:

- Qt for the desktop UI
- Tesseract OCR for text recognition
- Tesseract tessdata language files for OCR language support
- FFmpeg for video recording
- Inno Setup for installer packaging

See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for details.

## License

EShot is released under the MIT License. See [LICENSE](LICENSE).
