#!/usr/bin/env bash

export PROJECT_NAME=$1
export TARGET_DIST=$2
export BUILD_TYPE=$3 # 'static' or 'dynamic'

export OUTPUT_EXE="${PROJECT_NAME}-${BUILD_TYPE}-setup.exe"

printf "  [NSIS] Generating Windows installer for %s build...\n" "$BUILD_TYPE"

ICON_SOURCE="src/gui/media/minecraft_writable_book.ico"
if [ -f "$ICON_SOURCE" ]; then
    ICON_FILENAME=$(basename "$ICON_SOURCE")
    cp "$ICON_SOURCE" "${TARGET_DIST}/${ICON_FILENAME}"
    export ICON_PATH="$ICON_FILENAME"
    printf "  [NSIS] Icon copied from %s\n" "$ICON_SOURCE"
else
    printf "  [NSIS] Warning: Icon (.ico) not found. The compilation might fail if the template strictly requires it.\n"
    export ICON_PATH=""
fi

TEMPLATE="scripts/windows/installer.nsi.template"

envsubst '${PROJECT_NAME} ${OUTPUT_EXE} ${ICON_PATH}' < "$TEMPLATE" > "${TARGET_DIST}/installer.nsi"

CURRENT_DIR=$(pwd)
cd "$TARGET_DIST" || exit 1

if makensis "installer.nsi" > /dev/null; then
    printf "  [NSIS] Successfully created: %s\n" "$OUTPUT_EXE"
else
    printf "  [NSIS] Error: Failed to compile the installer.\n"
fi

#Clean up
rm -f "installer.nsi"
if [ -n "$ICON_PATH" ]; then
    rm -f "$ICON_PATH"
fi

cd "$CURRENT_DIR" || exit 1