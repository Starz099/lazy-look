#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
APPDIR="${BUILD_DIR}/AppDir"

echo "==> Preparing LazyFinder AppDir..."

# Ensure lazyfinder is built
if [[ ! -f "${BUILD_DIR}/src/ui/lazyfinder" ]]; then
    cmake --build "${BUILD_DIR}" --target lazyfinder -j"$(nproc)"
fi

# Clean & create AppDir structure
rm -rf "${APPDIR}"
mkdir -p "${APPDIR}/usr/bin"
mkdir -p "${APPDIR}/usr/lib"
mkdir -p "${APPDIR}/usr/share/applications"
mkdir -p "${APPDIR}/usr/share/icons/hicolor/scalable/apps"
mkdir -p "${APPDIR}/usr/share/lazyfinder/models"

cp "${BUILD_DIR}/src/ui/lazyfinder" "${APPDIR}/usr/bin/"
if [[ -f "${BUILD_DIR}/src/daemon/lazyfinderd" ]]; then
    cp "${BUILD_DIR}/src/daemon/lazyfinderd" "${APPDIR}/usr/bin/"
fi
cp "${ROOT_DIR}/resources/lazyfinder.desktop" "${APPDIR}/usr/share/applications/"
cp "${ROOT_DIR}/resources/icons/lazyfinder.svg" "${APPDIR}/usr/share/icons/hicolor/scalable/apps/"

# Bundle embedding model for zero-setup out of the box
MODEL_CANDIDATES=(
    "${ROOT_DIR}/models/embeddinggemma-2-Q8_0.gguf"
    "${ROOT_DIR}/models/embeddinggemma-2-BF16.gguf"
    "${BUILD_DIR}/models/embeddinggemma-2-Q8_0.gguf"
    "${XDG_DATA_HOME:-$HOME/.local/share}/lazyfinder/models/embeddinggemma-2-Q8_0.gguf"
    "${XDG_DATA_HOME:-$HOME/.local/share}/lazyfinder/models/embeddinggemma-2-BF16.gguf"
)
BUNDLED=0
for cand in "${MODEL_CANDIDATES[@]}"; do
    if [[ -f "${cand}" ]]; then
        echo "==> Bundling AI embedding model from: ${cand}"
        cp "${cand}" "${APPDIR}/usr/share/lazyfinder/models/embeddinggemma-2-Q8_0.gguf"
        BUNDLED=1
        break
    fi
done

if [[ ${BUNDLED} -eq 0 ]]; then
    echo "==> Notice: No local model found to bundle into AppImage."
fi

# Bundle companion vision projector if present
for vcand in "${ROOT_DIR}/models/mmproj-embeddinggemma-2.gguf" "${XDG_DATA_HOME:-$HOME/.local/share}/lazyfinder/models/mmproj-embeddinggemma-2.gguf"; do
    if [[ -f "${vcand}" ]]; then
        echo "==> Bundling vision projector from: ${vcand}"
        cp "${vcand}" "${APPDIR}/usr/share/lazyfinder/models/"
        break
    fi
done

# Top-level AppImage requirements
cp "${ROOT_DIR}/resources/lazyfinder.desktop" "${APPDIR}/"
cp "${ROOT_DIR}/resources/icons/lazyfinder.svg" "${APPDIR}/"
if [[ -f "${ROOT_DIR}/resources/icons/lazyfinder.png" ]]; then
    cp "${ROOT_DIR}/resources/icons/lazyfinder.png" "${APPDIR}/lazyfinder.png"
    cp "${ROOT_DIR}/resources/icons/lazyfinder.png" "${APPDIR}/.DirIcon"
elif command -v magick >/dev/null 2>&1; then
    magick "${ROOT_DIR}/resources/icons/lazyfinder.svg" -background transparent -resize 256x256 "${APPDIR}/.DirIcon" 2>/dev/null || true
    magick "${ROOT_DIR}/resources/icons/lazyfinder.svg" -background transparent -resize 256x256 "${APPDIR}/lazyfinder.png" 2>/dev/null || true
elif command -v convert >/dev/null 2>&1; then
    convert -background transparent "${ROOT_DIR}/resources/icons/lazyfinder.svg" -resize 256x256 "${APPDIR}/.DirIcon" 2>/dev/null || true
    convert -background transparent "${ROOT_DIR}/resources/icons/lazyfinder.svg" -resize 256x256 "${APPDIR}/lazyfinder.png" 2>/dev/null || true
fi

# Create AppRun entrypoint
cat <<'EOF' > "${APPDIR}/AppRun"
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export PATH="${HERE}/usr/bin:${PATH}"
export LD_LIBRARY_PATH="${HERE}/usr/lib:${HERE}/usr/lib64:${LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="${HERE}/usr/plugins:${QT_PLUGIN_PATH}"
export QML_IMPORT_PATH="${HERE}/usr/qml:${QML_IMPORT_PATH}"
export QML2_IMPORT_PATH="${HERE}/usr/qml:${QML2_IMPORT_PATH}"
exec "${HERE}/usr/bin/lazyfinder" "$@"
EOF
chmod +x "${APPDIR}/AppRun"

# Deploy Qt6 dependencies via linuxdeploy if available
if [[ -f "/tmp/linuxdeploy" && -f "/tmp/linuxdeploy-plugin-qt" ]]; then
    echo "==> Deploying Qt6 libraries into AppDir..."
    export QML_SOURCES_PATHS="${ROOT_DIR}/src/ui/qml"
    export QT_SELECT=6
    PATH="/tmp:${PATH}" /tmp/linuxdeploy --appimage-extract-and-run \
        --appdir="${APPDIR}" \
        --executable="${APPDIR}/usr/bin/lazyfinder" \
        --desktop-file="${APPDIR}/lazyfinder.desktop" \
        --icon-file="${APPDIR}/lazyfinder.svg" \
        --plugin=qt || true
fi

# Download appimagetool if not present
TOOL_PATH="/tmp/appimagetool"
if [[ ! -f "${TOOL_PATH}" ]]; then
    echo "==> Downloading appimagetool..."
    curl -sL https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage -o "${TOOL_PATH}"
    chmod +x "${TOOL_PATH}"
fi

# Package into final AppImage
echo "==> Packaging AppImage..."
ARCH=x86_64 "${TOOL_PATH}" --appimage-extract-and-run "${APPDIR}" "${BUILD_DIR}/lazyfinder-0.1.0-x86_64.AppImage"

echo "==> Done! AppImage generated at: ${BUILD_DIR}/lazyfinder-0.1.0-x86_64.AppImage"
