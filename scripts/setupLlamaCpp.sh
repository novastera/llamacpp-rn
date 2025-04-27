#!/bin/bash
# This script sets up the llama.cpp integration
# It can either use prebuilt binaries or build from source

set -e

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

# Ensure the llama.cpp version script is sourced
source "$SCRIPT_DIR/llama_cpp_version.sh"

# Function to check if a command exists
command_exists() {
  command -v "$1" &> /dev/null
}

# Check if we're being run without arguments and print usage if so
if [ "$#" -eq 0 ]; then
  echo "Usage: $0 {init|status|list-tags}"
  echo "  init       - Initialize and set the llama.cpp repository to the correct version"
  echo "  status     - Show current llama.cpp repository status"
  echo "  list-tags  - List available llama.cpp release tags"
  exit 1
fi

# Check for necessary tools
echo -e "${YELLOW}Checking for required tools...${NC}"
if ! command_exists git; then
  echo -e "${RED}Error: git is not installed. Please install git and try again.${NC}"
  exit 1
fi

# Set permissions for the scripts
echo -e "${YELLOW}Setting executable permissions for scripts...${NC}"
chmod +x "$SCRIPT_DIR/download_prebuilt.sh"
chmod +x "$SCRIPT_DIR/llama_cpp_version.sh"

# Ask user if they want to build from source
if [ "${LLAMACPPRN_BUILD_FROM_SOURCE}" = "true" ] || [ "$1" == "--source" ]; then
  BUILD_FROM_SOURCE=true
elif [ "$1" == "--prebuilt" ]; then
  BUILD_FROM_SOURCE=false
else
  # Default behavior: try prebuilt first, fall back to source if prebuilt fails
  BUILD_FROM_SOURCE=false
fi

if [ "$BUILD_FROM_SOURCE" = true ]; then
  echo -e "${YELLOW}Setting up llama.cpp from source...${NC}"
  "$SCRIPT_DIR/llama_cpp_version.sh" init
  "$SCRIPT_DIR/llama_cpp_version.sh" status
  
  # Simply ensure the llama.cpp repository is set up for building by Android
  echo -e "${GREEN}Setup completed - will build from source${NC}"
else
  echo -e "${YELLOW}Setting up llama.cpp using prebuilt binaries...${NC}"
  
  # Try to download prebuilt libraries using the 'init' command
  "$SCRIPT_DIR/download_prebuilt.sh" init
  
  echo -e "${YELLOW}Setting up iOS XCFramework...${NC}"
  
  # Note: We're expecting Android to be built from source anyway,
  # but we want to use prebuilt iOS framework if available
  if [ ! -d "ios/libs/llamacpp.xcframework" ]; then
    echo -e "${YELLOW}iOS prebuilt binaries not found or download failed.${NC}"
    echo -e "${YELLOW}Initializing llama.cpp repository for Android build...${NC}"
    
    # Initialize llama.cpp repository for Android
    "$SCRIPT_DIR/llama_cpp_version.sh" init
    "$SCRIPT_DIR/llama_cpp_version.sh" status
  else
    echo -e "${GREEN}iOS prebuilt framework found.${NC}"
    
    # Make sure that includes directory exists and has the necessary headers
    mkdir -p "ios/includes"
    mkdir -p "ios/framework/build-apple"
    
    # Copy framework to the framework directory as well (for backward compatibility)
    if [ ! -d "ios/framework/build-apple/llama.xcframework" ] && [ -d "ios/libs/llamacpp.xcframework" ]; then
      echo -e "${YELLOW}Copying framework to ios/framework/build-apple/...${NC}"
      mkdir -p "ios/framework/build-apple"
      cp -R "ios/libs/llamacpp.xcframework" "ios/framework/build-apple/llama.xcframework"
    fi
    
    echo -e "${YELLOW}Setting up iOS include files...${NC}"
    if [ -d "ios/libs/llamacpp.xcframework/ios-arm64/Headers" ]; then
      cp -R "ios/libs/llamacpp.xcframework/ios-arm64/Headers/"* "ios/includes/"
    elif [ -d "ios/libs/llamacpp.xcframework/ios-arm64/llamacpp.framework/Headers" ]; then
      cp -R "ios/libs/llamacpp.xcframework/ios-arm64/llamacpp.framework/Headers/"* "ios/includes/"
    fi
    
    # Always initialize llama.cpp repo for Android build
    if [ ! -d "cpp/llama.cpp/.git" ]; then
      echo -e "${YELLOW}Initializing llama.cpp repository for Android build...${NC}"
      "$SCRIPT_DIR/llama_cpp_version.sh" init
      "$SCRIPT_DIR/llama_cpp_version.sh" status
    fi
  fi
  
  echo -e "${GREEN}Setup completed successfully${NC}"
fi
