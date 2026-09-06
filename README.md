# Nexo Browser

Nexo is a lightweight native Linux browser built with **GTK 4 + WebKitGTK 6.0**.

## Included

- Fast native window and toolbar
- Back / forward / reload / stop / home
- Address bar with URL detection and web search fallback
- Real web pages through WebKitGTK
- Multiple tabs with close buttons
- Ctrl+click opens a link in a new tab
- `target=_blank` / popup navigation opens a new tab
- JavaScript, WebGL, WebAudio, media and page cache enabled
- Native GTK symbolic icons and a Nexo application icon
- CMake + Ninja release build
- GitHub Actions AppImage artifact

## Build locally

Ubuntu/Debian:

```bash
sudo apt update
sudo apt install cmake ninja-build pkg-config gcc libgtk-4-dev libwebkitgtk-6.0-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/nexo
```

The project targets the current stable WebKitGTK 6.0 API. WebKitGTK 2.52 is the current stable release line and includes rendering, scrolling, graphics and memory improvements useful for a small native browser.

## AppImage

Every push to `main` triggers `.github/workflows/build-appimage.yml`. The workflow builds a Release binary, creates an AppDir, runs linuxdeploy with the GTK plugin, and uploads `Nexo-x86_64.AppImage` as the `nexo-appimage` Actions artifact.

> AppImage portability still depends on the host graphics/system stack. WebKitGTK is a large system integration component, so the AppImage is intended primarily for modern x86_64 Linux distributions.
