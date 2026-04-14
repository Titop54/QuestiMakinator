#!/usr/bin/env bash

tools=("git" "python3" "clang" "clang++" "make" "cmake")
no_win=false
dynamic=false
build_all=false

#sudo apt install libfuse2
#wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
#chmod +x appimagetool-x86_64.AppImage
#sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --no_win|--no|--no_windows|--no-windows|--no_window|--no-window)
            no_win=true
            shift
            ;;
        --dynamic)
            dynamic=true
            shift
            ;;
        --all)
            build_all=true
            shift
            ;;
        --help|-h)
            printf "Usage: %s [OPTIONS]\n\n" "$0"
            printf "Options:\n"
            printf "  --all                  Build all 4 configurations (Linux & Windows, Static & Dynamic).\n"
            printf "                         Note: This overrides any Windows-skipping flags.\n"
            printf "  --no_win               Skip checking tools and building Windows (MinGW) libraries.\n"
            printf "                         Aliases: --no, --no_windows, --no-windows, --no_window, --no-window\n"
            printf "  --dynamic              Build dynamic libraries instead of the default static ones.\n"
            printf "  -h, --help             Show this help message and exit.\n"
            exit 0
            ;;
        *)
            printf "Unknown option: %s\n" "$1"
            printf "Use --help to see the list of valid options.\n"
            exit 1
            ;;
    esac
done

if [ "$build_all" = true ]; then
    if [ "$no_win" = true ]; then
        printf "Warning: '--all' overrides '--no_win'. Windows tools will be required and built.\n\n"
    fi
    no_win=false
fi

printf "Checking for required tools...\n"
for tool in "${tools[@]}"; do
    if ! command -v "$tool" &> /dev/null; then
        printf "Error: %s is not installed. Please install it using your package manager or from the official source.\n" "$tool"
        printf "This can be done using the command line or the graphical store or similar\n"
        printf "For example, %s can be done with sudo apt install %s on Debian or Ubuntu\n" "$tool" "$tool"
        exit 1
    fi
done

if [ "$no_win" = false ]; then
    mingw_tools=("x86_64-w64-mingw32-clang++" "x86_64-w64-mingw32-clang" "x86_64-w64-mingw32-windres")
    for tool in "${mingw_tools[@]}"; do
        if ! command -v "$tool" &> /dev/null; then
            printf "Error: %s is not found in PATH.\n" "$tool"
            printf "Please install llvm-mingw and ensure its 'bin' directory is in your PATH.\n"
            printf "Go to the llvm-mingw page (https://github.com/mstorsjo/llvm-mingw/releases), download the best for your OS \n and extract it somewhere (for example on /usr/bin/llvm-mingw)\n"
            exit 1
        fi
    done
fi

printf "All required tools are installed. Proceeding with vcpkg setup, this might take some time...\n"

git clone https://github.com/microsoft/vcpkg.git --depth=1

cd vcpkg || exit
./bootstrap-vcpkg.sh -disableMetrics

cd .. || exit
ROOT="$(pwd)/vcpkg"
export VCPKG_ROOT=$ROOT
export PATH=$VCPKG_ROOT:$PATH
export VCPKG_MAX_CONCURRENCY=$(nproc)
export VCPKG_DISABLE_METRICS=1

LIBS="imgui-sfml imgui[glfw-binding,opengl3-binding] nlohmann-json libwebp tinyobjloader backward-cpp glfw3"
vcpkg new --application
vcpkg add port $LIBS

if [ "$build_all" = true ]; then
    printf "\nBuilding all configurations, this may take some time\n"
    
    printf "\n[1/4] Installing x64-mingw-dynamic libraries\n"
    vcpkg install --triplet x64-mingw-dynamic --host-triplet=x64-mingw-dynamic --x-install-root=./vcpkg_installed/x64-mingw-dynamic
    
    printf "\n[2/4] Installing x64-mingw-static libraries\n"
    vcpkg install --triplet x64-mingw-static --host-triplet=x64-mingw-static --x-install-root=./vcpkg_installed/x64-mingw-static

    printf "\n[3/4] Installing x64-linux-dynamic libraries\n"
    vcpkg install --triplet x64-linux-dynamic --host-triplet=x64-linux-dynamic --x-install-root=./vcpkg_installed/x64-linux-dynamic
    
    printf "\n[4/4] Installing x64-linux-static libraries\n"
    vcpkg install --triplet x64-linux --host-triplet=x64-linux --x-install-root=./vcpkg_installed/x64-linux-static

else
    if [ "$dynamic" = true ]; then
        win_trip="x64-mingw-dynamic"
        lin_trip="x64-linux-dynamic"
        lin_root="x64-linux-dynamic"
    else
        win_trip="x64-mingw-static"
        lin_trip="x64-linux"
        lin_root="x64-linux-static"
    fi

    if [ "$no_win" = false ]; then
        printf "\nInstalling %s libraries\n" "$win_trip"
        vcpkg install --triplet "$win_trip" --host-triplet="$win_trip" --x-install-root="./vcpkg_installed/$win_trip"
    fi

    printf "\nInstalling %s libraries\n" "$lin_root"
    vcpkg install --triplet "$lin_trip" --host-triplet="$lin_trip" --x-install-root="./vcpkg_installed/$lin_root"
fi

python3 licenses.py --no_print $LIBS

printf "(FTL OR GPL-2.0-or-later) -> Choosen FTL license\n"
printf "(MIT OR CC-PDDC) -> Choosen MIT license\n"
printf "(Unlicense OR MIT-0) -> Choosen MIT-0 license\n"
printf "In case of license change, it will always be the less restrictive\n"