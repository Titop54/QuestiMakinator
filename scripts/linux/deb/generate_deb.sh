#!/usr/bin/env bash
export PROJECT_NAME=$1
export TARGET_DIST=$2
DEB_ROOT="${TARGET_DIST}/deb_root"

export PKG_NAME="${PROJECT_NAME,,}"
export VERSION="1.0.0"

printf "\n[Linux] Generando paquete .deb...\n"

mkdir -p "$DEB_ROOT/usr/bin/${PROJECT_NAME}"
mkdir -p "$DEB_ROOT/DEBIAN"

cp -r "$TARGET_DIST"/* "$DEB_ROOT/usr/bin/${PROJECT_NAME}/" 2>/dev/null
rm -rf "$DEB_ROOT/usr/bin/${PROJECT_NAME}/deb_root" 2>/dev/null
rm -rf "$DEB_ROOT/usr/bin/${PROJECT_NAME}/AppDir" 2>/dev/null
rm -rf "$DEB_ROOT/usr/bin/${PROJECT_NAME}/arch_root" 2>/dev/null

TEMPLATE_FILE="scripts/linux/deb/control.template"

if [ ! -f "$TEMPLATE_FILE" ]; then
    printf "Error crítico: No se encuentra la plantilla %s\n" "$TEMPLATE_FILE"
    exit 1
fi

envsubst < "$TEMPLATE_FILE" > "$DEB_ROOT/DEBIAN/control"

dpkg-deb --build "$DEB_ROOT" "${TARGET_DIST}/${PROJECT_NAME}-setup-linux.deb"

rm -rf "$DEB_ROOT"