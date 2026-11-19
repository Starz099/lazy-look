# LazyFinder

LazyFinder is a local file search tool for Linux desktop environments. It allows you to search files using natural language descriptions, exact keywords, or filenames through a floating search bar.

## Features

- Hybrid search: Combines local vector embeddings (Gemma-2 embedding model accelerated via Vulkan) with SQLite full-text keyword indexing.
- Deep file navigation and format support:
  - Documents and Code: Supports .pdf (opens directly to matching page in Papers, Evince, Okular), .pptx and .odp (opens to matching slide in LibreOffice), .docx, .xlsx, .odt, .ods, .epub, .rtf, HTML, Markdown, and common programming languages.
  - Images and Vision: Indexes .png, .jpg, .jpeg, .webp, .bmp, .gif, .tiff, and .svg with dimension extraction, EXIF metadata (camera, dates), and vector embeddings.
  - Video and Audio: Indexes .mp4, .mkv, .webm, .avi, .mov, .mp3, .wav, .flac, .ogg with companion or embedded subtitle tracks (.srt, .vtt), container chapters, and scene keyframe thumbnail generation, opening directly at the matching timestamp in mpv, VLC, Celluloid, or Totem.
- Background daemon: Runs as a single instance with low resource usage. The neural model automatically unloads from GPU VRAM and RAM after 5 minutes of inactivity.
- Wayland-friendly global shortcut: Toggle the search window using Ctrl + Shift + Space. The shortcut is only registered with GNOME while LazyFinder is running, releasing the key combination when the application exits.

## Installation

### Fedora, RHEL, and openSUSE (.rpm)

Download the `.rpm` package from the releases page and install:

```bash
# Fedora / RHEL
sudo dnf install ./lazyfinder-0.1.0-x86_64.rpm

# openSUSE
sudo zypper install ./lazyfinder-0.1.0-x86_64.rpm
```

### Ubuntu, Debian, Linux Mint, and Pop!_OS (.deb)

Download the `.deb` package from the releases page and install:

```bash
sudo apt install ./lazyfinder-0.1.0-amd64.deb
```

### Arch Linux, Manjaro, and EndeavourOS (PKGBUILD)

Install via the included Arch package definition:

```bash
cd packaging/arch
makepkg -si
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

## Search Prefixes & Filters

You can filter results to specific file categories by prefixing your search query:

| Prefix | Target Category | Example |
|---|---|---|
| `code:` or `type:code` | Source code files only | `code: calculate_hash` |
| `doc:` or `pdf:` | Documents, PDFs, and presentations | `doc: quarterly report` |
| `image:` or `img:` | Images and vector graphics | `image: architecture diagram` |
| `video:` or `vid:` | Video files and scene keyframes | `video: conference talk` |
| `audio:` or `sound:` | Audio recordings, podcasts, and music | `audio: episode 42` |
| `media:` or `type:media` | Audio and video media files | `media: keynote presentation` |

Typing a prefix on its own (such as `code:` or `video:`) will list recent files of that type.

## Command-Line Usage

| Option | Description |
|---|---|
| `lazyfinder` | Launch LazyFinder and show search bar |
| `lazyfinder --daemon` | Start LazyFinder in background daemon mode |
| `lazyfinder --toggle` | Toggle search bar visibility |
| `lazyfinder --show` | Show and focus search bar |
| `lazyfinder --hide` | Hide search bar |
| `lazyfinder --quit` | Quit running LazyFinder process |
| `lazyfinder --enable-autostart` | Enable background autostart on system login |
| `lazyfinder --disable-autostart` | Disable autostart on system login |

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
