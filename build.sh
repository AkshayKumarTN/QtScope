#!/bin/bash
# Build script for QtScope on macOS

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== QtScope Build Script ===${NC}"

# Find Qt installation
QT_PATH=""
if command -v brew &> /dev/null; then
    # Try Homebrew Qt
    QT_PATHS=(
        "/opt/homebrew/opt/qt@6"
        "/opt/homebrew/opt/qt"
        "/usr/local/opt/qt@6"
        "/usr/local/opt/qt"
    )
    
    for path in "${QT_PATHS[@]}"; do
        if [ -d "$path" ] && [ -f "$path/bin/qmake" ]; then
            QT_PATH="$path"
            break
        fi
    done
fi

if [ -z "$QT_PATH" ]; then
    echo -e "${RED}Error: Qt6 not found!${NC}"
    echo "Please install Qt6:"
    echo "  brew install qt@6"
    echo ""
    echo "Or set CMAKE_PREFIX_PATH manually:"
    echo "  cmake .. -DCMAKE_PREFIX_PATH=/path/to/qt"
    exit 1
fi

echo -e "${GREEN}Found Qt at: $QT_PATH${NC}"

# Export Qt path
export CMAKE_PREFIX_PATH="$QT_PATH"
export PATH="$QT_PATH/bin:$PATH"

# Check for CMake
if ! command -v cmake &> /dev/null; then
    echo -e "${RED}Error: CMake not found!${NC}"
    echo "Install with: brew install cmake"
    exit 1
fi

echo -e "${GREEN}CMake version: $(cmake --version | head -n1)${NC}"

# Clean previous build (optional)
if [ -d "build" ]; then
    read -p "Remove existing build directory? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo -e "${YELLOW}Removing build directory...${NC}"
        rm -rf build
    fi
fi

# Create build directory
mkdir -p build
cd build

# Configure
echo -e "${GREEN}Configuring with CMake...${NC}"
cmake .. -DCMAKE_PREFIX_PATH="$QT_PATH"

# Build
echo -e "${GREEN}Building project...${NC}"
cmake --build . -j$(sysctl -n hw.ncpu)

if [ $? -eq 0 ]; then
    echo -e "${GREEN}✓ Build successful!${NC}"
    echo ""
    echo "Run the application:"
    echo "  ./build/SimpleScope --ui"
    echo "  ./build/SimpleScope --cli"
    echo ""
    echo "Or from build directory:"
    echo "  cd build"
    echo "  ./SimpleScope --ui"
else
    echo -e "${RED}✗ Build failed!${NC}"
    exit 1
fi

