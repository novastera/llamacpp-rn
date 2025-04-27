#!/bin/bash
# This script downloads prebuilt llama.cpp binaries for iOS and Android

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get the version and hash from the llama.cpp version script
source $(dirname "$0")/llama_cpp_version.sh
LLAMA_CPP_TAG=$(echo $LLAMA_CPP_COMMIT | cut -c 1-8)
LLAMA_CPP_HASH=$LLAMA_CPP_COMMIT

# Directories for iOS
IOS_DIR="ios/libs"
IOS_INCLUDE_DIR="ios/includes"
IOS_FRAMEWORK_DIR="ios/framework/build-apple"

# Directories for Android
ANDROID_DIR="android/libs"
ANDROID_INCLUDE_DIR="android/src/main/cpp/includes"
ANDROID_JNILIB_DIR="android/src/main/jniLibs"

# Function to download a file
download_file() {
  local url="$1"
  local output_path="$2"
  
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

# Download and extract iOS XCFramework
download_ios_framework() {
  echo -e "${YELLOW}Setting up iOS XCFramework...${NC}"
  
  # Create directories if they don't exist
  mkdir -p "$IOS_DIR"
  mkdir -p "$IOS_INCLUDE_DIR"
  mkdir -p "$IOS_FRAMEWORK_DIR"
  
  # URL of the prebuilt framework
  local url="https://github.com/novastera-oss/llama.cpp-builds/releases/download/llamacpp-rn-$LLAMA_CPP_TAG/llamacpp-ios-xcframework.zip"
  
  # Check if URL exists
  if command -v curl &> /dev/null; then
    curl -Is "$url" | head -1 | grep "200" > /dev/null
    local url_exists=$?
  elif command -v wget &> /dev/null; then
    wget --spider --quiet "$url"
    local url_exists=$?
  else
    echo -e "${RED}Error: Neither curl nor wget is available. Please install one of them.${NC}"
    return 1
  fi
  
  if [ $url_exists -ne 0 ]; then
    echo -e "${RED}Error: The URL $url does not exist.${NC}"
    echo -e "${YELLOW}Trying fallback URL...${NC}"
    url="https://github.com/novastera-oss/llama.cpp-builds/releases/download/llamacpp-rn-latest/llamacpp-ios-xcframework.zip"
    
    if command -v curl &> /dev/null; then
      curl -Is "$url" | head -1 | grep "200" > /dev/null
      url_exists=$?
    elif command -v wget &> /dev/null; then
      wget --spider --quiet "$url"
      url_exists=$?
    fi
    
    if [ $url_exists -ne 0 ]; then
      echo -e "${RED}Error: The fallback URL does not exist either. Will need to build from source.${NC}"
      return 1
    fi
  fi
  
  echo -e "${YELLOW}Downloading iOS XCFramework from $url...${NC}"
  
  # Download the framework
  local temp_zip="$IOS_DIR/temp_framework.zip"
  if ! download_file "$url" "$temp_zip"; then
    echo -e "${RED}Error: Failed to download iOS XCFramework.${NC}"
    return 1
  fi
  
  # Extract the framework
  echo -e "${YELLOW}Extracting iOS XCFramework...${NC}"
  unzip -o "$temp_zip" -d "$IOS_DIR" > /dev/null
  
  # Check if extraction was successful
  if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to extract iOS XCFramework.${NC}"
    rm -f "$temp_zip"
    return 1
  fi
  
  # Debug: List contents of the iOS directory
  echo -e "${YELLOW}Contents of $IOS_DIR:${NC}"
  ls -la "$IOS_DIR"
  
  # Sometimes the framework is extracted to a subdirectory, try to find it
  # Look for any .xcframework directory
  local xcframework_path=$(find "$IOS_DIR" -name "*.xcframework" -type d | head -n 1)
  
  if [ -z "$xcframework_path" ]; then
    echo -e "${RED}Error: No .xcframework found in extracted files.${NC}"
    rm -f "$temp_zip"
    return 1
  fi
  
  echo -e "${YELLOW}Found framework at: $xcframework_path${NC}"
  
  # If it's in a subdirectory, move it to the expected location
  if [ "$xcframework_path" != "$IOS_DIR/llamacpp.xcframework" ]; then
    echo -e "${YELLOW}Moving framework to standard location...${NC}"
    mv "$xcframework_path" "$IOS_DIR/llamacpp.xcframework"
  fi
  
  # Also copy to the framework directory for compatibility
  mkdir -p "$IOS_FRAMEWORK_DIR"
  cp -R "$IOS_DIR/llamacpp.xcframework" "$IOS_FRAMEWORK_DIR/llama.xcframework"
  
  # Verify the framework has an iOS ARM64 slice
  if [ ! -d "$IOS_DIR/llamacpp.xcframework/ios-arm64" ] && [ ! -d "$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator" ]; then
    echo -e "${RED}Error: iOS ARM64 framework not found.${NC}"
    
    # Debug: Show what's in the xcframework
    echo -e "${YELLOW}Contents of xcframework:${NC}"
    ls -la "$IOS_DIR/llamacpp.xcframework/"
    
    rm -f "$temp_zip"
    return 1
  fi
  
  # Find headers
  echo -e "${YELLOW}Looking for header files...${NC}"
  local header_path=""
  
  # Check various possible locations for headers
  if [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64/llamacpp.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64/llamacpp.framework/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/llamacpp.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/llamacpp.framework/Headers"
  elif [ -d "$IOS_DIR/includes" ]; then
    header_path="$IOS_DIR/includes"
  fi
  
  if [ -z "$header_path" ]; then
    echo -e "${RED}Error: Could not find Headers directory.${NC}"
    echo -e "${YELLOW}Showing framework structure:${NC}"
    find "$IOS_DIR/llamacpp.xcframework" -type d | sort
    rm -f "$temp_zip"
    return 1
  fi
  
  echo -e "${YELLOW}Copying header files from $header_path...${NC}"
  cp -R "$header_path/"* "$IOS_INCLUDE_DIR/"
  
  # Clean up
  rm -f "$temp_zip"
  
  echo -e "${GREEN}iOS XCFramework downloaded and extracted successfully.${NC}"
  return 0
}

# Download and extract Android libraries
download_android_libraries() {
  echo -e "${YELLOW}Setting up Android libraries...${NC}"
  
  # Create directories if they don't exist
  mkdir -p "$ANDROID_DIR"
  mkdir -p "$ANDROID_INCLUDE_DIR"
  mkdir -p "$ANDROID_JNILIB_DIR/arm64-v8a"
  mkdir -p "$ANDROID_JNILIB_DIR/armeabi-v7a"
  mkdir -p "$ANDROID_JNILIB_DIR/x86"
  mkdir -p "$ANDROID_JNILIB_DIR/x86_64"
  
  # URL of the prebuilt Android libraries
  local url="https://github.com/novastera-oss/llama.cpp-builds/releases/download/llamacpp-rn-$LLAMA_CPP_TAG/llamacpp-android-libs.zip"
  
  # Check if URL exists
  if command -v curl &> /dev/null; then
    curl -Is "$url" | head -1 | grep "200" > /dev/null
    local url_exists=$?
  elif command -v wget &> /dev/null; then
    wget --spider --quiet "$url"
    local url_exists=$?
  else
    echo -e "${RED}Error: Neither curl nor wget is available. Please install one of them.${NC}"
    return 1
  fi
  
  if [ $url_exists -ne 0 ]; then
    echo -e "${RED}Error: The URL $url does not exist.${NC}"
    return 1
  fi
  
  echo -e "${YELLOW}Downloading Android libraries from $url...${NC}"
  
  # Download the libraries
  local temp_zip="$ANDROID_DIR/temp_libs.zip"
  if ! download_file "$url" "$temp_zip"; then
    echo -e "${RED}Error: Failed to download Android libraries.${NC}"
    return 1
  fi
  
  # Extract the libraries
  echo -e "${YELLOW}Extracting Android libraries...${NC}"
  unzip -o "$temp_zip" -d "$ANDROID_DIR" > /dev/null
  
  # Check if extraction was successful
  if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Failed to extract Android libraries.${NC}"
    rm -f "$temp_zip"
    return 1
  fi
  
  # Copy header files
  echo -e "${YELLOW}Copying header files...${NC}"
  cp -R "$ANDROID_DIR/includes/"* "$ANDROID_INCLUDE_DIR/"
  
  # Copy shared libraries to JNI directory
  echo -e "${YELLOW}Copying shared libraries to JNI directory...${NC}"
  cp "$ANDROID_DIR/arm64-v8a/libllama.so" "$ANDROID_JNILIB_DIR/arm64-v8a/" 2>/dev/null || true
  cp "$ANDROID_DIR/armeabi-v7a/libllama.so" "$ANDROID_JNILIB_DIR/armeabi-v7a/" 2>/dev/null || true
  cp "$ANDROID_DIR/x86/libllama.so" "$ANDROID_JNILIB_DIR/x86/" 2>/dev/null || true
  cp "$ANDROID_DIR/x86_64/libllama.so" "$ANDROID_JNILIB_DIR/x86_64/" 2>/dev/null || true
  
  # Clean up
  rm -f "$temp_zip"
  
  echo -e "${GREEN}Android libraries downloaded and extracted successfully.${NC}"
  return 0
}

# Main function
main() {
  local command="$1"
  local target_platform="$2"
  
  case "$command" in
    "init")
      # Support the "init" command used by setupLlamaCpp.sh
      download_ios_framework
      if [ $? -eq 0 ]; then
        echo -e "${GREEN}iOS libraries setup completed successfully.${NC}"
      else
        echo -e "${RED}iOS libraries setup failed.${NC}"
        exit 1
      fi
      download_android_libraries
      if [ $? -eq 0 ]; then
        echo -e "${GREEN}Android libraries setup completed successfully.${NC}"
      else
        echo -e "${RED}Android libraries setup failed.${NC}"
        exit 1
      fi
      ;;
    "status"|"list-tags")
      # These commands should be handled by llama_cpp_version.sh, just pass through
      echo "Current llama.cpp hash: $LLAMA_CPP_HASH"
      ;;
    *)
      # Original functionality for selecting platforms
      case "$command" in
        "ios")
          download_ios_framework
          if [ $? -eq 0 ]; then
            echo -e "${GREEN}iOS libraries setup completed successfully.${NC}"
          else
            echo -e "${RED}iOS libraries setup failed.${NC}"
            exit 1
          fi
          ;;
        "android")
          download_android_libraries
          if [ $? -eq 0 ]; then
            echo -e "${GREEN}Android libraries setup completed successfully.${NC}"
          else
            echo -e "${RED}Android libraries setup failed.${NC}"
            exit 1
          fi
          ;;
        "all")
          download_ios_framework
          ios_result=$?
          download_android_libraries
          android_result=$?
          
          if [ $ios_result -eq 0 ] && [ $android_result -eq 0 ]; then
            echo -e "${GREEN}All libraries setup completed successfully.${NC}"
          else
            echo -e "${RED}Library setup failed for one or more platforms.${NC}"
            exit 1
          fi
          ;;
        *)
          echo "Usage: $0 {ios|android|all|init|status|list-tags}"
          echo "  ios       - Download and setup iOS libraries"
          echo "  android   - Download and setup Android libraries"
          echo "  all       - Download and setup libraries for all platforms"
          echo "  init      - Initialize both iOS and Android libraries (used by setupLlamaCpp.sh)"
          echo "  status    - Show current llama.cpp repository status"
          echo "  list-tags - List available llama.cpp release tags"
          exit 1
          ;;
      esac
      ;;
  esac
}

# Run the script with provided arguments
main "$@" 