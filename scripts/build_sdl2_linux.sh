#!/usr/bin/env bash
#
# Build script for EKA2L1 SDL2 frontend on x86_64 Linux.
#
# Usage:
#   ./scripts/build_sdl2_linux.sh            # full build (install deps, configure, build)
#   ./scripts/build_sdl2_linux.sh --deps     # install dependencies only
#   ./scripts/build_sdl2_linux.sh --build    # configure + build only (skip deps)
#   ./scripts/build_sdl2_linux.sh --clean    # remove build directory and rebuild
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-sdl2"
BUILD_TYPE="${BUILD_TYPE:-Release}"
JOBS="${JOBS:-$(nproc)}"
SKIP_DEPS=0
CLEAN=0

for arg in "$@"; do
    case "$arg" in
        --deps)   install_deps_and_exit=1 ;;
        --build)  SKIP_DEPS=1 ;;
        --clean)  CLEAN=1 ;;
        --debug)  BUILD_TYPE="Debug" ;;
        -h|--help)
            echo "Usage: $0 [--deps|--build|--clean|--debug]"
            echo ""
            echo "  --deps    Install system dependencies only"
            echo "  --build   Skip dependency installation, configure + build"
            echo "  --clean   Remove build directory before building"
            echo "  --debug   Build in Debug mode (default: Release)"
            echo ""
            echo "Environment variables:"
            echo "  BUILD_TYPE   CMake build type (default: Release)"
            echo "  JOBS         Parallel jobs (default: nproc = $(nproc))"
            exit 0
            ;;
        *) echo "Unknown option: $arg"; exit 1 ;;
    esac
done

# --------------------------------------------------------------------------
# Step 1: Install dependencies
# --------------------------------------------------------------------------
install_deps() {
    echo "==> Installing build dependencies..."
    sudo apt-get update -qq
    sudo apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ccache \
        git \
        libsdl2-dev \
        libpulse-dev \
        libasound2-dev \
        libgtk-3-dev \
        libgl-dev \
        libglu1-mesa-dev \
        libx11-dev \
        libwayland-dev \
        libwayland-egl1 \
        libegl-dev \
        pkg-config
    echo "==> Dependencies installed."
}

if [[ "${install_deps_and_exit:-0}" == "1" ]]; then
    install_deps
    exit 0
fi

if [[ "$SKIP_DEPS" == "0" ]]; then
    install_deps
fi

# --------------------------------------------------------------------------
# Step 2: Init submodules
# --------------------------------------------------------------------------
echo "==> Initializing git submodules..."
cd "$ROOT_DIR"
git submodule update --init --recursive

# --------------------------------------------------------------------------
# Step 3: Clean if requested
# --------------------------------------------------------------------------
if [[ "$CLEAN" == "1" ]]; then
    echo "==> Cleaning build directory..."
    rm -rf "$BUILD_DIR"
fi

# --------------------------------------------------------------------------
# Step 4: Configure
# --------------------------------------------------------------------------
echo "==> Configuring CMake (${BUILD_TYPE})..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DEKA2L1_BUILD_SDL2_FRONTEND=ON \
    -DEKA2L1_NO_QT_FRONTEND=ON \
    -DEKA2L1_ENABLE_SCRIPTING_ABILITY=OFF \
    -DEKA2L1_BUILD_TOOLS=OFF \
    -DEKA2L1_BUILD_TESTS=OFF

# --------------------------------------------------------------------------
# Step 5: Build
# --------------------------------------------------------------------------
echo "==> Building eka2l1_sdl2 with ${JOBS} jobs..."
cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" --target eka2l1_sdl2 --parallel "$JOBS"

# --------------------------------------------------------------------------
# Done
# --------------------------------------------------------------------------
BINARY="${BUILD_DIR}/bin/eka2l1_sdl2"
if [[ -f "$BINARY" ]]; then
    echo ""
    echo "========================================="
    echo " Build successful!"
    echo " Binary: ${BINARY}"
    echo "========================================="
    echo ""
    echo "Usage examples:"
    echo "  ${BINARY} --help"
    echo "  ${BINARY} --listdevices"
    echo "  ${BINARY} --listapp"
    echo "  ${BINARY} --run <AppName>"
    echo "  ${BINARY} --run 0x<UID>"
else
    echo "ERROR: Binary not found at ${BINARY}"
    exit 1
fi
