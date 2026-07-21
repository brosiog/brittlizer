#!/usr/bin/env bash
#
# Build Carmy BrittLizer for macOS (Universal 2: arm64 + x86_64)
# Usage: ./scripts/build-macos.sh [Debug|Release|RelWithDebInfo]
#

set -euo pipefail

BUILD_TYPE="${1:-Release}"
BUILD_DIR="${PROJECT_SOURCE_DIR:-$(cd "$(dirname "$0")/.." && pwd)}/Build"

echo "═══ Carmy BrittLizer macOS Build ═══"
echo "  Build type: ${BUILD_TYPE}"
echo "  Build dir:  ${BUILD_DIR}"
echo ""

cmake -B "${BUILD_DIR}" -G Xcode \
    -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
    "$(dirname "$0")/.."

cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}" "$@"

echo ""
echo "═══ Build complete ═══"
echo "  AU:       ${BUILD_DIR}/${BUILD_TYPE}/Carmy BrittLizer.component"
echo "  VST3:     ${BUILD_DIR}/${BUILD_TYPE}/Carmy BrittLizer.vst3"
echo "  Standalone: ${BUILD_DIR}/${BUILD_TYPE}/Carmy BrittLizer.app"
echo ""
lipo -info "${BUILD_DIR}/${BUILD_TYPE}/Carmy BrittLizer.component/Contents/MacOS/Carmy BrittLizer" 2>/dev/null || true
echo "  (lipo info above confirms Universal Binary if built on macOS)"
