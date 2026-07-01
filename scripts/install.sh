#!/usr/bin/env bash

set -e

cd .. || exit -1

printf "[+] Starting full dependency and toolchain installation\n\n"

printf "[+] Installing Linux system tools and libraries...\n"

sudo apt update
sudo apt install -y \
    git python3 make cmake ccache ninja-build clang \
    clang-tools-18 libfuse2t64 nsis wget curl zstd \
    libx11-dev libxrandr-dev libxcursor-dev libxi-dev \
    libgl1-mesa-dev libglu1-mesa-dev libudev-dev


#sudo apt install libfuse2
#wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage
#chmod +x appimagetool-x86_64.AppImage
#sudo mv appimagetool-x86_64.AppImage /usr/local/bin/appimagetool
printf "\n[+] Setting up appimagetool...\n"
if ! command -v appimagetool &> /dev/null; then
    sudo wget https://github.com/AppImage/AppImageKit/releases/download/continuous/appimagetool-x86_64.AppImage -O /usr/local/bin/appimagetool
    sudo chmod +x /usr/local/bin/appimagetool
    printf "  -> appimagetool installed successfully.\n"
else
    printf "  -> appimagetool already present.\n"
fi

INSTALL_DIR="/usr/bin/llvm-mingw"
ARCHIVE_NAME="llvm-mingw.tar.zst"
TMP_DIR=$(mktemp -d)

printf "\n[+] Fetching the latest version of llvm-mingw for cross-compilation...\n"
DOWNLOAD_URL=$(curl -s https://api.github.com/repos/mstorsjo/llvm-mingw/releases/latest \
  | grep "browser_download_url" \
  | grep "ubuntu" \
  | grep "x86_64" \
  | grep "ucrt" \
  | head -n 1 \
  | cut -d '"' -f 4)

if [ -z "$DOWNLOAD_URL" ]; then
    printf "[!] Error: Could not automatically resolve the llvm-mingw download URL.\n"
    exit 1
fi

printf "  -> Downloading from: %s\n" "$DOWNLOAD_URL"
curl -L "$DOWNLOAD_URL" -o "$TMP_DIR/$ARCHIVE_NAME"

printf "  -> Cleaning up older installations if present...\n"
sudo rm -rf "$INSTALL_DIR"
sudo mkdir -p "$INSTALL_DIR"

printf "  -> Extracting to %s...\n" "$INSTALL_DIR"
sudo tar --strip-components=1 -axf "$TMP_DIR/$ARCHIVE_NAME" -C "$INSTALL_DIR"

printf "\n[+] Configuring environment variables permanently...\n"
BASH_RC="$HOME/.bashrc"
EXPORT_CMD="export PATH=\"\$PATH:$INSTALL_DIR/bin\""

if ! grep -q "$INSTALL_DIR/bin" "$BASH_RC"; then
    printf "\n# llvm-mingw toolchain for cross-compilation\n%s\n" "$EXPORT_CMD" >> "$BASH_RC"
    printf "  -> Appended toolchain path to %s\n" "$BASH_RC"
else
    printf "  -> PATH structure is already present in %s\n" "$BASH_RC"
fi

rm -rf "$TMP_DIR"

rm -rf ~/.cache/vcpkg

printf "[+] Environment installation completed successfully!\n"
printf "[!] MANDATORY: Run the following command to update your shell session:\n"
printf "    source ~/.bashrc\n"

source ~/.bashrc

cd scripts/ || exit 1