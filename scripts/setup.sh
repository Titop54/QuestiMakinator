#!/usr/bin/env bash

cd .. || exit -1

tools=("git" "python3" "clang" "clang++" "make" "cmake")
no_win=false
only_win=false
dynamic=false
build_all=false
clean=false
release_only=false
use_cache=true
cache_dir="$HOME/.vcpkg_global_cache"

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --clean)
            clean=true
            shift
            ;;
        --windows|--win|--window)
            only_win=true
            shift
            ;;
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
        --release|--release-only)
            release_only=true
            shift
            ;;
        --cache-dir)
            cache_dir="$2"
            shift 2
            ;;
        --no-cache)
            use_cache=false
            shift
            ;;
        --help|-h)
            printf "Usage: %s [OPTIONS]\n\n" "$0"
            printf "Options:\n"
            printf "  --all                  Build all 4 configurations (Linux & Windows, Static & Dynamic).\n"
            printf "                         Note: This overrides any Windows-skipping flags.\n"
            printf "  --windows, --win       Skip checking tools and building Linux libraries, build ONLY Windows.\n"
            printf "  --no_win               Skip checking tools and building Windows (MinGW) libraries.\n"
            printf "                         Aliases: --no, --no_windows, --no-windows, --no_window, --no-window\n"
            printf "  --dynamic              Build dynamic libraries instead of the default static ones.\n"
            printf "  --release              Build ONLY Release versions\n"
            printf "  --cache-dir <path>     Directory for binary caching (default: ~/.vcpkg_global_cache).\n"
            printf "  --no-cache             Disable vcpkg binary caching.\n"
            printf "  --clean                Clean build artifacts before setting up.\n"
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

if [ "$clean" = true ]; then
    printf "[!] Cleaning up previous installation artifacts...\n"
    rm -rf build vcpkg vcpkg_installed
    rm -f vcpkg.json vcpkg-configuration.json
    printf "[+] Cleanup complete. Proceeding with a fresh setup...\n\n"
fi

if [ "$build_all" = true ]; then
    if [ "$no_win" = true ] || [ "$only_win" = true ]; then
        printf "Warning: '--all' overrides '--no_win' and '--windows'. All tools will be required and built.\n\n"
    fi
    no_win=false
    only_win=false
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

    SYS_GXX="/usr/bin/x86_64-w64-mingw32-g++"
    SYS_GCC="/usr/bin/x86_64-w64-mingw32-gcc"

    if [ -f "$SYS_GXX" ] || [ -f "$SYS_GCC" ]; then
        DETECTED_COMPILER="${SYS_GXX}"
        [ ! -f "$SYS_GXX" ] && DETECTED_COMPILER="${SYS_GCC}"
        
        printf "\n[!] Traditional mingw32-g++/gcc compiler detected at: %s\n" "${DETECTED_COMPILER}"
        printf "[!] To prevent ABI mismatches and ensure everything is compiled cleanly using llvm-mingw,\n"
        printf "[!] please uninstall it first before proceeding.\n\n"
        printf "    Run the following command to remove it:\n"
        printf "    sudo apt purge g++-mingw-w64-x86-64 gcc-mingw-w64-x86-64 && sudo apt autoremove\n\n"
        exit 255
    fi

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

if [ "$use_cache" = true ]; then
    mkdir -p "$cache_dir"
    export VCPKG_BINARY_SOURCES="clear;files,$cache_dir,readwrite"
    printf "[+] Binary caching enabled at: %s\n" "$cache_dir"
fi

if [ "$release_only" = true ]; then
    export VCPKG_BUILD_TYPE=release
    printf "[+] Building ONLY Release packages.\n"
fi

LIBS="imgui-sfml imgui[glfw-binding,opengl3-binding] nlohmann-json libwebp tinyobjloader backward-cpp glfw3 glad glm tinyfiledialogs lodepng imgui-node-editor zlib"
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

    if [ "$only_win" = false ]; then
        printf "\nInstalling %s libraries\n" "$lin_root"
        vcpkg install --triplet "$lin_trip" --host-triplet="$lin_trip" --x-install-root="./vcpkg_installed/$lin_root"
    fi
fi

printf "(FTL OR GPL-2.0-or-later) -> Choosen FTL license\n"
printf "(MIT OR CC-PDDC) -> Choosen MIT license\n"
printf "(Unlicense OR MIT-0) -> Choosen MIT-0 license\n"
printf "In case of license change, it will always be the less restrictive\n"

cd scripts/ || exit 1

python3 licenses.py --no $LIBS