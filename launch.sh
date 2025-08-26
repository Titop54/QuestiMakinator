#!/bin/bash

# Configuración
PROJECT_NAME="QuestiMakinator"
VCPKG_ROOT="vcpkg/"
TRIPLET=""
BUILD_TYPE="Release"
FORCE_TRIPLET=false

# Procesar argumentos
for arg in "$@"
do
    case $arg in
        --windows | --window)
        TRIPLET="x64-mingw-static"
        FORCE_TRIPLET=true
        shift
        ;;
        --linux)
        TRIPLET="x64-linux"
        FORCE_TRIPLET=true
        shift
        ;;
        *)
        echo "Argumento no reconocido: $arg"
        echo "Uso: $0 [--windows | --linux]"
        exit 1
        ;;
    esac
done

# Detectar SO si no se forzó un triplet
if [ "$FORCE_TRIPLET" = false ]; then
    case "$(uname -s)" in
        Linux*)
            TRIPLET="x64-linux"
            ;;
        MINGW*|MSYS*)
            TRIPLET="x64-mingw-static"
            ;;
        *)
            echo "Sistema operativo no soportado"
            exit 1
            ;;
    esac
fi

# Crear directorio de build
mkdir -p build/${TRIPLET}
cd build/${TRIPLET}
export VCPKG_ROOT=vcpkg/
export PATH=$VCPKG_ROOT:$PATH

# Configurar CMake
if [ "$TRIPLET" = "x64-mingw-static" ]; then
    export CC=x86_64-w64-mingw32-gcc
    export CXX=x86_64-w64-mingw32-g++
    
    # Configurar CMake con toolchain específico para MinGW
    cmake ../../ \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=${TRIPLET} \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DVCPKG_INSTALLED_DIR=$PWD/vcpkg_installed/${TRIPLET} \
        -DVCPKG_HOST_TRIPLET=${TRIPLET} \
        -DCMAKE_SYSTEM_NAME=Windows \
        -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc \
        -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ \
        -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres
else
    # Configuración para Linux
    cmake ../../ \
        -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
        -DVCPKG_TARGET_TRIPLET=${TRIPLET} \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DVCPKG_INSTALLED_DIR=$PWD/vcpkg_installed/${TRIPLET} \
        -DVCPKG_HOST_TRIPLET=${TRIPLET}
fi

# Compilar
echo "Compiling"
cmake --build . --config ${BUILD_TYPE}

# Buscar el ejecutable (puede estar en diferentes ubicaciones dependiendo del generador)
EXECUTABLE_PATH=""
if [ -f "${BUILD_TYPE}/${PROJECT_NAME}.exe" ]; then
    EXECUTABLE_PATH="${BUILD_TYPE}/${PROJECT_NAME}.exe"
elif [ -f "${PROJECT_NAME}.exe" ]; then
    EXECUTABLE_PATH="${PROJECT_NAME}.exe"
elif [ -f "${BUILD_TYPE}/${PROJECT_NAME}" ]; then
    EXECUTABLE_PATH="${BUILD_TYPE}/${PROJECT_NAME}"
elif [ -f "${PROJECT_NAME}" ]; then
    EXECUTABLE_PATH="${PROJECT_NAME}"
else
    echo "No se pudo encontrar el ejecutable"
    exit 1
fi

echo "Executable can be found at this path ${EXECUTABLE_PATH}"