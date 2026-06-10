#!/bin/bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/../.." && pwd)"
DEST="${1:-}"

if [ -z "${DEST}" ]; then
	echo "Usage: $0 <path-to-.app-bundle>" >&2
	exit 1
fi

mkdir -p "${DEST}"

if [ ! -d "${ROOT_DIR}/baseq3" ]; then
	echo "error: missing ${ROOT_DIR}/baseq3" >&2
	exit 1
fi

echo "Copying baseq3 -> ${DEST}/"
rm -rf "${DEST}/baseq3"
cp -R "${ROOT_DIR}/baseq3" "${DEST}/"

VM_SRC=""
for candidate in \
	"${ROOT_DIR}/build/host-qvms/Release/baseq3/vm" \
	"${ROOT_DIR}/build/host-qvms/baseq3/vm" \
	"${ROOT_DIR}/build/Release/baseq3/vm" \
	"${ROOT_DIR}/build/baseq3/vm"; do
	if [ -d "${candidate}" ]; then
		VM_SRC="${candidate}"
		break
	fi
done

if [ -n "${VM_SRC}" ]; then
	mkdir -p "${DEST}/baseq3/vm"
	cp -f "${VM_SRC}"/*.qvm "${DEST}/baseq3/vm/" 2>/dev/null || true
fi

echo "Game data installed in ${DEST}"
