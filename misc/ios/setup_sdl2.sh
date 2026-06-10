#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
SDL_VERSION="${SDL_VERSION:-2.32.8}"
SDL_DIR="${ROOT_DIR}/misc/ios/sdl2-build/SDL2-${SDL_VERSION}"
SDL_TARBALL="${ROOT_DIR}/misc/ios/sdl2-build/SDL2-${SDL_VERSION}.tar.gz"
SDL_LIB="${ROOT_DIR}/misc/ios/sdl2-build/libSDL2-ios.a"
SDL_BUILD="${ROOT_DIR}/misc/ios/sdl2-build/xcode"

if [ -f "${SDL_LIB}" ] && [ "${FORCE_SDL2_REBUILD:-0}" != "1" ]; then
	echo "OK: ${SDL_LIB}"
	exit 0
fi

mkdir -p "${ROOT_DIR}/misc/ios/sdl2-build"

if [ ! -f "${SDL_DIR}/include/SDL.h" ]; then
	if [ ! -f "${SDL_TARBALL}" ]; then
		echo "Downloading SDL2 ${SDL_VERSION}..."
		curl -fL --retry 3 -o "${SDL_TARBALL}" \
			"https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL2-${SDL_VERSION}.tar.gz"
	fi
	rm -rf "${SDL_DIR}"
	tar -xzf "${SDL_TARBALL}" -C "${ROOT_DIR}/misc/ios/sdl2-build"
fi

xcodebuild \
	-project "${SDL_DIR}/Xcode/SDL/SDL.xcodeproj" \
	-scheme "Static Library-iOS" \
	-configuration Release \
	-sdk iphoneos \
	BUILD_DIR="${SDL_BUILD}" \
	ONLY_ACTIVE_ARCH=NO \
	ARCHS=arm64 \
	build

LIB_FOUND="$(find "${SDL_BUILD}" -name 'libSDL2.a' -print | head -n 1)"
if [ -z "${LIB_FOUND}" ]; then
	echo "Unable to find libSDL2.a after SDL build" >&2
	exit 1
fi

cp "${LIB_FOUND}" "${SDL_LIB}"
echo "Installed ${SDL_LIB}"
