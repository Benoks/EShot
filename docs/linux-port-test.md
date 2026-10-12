# Linux Port Test Notes

EShot keeps the Windows build intact and adds a Linux build path beside it.

## Release AppImage on CachyOS or GNOME

Download `EShot-v<version>-x86_64.AppImage` and double-click it. The first run:

1. Copies EShot to `~/.local/opt/EShot/EShot.AppImage`.
2. Adds EShot to the application menu.
3. Detects KDE/Plasma, GNOME, or the GTK fallback portal backend.
4. Uses PolicyKit to install missing FFmpeg, Tesseract, PipeWire/GStreamer, and portal packages.
5. Starts EShot and asks the desktop to approve the preferred PrintScreen shortcut.

Later launches come from the application menu and do not compile source code.
Wayland screenshot/recording permission dialogs are expected desktop security
behavior.

## Source-folder fallback on CachyOS

From the source folder, double-click `EShot-Linux.desktop`.

If the desktop asks whether to trust or allow launching the file, allow it. The
launcher opens a terminal, offers to install missing CachyOS dependencies, builds
the app on first launch, and then starts EShot.

## Ubuntu / Debian Setup

From the repository root:

```bash
chmod +x scripts/linux/*.sh
./scripts/linux/install-ubuntu-deps.sh
desktop-file-validate packaging/linux/io.github.benoks.EShot.desktop
appstreamcli validate --no-net packaging/linux/io.github.benoks.EShot.metainfo.xml
./scripts/linux/check-linux-runtime.sh
./scripts/linux/build-linux.sh
rm -rf linux-root
DESTDIR="$(pwd)/linux-root" cmake --install build-linux --prefix /usr
appstreamcli validate-tree --no-net ./linux-root
./dist-linux/bin/EShot
```

For the full build, metadata, smoke, and package gate:

```bash
./scripts/linux/verify-linux.sh
```

## CachyOS / Arch Setup

From the repository root:

```bash
chmod +x scripts/linux/*.sh
./scripts/linux/install-cachyos-deps.sh
desktop-file-validate packaging/linux/io.github.benoks.EShot.desktop
appstreamcli validate --no-net packaging/linux/io.github.benoks.EShot.metainfo.xml
./scripts/linux/check-linux-runtime.sh
./scripts/linux/build-linux.sh
rm -rf linux-root
DESTDIR="$(pwd)/linux-root" cmake --install build-linux --prefix /usr
appstreamcli validate-tree --no-net ./linux-root
./dist-linux/bin/EShot
```

For a rebuild-and-run loop:

```bash
./scripts/linux/run-linux.sh
```

To create a portable test archive:

```bash
./scripts/linux/package-linux.sh
```

To create the user-facing AppImage:

```bash
./scripts/linux/build-appimage.sh
./packages/EShot-v*-x86_64.AppImage --appimage-extract-and-run --test-gif
```

## Current Linux Behavior

- KDE/GNOME: global shortcuts prefer the desktop GlobalShortcuts portal on Wayland and X11.
- Other X11 desktops: global shortcuts fall back to XGrabKey when the portal is unavailable.
- Wayland: screenshot capture falls back to xdg-desktop-portal when direct Qt screen grab is blocked.
- Wayland: global shortcuts use the xdg-desktop-portal GlobalShortcuts interface when the desktop supports it.
- OCR: uses system `tesseract` or an app-local `tesseract` folder.
- Video: uses system `ffmpeg` on Windows/X11; Wayland recording uses xdg-desktop-portal ScreenCast plus GStreamer/PipeWire.
- Wayland video encoding: a working hardware H.264 encoder is used when one is found (VA-API `vah264enc`/`vah264lpenc`, then NVIDIA `nvh264enc`), otherwise `x264enc`, then `openh264enc`. EShot verifies each hardware encoder once with a short test encode in the background and caches the result until GStreamer or the GPU drivers change. The `videoHardwareEncoding` setting (default `true`) set to `false` always uses the software encoder. GIF recordings always use the software encoder.
- GIF: uses the built-in frame encoder on Windows/X11; Wayland GIF recording uses xdg-desktop-portal ScreenCast plus GStreamer/PipeWire.
- Autostart: writes an XDG autostart desktop file under `~/.config/autostart`.
- Updates: Linux selects the matching AppImage release asset, verifies its GitHub SHA-256 digest, replaces the current AppImage after exit, and restarts EShot.
- Ubuntu/Wayland needs `qt6-wayland`, GStreamer PipeWire plugins, `xdg-desktop-portal`, and a portal backend such as `xdg-desktop-portal-gtk`.
- CachyOS/Wayland needs `qt6-wayland`, GStreamer PipeWire plugins, and a desktop portal backend such as `xdg-desktop-portal-kde`.

## Test Checklist

- Build succeeds with `./scripts/linux/build-linux.sh`.
- `desktop-file-validate` and `appstreamcli validate --no-net` pass for the Linux packaging metadata.
- `DESTDIR="$(pwd)/linux-root" cmake --install build-linux --prefix /usr` followed by `appstreamcli validate-tree --no-net ./linux-root` passes after install.
- Runtime dependency check succeeds with `./scripts/linux/check-linux-runtime.sh`.
- The runtime check reports the ScreenCast portal plus the GStreamer recording elements (`pipewiresrc`, `videorate`, `videocrop`, `videoconvert`, `h264parse`, `mp4mux`, `splitmuxsrc`, `pulsesrc`, `audioconvert`, `audioresample`, `volume`, `audiomixer`), `x264enc` or `openh264enc`, and an AAC encoder as available; the optional hardware encoder elements are listed as extras.
- App starts from `./dist-linux/bin/EShot`.
- Tray icon appears.
- Capture opens from tray action.
- On X11, configured global capture hotkey opens capture.
- On Wayland, capture prompts through the portal if direct capture is blocked.
- On Wayland, configured global capture hotkey works on desktops that expose the GlobalShortcuts portal.
- On KDE Plasma 6 and GNOME, the first shortcut approval binds PrintScreen to EShot.
- On Wayland, GIF/video recording prompts through the ScreenCast portal, accepts the selected screen/window, and writes a non-empty output file.
- Selection, annotation, copy, save, OCR, upload, pin, GIF, and video flows each run once.
- A newer test release downloads, verifies, replaces the AppImage, and relaunches with the new version.
