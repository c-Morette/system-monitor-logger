#!/usr/bin/env bash
# Compila um alvo dentro do container.
# Argumento: linux-x64 | linux-x86 | win-x64 | win-x86
set -euo pipefail

TARGET="${1:-linux-x64}"

CMAKE_ARGS=(-DCMAKE_BUILD_TYPE=Release)
OUT_EXT=""

case "$TARGET" in
    linux-x64) ;;
    linux-x86) CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS=-m32) ;;
    win-x64)   CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE=/work/cmake/mingw-w64-x86_64.cmake); OUT_EXT=".exe" ;;
    win-x86)   CMAKE_ARGS+=(-DCMAKE_TOOLCHAIN_FILE=/work/cmake/mingw-w64-i686.cmake);   OUT_EXT=".exe" ;;
    *) echo "Alvo invalido: $TARGET (use linux-x64|linux-x86|win-x64|win-x86)"; exit 1 ;;
esac

BUILD_DIR="/work/build/${TARGET}"
rm -rf "$BUILD_DIR"

cmake -S /work -B "$BUILD_DIR" "${CMAKE_ARGS[@]}"
cmake --build "$BUILD_DIR" --parallel

mkdir -p /work/dist
cp "${BUILD_DIR}/SystemMonitorLogger${OUT_EXT}" \
   "/work/dist/SystemMonitorLogger-${TARGET}${OUT_EXT}"
echo "OK: dist/SystemMonitorLogger-${TARGET}${OUT_EXT}"
