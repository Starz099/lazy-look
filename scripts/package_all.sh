#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

echo "=========================================="
echo "  LazyFinder Multi-Format Release Builder "
echo "=========================================="

echo "==> 1. Compiling Release Target..."
cmake -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target lazyfinder -j"$(nproc)"

echo "==> 2. Building RPM Package (.rpm)..."
(cd "${BUILD_DIR}" && cpack -G RPM)

echo "==> 3. Building Debian / Ubuntu Package (.deb)..."
(cd "${BUILD_DIR}" && cpack -G DEB)

echo "==> 4. Building Universal AppImage (.AppImage)..."
"${ROOT_DIR}/scripts/build_appimage.sh"

echo ""
echo "=========================================="
echo "  Build Complete! Generated Artifacts:   "
echo "=========================================="
ls -lh "${BUILD_DIR}"/lazyfinder-0.1.0-*.{rpm,deb,AppImage}
