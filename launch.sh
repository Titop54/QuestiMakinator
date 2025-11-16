#!/bin/bash

# Configuración
PROJECT_NAME="QuestiMakinator"
VCPKG_ROOT="vcpkg/"
TRIPLET="x64-linux"
BUILD_TYPE="Release"
NO_CONFIG=false

# Procesar argumentos
while [[ $# -gt 0 ]]; do
    case $1 in
        --windows | --window)
            TRIPLET="x64-mingw-static"
            shift
            ;;
        --linux)
            TRIPLET="x64-linux"
            shift
            ;;
        --no_config)
            NO_CONFIG=true
            shift
            ;;
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --release)
            BUILD_TYPE="Release"
            shift
            ;;
        *)
            printf "Argument not valid: %s\n" "$1"
            printf "Valid arguments: [--windows | --linux] [--no_config] [--debug | --release]\n"
            exit 1
            ;;
    esac
done


mkdir -p build/${TRIPLET}
cd build/${TRIPLET} || exit
export VCPKG_ROOT=../../vcpkg/
export PATH=$VCPKG_ROOT:$PATH

if [ "$NO_CONFIG" = false ]; then
    if [ "$TRIPLET" = "x64-mingw-static" ]; then
        cmake ../../ \
            -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
            -DVCPKG_TARGET_TRIPLET=${TRIPLET} \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DVCPKG_INSTALLED_DIR="$PWD/../../vcpkg_installed/${TRIPLET}" \
            -DVCPKG_HOST_TRIPLET=${TRIPLET} \
            -DCMAKE_SYSTEM_NAME="Windows" \
            -DCMAKE_C_COMPILER=x86_64-w64-mingw32-clang \
            -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-clang++ \
            -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
    else
        # Linux
        cmake ../../ \
            -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
            -DVCPKG_TARGET_TRIPLET=${TRIPLET} \
            -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DVCPKG_INSTALLED_DIR="$PWD/../../vcpkg_installed/${TRIPLET}" \
            -DVCPKG_HOST_TRIPLET=${TRIPLET} \
            -DCMAKE_C_COMPILER=clang \
            -DCMAKE_CXX_COMPILER=clang++
    fi
fi

# Compilar
printf "Compiling\n"
cmake --build . --config ${BUILD_TYPE}

# Buscar el ejecutable en las ubicaciones especificadas
EXECUTABLE_PATH=""
PROJECT_ROOT="$(cd ../.. && pwd)"  # Directorio raíz del proyecto

# Verificar en las tres ubicaciones posibles
if [ -f "${PROJECT_ROOT}/build" ]; then
    EXECUTABLE_PATH="${PROJECT_ROOT}/build"
elif [ -f "${PROJECT_ROOT}/build/${TRIPLET}/${PROJECT_NAME}" ]; then
    EXECUTABLE_PATH="${PROJECT_ROOT}/build/${TRIPLET}/${PROJECT_NAME}"
elif [ -f "${PROJECT_ROOT}/build/${TRIPLET}/${PROJECT_NAME}.exe" ]; then
    EXECUTABLE_PATH="${PROJECT_ROOT}/build/${TRIPLET}/${PROJECT_NAME}.exe"
fi

if [ -n "$EXECUTABLE_PATH" ]; then
    printf "Executable can be found at this path: %s\n" "$EXECUTABLE_PATH"
else
    printf "No se pudo encontrar el ejecutable\n"
    printf "Se buscó en:\n"
    printf "  %s/build\n" "$PROJECT_ROOT"
    printf "  %s/build/%s/%s\n" "$PROJECT_ROOT" "$TRIPLET" "$PROJECT_NAME"
    printf "  %s/build/%s/%s.exe\n" "$PROJECT_ROOT" "$TRIPLET" "$PROJECT_NAME"
fi