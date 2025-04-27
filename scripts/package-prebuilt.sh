#!/bin/bash
# This script packages prebuilt binaries for distribution with the npm package

set -e

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PACKAGE_DIR="$( cd "$SCRIPT_DIR/.." && pwd )"

echo -e "${YELLOW}Preparing prebuilt binaries for packaging...${NC}"

# Ensure prebuilt directories exist
mkdir -p "$PACKAGE_DIR/prebuilt/ios"
mkdir -p "$PACKAGE_DIR/prebuilt/android/includes"
mkdir -p "$PACKAGE_DIR/prebuilt/android/arm64-v8a"
mkdir -p "$PACKAGE_DIR/prebuilt/android/armeabi-v7a"
mkdir -p "$PACKAGE_DIR/prebuilt/android/x86"
mkdir -p "$PACKAGE_DIR/prebuilt/android/x86_64"

# First, ensure we have the latest binaries
"$SCRIPT_DIR/setupLlamaCpp.sh" init

# Check if iOS framework exists
if [ -d "$PACKAGE_DIR/ios/libs/llamacpp.xcframework" ]; then
  echo -e "${YELLOW}Packaging iOS framework...${NC}"
  rm -rf "$PACKAGE_DIR/prebuilt/ios/llamacpp.xcframework"
  cp -R "$PACKAGE_DIR/ios/libs/llamacpp.xcframework" "$PACKAGE_DIR/prebuilt/ios/"
  echo -e "${GREEN}iOS framework packaged successfully.${NC}"
else
  echo -e "${RED}iOS framework not found. Run setupLlamaCpp.sh first.${NC}"
  exit 1
fi

# Check if Android libraries exist
ANDROID_INCLUDES="$PACKAGE_DIR/android/src/main/cpp/includes"
ANDROID_LIBS="$PACKAGE_DIR/android/src/main/jniLibs"

# Package Android includes if they exist
if [ -d "$ANDROID_INCLUDES" ] && [ "$(ls -A "$ANDROID_INCLUDES")" ]; then
  echo -e "${YELLOW}Packaging Android includes...${NC}"
  cp -R "$ANDROID_INCLUDES/"* "$PACKAGE_DIR/prebuilt/android/includes/"
  echo -e "${GREEN}Android includes packaged successfully.${NC}"
else
  echo -e "${RED}Android includes not found. You'll need to build from source.${NC}"
fi

# Package Android libraries for each architecture
for arch in arm64-v8a armeabi-v7a x86 x86_64; do
  if [ -f "$ANDROID_LIBS/$arch/libllama.so" ]; then
    echo -e "${YELLOW}Packaging Android $arch library...${NC}"
    cp "$ANDROID_LIBS/$arch/libllama.so" "$PACKAGE_DIR/prebuilt/android/$arch/"
    echo -e "${GREEN}Android $arch library packaged successfully.${NC}"
  else
    echo -e "${RED}Android $arch library not found. You'll need to build Android from source.${NC}"
  fi
done

echo -e "${GREEN}Prebuilt binaries prepared for packaging.${NC}"
echo -e "${YELLOW}To include these in your npm package, commit these files to your repository.${NC}"
echo -e "${YELLOW}They will be included in the npm package when you publish.${NC}" 