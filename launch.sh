#!/usr/bin/env bash

# Configuración
PROJECT_NAME="QuestiMakinator"
VCPKG_ROOT_DIR="$(pwd)/vcpkg/"
BUILD_TYPE="Release"
NO_CONFIG=false
DYNAMIC=false
OS_TARGET="linux"
TIME="OFF"
BUILD_ALL=false
UNITY_FLAG="OFF"

# Procesar argumentos
while [[ $# -gt 0 ]]; do
    case $1 in
        --all)
            BUILD_ALL=true
            shift
            ;;
        --windows | --window)
            OS_TARGET="windows"
            shift
            ;;
        --linux)
            OS_TARGET="linux"
            shift
            ;;
        --no_config | --no | --no-config)
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
        --dynamic)
            DYNAMIC=true
            shift
            ;;
        --time)
            TIME="ON"
            shift
            ;;
        --unity)
            UNITY_FLAG="ON"
            shift
            ;;
        --help | -h)
            printf "Usage: %s [OPTIONS]\n\n" "$0"
            printf "Options:\n"
            printf "  --all                         Build ALL targets (Windows/Linux, Static/Dynamic).\n"
            printf "  --windows, --window           Build for Windows target.\n"
            printf "  --linux                       Build for Linux target (Default).\n"
            printf "  --no_config                   Skip configuration step.\n"
            printf "                                Aliases: --no, --no-config\n"
            printf "  --debug                       Set build type to Debug.\n"
            printf "  --release                     Set build type to Release (Default).\n"
            printf "  --dynamic                     Build dynamic libraries (Default is static).\n"
            printf "  --time                        Enable time-trace for compilation profiling.\n"
            printf "  --unity                       Enable big compilation of files.\n"
            printf "  -h, --help                    Show this help message and exit.\n"
            exit 0
            ;;
        *)
            printf "Argument not valid: %s\n" "$1"
            printf "Use --help to see the list of valid options.\n"
            exit 1
            ;;
    esac
done

TARGET_TRIPLETS=()
if [ "$BUILD_ALL" = true ]; then
    TARGET_TRIPLETS=("x64-linux" "x64-linux-dynamic" "x64-mingw-static" "x64-mingw-dynamic")
else
    if [ "$OS_TARGET" = "windows" ]; then
        if [ "$DYNAMIC" = true ]; then
            TRIPLET="x64-mingw-dynamic"
        else
            TRIPLET="x64-mingw-static"
        fi
    else
        if [ "$DYNAMIC" = true ]; then
            TRIPLET="x64-linux-dynamic"
        else
            TRIPLET="x64-linux"
        fi
    fi
    TARGET_TRIPLETS=("$TRIPLET")
fi

PROJECT_ROOT="$(pwd)"

if command -v ccache &> /dev/null; then
    export CCACHE_DEPEND=1
    export CCACHE_NOCOMPRESS=true
    export CCACHE_MAXSIZE=10G
else
    printf "There is no ccache installed, recommended to install it\n"
fi

for CURRENT_TRIPLET in "${TARGET_TRIPLETS[@]}"; do
    printf "Building: %s (Config: %s)\n" "$CURRENT_TRIPLET" "$BUILD_TYPE"

    CMAKE_DYNAMIC_FLAG="OFF"
    [[ "$CURRENT_TRIPLET" == *"dynamic"* ]] && CMAKE_DYNAMIC_FLAG="ON"

    mkdir -p "build/${CURRENT_TRIPLET}"
    
    cd "build/${CURRENT_TRIPLET}" || exit
    export VCPKG_ROOT="$VCPKG_ROOT_DIR"
    export PATH=$VCPKG_ROOT:$PATH

    if [ "$NO_CONFIG" = false ]; then
        if [[ "$CURRENT_TRIPLET" == "x64-mingw"* ]]; then
            cmake ../../ \
                -DCMAKE_UNITY_BUILD=${UNITY_FLAG} \
                -DUSE_TIME_TRACE=${TIME} \
                -DDYNAMIC_BUILD=${CMAKE_DYNAMIC_FLAG} \
                -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
                -DVCPKG_TARGET_TRIPLET=${CURRENT_TRIPLET} \
                -DVCPKG_INSTALLED_DIR="${PROJECT_ROOT}/vcpkg_installed/${CURRENT_TRIPLET}" \
                -DVCPKG_HOST_TRIPLET=${CURRENT_TRIPLET} \
                -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                -DCMAKE_SYSTEM_NAME="Windows" \
                -DCMAKE_C_COMPILER=x86_64-w64-mingw32-clang \
                -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-clang++ \
                -DCMAKE_RC_COMPILER=x86_64-w64-mingw32-windres \
                -G Ninja
        else
            # Linux
            cmake ../../ \
                -DCMAKE_UNITY_BUILD=${UNITY_FLAG} \
                -DUSE_TIME_TRACE=${TIME} \
                -DDYNAMIC_BUILD=${CMAKE_DYNAMIC_FLAG} \
                -DCMAKE_TOOLCHAIN_FILE=${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake \
                -DVCPKG_INSTALLED_DIR="${PROJECT_ROOT}/vcpkg_installed/${CURRENT_TRIPLET}" \
                -DVCPKG_HOST_TRIPLET=${CURRENT_TRIPLET} \
                -DVCPKG_TARGET_TRIPLET=${CURRENT_TRIPLET} \
                -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                -DCMAKE_C_COMPILER=clang \
                -DCMAKE_CXX_COMPILER=clang++ \
                -G Ninja
        fi
    fi

    # Compilar
    printf "\nCompiling...\n"
    cmake --build . --config ${BUILD_TYPE} --parallel

    cd "$PROJECT_ROOT" || exit

    EXECUTABLE_PATH=""
    POSSIBLE_PATH_1="${PROJECT_ROOT}/build/${CURRENT_TRIPLET}/${PROJECT_NAME}"
    POSSIBLE_PATH_2="${PROJECT_ROOT}/build/${CURRENT_TRIPLET}/${PROJECT_NAME}.exe"
    POSSIBLE_PATH_3="${PROJECT_ROOT}/build/${PROJECT_NAME}"
    POSSIBLE_PATH_4="${PROJECT_ROOT}/build/${PROJECT_NAME}.exe"

    if [ -f "$POSSIBLE_PATH_1" ]; then
        EXECUTABLE_PATH="$POSSIBLE_PATH_1"
    elif [ -f "$POSSIBLE_PATH_2" ]; then
        EXECUTABLE_PATH="$POSSIBLE_PATH_2"
    elif [ -f "$POSSIBLE_PATH_3" ]; then
        EXECUTABLE_PATH="$POSSIBLE_PATH_3"
    elif [ -f "$POSSIBLE_PATH_4" ]; then
        EXECUTABLE_PATH="$POSSIBLE_PATH_4"
    fi

    if [ -n "$EXECUTABLE_PATH" ]; then
        printf "  -> Executable successfully found at: %s\n" "$EXECUTABLE_PATH"
    else
        printf "\n  [!] Error: Could not find the executable for '%s'.\n" "$PROJECT_NAME"
        printf "  Searched in the following locations:\n"
        printf "    - %s\n" "$POSSIBLE_PATH_1"
        printf "    - %s\n" "$POSSIBLE_PATH_2"
        printf "    - %s\n" "$POSSIBLE_PATH_3"
        printf "    - %s\n" "$POSSIBLE_PATH_4"
        printf "\n  Please ensure the compilation finished successfully.\n"
    fi
done

printf "\n[+] Build process finished!\n"