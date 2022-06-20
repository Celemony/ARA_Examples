#!/bin/sh

TMP_ENV_FILE=/tmp/mangle-plugin-version-$$.sh

CMAKE_VERSION_FILE="$1"
INFOPLIST_FILE="$2"

echo "${CMAKE_VERSION_FILE} -> ${INFOPLIST_FILE}"

# generate shell-compatible env file
cat "${CMAKE_VERSION_FILE}" | grep '^set' | sed -e 's;set(;;g' -e 's;);;g' -e 's; ;=;g' >  "${TMP_ENV_FILE}"
# source variables
. "${TMP_ENV_FILE}"
rm -f "${TMP_ENV_FILE}"

VERSION_NUMBER=$((${ARA_MAJOR_VERSION} * 65536 + ${ARA_MINOR_VERSION} * 256 + ${ARA_PATCH_VERSION}))
echo "${ARA_MAJOR_VERSION}.${ARA_MINOR_VERSION}.${ARA_PATCH_VERSION} => ${VERSION_NUMBER}"

# AudioComponent array index is omitted here to allow replacement to work across multiple plug-ins in the same plist
plutil -replace AudioComponents.version -integer ${VERSION_NUMBER} "${INFOPLIST_FILE}"

exit 0
