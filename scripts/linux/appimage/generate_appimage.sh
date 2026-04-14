#!/bin/bash

export PROJECT_NAME=$1
export TARGET_DIST=$2
export BUILD_TYPE=$3 # 'static' or 'dynamic'

# Convert TARGET_DIST to an absolute path to avoid reference errors
ABS_TARGET_DIST="$(cd "$TARGET_DIST" && pwd)"
APPDIR="${ABS_TARGET_DIST}/AppDir"
OUTPUT_FILE="${ABS_TARGET_DIST}/${PROJECT_NAME}-${BUILD_TYPE}.AppImage"

printf "  [AppImage] Organizing structure for %s build...\n" "$BUILD_TYPE"

#Clean and start again
rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin"
mkdir -p "$APPDIR/usr/lib"

cp "$ABS_TARGET_DIST/${PROJECT_NAME}" "$APPDIR/usr/bin/"

if [ -d "$ABS_TARGET_DIST/libs" ]; then
    cp -P "$ABS_TARGET_DIST/libs"/*.so* "$APPDIR/usr/lib/" 2>/dev/null || true
fi

#Generate files using the templates
envsubst '${PROJECT_NAME}' < "scripts/linux/appimage/desktop.template" > "$APPDIR/${PROJECT_NAME}.desktop"
envsubst '${PROJECT_NAME}' < "scripts/linux/appimage/AppRun.template" > "$APPDIR/AppRun"
chmod +x "$APPDIR/AppRun"

#Icon handling
ICON_FOUND=false
for EXT in png webp ico; do
    ICON_SRC="src/gui/media/minecraft_writable_book.${EXT}"
    if [ -f "$ICON_SRC" ]; then
        cp "$ICON_SRC" "$APPDIR/${PROJECT_NAME}.png"
        ln -sf "${PROJECT_NAME}.png" "$APPDIR/.DirIcon"
        printf "  [AppImage] Icon copied from %s\n" "$ICON_SRC"
        ICON_FOUND=true
        break
    fi
done

if [ "$ICON_FOUND" = false ]; then
    printf "  [AppImage] Warning: Icon not found. AppImage will lack an icon.\n"
fi

export ARCH=x86_64

appimagetool "$APPDIR" "$OUTPUT_FILE" > /dev/null
chmod +x "$OUTPUT_FILE"

rm -rf "$APPDIR"

printf "  [AppImage] Successfully created: %s\n" "${PROJECT_NAME}-${BUILD_TYPE}.AppImage"