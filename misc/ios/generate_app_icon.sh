#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
ICON="${ROOT_DIR}/misc/macos/quake3_flat.icns"
APP="${1:-}"

if [ -z "${APP}" ]; then
	echo "Usage: $0 <path-to-.app-bundle>" >&2
	exit 1
fi

if [ ! -f "${ICON}" ]; then
	echo "warning: ${ICON} not found, skipping icon generation" >&2
	exit 0
fi

WORK="$(mktemp -d)"
trap 'rm -rf "${WORK}"' EXIT

iconutil --convert iconset -o "${WORK}/master.iconset" "${ICON}"

MASTER=""
for candidate in \
	"${WORK}/master.iconset/icon_512x512@2x.png" \
	"${WORK}/master.iconset/icon_512x512.png" \
	"${WORK}/master.iconset/icon_256x256@2x.png"; do
	if [ -f "${candidate}" ]; then
		MASTER="${candidate}"
		break
	fi
done

if [ -z "${MASTER}" ]; then
	echo "warning: no usable PNG found in ${ICON}" >&2
	exit 0
fi

sips -z 180 180 "${MASTER}" --out "${APP}/AppIcon60x60@3x.png" >/dev/null
sips -z 120 120 "${MASTER}" --out "${APP}/AppIcon60x60@2x.png" >/dev/null
sips -z 152 152 "${MASTER}" --out "${APP}/AppIcon76x76@2x.png" >/dev/null
