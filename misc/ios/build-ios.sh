#!/bin/bash
set -euo pipefail

# Xcode does not inherit the interactive shell's PATH. Include the standard
# Homebrew, MacPorts and CMake.app locations for GUI-initiated builds.
export PATH="${PATH}:/opt/homebrew/bin:/usr/local/bin:/opt/local/bin:/Applications/CMake.app/Contents/bin"
if ! command -v cmake >/dev/null 2>&1; then
	echo "error: CMake was not found. Install CMake (Homebrew, MacPorts or CMake.app), or add its bin directory to PATH." >&2
	exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SDL_VERSION="${SDL_VERSION:-2.32.8}"
SDL_DIR="${ROOT_DIR}/misc/ios/sdl2-build/SDL2-${SDL_VERSION}"
SDL_LIB="${ROOT_DIR}/misc/ios/sdl2-build/libSDL2-ios.a"
HOST_BUILD="${ROOT_DIR}/build/host-qvms"
IOS_BUILD="${ROOT_DIR}/build/ios-port"
APP="${ROOT_DIR}/build/ios/ioquake3.app"

cd "${ROOT_DIR}"

# Xcode injects iPhone SDK settings in shell phases. Those break the host-side
# tools/QVM build, so clear them before invoking host CMake.
unset SDKROOT SDKROOT_iphoneos IPHONEOS_DEPLOYMENT_TARGET
unset EFFECTIVE_PLATFORM_NAME PLATFORM_NAME ARCHS CONFIGURATION BUILD_DIR
unset CMAKE_OSX_ARCHITECTURES CMAKE_OSX_DEPLOYMENT_TARGET CMAKE_OSX_SYSROOT
unset CC CXX OBJC CFLAGS CXXFLAGS OBJCFLAGS CPPFLAGS LDFLAGS

if [ ! -f "${SDL_LIB}" ]; then
	"${SCRIPT_DIR}/setup_sdl2.sh"
fi

echo "=== Building host QVMs ==="
(
# Use macOS explicitly, including for the nested QVM tools build. Refresh the
# host cache to discard SDK/compiler checks left by earlier Xcode launches.
export SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"
cmake --fresh -S . -B "${HOST_BUILD}" \
	-DCMAKE_OSX_SYSROOT="${SDKROOT}" \
	-DBUILD_CLIENT=OFF \
	-DBUILD_SERVER=OFF \
	-DBUILD_MACOS_APP=OFF \
	-DBUILD_RENDERER_GL1=OFF \
	-DBUILD_RENDERER_GL2=OFF \
	-DBUILD_GAME_LIBRARIES=OFF \
	-DBUILD_GAME_QVMS=ON
cmake --build "${HOST_BUILD}" --config Release -j"$(sysctl -n hw.ncpu 2>/dev/null || echo 4)"
)

echo "=== Generating iOS project ==="
cmake -S . -B "${IOS_BUILD}" -G Xcode \
	-DCMAKE_SYSTEM_NAME=iOS \
	-DCMAKE_OSX_SYSROOT=iphoneos \
	-DCMAKE_OSX_ARCHITECTURES=arm64 \
	-DSDL2_INCLUDE_DIRS="${SDL_DIR}/include" \
	-DSDL2_LIBRARIES="${SDL_LIB}"

echo "=== Building iOS client ==="
cmake --build "${IOS_BUILD}" --config Release --target ioquake3 -- -sdk iphoneos CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO

APP_BUNDLE="$(find "${IOS_BUILD}" -type d -name ioquake3.app | head -n 1)"
BIN="$(find "${IOS_BUILD}" -type f -name ioquake3 -perm +111 ! -path "*/.*" | head -n 1)"
if [ -z "${APP_BUNDLE}" ] && [ -z "${BIN}" ]; then
	echo "error: could not locate iOS build output in ${IOS_BUILD}" >&2
	exit 1
fi

rm -rf "${APP}"
if [ -n "${APP_BUNDLE}" ]; then
	cp -R "${APP_BUNDLE}" "${APP}"
else
	mkdir -p "${APP}"
	cp "${BIN}" "${APP}/ioquake3"
	chmod +x "${APP}/ioquake3"
fi

# Project metadata must win over the CMake bundle (iPad native + launch screen).
cp "${SCRIPT_DIR}/Info.plist" "${APP}/Info.plist"
cp "${SCRIPT_DIR}/LaunchScreen.storyboard" "${APP}/LaunchScreen.storyboard"

"${SCRIPT_DIR}/copy_game_data.sh" "${APP}"
"${SCRIPT_DIR}/generate_app_icon.sh" "${APP}" || true

echo
echo "Built: ${APP}"
