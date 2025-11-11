#!/bin/bash
#set -e

git clone https://github.com/microsoft/vcpkg.git

cd vcpkg
./bootstrap-vcpkg.sh -disableMetrics

# Añadir los puertos necesarios
cd ..
export VCPKG_ROOT="$(pwd)/vcpkg"
export PATH=$VCPKG_ROOT:$PATH
vcpkg new --application
vcpkg add port imgui-sfml sfml

echo -e "\nInstalando para x64-mingw-static"
vcpkg install --triplet x64-mingw-static --host-triplet=x64-mingw-static --x-install-root=./vcpkg_installed/x64-mingw-static

echo -e "\nInstalando para x64-linux"
vcpkg install --triplet x64-linux --host-triplet=x64-linux --x-install-root=./vcpkg_installed/x64-linux

python3 licenses.py --no_print
vcpkg license-report
prinft "You can get all licenses on vcpkg_installed/licenses/licenses.csv"

printf "(FTL OR GPL-2.0-or-later) -> Choosen FTL license\n"
printf "(MIT OR CC-PDDC) -> Choosen MIT license\n"
printf "(Unlicense OR MIT-0) -> Choosen MIT-0 license\n"

