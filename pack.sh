#!/usr/bin/env bash

PROJECT_NAME="QuestiMakinator"
OUTPUT_BASE="build/output"

DO_WINDOWS=false
DO_LINUX=false
DO_STATIC=false
DO_DYNAMIC=false

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
        --all)
            DO_WINDOWS=true
            DO_LINUX=true
            DO_STATIC=true
            DO_DYNAMIC=true
            shift
            ;;
        --help | -h)
            printf "Usage: %s [OPTIONS]\n\n" "$0"
            printf "Options:\n"
            printf "  --all                 Package ALL configurations (Windows/Linux, Static/Dynamic).\n"
            printf "  --windows             Package for Windows target.\n"
            printf "                        Aliases: --window\n"
            printf "  --linux               Package for Linux target.\n"
            printf "  --static              Package static build.\n"
            printf "  --dynamic             Package dynamic build.\n"
            printf "  -h, --help            Show this help message and exit.\n\n"
            printf "Note: If no OS or build type is specified, the script defaults to ALL available configurations.\n"
            exit 0
            ;;
        *)
            printf "Argument not valid: %s\n" "$1"
            printf "Use --help to see the list of valid options.\n"
            exit 1
            ;;
    esac
done

# If no specific OS is selected, default to both
if [ "$DO_WINDOWS" = false ] && [ "$DO_LINUX" = false ]; then
    DO_WINDOWS=true
    DO_LINUX=true
fi

# If no specific build type is selected, default to both
if [ "$DO_STATIC" = false ] && [ "$DO_DYNAMIC" = false ]; then
    DO_STATIC=true
    DO_DYNAMIC=true
fi

printf "Checking for required tools...\n"
if [ "$DO_WINDOWS" = true ]; then
    if ! command -v makensis &> /dev/null && ! command -v nsis &> /dev/null; then
        printf "\nError: NSIS is not installed.\n"
        printf "Please install it (e.g., sudo apt install nsis) to package for Windows.\n"
        exit 1
    fi
fi

if [ "$DO_LINUX" = true ]; then
    if ! command -v appimagetool &> /dev/null; then
        printf "\nError: 'appimagetool' is not installed.\n"
        printf "To install it, please run the following commands:\n"
        printf "  sudo apt install libfuse2\n"
        printf "  wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage\n"
        printf "  chmod +x appimagetool-x86_64.AppImage\n"
        printf "  sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool\n"
        exit 1
    fi
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

if [ -f "vcpkg_installed/licenses/licenses.csv" ]; then
    cp "vcpkg_installed/licenses/licenses.csv" "$OUTPUT_BASE/THIRD_PARTY_LICENSES.csv"
    printf "License report added to the packages output.\n"
fi

for TRIPLET in "${TRIPLETS[@]}"; do
    printf " Packaging: %s\n" "$TRIPLET"

    TARGET_DIST="${OUTPUT_BASE}/${TRIPLET}"
    VCPKG_TRIPLET_DIR="vcpkg_installed/${TRIPLET}/${TRIPLET}"

    if [ ! -d "$VCPKG_TRIPLET_DIR" ]; then
        printf "  [!] Missing directory: %s\n" "$VCPKG_TRIPLET_DIR"
        printf "  Skipping %s, not compiled!\n\n" "$TRIPLET"
        continue
    fi

    rm -rf "$TARGET_DIST"
    mkdir -p "$TARGET_DIST"

    IS_WINDOWS=false
    [[ "$TRIPLET" == *"mingw"* ]] && IS_WINDOWS=true
    
    IS_DYNAMIC=false
    [[ "$TRIPLET" == *"dynamic"* ]] && IS_DYNAMIC=true

    BUILD_TYPE_NAME="static"
    [ "$IS_DYNAMIC" = true ] && BUILD_TYPE_NAME="dynamic"

    #Executable search
    EXECUTABLE_PATH=""
    PROJECT_ROOT="$(cd . && pwd)"

    POSSIBLE_PATH_1="${PROJECT_ROOT}/build/${TRIPLET}/${PROJECT_NAME}"
    POSSIBLE_PATH_2="${PROJECT_ROOT}/build/${TRIPLET}/${PROJECT_NAME}.exe"
    POSSIBLE_PATH_3="${PROJECT_ROOT}/build/${PROJECT_NAME}"
    POSSIBLE_PATH_4="${PROJECT_ROOT}/build/${PROJECT_NAME}.exe"

    if [ -f "$POSSIBLE_PATH_1" ]; then EXECUTABLE_PATH="$POSSIBLE_PATH_1";
    elif [ -f "$POSSIBLE_PATH_2" ]; then EXECUTABLE_PATH="$POSSIBLE_PATH_2";
    elif [ -f "$POSSIBLE_PATH_3" ]; then EXECUTABLE_PATH="$POSSIBLE_PATH_3";
    elif [ -f "$POSSIBLE_PATH_4" ]; then EXECUTABLE_PATH="$POSSIBLE_PATH_4"; fi

    if [ -n "$EXECUTABLE_PATH" ]; then
        printf "  -> Executable successfully found at: %s\n" "$EXECUTABLE_PATH"
    else
        printf "\n  [!] Error: Could not find the executable for '%s'.\n" "$PROJECT_NAME"
        printf "  Skipping %s because it hasn't been compiled!\n\n" "$TRIPLET"
        continue
    fi
    
    cp "$EXECUTABLE_PATH" "$TARGET_DIST/"
    printf "Executable copied successfully.\n"

    #Copying library
    if [ "$IS_DYNAMIC" = true ] && [ "$IS_WINDOWS" = true ]; then
        [ -d "${VCPKG_TRIPLET_DIR}/bin" ] && cp "${VCPKG_TRIPLET_DIR}/bin/"*.dll "$TARGET_DIST/" 2>/dev/null && printf "vcpkg libraries (DLLs) copied.\n"

        COMPILER_PATH=$(command -v x86_64-w64-mingw32-clang++)
        if [ -n "$COMPILER_PATH" ]; then
            printf "Looking for base compiler libraries...\n"
            LLVM_BASE=$(dirname "$(dirname "$COMPILER_PATH")")
            
            ARCH_PATH="*/x86_64-w64-mingw32/*"

            LIBCPP=$(find "$LLVM_BASE" -path "$ARCH_PATH" -name "libc++.dll" | head -n 1)
            LIBUNWIND=$(find "$LLVM_BASE" -path "$ARCH_PATH" -name "libunwind.dll" | head -n 1)
            LIBPTHREAD=$(find "$LLVM_BASE" -path "$ARCH_PATH" -name "libwinpthread-1.dll" | head -n 1)
            
            [ -n "$LIBCPP" ] && cp "$LIBCPP" "$TARGET_DIST/"
            [ -n "$LIBUNWIND" ] && cp "$LIBUNWIND" "$TARGET_DIST/"
            [ -n "$LIBPTHREAD" ] && cp "$LIBPTHREAD" "$TARGET_DIST/"
            
            printf "Compiler libraries (64-bit) copied.\n"
        fi

    elif [ "$IS_DYNAMIC" = true ] && [ "$IS_WINDOWS" = false ]; then
        mkdir -p "$TARGET_DIST/libs"
        [ -d "${VCPKG_TRIPLET_DIR}/lib" ] && cp -P "${VCPKG_TRIPLET_DIR}/lib/"*.so* "$TARGET_DIST/libs/" 2>/dev/null && printf "vcpkg libraries (.so) copied to libs/ directory.\n"
    fi

    printf "Running packaging scripts...\n"
    
    if [ "$IS_WINDOWS" = true ] && [ "$IS_DYNAMIC" = true ]; then
        bash scripts/windows/generate_nsis.sh "$PROJECT_NAME" "$TARGET_DIST" "$BUILD_TYPE_NAME"
    fi
    
    if [ "$IS_WINDOWS" = false ]; then
        bash scripts/linux/appimage/generate_appimage.sh "$PROJECT_NAME" "$TARGET_DIST" "$BUILD_TYPE_NAME"
        #bash scripts/linux/deb/generate_deb.sh "$PROJECT_NAME" "$TARGET_DIST" "$BUILD_TYPE_NAME"
        #bash scripts/linux/arch/generate_arch.sh "$PROJECT_NAME" "$TARGET_DIST" "$BUILD_TYPE_NAME"
    fi

    if [ "$IS_DYNAMIC" = true ]; then
        printf "Cleaning up intermediate files...\n"
        if [ "$IS_WINDOWS" = true ]; then
            rm -f "$TARGET_DIST"/*.dll
            rm -f "$TARGET_DIST/${PROJECT_NAME}.exe"
        else
            rm -rf "$TARGET_DIST/libs"
            rm -f "$TARGET_DIST/${PROJECT_NAME}"
        fi
        printf "Cleanup completed.\n"
    fi

    if [ "$IS_WINDOWS" = false ] && [ "$IS_DYNAMIC" = false ]; then
        rm -f "$TARGET_DIST/${PROJECT_NAME}" 
    fi


    printf " -> Packaging of %s completed!\n\n" "$TRIPLET"
done

printf "\nProcess finished. Everything is ready in %s/\n" "$OUTPUT_BASE"