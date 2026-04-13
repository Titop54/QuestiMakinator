#!/bin/bash

PROJECT_NAME="QuestiMakinator"
BUILD_DIR="build"
OUTPUT_BASE="build/output"

DO_WINDOWS=false
DO_LINUX=false
DO_STATIC=false
DO_DYNAMIC=false
PACK_ACTIVE=false

# 1. Procesar argumentos
while [[ $# -gt 0 ]]; do
    case $1 in
        --windows | --window)
            DO_WINDOWS=true
            shift
            ;;
        --linux)
            DO_LINUX=true
            shift
            ;;
        --static)
            DO_STATIC=true
            shift
            ;;
        --dynamic)
            DO_DYNAMIC=true
            shift
            ;;
        --pack)
            PACK_ACTIVE=true
            shift
            ;;
        *)
            printf "Argumento no válido: %s\n" "$1"
            printf "Uso: ./pack.sh [--windows | --linux] [--static | --dynamic]\n"
            exit 1
            ;;
    esac
done

if [ "$DO_WINDOWS" = false ] && [ "$DO_LINUX" = false ]; then
    DO_WINDOWS=true
    DO_LINUX=true
fi

if [ "$DO_STATIC" = false ] && [ "$DO_DYNAMIC" = false ]; then
    DO_STATIC=true
    DO_DYNAMIC=true
fi

TRIPLETS=()
if [ "$DO_WINDOWS" = true ]; then
    if [ "$DO_STATIC" = true ]; then TRIPLETS+=("x64-mingw-static"); fi
    if [ "$DO_DYNAMIC" = true ]; then TRIPLETS+=("x64-mingw-dynamic"); fi
fi
if [ "$DO_LINUX" = true ]; then
    if [ "$DO_STATIC" = true ]; then TRIPLETS+=("x64-linux"); fi
    if [ "$DO_DYNAMIC" = true ]; then TRIPLETS+=("x64-linux-dynamic"); fi
fi

for TRIPLET in "${TRIPLETS[@]}"; do
    printf "\n=========================================\n"
    printf "Empaquetando: %s\n" "$TRIPLET"
    printf "=========================================\n"

    TARGET_DIST="${OUTPUT_BASE}/${TRIPLET}"

    rm -rf "$TARGET_DIST"
    mkdir -p "$TARGET_DIST"

    IS_WINDOWS=false
    if [[ "$TRIPLET" == *"mingw"* ]]; then IS_WINDOWS=true; fi
    
    IS_DYNAMIC=false
    if [[ "$TRIPLET" == *"dynamic"* ]]; then IS_DYNAMIC=true; fi

    # Copiar exe
    if [ "$IS_WINDOWS" = true ]; then
        EXE_PATH="${BUILD_DIR}/${TRIPLET}/${PROJECT_NAME}.exe"
    else
        EXE_PATH="${BUILD_DIR}/${TRIPLET}/${PROJECT_NAME}"
    fi

    if [ ! -f "$EXE_PATH" ]; then
        printf "Error: No se encontró el ejecutable en %s.\n" "$EXE_PATH"
        printf "¡Ignorando %s porque no ha sido compilado!\n" "$TRIPLET"
        continue
    fi
    
    cp "$EXE_PATH" "$TARGET_DIST/"
    printf "Ejecutable copiado.\n"

    # Copiar librerias
    if [ "$IS_DYNAMIC" = true ]; then
        VCPKG_TRIPLET_DIR="vcpkg_installed/${TRIPLET}/${TRIPLET}"
        
        if [ "$IS_WINDOWS" = true ]; then
            if [ -d "${VCPKG_TRIPLET_DIR}/bin" ]; then
                cp "${VCPKG_TRIPLET_DIR}/bin/"*.dll "$TARGET_DIST/" 2>/dev/null || true
                printf "Librerías de vcpkg (DLLs) copiadas.\n"
            fi

            printf "Buscando librerías base del compilador...\n"
            COMPILER_PATH=$(command -v x86_64-w64-mingw32-clang++)
            if [ -n "$COMPILER_PATH" ]; then
                LLVM_BASE=$(dirname "$(dirname "$COMPILER_PATH")")
                LIBCPP=$(find "$LLVM_BASE" -name "libc++.dll" | head -n 1)
                LIBUNWIND=$(find "$LLVM_BASE" -name "libunwind.dll" | head -n 1)
                
                if [ -n "$LIBCPP" ]; then cp "$LIBCPP" "$TARGET_DIST/"; fi
                if [ -n "$LIBUNWIND" ]; then cp "$LIBUNWIND" "$TARGET_DIST/"; fi
                printf "Librerías del compilador copiadas.\n"
            fi
        else
            mkdir -p "$TARGET_DIST/libs"
            if [ -d "${VCPKG_TRIPLET_DIR}/lib" ]; then
                cp -P "${VCPKG_TRIPLET_DIR}/lib/"*.so* "$TARGET_DIST/libs/" 2>/dev/null || true
                printf "Librerías de vcpkg (SOs) copiadas a la carpeta libs/.\n"
            fi
        fi
    fi

    if [ "$PACK_ACTIVE" = true ]; then
        
        if [[ "$TRIPLET" == "x64-mingw-dynamic" ]]; then
            bash scripts/windows/generate_nsis.sh "$PROJECT_NAME" "$TARGET_DIST"
        elif [[ "$TRIPLET" == *"linux"* ]]; then
            bash scripts/linux/appimage/generate_appimage.sh "$PROJECT_NAME" "$TARGET_DIST"
            bash scripts/linux/deb/generate_deb.sh "$PROJECT_NAME" "$TARGET_DIST"
            bash scripts/linux/arch/generate_arch.sh "$PROJECT_NAME" "$TARGET_DIST"
        fi
    fi

    printf -- "-> ¡Empaquetado de %s completado\n" "$TRIPLET"
done

printf "\nProceso finalizado. Todo listo en %s/\n" "$OUTPUT_BASE"