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
