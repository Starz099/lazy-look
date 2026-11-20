# LazyLook

LazyLook is a fast, local semantic file search tool for Linux desktop environments. It allows you to search files using natural language descriptions, exact keywords, or filenames through a lightweight, floating search bar.

## Features

- **Hybrid Search**: Combines local neural vector embeddings (EmbeddingGemma-2 Q8_0 model accelerated via Vulkan GPU offload) with SQLite FTS5 full-text keyword indexing and trigram file name matching using Reciprocal Rank Fusion (RRF).
- **Deep File Navigation & Format Support**:
  - **Documents and Code**: Supports `.pdf` (opens directly to matching page in Papers, Evince, Okular), `.pptx` and `.odp` (opens to matching slide in LibreOffice), `.docx`, `.xlsx`, `.odt`, `.ods`, `.epub`, `.rtf`, HTML, Markdown, and common programming languages.
  - **Scanned Documents & Images**: Extracts dimensions and EXIF metadata (camera, dates), with Tesseract OCR fallback for scanned documents, handwritten notes, and screenshots.
  - **Video and Audio**: Indexes `.mp4`, `.mkv`, `.webm`, `.avi`, `.mov`, `.mp3`, `.wav`, `.flac`, `.ogg` with companion or embedded subtitle tracks (`.srt`, `.vtt`), container chapters, and scene keyframe thumbnail generation, opening directly at the matching timestamp in mpv, VLC, Celluloid, or Totem.
- **D-Bus File Manager Reveal**: Selects and highlights files directly inside your desktop file manager (Nautilus, Dolphin, Nemo, Thunar) via `org.freedesktop.FileManager1.ShowItems`.
- **Background Daemon & Memory Saver**: Runs as a single instance with low resource usage. The neural model automatically unloads from GPU VRAM and RAM after 5 minutes of inactivity.
- **Wayland-Friendly Global Shortcut**: Toggle the search window using `Ctrl + Shift + Space`. The shortcut is registered with GNOME while LazyLook is running, releasing the key combination when the application exits.

## Installation

### Fedora, RHEL, and openSUSE (.rpm)

Download the `.rpm` package from the releases page and install:

```bash
# Fedora / RHEL
sudo dnf install ./lazylook-0.1.0-x86_64.rpm

# openSUSE
sudo zypper install ./lazylook-0.1.0-x86_64.rpm
```

### Ubuntu, Debian, Linux Mint, and Pop!_OS (.deb)

Download the `.deb` package from the releases page and install:

```bash
sudo apt install ./lazylook-0.1.0-amd64.deb
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
chmod +x lazylook-0.1.0-x86_64.AppImage
./lazylook-0.1.0-x86_64.AppImage
```

> **Note**: The official AppImage bundles the neural embedding model (`embeddinggemma-2-Q8_0.gguf`) for zero-setup execution out of the box.

## AI Model Setup

LazyLook uses Google's `embeddinggemma-2-Q8_0.gguf` (296 MB) for on-device semantic vector search.

1. **AppImage**: Pre-bundled; no download required.
2. **In-App Download**: If running from native packages or source without bundled weights, LazyLook provides an in-app one-click downloader with live progress tracking.
3. **Setup Script**: You can also download the model from the terminal:
   ```bash
   ./scripts/setup_models.sh
   ```
4. **Resolution Order**:
   - `LAZYLOOK_MODELS_DIR` environment override (falls back to `LAZYFINDER_MODELS_DIR`)
   - Bundled AppImage path (`$APPDIR/usr/share/lazylook/models/`)
   - User data directory (`~/.local/share/lazylook/models/`)
   - System directory (`/usr/share/lazylook/models/`)
   - Current working directory (`./models/`)

## Building from Source

### Dependencies

**Required Build Dependencies**:
- C++20 compliant compiler (GCC 13+ or Clang 17+)
- CMake 3.28 or later
- Qt 6 (Core, Gui, Widgets, Qml, Quick, QuickControls2, DBus, Network, Svg)
- Vulkan development headers and runtime loader (`vulkan-headers`, `vulkan-loader`)
- SQLite 3 development headers (`libsqlite3-dev` or `sqlite-devel`)

**Optional Runtime Dependencies**:
- `poppler-cpp` or `poppler-utils` (`pdftotext`) for PDF text parsing
- `tesseract` for scanned document and image OCR fallback
- `ffmpeg` for video scene keyframe thumbnail extraction and chapter parsing

```bash
# Fedora / RHEL
sudo dnf install gcc-c++ cmake qt6-qtbase-devel qt6-qtdeclarative-devel qt6-qtsvg-devel \
  vulkan-headers vulkan-loader-devel sqlite-devel poppler-cpp-devel tesseract ffmpeg

# Ubuntu / Debian
sudo apt install build-essential cmake qt6-base-dev qt6-declarative-dev libqt6svg6-dev \
  libvulkan-dev libsqlite3-dev libpoppler-cpp-dev tesseract-ocr ffmpeg
```

### Build Instructions

```bash
git clone https://github.com/Starz099/lazy-look.git
cd lazy-look

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Run test suites
ctest --test-dir build --output-on-failure

# Install system-wide
sudo cmake --install build
```

### Packaging & Release Generation

To generate release artifacts locally:

```bash
# Generate RPM package
cd build && cpack -G RPM

# Generate DEB package
cd build && cpack -G DEB

# Generate standalone AppImage (with bundled model)
./scripts/build_appimage.sh

# Or generate all packages at once
./scripts/package_all.sh
```

## Running Tests

LazyLook includes a comprehensive automated test suite covering all core engine subsystems:

```bash
ctest --test-dir build --output-on-failure
```

The test suites verify:
- `ExtractorTests`: Multi-format document text extraction, Office XML, code tokenization, image metadata, audio slicing, and video keyframes.
- `DatabaseTests`: SQLite schema initialization, migration, FTS5 keyword indexing, `sqlite-vec` dense vector KNN search, and RRF ranking.
- `ConfigTests`: XDG path expansion, model search directory discovery, priority resolution, and environment overrides.
- `EmbedderTests`: Gemma-2 embedding model loading, L2 unit normalization, cosine similarity mathematics, and GPU VRAM auto-sleep unloading.
- `IndexerWatcherTests`: File ignore rules (`.git`, `node_modules`, hidden files, glob patterns), CRUD file indexing/removal, and inotify recursive change detection.

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
| `lazylook` | Launch LazyLook and show search bar |
| `lazylook --daemon` | Start LazyLook in background daemon mode |
| `lazylook --toggle` | Toggle search bar visibility |
| `lazylook --show` | Show and focus search bar |
| `lazylook --hide` | Hide search bar |
| `lazylook --quit` | Quit running LazyLook process |
| `lazylook --enable-autostart` | Enable background autostart on system login |
| `lazylook --disable-autostart` | Disable autostart on system login |

## License

MIT License
