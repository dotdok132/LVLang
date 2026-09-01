#!/usr/bin/env bash
# ==============================================================================
# Official One-Line Installer for LVLang Engine & Ecosystem
# Usage: bash install.sh   OR   curl -sSL https://raw.githubusercontent.com/dotdok132/LVLang/main/install.sh | bash
# ==============================================================================

set -e

GREEN='\033[32m'
CYAN='\033[36m'
YELLOW='\033[33m'
RED='\033[31m'
NC='\033[0m'

echo -e "${CYAN}=== 🚀 Installing LVLang AI-Native Bytecode Engine & Ecosystem ===${NC}"

# Detect OS & Package Manager
if [ -f /etc/os-release ]; then
    . /etc/os-release
    OS=$NAME
fi

echo -e "${YELLOW}[1/4] Checking & Installing system dependencies...${NC}"

if command -v apt-get &> /dev/null; then
    sudo apt-get update -qq || true
    sudo apt-get install -y -qq gcc git make libsdl2-dev libssl-dev || true
elif command -v pacman &> /dev/null; then
    sudo pacman -Sy --noconfirm gcc git make sdl2 openssl || true
elif command -v dnf &> /dev/null; then
    sudo dnf install -y gcc git make SDL2-devel openssl-devel || true
fi

# Target Directory
PARENT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo -e "${YELLOW}[2/4] Cloning satellite FFI plugin repositories into '${PARENT_DIR}'...${NC}"

PLUGINS=(
    "lvlang-system"
    "lvlang-crypto"
    "lvlang-keyboard"
    "lvlang-sdl2"
    "lvlang-time"
    "lvlang-string"
    "lvlang-ansi"
    "lvlang-heap"
    "lvlang-net"
)

for plugin in "${PLUGINS[@]}"; do
    TARGET_PATH="${PARENT_DIR}/${plugin}"
    if [ ! -d "$TARGET_PATH" ]; then
        echo -e "  [+] Cloning ${plugin}..."
        git clone --quiet "https://github.com/dotdok132/${plugin}.git" "$TARGET_PATH" || echo -e "  [!] Warning: Failed to clone ${plugin}"
    else
        echo -e "  [✓] ${plugin} already exists."
    fi
done

echo -e "${YELLOW}[3/4] Building LVLang C99 Engine ('lvlc')...${NC}"
CDIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$CDIR"

gcc -O3 -Wall lvlc.c -o lvlc -lSDL2 -lm

echo -e "${YELLOW}[4/4] Installing binary...${NC}"

if [ -w "/usr/local/bin" ]; then
    cp lvlc /usr/local/bin/lvlc
    echo -e "${GREEN}[✓] Installed binary to /usr/local/bin/lvlc${NC}"
elif command -v sudo &> /dev/null; then
    sudo cp lvlc /usr/local/bin/lvlc || true
    echo -e "${GREEN}[✓] Installed binary to /usr/local/bin/lvlc (via sudo)${NC}"
else
    mkdir -p "$HOME/.local/bin"
    cp lvlc "$HOME/.local/bin/lvlc"
    echo -e "${GREEN}[✓] Installed binary to $HOME/.local/bin/lvlc${NC}"
fi

echo -e "${GREEN}=== 🎉 LVLang Successfully Installed! ===${NC}"
echo -e "Run '${CYAN}lvlc --help${NC}' or '${CYAN}lvlc --json "01x8F 05xFF"${NC}' to test execution."
