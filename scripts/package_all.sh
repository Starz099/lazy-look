#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

echo "LazyLook Multi-Format Release Builder"

echo "Compiling Release Target..."
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target lazylook -j"$(nproc)"

echo "Building RPM Package (.rpm)..."
(cd "${BUILD_DIR}" && cpack -G RPM)

echo "Building Debian / Ubuntu Package (.deb)..."
(cd "${BUILD_DIR}" && cpack -G DEB)

echo "Building Universal AppImage (.AppImage)..."
"${ROOT_DIR}/scripts/build_appimage.sh"

echo ""
echo "Build complete. Generated artifacts:"
ls -lh "${BUILD_DIR}"/lazylook-0.1.0-*.{rpm,deb,AppImage}
