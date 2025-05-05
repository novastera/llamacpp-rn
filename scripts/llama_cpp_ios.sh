#!/bin/bash
# llama_cpp_ios.sh - Downloads and sets up iOS framework from llama.cpp

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGE_DIR="$( cd "$SCRIPT_DIR/.." && pwd )"

# Import version settings
. "$SCRIPT_DIR/used_version.sh"

# iOS framework paths
IOS_FRAMEWORK_DIR="$PACKAGE_DIR/ios/framework/build-apple"
IOS_INCLUDE_DIR="$PACKAGE_DIR/ios/include"
TEMP_DIR="$PACKAGE_DIR/.llamacpp-temp"

# Function to download a file
download_file() {
  local url="$1"
  local output_path="$2"
  
  echo -e "${YELLOW}Downloading from: $url${NC}"
  echo -e "${YELLOW}Saving to: $output_path${NC}"
  
  if command -v curl &> /dev/null; then
    curl -L -o "$output_path" "$url"
    return $?
  elif command -v wget &> /dev/null; then
    wget -O "$output_path" "$url"
    return $?
  else
    echo -e "${RED}Error: Neither curl nor wget is available. Please install one of them.${NC}"
    return 1
  fi
}

# Download and setup iOS framework
download_ios_framework() {
  echo -e "${YELLOW}Downloading iOS framework (version: $LLAMA_CPP_TAG)...${NC}"
  
  # Create directories
  mkdir -p "$IOS_FRAMEWORK_DIR"
  mkdir -p "$IOS_INCLUDE_DIR"
  mkdir -p "$TEMP_DIR"
  
  # Check if framework already exists and is valid
  if [ -d "$IOS_FRAMEWORK_DIR/llama.xcframework" ] && 
     { [ -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64/llama.framework" ] || 
       [ -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64_x86_64-simulator/llama.framework" ]; }; then
    echo -e "${GREEN}iOS framework already exists at: $IOS_FRAMEWORK_DIR/llama.xcframework${NC}"
    echo -e "${YELLOW}Use '--force' to redownload or 'clean' to remove it${NC}"
    return 0
  fi
  
  # Download URL
  local url="https://github.com/ggml-org/llama.cpp/releases/download/$LLAMA_CPP_TAG/llama-$LLAMA_CPP_TAG-xcframework.zip"
  local temp_zip="$TEMP_DIR/ios_framework.zip"
  
  echo -e "${YELLOW}Using URL: $url${NC}"
  
  # Download the framework
  if ! download_file "$url" "$temp_zip"; then
    echo -e "${RED}Failed to download iOS framework${NC}"
    return 1
  fi
  
  # Remove any existing framework
  rm -rf "$IOS_FRAMEWORK_DIR/llama.xcframework"
  
  # Extract the framework
  echo -e "${YELLOW}Extracting framework...${NC}"
  unzip -q -o "$temp_zip" -d "$TEMP_DIR"
  
  # Find the extracted framework
  local xcframework_path=$(find "$TEMP_DIR" -name "*.xcframework" -type d | head -n 1)
  
  if [ -z "$xcframework_path" ]; then
    echo -e "${RED}Error: No .xcframework found in downloaded archive${NC}"
    rm -f "$temp_zip"
    return 1
  fi
  
  # Move it to the final location
  echo -e "${YELLOW}Moving framework to: $IOS_FRAMEWORK_DIR/llama.xcframework${NC}"
  mv "$xcframework_path" "$IOS_FRAMEWORK_DIR/llama.xcframework"
  
  # Verify the framework has the necessary slices
  if [[ ! -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64/llama.framework" && 
        ! -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64_x86_64-simulator/llama.framework" ]]; then
    echo -e "${RED}Error: iOS framework slices not found in downloaded archive${NC}"
    find "$IOS_FRAMEWORK_DIR/llama.xcframework" -type d | sort
    rm -f "$temp_zip"
    return 1
  fi
  
  # Copy header file to include directory
  echo -e "${YELLOW}Copying header files...${NC}"
  if [ -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64/llama.framework/Headers" ]; then
    cp -n "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64/llama.framework/Headers/llama-cpp.h" "$IOS_INCLUDE_DIR/" 2>/dev/null || true
  elif [ -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64_x86_64-simulator/llama.framework/Headers" ]; then
    cp -n "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64_x86_64-simulator/llama.framework/Headers/llama-cpp.h" "$IOS_INCLUDE_DIR/" 2>/dev/null || true
  fi
  
  # Clean up temporary files
  rm -f "$temp_zip"
  
  echo -e "${GREEN}iOS framework downloaded and installed successfully to:${NC}"
  echo -e "${GREEN}$IOS_FRAMEWORK_DIR/llama.xcframework${NC}"
  
  # List framework architectures
  echo -e "${YELLOW}Framework slices available:${NC}"
  find "$IOS_FRAMEWORK_DIR/llama.xcframework" -maxdepth 1 -type d -not -path "$IOS_FRAMEWORK_DIR/llama.xcframework" | sort
  
  return 0
}

# Check if iOS framework exists and is valid
check_ios_framework() {
  echo -e "${YELLOW}Checking iOS framework...${NC}"
  
  if [ ! -d "$IOS_FRAMEWORK_DIR/llama.xcframework" ]; then
    echo -e "${RED}iOS framework not found at: $IOS_FRAMEWORK_DIR/llama.xcframework${NC}"
    return 1
  fi
  
  if [[ ! -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64/llama.framework" && 
        ! -d "$IOS_FRAMEWORK_DIR/llama.xcframework/ios-arm64_x86_64-simulator/llama.framework" ]]; then
    echo -e "${RED}iOS framework exists but appears to be invalid${NC}"
    find "$IOS_FRAMEWORK_DIR/llama.xcframework" -maxdepth 2 -type d | sort
    return 1
  fi
  
  echo -e "${GREEN}iOS framework is valid at: $IOS_FRAMEWORK_DIR/llama.xcframework${NC}"
  echo -e "${YELLOW}Framework slices available:${NC}"
  find "$IOS_FRAMEWORK_DIR/llama.xcframework" -maxdepth 1 -type d -not -path "$IOS_FRAMEWORK_DIR/llama.xcframework" | sort
  
  return 0
}

# Clean up iOS framework
clean_ios_framework() {
  echo -e "${YELLOW}Cleaning iOS framework...${NC}"
  
  rm -rf "$IOS_FRAMEWORK_DIR/llama.xcframework"
  rm -rf "$TEMP_DIR"
  
  echo -e "${GREEN}iOS framework cleaned successfully${NC}"
  return 0
}

# Main function
main() {
  if [ $# -eq 0 ]; then
    echo "Usage: $0 {init|check|clean|version} [--force]"
    echo "  init     - Download and setup iOS framework"
    echo "  check    - Check if iOS framework exists and is valid"
    echo "  clean    - Remove iOS framework"
    echo "  version  - Show the llama.cpp version used"
    echo ""
    echo "Options:"
    echo "  --force  - Force redownload even if framework exists"
    exit 1
  fi
  
  local command="$1"
  shift
  
  case "$command" in
    "init")
      # Check for --force flag
      if [ "$1" = "--force" ]; then
        rm -rf "$IOS_FRAMEWORK_DIR/llama.xcframework"
      fi
      download_ios_framework
      ;;
    "check")
      check_ios_framework
      ;;
    "clean")
      clean_ios_framework
      ;;
    "version")
      echo "Using llama.cpp version: $LLAMA_CPP_TAG"
      ;;
    *)
      echo "Unknown command: $command"
      echo "Usage: $0 {init|check|clean|version} [--force]"
      exit 1
      ;;
  esac
}

# Run main with all arguments
main "$@" 