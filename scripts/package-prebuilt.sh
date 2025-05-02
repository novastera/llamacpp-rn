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
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
PREBUILT_DIR="$ROOT_DIR/prebuilt"
PREBUILT_IOS_DIR="$PREBUILT_DIR/ios"
PREBUILT_ANDROID_DIR="$PREBUILT_DIR/android"

echo -e "${YELLOW}Preparing prebuilt binaries for packaging...${NC}"

# Create prebuilt directories
echo "Creating prebuilt directories..."
mkdir -p "$PREBUILT_IOS_DIR"
mkdir -p "$PREBUILT_ANDROID_DIR/arm64-v8a"
mkdir -p "$PREBUILT_ANDROID_DIR/x86_64"

# Check if iOS framework exists and copy it
if [ -d "$ROOT_DIR/ios/libs/llamacpp.xcframework" ]; then
    echo "Packaging iOS xcframework..."
    # Remove existing prebuilt framework if it exists
    rm -rf "$PREBUILT_IOS_DIR/llamacpp.xcframework"
    # Copy the framework
    cp -R "$ROOT_DIR/ios/libs/llamacpp.xcframework" "$PREBUILT_IOS_DIR/"
    echo "✅ iOS framework packaged successfully"
else
    echo "⚠️ iOS framework not found! Run scripts/build_ios.sh first."
fi

# Check if Android libs exist and copy them
if [ -d "$ROOT_DIR/android/src/main/jniLibs" ]; then
    echo "Packaging Android libraries..."
    
    # arm64-v8a
    if [ -f "$ROOT_DIR/android/src/main/jniLibs/arm64-v8a/libllama.so" ]; then
        cp "$ROOT_DIR/android/src/main/jniLibs/arm64-v8a/libllama.so" "$PREBUILT_ANDROID_DIR/arm64-v8a/"
        echo "✅ Android arm64-v8a library packaged successfully"
    else
        echo "⚠️ Android arm64-v8a library not found!"
    fi
    
    # x86_64
    if [ -f "$ROOT_DIR/android/src/main/jniLibs/x86_64/libllama.so" ]; then
        cp "$ROOT_DIR/android/src/main/jniLibs/x86_64/libllama.so" "$PREBUILT_ANDROID_DIR/x86_64/"
        echo "✅ Android x86_64 library packaged successfully"
    else
        echo "⚠️ Android x86_64 library not found!"
    fi
else
    echo "⚠️ Android libraries not found! Run scripts/build_android.sh first."
fi

# Create version file with commit info for tracking
echo "Creating version info file..."
LLAMA_CPP_DIR="$ROOT_DIR/cpp/llama.cpp"
if [ -d "$LLAMA_CPP_DIR/.git" ]; then
    cd "$LLAMA_CPP_DIR"
    LLAMA_COMMIT=$(git rev-parse HEAD)
    LLAMA_VERSION=$(git describe --tags --always)
    cd "$ROOT_DIR"
    if [ -d "$ROOT_DIR/.git" ]; then
        PACKAGE_COMMIT=$(git rev-parse HEAD)
        PACKAGE_VERSION=$(git describe --tags --always 2>/dev/null || echo "unknown")
    else
        PACKAGE_COMMIT="unknown"
        PACKAGE_VERSION="unknown"
    fi
else
    LLAMA_COMMIT="unknown"
    LLAMA_VERSION="unknown"
    PACKAGE_COMMIT="unknown"
    PACKAGE_VERSION="unknown"
fi

cat > "$PREBUILT_DIR/version.json" << EOF
{
  "packageVersion": "$(node -e "console.log(require('$ROOT_DIR/package.json').version)")",
  "packageCommit": "$PACKAGE_COMMIT",
  "packageGitVersion": "$PACKAGE_VERSION",
  "llamaCppVersion": "$LLAMA_VERSION",
  "llamaCppCommit": "$LLAMA_COMMIT",
  "buildDate": "$(date -u +"%Y-%m-%dT%H:%M:%SZ")"
}
EOF

echo "✅ Prebuilt binaries packaged successfully!"
echo "You can now publish your npm package with prebuilt binaries." 