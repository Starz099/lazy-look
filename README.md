# LazyFinder

LazyFinder is a local file search tool for Linux desktop environments. It allows you to search files using natural language descriptions, exact keywords, or filenames through a floating search bar.

## Features

- Hybrid search: Combines local vector embeddings (Gemma-2 embedding model accelerated via Vulkan) with SQLite full-text keyword indexing.
- Deep file navigation:
  - PDF: Extracts text page by page and opens files directly to matching pages in supported viewers (Papers, Evince, Okular).
  - Presentations: Navigates directly to matching slides in LibreOffice.
  - Video and Audio: Indexes embedded or companion subtitle tracks (.srt, .vtt) and opens files at the exact matching timestamp in VLC or mpv.
  - Documents and Code: Supports .docx, .xlsx, .pptx, .odt, .ods, .odp, Markdown, plain text, and common programming languages.
- Background daemon: Runs as a single instance with low resource usage. The neural model automatically unloads from GPU VRAM and RAM after 5 minutes of inactivity.
- Wayland-friendly global shortcut: Toggle the search window using Ctrl + Shift + Space. The shortcut is only registered with GNOME while LazyFinder is running, releasing the key combination when the application exits.

## Installation

### Fedora and RHEL (.rpm)

Download the `.rpm` package from the releases page and install:

```bash
sudo dnf install ./lazyfinder-0.1.0-x86_64.rpm
```

### Ubuntu and Debian (.deb)

Download the `.deb` package from the releases page and install:

```bash
sudo apt install ./lazyfinder-0.1.0-amd64.deb
```

### Universal Linux (.AppImage)

Download the `.AppImage` package, make it executable, and run:

```bash
chmod +x lazyfinder-0.1.0-x86_64.AppImage
./lazyfinder-0.1.0-x86_64.AppImage
```

## Building from Source

### Dependencies

- C++20 compliant compiler (GCC 13+ or Clang 17+)
- CMake 3.28 or later
- Qt 6 (Core, Gui, Widgets, Qml, Quick, QuickControls2)
- Vulkan development headers and runtime loader
- SQLite 3 development headers

### Build Instructions

```bash
git clone https://github.com/your-username/lazy-finder.git
cd lazy-finder

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run tests
ctest --test-dir build --output-on-failure

# Install system-wide
sudo cmake --install build
```

### Package Generation

To generate RPM, DEB, or AppImage packages locally:

```bash
# Generate RPM
cd build && cpack -G RPM

# Generate DEB
cd build && cpack -G DEB

# Generate AppImage
./scripts/build_appimage.sh

# Or build all three at once
./scripts/package_all.sh
```

## Keyboard Shortcuts

| Shortcut | Description |
|---|---|
| Ctrl + Shift + Space | Toggle search window visibility |
| Up / Down | Navigate search results |
| Return | Open selected file at matching page or timestamp |
| Ctrl + Return | Open file location in file manager |
| Ctrl + C | Copy file path to clipboard |
| Ctrl + , | Open settings sheet |
| Escape | Clear query, close settings, or hide window |

## License

MIT License
