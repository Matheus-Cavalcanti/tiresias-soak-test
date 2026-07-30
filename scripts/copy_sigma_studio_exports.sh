#!/bin/sh

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
CONFIG_FILE="${SCRIPT_DIR}/copy_sigma_studio_exports.conf"

if [ ! -f "${CONFIG_FILE}" ]; then
	echo "Configuration file not found: ${CONFIG_FILE}" >&2
	exit 1
fi

# shellcheck source=copy_sigma_studio_exports.conf
. "${CONFIG_FILE}"

if [ ! -d "${SOURCE_DIR}" ]; then
	echo "Source directory not found: ${SOURCE_DIR}" >&2
	exit 1
fi

mkdir -p "${DESTINATION_DIR}"
cp -R "${SOURCE_DIR}/." "${DESTINATION_DIR}/"

echo "Copied SigmaStudio exports from ${SOURCE_DIR} to ${DESTINATION_DIR}"
