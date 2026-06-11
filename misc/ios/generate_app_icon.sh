#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ICON_PNG="${ROOT_DIR}/misc/icon_quake3.png"
APP="${1:-}"

if [ -z "${APP}" ]; then
	echo "Usage: $0 <path-to-.app-bundle>" >&2
	exit 1
fi

if [ ! -f "${ICON_PNG}" ]; then
	echo "warning: ${ICON_PNG} not found, skipping icon generation" >&2
	exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

MASTER="${WORK}/master.png"
sips -z 1024 1024 "${ICON_PNG}" --out "${MASTER}" >/dev/null

mkdir -p "${APP}"

ASSETS="${WORK}/Assets.xcassets"
ICONSET="${ASSETS}/AppIcon.appiconset"
mkdir -p "${ICONSET}"
cp "${SCRIPT_DIR}/Assets.xcassets/Contents.json" "${ASSETS}/"
cp "${SCRIPT_DIR}/Assets.xcassets/AppIcon.appiconset/Contents.json" "${ICONSET}/"

sips -z 180 180 "${MASTER}" --out "${ICONSET}/AppIcon60x60@3x.png" >/dev/null
sips -z 120 120 "${MASTER}" --out "${ICONSET}/AppIcon60x60@2x.png" >/dev/null
sips -z 152 152 "${MASTER}" --out "${ICONSET}/AppIcon76x76@2x.png" >/dev/null
sips -z 167 167 "${MASTER}" --out "${ICONSET}/AppIcon83.5x83.5@2x.png" >/dev/null
sips -z 1024 1024 "${MASTER}" --out "${ICONSET}/AppIcon1024.png" >/dev/null

rm -f "${APP}"/AppIcon*.png "${APP}/Assets.car"

xcrun actool \
	--output-partial-info-plist "${WORK}/partial.plist" \
	--app-icon AppIcon \
	--compress-pngs \
	--enable-on-demand-resources NO \
	--development-region en \
	--target-device iphone \
	--target-device ipad \
	--minimum-deployment-target 13.0 \
	--platform iphoneos \
	--compile "${APP}" \
	"${ASSETS}" >/dev/null

if [ -f "${APP}/Info.plist" ] && [ -f "${WORK}/partial.plist" ]; then
	/usr/libexec/PlistBuddy -c "Merge ${WORK}/partial.plist" "${APP}/Info.plist"
fi

echo "App icon catalog compiled into ${APP}"
