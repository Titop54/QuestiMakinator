#!/bin/bash

tools=("git" "python3" "clang" "clang++" "make" "cmake" "appimagetool")
no_win=false
dynamic=false

#sudo apt install libfuse2
#wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
#chmod +x appimagetool-x86_64.AppImage
#sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --no_win)
            no_win=true
            shift
            ;;
        --dynamic)
            dynamic=true
            shift
            ;;
        *)
            printf "Unknown option: %s\n" "$1"
            exit 1
            ;;
    esac
done

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
    mingw_tools=("x86_64-w64-mingw32-clang++" "x86_64-w64-mingw32-clang" "x86_64-w64-mingw32-windres" "nsis")
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

git clone https://github.com/microsoft/vcpkg.git

cd vcpkg || exit
./bootstrap-vcpkg.sh -disableMetrics

cd .. || exit
ROOT="$(pwd)/vcpkg"
export VCPKG_ROOT=$ROOT
export PATH=$VCPKG_ROOT:$PATH

vcpkg new --application
vcpkg add port imgui-sfml imgui[glfw-binding,opengl3-binding] nlohmann-json libwebp tinyobjloader backward-cpp glfw3[wayland]

if [ "$no_win" = false ]; then
    if [ "$dynamic" = true ]; then
        printf "\nInstalling x64-mingw-dynamic libraries\n"
        vcpkg install --triplet x64-mingw-dynamic --host-triplet=x64-mingw-dynamic --x-install-root=./vcpkg_installed/x64-mingw-dynamic
    else
        printf "\nInstalling x64-mingw-static libraries\n"
        vcpkg install --triplet x64-mingw-static --host-triplet=x64-mingw-static --x-install-root=./vcpkg_installed/x64-mingw-static
    fi
fi

if [ "$dynamic" = true ]; then
    printf "\nInstalling x64-linux-dynamic libraries\n"
    vcpkg install --triplet x64-linux-dynamic --host-triplet=x64-linux-dynamic --x-install-root=./vcpkg_installed/x64-linux-dynamic
else
    printf "\nInstalling x64-linux-static libraries\n"
    vcpkg install --triplet x64-linux --host-triplet=x64-linux --x-install-root=./vcpkg_installed/x64-linux-static
fi

python3 licenses.py --no_print
vcpkg license-report
printf "You can get all licenses on vcpkg_installed/licenses/licenses.csv\n"

printf "(FTL OR GPL-2.0-or-later) -> Choosen FTL license\n"
printf "(MIT OR CC-PDDC) -> Choosen MIT license\n"
printf "(Unlicense OR MIT-0) -> Choosen MIT-0 license\n"
printf "In case of license change, it will always be the less restrictive\n"