#!/usr/bin/env bash
export PROJECT_NAME=$1
export TARGET_DIST=$2
export PKG_NAME="${PROJECT_NAME,,}"
export VERSION="1.0.0"


ARCH_ROOT="$(pwd)/${TARGET_DIST}/arch_root"
TAR_NAME="${PKG_NAME}-${VERSION}-x86_64.tar.gz"

printf "\n[Arch] Preparing for Arch Linux...\n"

mkdir -p "$ARCH_ROOT"

cd "$TARGET_DIST" || exit 1
tar -czf "$ARCH_ROOT/$TAR_NAME" --exclude='deb_root' --exclude='AppDir' --exclude='arch_root' --exclude='*.AppImage' --exclude='*.deb' *
cd - > /dev/null

TEMPLATE_FILE="scripts/linux/arch/PKGBUILD.template"
envsubst '${PKG_NAME} ${VERSION} ${PROJECT_NAME}' < "$TEMPLATE_FILE" > "$ARCH_ROOT/PKGBUILD"

if command -v makepkg &> /dev/null; then
    printf "We are on arch, building package...\n"
    cd "$ARCH_ROOT" || exit 1
    makepkg -sf --noconfirm
    mv *.pkg.tar.zst ../
    cd - > /dev/null
    printf "Generated!\n"
else
    printf "We are not in Arch family Linux: Files are ready in %s\n" "$ARCH_ROOT"
fi