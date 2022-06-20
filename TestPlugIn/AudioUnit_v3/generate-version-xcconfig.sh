#!/bin/sh

CMAKE_VERSION_FILE="$1"
XCCONFIG_FILE="$2"

echo "${CMAKE_VERSION_FILE} -> ${XCCONFIG_FILE}"

# generate shell-compatible env file
cat "${CMAKE_VERSION_FILE}" | grep '^set' | sed -e 's;set(;;g' -e 's;);;g' -e 's; ;=;g' >  "${XCCONFIG_FILE}"
# source variables
. "${XCCONFIG_FILE}"

VERSION_NUMBER=$((${ARA_MAJOR_VERSION} * 65536 + ${ARA_MINOR_VERSION} * 256 + ${ARA_PATCH_VERSION}))
echo "${ARA_MAJOR_VERSION}.${ARA_MINOR_VERSION}.${ARA_PATCH_VERSION} => ${VERSION_NUMBER}"
echo "ARA_PLUGIN_VERSION_NUMBER=${VERSION_NUMBER}" >> "${XCCONFIG_FILE}"

exit 0
