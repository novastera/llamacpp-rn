#!/bin/bash
# This script downloads prebuilt llama.cpp binaries for iOS and Android

# Disable set -e to see all errors
# set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get the version and hash from the llama.cpp version script
source $(dirname "$0")/llama_cpp_version.sh
# Use the explicitly defined tag for prebuilt binary downloads
# LLAMA_CPP_TAG is already set in llama_cpp_version.sh
LLAMA_CPP_HASH=$LLAMA_CPP_COMMIT

echo -e "${YELLOW}Using tag: $LLAMA_CPP_TAG (from commit: $LLAMA_CPP_HASH)${NC}"

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
  
  echo -e "${YELLOW}Downloading from: $url${NC}"
  echo -e "${YELLOW}Saving to: $output_path${NC}"
  
  if command -v curl &> /dev/null; then
    echo -e "${YELLOW}Using curl for download${NC}"
    curl -L -o "$output_path" "$url"
    local result=$?
    echo -e "${YELLOW}curl exit code: $result${NC}"
    return $result
  elif command -v wget &> /dev/null; then
    echo -e "${YELLOW}Using wget for download${NC}"
    wget -O "$output_path" "$url"
    local result=$?
    echo -e "${YELLOW}wget exit code: $result${NC}"
    return $result
  else
    echo -e "${RED}Error: Neither curl nor wget is available. Please install one of them.${NC}"
    return 1
  fi
}

# Download and extract iOS XCFramework
download_ios_framework() {
  echo -e "${YELLOW}Setting up iOS XCFramework...${NC}"
  
  # Clean up more thoroughly to save space
  echo -e "${YELLOW}Cleaning up previous iOS framework downloads and build artifacts...${NC}"
  rm -rf "$IOS_DIR/llamacpp.xcframework"
  rm -rf "$IOS_DIR/temp_framework.zip"
  rm -rf "$IOS_FRAMEWORK_DIR/llama.xcframework"
  # Also remove any intermediate build files
  rm -rf "ios/framework/build"
  
  # Create directories if they don't exist
  mkdir -p "$IOS_DIR"
  mkdir -p "$IOS_INCLUDE_DIR"
  mkdir -p "$IOS_FRAMEWORK_DIR"
  
  # Use the exact URL format from the official llama.cpp releases
  local url="https://github.com/ggml-org/llama.cpp/releases/download/$LLAMA_CPP_TAG/llama-$LLAMA_CPP_TAG-xcframework.zip"
  
  echo -e "${YELLOW}Checking URL: $url${NC}"
  
  # Check if URL exists
  if command -v curl &> /dev/null; then
    echo -e "${YELLOW}Using curl to check URL${NC}"
    curl -IsL "$url" | head -1
    curl -IsL "$url" | grep -E "200|HTTP/2 200" > /dev/null
    local url_exists=$?
    echo -e "${YELLOW}URL check result: $url_exists${NC}"
  elif command -v wget &> /dev/null; then
    echo -e "${YELLOW}Using wget to check URL${NC}"
    wget --spider --quiet "$url"
    local url_exists=$?
    echo -e "${YELLOW}URL check result: $url_exists${NC}"
  else
    echo -e "${RED}Error: Neither curl nor wget is available. Please install one of them.${NC}"
    return 1
  fi
  
  if [ $url_exists -ne 0 ]; then
    echo -e "${RED}Error: The URL $url does not exist.${NC}"
    return 1
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
  local unzip_result=$?
  echo -e "${YELLOW}Unzip result: $unzip_result${NC}"
  
  # Check if extraction was successful
  if [ $unzip_result -ne 0 ]; then
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
  echo -e "${YELLOW}Found framework at: $xcframework_path${NC}"
  
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
  
  # Verify the framework has the necessary slices - updated to handle llama.framework inside
  if [[ ! -d "$IOS_DIR/llamacpp.xcframework/ios-arm64/llama.framework" && 
        ! -d "$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/llama.framework" ]]; then
    echo -e "${RED}Error: iOS/simulator framework not found.${NC}"
    
    # Debug: Show what's in the xcframework
    echo -e "${YELLOW}Contents of xcframework:${NC}"
    find "$IOS_DIR/llamacpp.xcframework" -type d | sort
    
    rm -f "$temp_zip"
    return 1
  fi
  
  # Find headers
  echo -e "${YELLOW}Looking for header files...${NC}"
  local header_path=""
  
  # Check various possible locations for headers - updating to match the actual structure
  if [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64/llama.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64/llama.framework/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/llama.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64_x86_64-simulator/llama.framework/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/macos-arm64_x86_64/llama.framework/Versions/A/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/macos-arm64_x86_64/llama.framework/Versions/A/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/tvos-arm64/llama.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/tvos-arm64/llama.framework/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/xros-arm64/llama.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/xros-arm64/llama.framework/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64/Headers"
  elif [ -d "$IOS_DIR/llamacpp.xcframework/ios-arm64/llamacpp.framework/Headers" ]; then
    header_path="$IOS_DIR/llamacpp.xcframework/ios-arm64/llamacpp.framework/Headers"
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
  
  # Clean up previous downloaded libraries to save space
  echo -e "${YELLOW}Cleaning up previous Android library downloads and build artifacts...${NC}"
  rm -rf "$ANDROID_DIR"
  # Also clean build directories for Android
  rm -rf "android/build"
  rm -rf "android/.cxx"
  
  # Create directories if they don't exist
  mkdir -p "$ANDROID_DIR"
  mkdir -p "$ANDROID_INCLUDE_DIR"
  mkdir -p "$ANDROID_JNILIB_DIR/arm64-v8a"
  mkdir -p "$ANDROID_JNILIB_DIR/armeabi-v7a"
  mkdir -p "$ANDROID_JNILIB_DIR/x86"
  mkdir -p "$ANDROID_JNILIB_DIR/x86_64"
  
  # Use the exact URL format from the official llama.cpp releases for Android
  local url="https://github.com/ggml-org/llama.cpp/releases/download/$LLAMA_CPP_TAG/llama-$LLAMA_CPP_TAG-bin-android.zip"
  
  echo -e "${YELLOW}Checking URL: $url${NC}"
  
  # Check if URL exists
  if command -v curl &> /dev/null; then
    echo -e "${YELLOW}Using curl to check URL${NC}"
    curl -IsL "$url" | head -1
    curl -IsL "$url" | grep -E "200|HTTP/2 200" > /dev/null
    local url_exists=$?
    echo -e "${YELLOW}URL check result: $url_exists${NC}"
  elif command -v wget &> /dev/null; then
    echo -e "${YELLOW}Using wget to check URL${NC}"
    wget --spider --quiet "$url"
    local url_exists=$?
    echo -e "${YELLOW}URL check result: $url_exists${NC}"
  else
    echo -e "${RED}Error: Neither curl nor wget is available. Please install one of them.${NC}"
    return 1
  fi
  
  if [ $url_exists -ne 0 ]; then
    echo -e "${RED}Error: The URL $url does not exist.${NC}"
    
    # Try the fallback URL with 'latest' tag
    echo -e "${YELLOW}Trying fallback URL with 'latest' tag${NC}"
    url="https://github.com/novastera-oss/llama.cpp-builds/releases/download/llamacpp-rn-latest/llamacpp-android-libs.zip"
    echo -e "${YELLOW}Fallback URL: $url${NC}"
    
    if command -v curl &> /dev/null; then
      curl -IsL "$url" | head -1
      curl -IsL "$url" | grep -E "200|HTTP/2 200" > /dev/null
      url_exists=$?
    elif command -v wget &> /dev/null; then
      wget --spider --quiet "$url"
      url_exists=$?
    fi
    
    echo -e "${YELLOW}Fallback URL check result: $url_exists${NC}"
    
    if [ $url_exists -ne 0 ]; then
      echo -e "${RED}Error: The URL $url does not exist either. Will need to build from source.${NC}"
      return 1
    fi
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

# Function to clean all downloaded frameworks and build artifacts
clean_all() {
  echo -e "${YELLOW}Performing complete cleanup of all downloaded frameworks and build artifacts...${NC}"
  
  # iOS cleanup
  rm -rf "$IOS_DIR/llamacpp.xcframework"
  rm -rf "$IOS_DIR/temp_framework.zip"
  rm -rf "$IOS_FRAMEWORK_DIR/llama.xcframework"
  rm -rf "ios/framework/build"
  
  # Android cleanup
  rm -rf "$ANDROID_DIR"
  rm -rf "android/build"
  rm -rf "android/.cxx"
  rm -rf "android/src/main/jniLibs/arm64-v8a/libllama.so"
  rm -rf "android/src/main/jniLibs/armeabi-v7a/libllama.so"
  rm -rf "android/src/main/jniLibs/x86/libllama.so"
  rm -rf "android/src/main/jniLibs/x86_64/libllama.so"
  
  echo -e "${GREEN}Cleanup complete.${NC}"
  return 0
}

# Main function
main() {
  local command="$1"
  local target_platform="$2"
  
  case "$command" in
    "init")
      # Support the "init" command used by setupLlamaCpp.sh
      echo -e "${YELLOW}Downloading iOS framework...${NC}"
      download_ios_framework
      result=$?
      if [ $result -eq 0 ]; then
        echo -e "${GREEN}iOS libraries setup completed successfully.${NC}"
      else
        echo -e "${RED}iOS libraries setup failed. Will need to build from source.${NC}"
        # Create basic directory structure
        mkdir -p "ios/includes"
        mkdir -p "ios/libs"
        mkdir -p "ios/framework/build-apple"
      fi
      
      # Skip Android download, will build from source
      echo -e "${YELLOW}Android libraries will be built from source.${NC}"
      # Create basic directory structure for Android
      mkdir -p "android/src/main/cpp/includes"
      mkdir -p "android/src/main/jniLibs/arm64-v8a"
      mkdir -p "android/src/main/jniLibs/armeabi-v7a"
      mkdir -p "android/src/main/jniLibs/x86"
      mkdir -p "android/src/main/jniLibs/x86_64"
      
      # Return success to allow script to continue with source build
      return 0
      ;;
    "clean")
      # Clean up all downloaded frameworks and build artifacts
      clean_all
      return $?
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
            return 0
          else
            echo -e "${RED}iOS libraries setup failed.${NC}"
            return 1
          fi
          ;;
        "android")
          echo -e "${YELLOW}Android libraries will be built from source.${NC}"
          # Create basic directory structure for Android
          mkdir -p "android/src/main/cpp/includes"
          mkdir -p "android/src/main/jniLibs/arm64-v8a"
          mkdir -p "android/src/main/jniLibs/armeabi-v7a"
          mkdir -p "android/src/main/jniLibs/x86"
          mkdir -p "android/src/main/jniLibs/x86_64"
          return 0
          ;;
        "all")
          download_ios_framework
          ios_result=$?
          
          # Create Android directory structure
          mkdir -p "android/src/main/cpp/includes"
          mkdir -p "android/src/main/jniLibs/arm64-v8a"
          mkdir -p "android/src/main/jniLibs/armeabi-v7a"
          mkdir -p "android/src/main/jniLibs/x86"
          mkdir -p "android/src/main/jniLibs/x86_64"
          
          if [ $ios_result -eq 0 ]; then
            echo -e "${GREEN}All libraries setup completed successfully.${NC}"
            return 0
          else
            echo -e "${RED}iOS library setup failed, Android directories created.${NC}"
            return 1
          fi
          ;;
        *)
          echo "Usage: $0 {ios|android|all|init|status|list-tags|clean}"
          echo "  ios       - Download and setup iOS libraries"
          echo "  android   - Download and setup Android libraries"
          echo "  all       - Download and setup libraries for all platforms"
          echo "  init      - Initialize both iOS and Android libraries (used by setupLlamaCpp.sh)"
          echo "  status    - Show current llama.cpp repository status"
          echo "  list-tags - List available llama.cpp release tags"
          echo "  clean     - Clean all downloaded frameworks and build artifacts to save space"
          return 1
          ;;
      esac
      ;;
  esac
}

# Run the script with provided arguments
main "$@" 