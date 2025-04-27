#!/bin/bash
# This script sets up the llama.cpp integration
# It prioritizes pre-packaged binaries, then tries prebuilt downloads, then builds from source

set -e

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# Get package root directory (one level up from script dir)
PACKAGE_DIR="$( cd "$SCRIPT_DIR/.." && pwd )"

# Ensure the llama.cpp version script is sourced
source "$SCRIPT_DIR/llama_cpp_version.sh"

# Function to check if a command exists
command_exists() {
  command -v "$1" &> /dev/null
}

# Check if we're being run without arguments and print usage if so
if [ "$#" -eq 0 ]; then
  echo "Usage: $0 {init|status|list-tags|clean}"
  echo "  init       - Initialize and set the llama.cpp repository to the correct version"
  echo "  status     - Show current llama.cpp repository status"
  echo "  list-tags  - List available llama.cpp release tags"
  echo "  clean      - Clean up all downloaded frameworks and build artifacts"
  exit 1
fi

# Handle clean command directly
if [ "$1" = "clean" ]; then
  echo -e "${YELLOW}Cleaning up all downloaded frameworks and build artifacts...${NC}"
  "$SCRIPT_DIR/download_prebuilt.sh" clean
  exit $?
fi

# Set permissions for the scripts
echo -e "${YELLOW}Setting executable permissions for scripts...${NC}"
chmod +x "$SCRIPT_DIR/download_prebuilt.sh"
chmod +x "$SCRIPT_DIR/llama_cpp_version.sh"

# Setup directories
mkdir -p "ios/includes"
mkdir -p "ios/libs"
mkdir -p "ios/framework/build-apple"
mkdir -p "android/src/main/cpp/includes"
mkdir -p "android/src/main/jniLibs/arm64-v8a"
mkdir -p "android/src/main/jniLibs/armeabi-v7a"
mkdir -p "android/src/main/jniLibs/x86"
mkdir -p "android/src/main/jniLibs/x86_64"

# Ask user if they want to build from source
if [ "${LLAMACPPRN_BUILD_FROM_SOURCE}" = "true" ] || [ "$1" == "--source" ]; then
  BUILD_FROM_SOURCE=true
elif [ "$1" == "--prebuilt" ]; then
  BUILD_FROM_SOURCE=false
else
  # Default behavior: try packaged binaries first, then prebuilt downloads, then source
  BUILD_FROM_SOURCE=false
fi

if [ "$BUILD_FROM_SOURCE" = true ]; then
  echo -e "${YELLOW}Setting up llama.cpp from source...${NC}"
  "$SCRIPT_DIR/llama_cpp_version.sh" init
  "$SCRIPT_DIR/llama_cpp_version.sh" status
  
  # Simply ensure the llama.cpp repository is set up for building by Android
  echo -e "${GREEN}Setup completed - will build from source${NC}"
else
  # First check if pre-packaged binaries are available (these would be included in the npm package)
  PACKAGED_IOS_FRAMEWORK="$PACKAGE_DIR/prebuilt/ios/llamacpp.xcframework"
  PACKAGED_ANDROID_LIBS="$PACKAGE_DIR/prebuilt/android"
  
  IOS_SETUP_DONE=false
  ANDROID_SETUP_DONE=false
  
  # 1. Check for pre-packaged iOS binaries
  if [ -d "$PACKAGED_IOS_FRAMEWORK" ] && 
     { [ -d "$PACKAGED_IOS_FRAMEWORK/ios-arm64/llama.framework" ] || 
       [ -d "$PACKAGED_IOS_FRAMEWORK/ios-arm64_x86_64-simulator/llama.framework" ]; }; then
    echo -e "${GREEN}Using pre-packaged iOS binaries...${NC}"
    
    # Copy the framework
    cp -R "$PACKAGED_IOS_FRAMEWORK" "ios/libs/llamacpp.xcframework"
    cp -R "$PACKAGED_IOS_FRAMEWORK" "ios/framework/build-apple/llama.xcframework"
    
    # Copy headers
    if [ -d "$PACKAGED_IOS_FRAMEWORK/ios-arm64/llama.framework/Headers" ]; then
      cp -R "$PACKAGED_IOS_FRAMEWORK/ios-arm64/llama.framework/Headers/"* "ios/includes/"
    elif [ -d "$PACKAGED_IOS_FRAMEWORK/ios-arm64_x86_64-simulator/llama.framework/Headers" ]; then
      cp -R "$PACKAGED_IOS_FRAMEWORK/ios-arm64_x86_64-simulator/llama.framework/Headers/"* "ios/includes/"
    elif [ -d "$PACKAGED_IOS_FRAMEWORK/xros-arm64/llama.framework/Headers" ]; then
      cp -R "$PACKAGED_IOS_FRAMEWORK/xros-arm64/llama.framework/Headers/"* "ios/includes/"
    elif [ -d "$PACKAGED_IOS_FRAMEWORK/macos-arm64_x86_64/llama.framework/Versions/A/Headers" ]; then
      cp -R "$PACKAGED_IOS_FRAMEWORK/macos-arm64_x86_64/llama.framework/Versions/A/Headers/"* "ios/includes/"
    fi
    
    IOS_SETUP_DONE=true
    echo -e "${GREEN}iOS framework setup complete from pre-packaged binaries.${NC}"
  fi
  
  # 2. Check for pre-packaged Android binaries
  if [ -d "$PACKAGED_ANDROID_LIBS" ] && [ -d "$PACKAGED_ANDROID_LIBS/includes" ]; then
    echo -e "${GREEN}Using pre-packaged Android binaries...${NC}"
    
    # Copy includes
    cp -R "$PACKAGED_ANDROID_LIBS/includes/"* "android/src/main/cpp/includes/"
    
    # Copy libs for each architecture
    if [ -f "$PACKAGED_ANDROID_LIBS/arm64-v8a/libllama.so" ]; then
      cp "$PACKAGED_ANDROID_LIBS/arm64-v8a/libllama.so" "android/src/main/jniLibs/arm64-v8a/"
    fi
    if [ -f "$PACKAGED_ANDROID_LIBS/armeabi-v7a/libllama.so" ]; then
      cp "$PACKAGED_ANDROID_LIBS/armeabi-v7a/libllama.so" "android/src/main/jniLibs/armeabi-v7a/"
    fi
    if [ -f "$PACKAGED_ANDROID_LIBS/x86/libllama.so" ]; then
      cp "$PACKAGED_ANDROID_LIBS/x86/libllama.so" "android/src/main/jniLibs/x86/"
    fi
    if [ -f "$PACKAGED_ANDROID_LIBS/x86_64/libllama.so" ]; then
      cp "$PACKAGED_ANDROID_LIBS/x86_64/libllama.so" "android/src/main/jniLibs/x86_64/"
    fi
    
    ANDROID_SETUP_DONE=true
    echo -e "${GREEN}Android libraries setup complete from pre-packaged binaries.${NC}"
  fi
  
  # 3. If iOS binaries weren't packaged, try downloading them
  if [ "$IOS_SETUP_DONE" = false ]; then
    echo -e "${YELLOW}Pre-packaged iOS binaries not found, trying to download...${NC}"
    "$SCRIPT_DIR/download_prebuilt.sh" ios
    DOWNLOAD_RESULT=$?
    
    # Check if iOS framework was downloaded successfully
    if [ ! -d "ios/libs/llamacpp.xcframework" ] || 
       { [ ! -d "ios/libs/llamacpp.xcframework/ios-arm64/llama.framework" ] && 
         [ ! -d "ios/libs/llamacpp.xcframework/ios-arm64_x86_64-simulator/llama.framework" ] &&
         [ ! -d "ios/libs/llamacpp.xcframework/xros-arm64/llama.framework" ]; } ||
       [ $DOWNLOAD_RESULT -ne 0 ]; then
      echo -e "${YELLOW}iOS prebuilt binaries not found or download failed.${NC}"
      echo -e "${YELLOW}Will need to initialize llama.cpp repository...${NC}"
      
      # Initialize llama.cpp repository for building from source
      "$SCRIPT_DIR/llama_cpp_version.sh" init
      "$SCRIPT_DIR/llama_cpp_version.sh" status
    else
      echo -e "${GREEN}iOS prebuilt framework downloaded successfully.${NC}"
      
      # Copy framework to the framework directory if not already there
      if [ ! -d "ios/framework/build-apple/llama.xcframework" ] && [ -d "ios/libs/llamacpp.xcframework" ]; then
        echo -e "${YELLOW}Copying framework to ios/framework/build-apple/...${NC}"
        cp -R "ios/libs/llamacpp.xcframework" "ios/framework/build-apple/llama.xcframework"
      fi
      
      echo -e "${YELLOW}Setting up iOS include files...${NC}"
      # Try to find headers in any available slices
      if [ -d "ios/libs/llamacpp.xcframework/ios-arm64/llama.framework/Headers" ]; then
        cp -R "ios/libs/llamacpp.xcframework/ios-arm64/llama.framework/Headers/"* "ios/includes/"
      elif [ -d "ios/libs/llamacpp.xcframework/ios-arm64_x86_64-simulator/llama.framework/Headers" ]; then
        cp -R "ios/libs/llamacpp.xcframework/ios-arm64_x86_64-simulator/llama.framework/Headers/"* "ios/includes/"
      elif [ -d "ios/libs/llamacpp.xcframework/xros-arm64/llama.framework/Headers" ]; then
        cp -R "ios/libs/llamacpp.xcframework/xros-arm64/llama.framework/Headers/"* "ios/includes/"
      elif [ -d "ios/libs/llamacpp.xcframework/macos-arm64_x86_64/llama.framework/Versions/A/Headers" ]; then
        cp -R "ios/libs/llamacpp.xcframework/macos-arm64_x86_64/llama.framework/Versions/A/Headers/"* "ios/includes/"
      fi
      
      IOS_SETUP_DONE=true
    fi
  fi
  
  # 4. If Android binaries weren't packaged and we haven't initialized git already, initialize now
  if [ "$ANDROID_SETUP_DONE" = false ] && [ ! -d "cpp/llama.cpp/.git" ]; then
    echo -e "${YELLOW}Setting up llama.cpp repository for Android build...${NC}"
    "$SCRIPT_DIR/llama_cpp_version.sh" init
    "$SCRIPT_DIR/llama_cpp_version.sh" status
  fi
  
  echo -e "${GREEN}Setup completed successfully${NC}"
fi
