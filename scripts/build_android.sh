#!/bin/bash
set -e

# Version configuration
OPENCL_VERSION="3.0"  # OpenCL version to target
NDK_VERSION="27.2.12479018"  # Specific NDK version
ANDROID_MIN_SDK="33"
ANDROID_TARGET_SDK="35"
ANDROID_PLATFORM="android-$ANDROID_MIN_SDK"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get the absolute path of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# Get project root directory (one level up from script dir)
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Print usage information
print_usage() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  --help                 Print this help message"
  echo "  --abi=[all|arm64-v8a|x86_64]  Specify which ABI to build for (default: all)"
  echo "  --no-opencl            Disable OpenCL GPU acceleration"
  echo "  --debug                Build in debug mode"
  echo "  --clean                Clean previous builds before building"
  echo "  --install-deps         Install dependencies (OpenCL, etc.)"
  echo "  --install-ndk          Install Android NDK version $NDK_VERSION"
}

# Default values
BUILD_ABI="all"
BUILD_OPENCL=true
BUILD_TYPE="Release"
CLEAN_BUILD=false
INSTALL_DEPS=false
INSTALL_NDK=false

# Parse arguments
for arg in "$@"; do
  case $arg in
    --help)
      print_usage
      exit 0
      ;;
    --abi=*)
      BUILD_ABI="${arg#*=}"
      ;;
    --no-opencl)
      BUILD_OPENCL=false
      ;;
    --debug)
      BUILD_TYPE="Debug"
      ;;
    --clean)
      CLEAN_BUILD=true
      ;;
    --install-deps)
      INSTALL_DEPS=true
      ;;
    --install-ndk)
      INSTALL_NDK=true
      ;;
    *)
      echo -e "${RED}Unknown argument: $arg${NC}"
      print_usage
      exit 1
      ;;
  esac
done

# Define directories
ANDROID_DIR="$PROJECT_ROOT/android"
ANDROID_JNI_DIR="$ANDROID_DIR/src/main/jniLibs"
ANDROID_CPP_DIR="$ANDROID_DIR/src/main/cpp"
CPP_DIR="$PROJECT_ROOT/cpp"
LLAMA_CPP_DIR="$CPP_DIR/llama.cpp"

# Third-party directories
THIRD_PARTY_DIR="$PROJECT_ROOT/third_party"
OPENCL_HEADERS_DIR="$THIRD_PARTY_DIR/OpenCL-Headers"
OPENCL_LOADER_DIR="$THIRD_PARTY_DIR/OpenCL-ICD-Loader"
OPENCL_INCLUDE_DIR="$THIRD_PARTY_DIR/opencl/include"
OPENCL_LIB_DIR="$THIRD_PARTY_DIR/opencl/lib"

# Create necessary directories
mkdir -p "$ANDROID_JNI_DIR/arm64-v8a"
mkdir -p "$ANDROID_JNI_DIR/x86_64"
mkdir -p "$ANDROID_CPP_DIR/include"
mkdir -p "$OPENCL_INCLUDE_DIR"
mkdir -p "$OPENCL_LIB_DIR"

# Determine platform and setup environment
if [[ "$OSTYPE" == "darwin"* ]]; then
  echo -e "${YELLOW}Building on macOS${NC}"
  # Check if we're on ARM Mac
  if [[ $(uname -m) == "arm64" ]]; then
    echo -e "${YELLOW}Detected ARM64 macOS${NC}"
  else
    echo -e "${YELLOW}Detected Intel macOS${NC}"
  fi
  # Detect number of cores on macOS
  N_CORES=$(sysctl -n hw.ncpu)
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
  echo -e "${YELLOW}Building on Linux${NC}"
  # Detect number of cores on Linux
  N_CORES=$(nproc)
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
  echo -e "${YELLOW}Building on Windows${NC}"
  # Detect number of cores on Windows
  N_CORES=$NUMBER_OF_PROCESSORS
else
  echo -e "${YELLOW}Unknown OS type: $OSTYPE, assuming 4 cores${NC}"
  N_CORES=4
fi

echo -e "${YELLOW}Using $N_CORES cores for building${NC}"

# Set up and verify NDK path
if [ -z "$ANDROID_HOME" ]; then
  if [ -n "$ANDROID_SDK_ROOT" ]; then
    ANDROID_HOME="$ANDROID_SDK_ROOT"
  elif [ -d "$HOME/Android/Sdk" ]; then
    ANDROID_HOME="$HOME/Android/Sdk"
  elif [ -d "$HOME/Library/Android/sdk" ]; then
    ANDROID_HOME="$HOME/Library/Android/sdk"
  else
    echo -e "${RED}Android SDK not found. Please set ANDROID_HOME or ANDROID_SDK_ROOT.${NC}"
    exit 1
  fi
fi

NDK_PATH="$ANDROID_HOME/ndk/$NDK_VERSION"

if [ ! -d "$NDK_PATH" ]; then
  echo -e "${RED}NDK version $NDK_VERSION not found at $NDK_PATH${NC}"
  echo -e "${YELLOW}Available NDK versions:${NC}"
  ls -la "$ANDROID_HOME/ndk" || echo "No NDK directory found"
  echo -e "${YELLOW}Please install NDK $NDK_VERSION using Android SDK Manager:${NC}"
  echo -e "${YELLOW}\$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager \"ndk;$NDK_VERSION\"${NC}"
  echo -e "${YELLOW}Or use the --install-ndk option${NC}"
  exit 1
fi

# Setup build environment
CMAKE_TOOLCHAIN_FILE="$NDK_PATH/build/cmake/android.toolchain.cmake"
echo -e "${GREEN}Using NDK at: $NDK_PATH${NC}"
echo -e "${GREEN}Using toolchain file: $CMAKE_TOOLCHAIN_FILE${NC}"

# Install NDK if requested
if [ "$INSTALL_NDK" = true ]; then
  echo -e "${YELLOW}Installing Android NDK version $NDK_VERSION...${NC}"
  
  # Check if Android SDK is available
  if [ -z "$ANDROID_HOME" ] && [ -z "$ANDROID_SDK_ROOT" ]; then
    echo -e "${RED}Android SDK not found. Please set ANDROID_HOME or ANDROID_SDK_ROOT.${NC}"
    exit 1
  fi
  
  # Use sdkmanager to install the NDK
  SDKMANAGER_CMD="$ANDROID_HOME/cmdline-tools/latest/bin/sdkmanager"
  if [ ! -f "$SDKMANAGER_CMD" ]; then
    SDKMANAGER_CMD="$(find $ANDROID_HOME/cmdline-tools -name sdkmanager | head -1)"
  fi
  
  if [ -z "$SDKMANAGER_CMD" ] || [ ! -f "$SDKMANAGER_CMD" ]; then
    echo -e "${RED}sdkmanager not found. Please install Android command line tools.${NC}"
    exit 1
  fi
  
  echo -e "${YELLOW}Using sdkmanager: $SDKMANAGER_CMD${NC}"
  
  # Install platforms and build tools
  echo -e "${YELLOW}Installing Android platform $ANDROID_TARGET_SDK and build tools...${NC}"
  "$SDKMANAGER_CMD" "platforms;android-$ANDROID_TARGET_SDK" "build-tools;$ANDROID_TARGET_SDK.0.0"
  
  # Install specific NDK version
  echo -e "${YELLOW}Installing NDK version $NDK_VERSION...${NC}"
  "$SDKMANAGER_CMD" "ndk;$NDK_VERSION"
  
  # Set environment variable
  if [ -n "$GITHUB_ENV" ]; then
    # If running in GitHub Actions
    echo "ANDROID_NDK_HOME=$ANDROID_HOME/ndk/$NDK_VERSION" >> $GITHUB_ENV
    echo -e "${GREEN}✅ Set ANDROID_NDK_HOME environment variable in GitHub Actions${NC}"
  else
    # For local development, just print the path
    echo -e "${GREEN}✅ NDK installed at: $ANDROID_HOME/ndk/$NDK_VERSION${NC}"
    echo -e "${YELLOW}You may want to set ANDROID_NDK_HOME environment variable:${NC}"
    echo -e "${YELLOW}export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/$NDK_VERSION${NC}"
  fi
  
  # Verify NDK installation
  if [ -d "$ANDROID_HOME/ndk/$NDK_VERSION" ]; then
    echo -e "${GREEN}✅ Android NDK $NDK_VERSION successfully installed${NC}"
  else
    echo -e "${RED}❌ Failed to install Android NDK $NDK_VERSION${NC}"
    exit 1
  fi
fi

# Install dependencies if requested
if [ "$INSTALL_DEPS" = true ] && [ "$BUILD_OPENCL" = true ]; then
  echo -e "${YELLOW}Setting up OpenCL dependencies for Android...${NC}"
  
  # Clone OpenCL Headers if not already present
  if [ ! -d "$OPENCL_HEADERS_DIR" ]; then
    echo -e "${YELLOW}Cloning OpenCL-Headers...${NC}"
    git clone --depth 1 https://github.com/KhronosGroup/OpenCL-Headers.git "$OPENCL_HEADERS_DIR"
    
    # Copy headers to include directory
    mkdir -p "$OPENCL_INCLUDE_DIR"
    cp -r "$OPENCL_HEADERS_DIR/CL" "$OPENCL_INCLUDE_DIR/"
    
    # Also copy headers to NDK sysroot as recommended in the docs
    NDK_SYSROOT_INCLUDE="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include"
    if [ -d "$NDK_SYSROOT_INCLUDE" ]; then
      echo -e "${YELLOW}Copying OpenCL headers to NDK sysroot...${NC}"
      mkdir -p "$NDK_SYSROOT_INCLUDE/CL"
      cp -r "$OPENCL_HEADERS_DIR/CL/"* "$NDK_SYSROOT_INCLUDE/CL/"
    fi
    
    echo -e "${GREEN}✅ OpenCL headers installed${NC}"
  else
    echo -e "${GREEN}OpenCL headers already exist${NC}"
  fi
  
  # Function to build OpenCL ICD Loader for a specific ABI
  build_opencl_for_abi() {
    local ABI=$1
    echo -e "${YELLOW}Building OpenCL ICD Loader for $ABI...${NC}"
    
    # Create architecture-specific directory
    local OPENCL_ABI_LIB_DIR="$OPENCL_LIB_DIR/$ABI"
    mkdir -p "$OPENCL_ABI_LIB_DIR"
    
    # Clone OpenCL ICD Loader if not already present
    if [ ! -d "$OPENCL_LOADER_DIR" ]; then
      echo -e "${YELLOW}Cloning OpenCL-ICD-Loader...${NC}"
      git clone --depth 1 https://github.com/KhronosGroup/OpenCL-ICD-Loader.git "$OPENCL_LOADER_DIR"
    fi
    
    # Create build directory
    local OPENCL_LOADER_BUILD_DIR="$OPENCL_LOADER_DIR/build-$ABI"
    mkdir -p "$OPENCL_LOADER_BUILD_DIR"
    
    # Build OpenCL ICD Loader
    cd "$OPENCL_LOADER_BUILD_DIR"
    
    # Ensure we pass absolute paths to CMake
    ABS_OPENCL_INCLUDE_DIR=$(readlink -f "$OPENCL_INCLUDE_DIR")
    
    # Set the proper architecture for the build
    if [ "$ABI" = "arm64-v8a" ]; then
      TARGET_ARCH="aarch64-linux-android"
    elif [ "$ABI" = "x86_64" ]; then
      TARGET_ARCH="x86_64-linux-android"
    else
      echo -e "${RED}Unsupported ABI: $ABI${NC}"
      return 1
    fi
    
    echo -e "${YELLOW}Building OpenCL for $ABI using $TARGET_ARCH${NC}"
    
    cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
      -DOPENCL_ICD_LOADER_HEADERS_DIR="$ABS_OPENCL_INCLUDE_DIR" \
      -DANDROID_ABI="$ABI" \
      -DANDROID_PLATFORM="$ANDROID_PLATFORM" \
      -DANDROID_STL=c++_shared
    
    ninja
    
    # Make sure we have the library
    if [ -f "$OPENCL_LOADER_BUILD_DIR/libOpenCL.so" ]; then
      # Copy the library to the lib directory
      cp "$OPENCL_LOADER_BUILD_DIR/libOpenCL.so" "$OPENCL_ABI_LIB_DIR/"
      
      # Also copy to NDK sysroot lib directory as recommended in the docs
      local NDK_SYSROOT_LIB
      if [ "$ABI" = "arm64-v8a" ]; then
        NDK_SYSROOT_LIB="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android"
      elif [ "$ABI" = "x86_64" ]; then
        NDK_SYSROOT_LIB="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/x86_64-linux-android"
      fi
      
      if [ -d "$NDK_SYSROOT_LIB" ]; then
        echo -e "${YELLOW}Copying libOpenCL.so to NDK sysroot for $ABI...${NC}"
        cp "$OPENCL_LOADER_BUILD_DIR/libOpenCL.so" "$NDK_SYSROOT_LIB/"
      fi
      
      echo -e "${GREEN}✅ OpenCL ICD Loader built for $ABI and installed${NC}"
    else
      echo -e "${RED}Failed to build libOpenCL.so for $ABI${NC}"
      echo -e "${YELLOW}Creating stub library to proceed with build${NC}"
      
      # Create a minimal stub OpenCL library
      echo "Creating stub OpenCL library for $ABI..."
      touch "$OPENCL_ABI_LIB_DIR/libOpenCL.so"
    fi
    
    # Return to project root
    cd "$PROJECT_ROOT"
  }
  
  # Build OpenCL for each ABI
  if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "arm64-v8a" ]; then
    build_opencl_for_abi "arm64-v8a"
  fi
  
  if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "x86_64" ]; then
    build_opencl_for_abi "x86_64"
  fi
  
  # Make sure all ABI directories have at least an empty OpenCL library
  if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "arm64-v8a" ]; then
    mkdir -p "$OPENCL_LIB_DIR/arm64-v8a"
    if [ ! -f "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so" ]; then
      echo -e "${YELLOW}Creating stub OpenCL library for arm64-v8a...${NC}"
      touch "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so"
    fi
  fi
  
  if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "x86_64" ]; then
    mkdir -p "$OPENCL_LIB_DIR/x86_64"
    if [ ! -f "$OPENCL_LIB_DIR/x86_64/libOpenCL.so" ]; then
      echo -e "${YELLOW}Creating stub OpenCL library for x86_64...${NC}"
      touch "$OPENCL_LIB_DIR/x86_64/libOpenCL.so"
    fi
  fi
fi

# Verify llama.cpp exists
if [ ! -d "$LLAMA_CPP_DIR" ]; then
  echo -e "${RED}llama.cpp not found at $LLAMA_CPP_DIR${NC}"
  echo -e "${YELLOW}Running llama_cpp_version.sh to initialize it...${NC}"
  "$SCRIPT_DIR/llama_cpp_version.sh" init
  if [ ! -d "$LLAMA_CPP_DIR" ]; then
    echo -e "${RED}Failed to initialize llama.cpp${NC}"
    exit 1
  fi
fi

# Copy necessary headers
cp "$LLAMA_CPP_DIR/include/llama.h" "$ANDROID_CPP_DIR/include/"
cp "$LLAMA_CPP_DIR/include/llama-cpp.h" "$ANDROID_CPP_DIR/include/"

# Function to build for a specific ABI
build_for_abi() {
  local ABI=$1
  echo -e "${YELLOW}Building for $ABI...${NC}"
  
  local BUILD_DIR="$PROJECT_ROOT/build-android-$ABI"
  
  # Clean build directory if requested
  if [ "$CLEAN_BUILD" = true ] && [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning previous build for $ABI${NC}"
    rm -rf "$BUILD_DIR"
  fi
  
  mkdir -p "$BUILD_DIR"
  
  # Setup CMake flags
  CMAKE_FLAGS=()
  CMAKE_FLAGS+=("-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE")
  CMAKE_FLAGS+=("-DANDROID_ABI=$ABI")
  CMAKE_FLAGS+=("-DANDROID_PLATFORM=$ANDROID_PLATFORM")
  CMAKE_FLAGS+=("-DCMAKE_BUILD_TYPE=$BUILD_TYPE")
  CMAKE_FLAGS+=("-DBUILD_SHARED_LIBS=OFF")
  CMAKE_FLAGS+=("-DLLAMA_BUILD_TESTS=OFF")
  CMAKE_FLAGS+=("-DLLAMA_BUILD_EXAMPLES=OFF")
  CMAKE_FLAGS+=("-DLLAMA_CURL=OFF")  # Disable CURL to avoid dependency issues
  CMAKE_FLAGS+=("-DGGML_VULKAN=OFF")  # Explicitly disable Vulkan
  
  # Configure OpenCL
  if [ "$BUILD_OPENCL" = true ]; then
    # Always explicitly set paths and make sure they exist
    if [ ! -d "$OPENCL_INCLUDE_DIR/CL" ]; then
      echo -e "${YELLOW}OpenCL headers not found, attempting to download...${NC}"
      mkdir -p "$OPENCL_INCLUDE_DIR"
      git clone --depth 1 https://github.com/KhronosGroup/OpenCL-Headers.git "$OPENCL_HEADERS_DIR" || true
      if [ -d "$OPENCL_HEADERS_DIR/CL" ]; then
        cp -r "$OPENCL_HEADERS_DIR/CL" "$OPENCL_INCLUDE_DIR/"
      fi
    fi
    
    # Use ABI-specific OpenCL library
    local OPENCL_ABI_LIB_DIR="$OPENCL_LIB_DIR/$ABI"
    mkdir -p "$OPENCL_ABI_LIB_DIR"
    
    if [ ! -f "$OPENCL_ABI_LIB_DIR/libOpenCL.so" ]; then
      echo -e "${YELLOW}libOpenCL.so not found for $ABI, creating empty stub file...${NC}"
      touch "$OPENCL_ABI_LIB_DIR/libOpenCL.so"
    fi
    
    # Get absolute paths to ensure they're correctly found by CMake
    ABS_OPENCL_INCLUDE_DIR=$(readlink -f "$OPENCL_INCLUDE_DIR")
    ABS_OPENCL_LIB=$(readlink -f "$OPENCL_ABI_LIB_DIR/libOpenCL.so")
    
    CMAKE_FLAGS+=("-DGGML_OPENCL=ON")
    CMAKE_FLAGS+=("-DGGML_OPENCL_EMBED_KERNELS=ON")  # As recommended in docs
    CMAKE_FLAGS+=("-DGGML_OPENCL_USE_ADRENO_KERNELS=ON")  # As recommended for Adreno GPUs
    
    # Explicitly provide both OpenCL paths to avoid CMake's Find module
    CMAKE_FLAGS+=("-DOpenCL_INCLUDE_DIR=$ABS_OPENCL_INCLUDE_DIR")
    CMAKE_FLAGS+=("-DOpenCL_LIBRARY=$ABS_OPENCL_LIB")
    
    # Also define these directly to bypass Find module
    CMAKE_FLAGS+=("-DOPENCL_FOUND=ON")
    CMAKE_FLAGS+=("-DOPENCL_INCLUDE_DIRS=$ABS_OPENCL_INCLUDE_DIR")
    CMAKE_FLAGS+=("-DOPENCL_LIBRARIES=$ABS_OPENCL_LIB")
    
    echo -e "${GREEN}OpenCL support enabled (version $OPENCL_VERSION)${NC}"
    echo -e "${GREEN}Using OpenCL include dir: $ABS_OPENCL_INCLUDE_DIR${NC}"
    echo -e "${GREEN}Using OpenCL library for $ABI: $ABS_OPENCL_LIB${NC}"
  else
    CMAKE_FLAGS+=("-DGGML_OPENCL=OFF")
    echo -e "${YELLOW}Building without OpenCL support${NC}"
  fi
  
  # Configure with CMake
  echo -e "${YELLOW}Configuring CMake for $ABI...${NC}"
  
  # Create a custom toolchain file to properly handle OpenCL detection
  CUSTOM_TOOLCHAIN_FILE="$BUILD_DIR/android-custom.toolchain.cmake"
  cat > "$CUSTOM_TOOLCHAIN_FILE" << EOF
# Include the standard Android toolchain file
include("$CMAKE_TOOLCHAIN_FILE")

# Force OpenCL to be found if we're building with OpenCL
if(DEFINED OpenCL_INCLUDE_DIR AND DEFINED OpenCL_LIBRARY)
  set(OpenCL_FOUND TRUE CACHE BOOL "OpenCL Found" FORCE)
  set(OPENCL_FOUND TRUE CACHE BOOL "OpenCL Found" FORCE)
  set(OPENCL_INCLUDE_DIRS "\${OpenCL_INCLUDE_DIR}" CACHE PATH "OpenCL Include Dirs" FORCE)
  set(OPENCL_LIBRARIES "\${OpenCL_LIBRARY}" CACHE PATH "OpenCL Libraries" FORCE)
endif()
EOF
  
  # Use our custom toolchain file
  CMAKE_FLAGS=("${CMAKE_FLAGS[@]/-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE/-DCMAKE_TOOLCHAIN_FILE=$CUSTOM_TOOLCHAIN_FILE}")
  
  # Try to build with GPU acceleration
  if ! cmake -S "$LLAMA_CPP_DIR" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"; then
    echo -e "${RED}Failed to configure with OpenCL.${NC}"
    echo -e "${RED}Please check the CMake error messages above.${NC}"
    echo -e "${YELLOW}Please ensure you have the proper dependencies installed with --install-deps${NC}"
    
    # If the error is about OpenCL, try to build without it
    if grep -q "Could NOT find OpenCL" "$BUILD_DIR/CMakeFiles/CMakeError.log" 2>/dev/null; then
      echo -e "${YELLOW}OpenCL detection failed, trying again with CPU only...${NC}"
      CMAKE_FLAGS=("${CMAKE_FLAGS[@]/-DGGML_OPENCL=ON/-DGGML_OPENCL=OFF}")
      if ! cmake -S "$LLAMA_CPP_DIR" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"; then
        echo -e "${RED}Failed to configure even without OpenCL.${NC}"
        exit 1
      else
        echo -e "${GREEN}Successfully configured without OpenCL.${NC}"
        BUILD_OPENCL=false
      fi
    else
      exit 1
    fi
  fi
  
  # Build with CMake
  echo -e "${YELLOW}Building for $ABI...${NC}"
  if ! cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j "$N_CORES"; then
    echo -e "${RED}Build failed. Please check error messages above.${NC}"
    exit 1
  fi
  
  # Check if static library is built
  if [ -f "$BUILD_DIR/src/libllama.a" ]; then
    echo -e "${GREEN}Static library libllama.a built successfully for $ABI${NC}"
    
    # Create shared library explicitly
    echo -e "${YELLOW}Creating shared library for $ABI...${NC}"
    
    # Setup proper clang path based on OS
    CLANG_PATH="$NDK_PATH/toolchains/llvm/prebuilt"
    if [[ "$OSTYPE" == "linux-gnu"* ]]; then
      CLANG_PATH="$CLANG_PATH/linux-x86_64"
    elif [[ "$OSTYPE" == "darwin"* ]]; then
      if [[ $(uname -m) == "arm64" ]]; then
        CLANG_PATH="$CLANG_PATH/darwin-arm64"
      else
        CLANG_PATH="$CLANG_PATH/darwin-x86_64"
      fi
    elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
      CLANG_PATH="$CLANG_PATH/windows-x86_64"
    fi
    
    CLANG_BIN=""
    if [ "$ABI" = "arm64-v8a" ]; then
      CLANG_BIN="aarch64-linux-android"
    elif [ "$ABI" = "x86_64" ]; then
      CLANG_BIN="x86_64-linux-android"
    fi
    
    # Full path to clang compiler
    CLANG_EXEC="$CLANG_PATH/bin/${CLANG_BIN}${ANDROID_MIN_SDK}-clang++"
    
    if [ ! -f "$CLANG_EXEC" ]; then
      echo -e "${RED}Clang not found at: $CLANG_EXEC${NC}"
      echo -e "${YELLOW}Looking for clang...${NC}"
      CLANG_EXEC=$(find "$NDK_PATH" -name "${CLANG_BIN}${ANDROID_MIN_SDK}-clang++" | head -1)
      
      if [ -z "$CLANG_EXEC" ]; then
        echo -e "${RED}Could not find clang for $ABI${NC}"
        exit 1
      else
        echo -e "${GREEN}Found clang at: $CLANG_EXEC${NC}"
      fi
    fi
    
    # Use ABI-specific OpenCL library
    OPENCL_ABI_LIB_DIR="$OPENCL_LIB_DIR/$ABI"
    
    # Set OpenCL library for linking
    OPENCL_LIB=""
    if [ "$BUILD_OPENCL" = true ]; then
      if [ -f "$OPENCL_ABI_LIB_DIR/libOpenCL.so" ]; then
        OPENCL_LIB="$OPENCL_ABI_LIB_DIR/libOpenCL.so"
        echo -e "${GREEN}Using OpenCL library for $ABI: $OPENCL_LIB${NC}"
      else
        echo -e "${RED}libOpenCL.so not found at expected location for $ABI: $OPENCL_ABI_LIB_DIR/libOpenCL.so${NC}"
        echo -e "${YELLOW}Checking alternative locations...${NC}"
        
        # Check in NDK sysroot - use architecture-specific directory
        local NDK_SYSROOT_LIB
        if [ "$ABI" = "arm64-v8a" ]; then
          NDK_SYSROOT_LIB="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/libOpenCL.so"
        elif [ "$ABI" = "x86_64" ]; then
          NDK_SYSROOT_LIB="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/x86_64-linux-android/libOpenCL.so"
        fi
        
        if [ -f "$NDK_SYSROOT_LIB" ]; then
          OPENCL_LIB="$NDK_SYSROOT_LIB"
          echo -e "${GREEN}Found OpenCL library in NDK sysroot for $ABI: $OPENCL_LIB${NC}"
        else
          # Look for OpenCL library for this specific ABI in the project
          OPENCL_LIB=$(find "$PROJECT_ROOT" -name "libOpenCL.so" -path "*$ABI*" | head -1)
          
          if [ -n "$OPENCL_LIB" ]; then
            echo -e "${GREEN}Found OpenCL library for $ABI at: $OPENCL_LIB${NC}"
          else
            echo -e "${YELLOW}Could not find libOpenCL.so for $ABI. Building without OpenCL support.${NC}"
            BUILD_OPENCL=false
          fi
        fi
      fi
    fi
    
    # Collect all libraries for linking
    CORE_LIBS=("$BUILD_DIR/src/libllama.a" "$BUILD_DIR/ggml/src/libggml.a" "$BUILD_DIR/ggml/src/libggml-cpu.a" "$BUILD_DIR/common/libcommon.a")
    
    # Add OpenCL specific libraries if OpenCL is enabled
    if [ "$BUILD_OPENCL" = true ] && [ -f "$BUILD_DIR/ggml/src/ggml-opencl/libggml-opencl.a" ]; then
      CORE_LIBS+=("$BUILD_DIR/ggml/src/ggml-opencl/libggml-opencl.a")
    fi
    
    # Join array elements with spaces
    LIBS_STRING="${CORE_LIBS[*]}"
    
    # Linking command with exact paths to libraries
    echo -e "${YELLOW}Linking shared library for $ABI...${NC}"
    
    OPENCL_LINK_ARG=""
    if [ "$BUILD_OPENCL" = true ] && [ -n "$OPENCL_LIB" ]; then
      OPENCL_LINK_ARG="$OPENCL_LIB"
    fi
    
    if ! "$CLANG_EXEC" -shared -fPIC -o "$BUILD_DIR/libllama.so" \
      -Wl,--whole-archive $LIBS_STRING \
      -Wl,--no-whole-archive -landroid -llog $OPENCL_LINK_ARG; then
      
      echo -e "${RED}❌ Failed to create shared library for $ABI${NC}"
      
      # Try linking without OpenCL as a last resort
      if [ "$BUILD_OPENCL" = true ] && [ -n "$OPENCL_LIB" ]; then
        echo -e "${YELLOW}Trying to link without OpenCL...${NC}"
        if ! "$CLANG_EXEC" -shared -fPIC -o "$BUILD_DIR/libllama.so" \
          -Wl,--whole-archive $LIBS_STRING \
          -Wl,--no-whole-archive -landroid -llog; then
          echo -e "${RED}❌ All linking attempts failed for $ABI${NC}"
          exit 1
        else
          echo -e "${GREEN}✅ Successfully linked without OpenCL for $ABI${NC}"
        fi
      else
        exit 1
      fi
    fi
    
    # Copy library to JNI directory
    cp "$BUILD_DIR/libllama.so" "$ANDROID_JNI_DIR/$ABI/"
    echo -e "${GREEN}✅ Successfully built libllama.so for $ABI${NC}"
    
    # Also copy OpenCL library if available for runtime
    if [ "$BUILD_OPENCL" = true ] && [ -n "$OPENCL_LIB" ] && [ -f "$OPENCL_LIB" ]; then
      cp "$OPENCL_LIB" "$ANDROID_JNI_DIR/$ABI/"
      echo -e "${GREEN}✅ Copied OpenCL library to JNI dir for runtime use${NC}"
    fi
  else
    echo -e "${RED}❌ Failed to build static library for $ABI${NC}"
    exit 1
  fi
}

# Start the build process
echo -e "${YELLOW}Starting Android build process...${NC}"
t0=$(date +%s)

# Build for requested ABIs
if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "arm64-v8a" ]; then
  build_for_abi "arm64-v8a"
fi

if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "x86_64" ]; then
  build_for_abi "x86_64"
fi

t1=$(date +%s)
echo -e "${GREEN}Android build completed in $((t1 - t0)) seconds${NC}"
echo -e "${GREEN}Libraries are in ${ANDROID_JNI_DIR}${NC}"
