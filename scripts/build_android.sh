#!/bin/bash
set -e

# Get the absolute path of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# Get project root directory (one level up from script dir)
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Source the version information
. "$SCRIPT_DIR/used_version.sh"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Print usage information
print_usage() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  --help                 Print this help message"
  echo "  --abi=[all|arm64-v8a|x86_64]  Specify which ABI to build for (default: all)"
  echo "  --no-opencl            Disable OpenCL GPU acceleration"
  echo "  --no-vulkan            Disable Vulkan GPU acceleration"
  echo "  --vulkan               Enable Vulkan GPU acceleration (default)"
  echo "  --debug                Build in debug mode"
  echo "  --clean                Clean previous builds before building"
  echo "  --clean-prebuilt       Clean entire prebuilt directory for a fresh start"
  echo "  --install-deps         Install dependencies (OpenCL, etc.)"
  echo "  --install-ndk          Install Android NDK version $NDK_VERSION"
  echo "  --glslc-path=[path]    Specify a custom path to the GLSLC compiler"
}

# Default values
BUILD_ABI="all"
BUILD_OPENCL=true
BUILD_VULKAN=true  # Enable Vulkan by default
BUILD_TYPE="Release"
CLEAN_BUILD=false
CLEAN_PREBUILT=false
INSTALL_DEPS=false
INSTALL_NDK=false
CUSTOM_GLSLC_PATH=""

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
    --no-vulkan)
      BUILD_VULKAN=false
      ;;
    --vulkan)
      BUILD_VULKAN=true
      ;;
    --debug)
      BUILD_TYPE="Debug"
      ;;
    --clean)
      CLEAN_BUILD=true
      ;;
    --clean-prebuilt)
      CLEAN_PREBUILT=true
      ;;
    --install-deps)
      INSTALL_DEPS=true
      ;;
    --install-ndk)
      INSTALL_NDK=true
      ;;
    --glslc-path=*)
      CUSTOM_GLSLC_PATH="${arg#*=}"
      ;;
    *)
      echo -e "${RED}Unknown argument: $arg${NC}"
      print_usage
      exit 1
      ;;
  esac
done

# Define prebuilt directory for all intermediary files
PREBUILT_DIR="$PROJECT_ROOT/prebuilt"
PREBUILT_LIBS_DIR="$PREBUILT_DIR/libs"
PREBUILT_EXTERNAL_DIR="$PREBUILT_LIBS_DIR/external"

# Define directories
ANDROID_DIR="$PROJECT_ROOT/android"
ANDROID_JNI_DIR="$ANDROID_DIR/src/main/jniLibs"
ANDROID_CPP_DIR="$ANDROID_DIR/src/main/cpp"
CPP_DIR="$PROJECT_ROOT/cpp"
LLAMA_CPP_DIR="$CPP_DIR/llama.cpp"

# Third-party directories in prebuilt directory
THIRD_PARTY_DIR="$PREBUILT_DIR/third_party"
OPENCL_HEADERS_DIR="$THIRD_PARTY_DIR/OpenCL-Headers"
OPENCL_LOADER_DIR="$THIRD_PARTY_DIR/OpenCL-ICD-Loader"
OPENCL_INCLUDE_DIR="$PREBUILT_EXTERNAL_DIR/opencl/include"
OPENCL_LIB_DIR="$PREBUILT_EXTERNAL_DIR/opencl/lib"
VULKAN_HEADERS_DIR="$THIRD_PARTY_DIR/Vulkan-Headers"
VULKAN_INCLUDE_DIR="$PREBUILT_EXTERNAL_DIR/vulkan/include"

# Create necessary directories
mkdir -p "$PREBUILT_DIR"
mkdir -p "$PREBUILT_LIBS_DIR"
mkdir -p "$PREBUILT_EXTERNAL_DIR"
mkdir -p "$ANDROID_JNI_DIR/arm64-v8a"
mkdir -p "$ANDROID_JNI_DIR/x86_64"
mkdir -p "$ANDROID_CPP_DIR/include"
mkdir -p "$OPENCL_INCLUDE_DIR"
mkdir -p "$OPENCL_LIB_DIR"
mkdir -p "$VULKAN_INCLUDE_DIR"

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
if [ "$INSTALL_DEPS" = true ]; then
  # First handle OpenCL dependencies if enabled
  if [ "$BUILD_OPENCL" = true ]; then
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
  fi
  
  # Handle Vulkan dependencies
  if [ "$BUILD_VULKAN" = true ]; then
    echo -e "${YELLOW}Setting up Vulkan dependencies for Android...${NC}"
    
    # Create required directories
    mkdir -p "$VULKAN_INCLUDE_DIR/vulkan"
    
    # First check if Vulkan-Headers repo exists
    if [ ! -d "$VULKAN_HEADERS_DIR" ]; then
      echo -e "${YELLOW}Cloning Vulkan-Headers...${NC}"
      git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git "$VULKAN_HEADERS_DIR"
    fi
    
    # Check if we have the main Vulkan headers and copy them
    if [ -d "$VULKAN_HEADERS_DIR/include/vulkan" ]; then
      echo -e "${GREEN}Found Vulkan headers in Vulkan-Headers repository${NC}"
      cp -r "$VULKAN_HEADERS_DIR/include/vulkan/"* "$VULKAN_INCLUDE_DIR/vulkan/"
      
      # Also copy vk_video headers if they exist
      if [ -d "$VULKAN_HEADERS_DIR/include/vk_video" ]; then
        echo -e "${GREEN}Found vk_video headers in Vulkan-Headers repository${NC}"
        mkdir -p "$VULKAN_INCLUDE_DIR/vk_video"
        cp -r "$VULKAN_HEADERS_DIR/include/vk_video/"* "$VULKAN_INCLUDE_DIR/vk_video/"
      fi
      
      # Download Vulkan C++ header if needed
      if [ ! -f "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" ]; then
        echo -e "${YELLOW}Downloading vulkan.hpp...${NC}"
        if command -v wget &> /dev/null; then
          wget -q --show-progress "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Hpp/main/vulkan/vulkan.hpp" -O "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp"
        elif command -v curl &> /dev/null; then
          curl -s -o "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Hpp/main/vulkan/vulkan.hpp"
        fi
      fi
      
      # Download any missing vk_video headers
      if [ ! -d "$VULKAN_INCLUDE_DIR/vk_video" ]; then
        echo -e "${YELLOW}Downloading vk_video headers...${NC}"
        mkdir -p "$VULKAN_INCLUDE_DIR/vk_video"
        
        VK_VIDEO_HEADERS=(
          "vulkan_video_codec_h264std.h"
          "vulkan_video_codec_h264std_decode.h"
          "vulkan_video_codec_h264std_encode.h"
          "vulkan_video_codec_h265std.h"
          "vulkan_video_codec_h265std_decode.h"
          "vulkan_video_codec_h265std_encode.h"
          "vulkan_video_codec_av1std.h"
          "vulkan_video_codec_av1std_decode.h"
          "vulkan_video_codecs_common.h"
        )
        
        for HEADER in "${VK_VIDEO_HEADERS[@]}"; do
          if [ ! -f "$VULKAN_INCLUDE_DIR/vk_video/$HEADER" ]; then
            if command -v wget &> /dev/null; then
              wget -q "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/main/include/vk_video/$HEADER" -O "$VULKAN_INCLUDE_DIR/vk_video/$HEADER"
            elif command -v curl &> /dev/null; then
              curl -s -o "$VULKAN_INCLUDE_DIR/vk_video/$HEADER" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/main/include/vk_video/$HEADER"
            fi
          fi
        done
      fi
      
      # Copy headers to NDK sysroot
      NDK_SYSROOT_INCLUDE="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include"
      if [ -d "$NDK_SYSROOT_INCLUDE" ]; then
        echo -e "${YELLOW}Copying Vulkan headers to NDK sysroot...${NC}"
        mkdir -p "$NDK_SYSROOT_INCLUDE/vulkan"
        cp -r "$VULKAN_INCLUDE_DIR/vulkan/"* "$NDK_SYSROOT_INCLUDE/vulkan/"
        
        # Also copy vk_video headers
        if [ -d "$VULKAN_INCLUDE_DIR/vk_video" ]; then
          mkdir -p "$NDK_SYSROOT_INCLUDE/vk_video"
          cp -r "$VULKAN_INCLUDE_DIR/vk_video/"* "$NDK_SYSROOT_INCLUDE/vk_video/"
        fi
      fi
      
      # Setup Vulkan shader compilation with GLSLC - crucial for cross-compilation
      echo -e "${YELLOW}Setting up Vulkan shader compiler (GLSLC)...${NC}"
      
      # First, check if a custom GLSLC path was provided
      if [ -n "$CUSTOM_GLSLC_PATH" ]; then
        if [ -f "$CUSTOM_GLSLC_PATH" ]; then
          echo -e "${GREEN}Using custom GLSLC: $CUSTOM_GLSLC_PATH${NC}"
          GLSLC_EXECUTABLE="$CUSTOM_GLSLC_PATH"
          chmod +x "$GLSLC_EXECUTABLE" || true
        else
          echo -e "${RED}Custom GLSLC path provided but file does not exist: $CUSTOM_GLSLC_PATH${NC}"
          GLSLC_EXECUTABLE=""
        fi
      # If not, try to find GLSLC in the environment
      elif [ -n "$GLSLC_EXECUTABLE" ] && [ -f "$GLSLC_EXECUTABLE" ]; then
        echo -e "${GREEN}Using GLSLC from environment: $GLSLC_EXECUTABLE${NC}"
        chmod +x "$GLSLC_EXECUTABLE" || true
      else
        # Try to find GLSLC in the NDK shader-tools
        GLSLC_EXECUTABLE=""
        NDK_SHADER_TOOLS="$NDK_PATH/shader-tools/linux-x86_64"
        
        if [ -d "$NDK_SHADER_TOOLS" ]; then
          GLSLC_EXECUTABLE="$NDK_SHADER_TOOLS/glslc"
          if [ -f "$GLSLC_EXECUTABLE" ]; then
            echo -e "${GREEN}Using GLSLC from NDK shader-tools: $GLSLC_EXECUTABLE${NC}"
            chmod +x "$GLSLC_EXECUTABLE" || true
          else
            echo -e "${YELLOW}GLSLC not found in expected NDK location: $GLSLC_EXECUTABLE${NC}"
            GLSLC_EXECUTABLE=""
          fi
        fi
        
        # If we didn't find GLSLC in the standard location, search more broadly
        if [ -z "$GLSLC_EXECUTABLE" ]; then
          echo -e "${YELLOW}Searching for GLSLC in the NDK...${NC}"
          
          # Use different find syntax based on OS
          if [[ "$OSTYPE" == "darwin"* ]]; then
            # macOS version - doesn't support -executable flag
            GLSLC_EXECUTABLE=$(find "$NDK_PATH" -name "glslc" -type f -perm +111 2>/dev/null | head -1)
          else
            # Linux version
            GLSLC_EXECUTABLE=$(find "$NDK_PATH" -name "glslc" -type f -executable 2>/dev/null | head -1)
          fi
          
          if [ -n "$GLSLC_EXECUTABLE" ]; then
            echo -e "${GREEN}Found GLSLC at: $GLSLC_EXECUTABLE${NC}"
            chmod +x "$GLSLC_EXECUTABLE" || true
          else
            echo -e "${YELLOW}GLSLC not found in NDK, will use system GLSLC if available${NC}"
            if [[ "$OSTYPE" == "darwin"* ]]; then
              # Try standard macOS locations
              for path in "/usr/local/bin/glslc" "/opt/homebrew/bin/glslc" "/usr/bin/glslc"; do
                if [ -f "$path" ]; then
                  GLSLC_EXECUTABLE="$path"
                  echo -e "${GREEN}Found system GLSLC at: $GLSLC_EXECUTABLE${NC}"
                  break
                fi
              done
              
              if [ -z "$GLSLC_EXECUTABLE" ]; then
                GLSLC_EXECUTABLE=$(which glslc 2>/dev/null || echo "")
              fi
            else
              # Try which on Linux
              GLSLC_EXECUTABLE=$(which glslc 2>/dev/null || echo "")
            fi
            
            if [ -z "$GLSLC_EXECUTABLE" ]; then
              echo -e "${RED}GLSLC not found in system. This may cause issues with Vulkan shader compilation.${NC}"
            else
              echo -e "${GREEN}Using system GLSLC: $GLSLC_EXECUTABLE${NC}"
            fi
          fi
        fi
      fi
      
      # Verify GLSLC permissions and existence
      if [ -n "$GLSLC_EXECUTABLE" ]; then
        echo -e "${YELLOW}Checking GLSLC executable permissions...${NC}"
        ls -la "$GLSLC_EXECUTABLE" || echo "Cannot access GLSLC file"
        
        if [ -f "$GLSLC_EXECUTABLE" ] && [ ! -x "$GLSLC_EXECUTABLE" ]; then
          echo -e "${YELLOW}GLSLC is not executable, attempting to fix...${NC}"
          chmod +x "$GLSLC_EXECUTABLE" || true
        fi
      else
        echo -e "${RED}WARNING: No GLSLC compiler found! Vulkan shader compilation may fail.${NC}"
      fi
    else
      echo -e "${RED}Vulkan headers not found in expected location. Disabling Vulkan.${NC}"
      BUILD_VULKAN=false
    fi
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
  echo -e "${YELLOW}Running setupLlamaCpp.sh to initialize it...${NC}"
  "$SCRIPT_DIR/setupLlamaCpp.sh" init
  if [ ! -d "$LLAMA_CPP_DIR" ]; then
    echo -e "${RED}Failed to initialize llama.cpp${NC}"
    exit 1
  fi
fi

# Check that we're using the correct version
if [ -d "$LLAMA_CPP_DIR/.git" ]; then
  cd "$LLAMA_CPP_DIR"
  CURRENT_COMMIT=$(git rev-parse HEAD 2>/dev/null || echo "none")
  cd - > /dev/null
  
  if [ "$CURRENT_COMMIT" != "$LLAMA_CPP_COMMIT" ]; then
    echo -e "${YELLOW}llama.cpp is at commit $CURRENT_COMMIT, but we need $LLAMA_CPP_COMMIT${NC}"
    echo -e "${YELLOW}Running setupLlamaCpp.sh to update it...${NC}"
    "$SCRIPT_DIR/setupLlamaCpp.sh" init
  fi
else
  echo -e "${YELLOW}llama.cpp is not a git repository. Running setupLlamaCpp.sh to properly initialize it...${NC}"
  "$SCRIPT_DIR/setupLlamaCpp.sh" init
fi

# Copy necessary headers
cp "$LLAMA_CPP_DIR/include/llama.h" "$ANDROID_CPP_DIR/include/"
cp "$LLAMA_CPP_DIR/include/llama-cpp.h" "$ANDROID_CPP_DIR/include/"

# Setup Vulkan dependencies
setup_vulkan() {
  echo -e "${YELLOW}Setting up Vulkan dependencies for Android...${NC}"
  
  # Create required directories
  mkdir -p "$VULKAN_INCLUDE_DIR/vulkan"
  
  # First check if Vulkan-Headers repo exists
  if [ ! -d "$VULKAN_HEADERS_DIR" ]; then
    echo -e "${YELLOW}Cloning Vulkan-Headers...${NC}"
    git clone --depth 1 https://github.com/KhronosGroup/Vulkan-Headers.git "$VULKAN_HEADERS_DIR"
  fi
  
  # Check if we have the main Vulkan headers and copy them
  if [ -d "$VULKAN_HEADERS_DIR/include/vulkan" ]; then
    echo -e "${GREEN}Found Vulkan headers in Vulkan-Headers repository${NC}"
    cp -r "$VULKAN_HEADERS_DIR/include/vulkan/"* "$VULKAN_INCLUDE_DIR/vulkan/"
    
    # Also copy vk_video headers if they exist
    if [ -d "$VULKAN_HEADERS_DIR/include/vk_video" ]; then
      echo -e "${GREEN}Found vk_video headers in Vulkan-Headers repository${NC}"
      mkdir -p "$VULKAN_INCLUDE_DIR/vk_video"
      cp -r "$VULKAN_HEADERS_DIR/include/vk_video/"* "$VULKAN_INCLUDE_DIR/vk_video/"
    fi
    
    # Download Vulkan C++ header if needed
    if [ ! -f "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" ]; then
      echo -e "${YELLOW}Downloading vulkan.hpp...${NC}"
      if command -v wget &> /dev/null; then
        wget -q --show-progress "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Hpp/main/vulkan/vulkan.hpp" -O "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp"
      elif command -v curl &> /dev/null; then
        curl -s -o "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Hpp/main/vulkan/vulkan.hpp"
      fi
    fi
    
    # Download any missing vk_video headers
    if [ ! -d "$VULKAN_INCLUDE_DIR/vk_video" ]; then
      echo -e "${YELLOW}Downloading vk_video headers...${NC}"
      mkdir -p "$VULKAN_INCLUDE_DIR/vk_video"
      
      VK_VIDEO_HEADERS=(
        "vulkan_video_codec_h264std.h"
        "vulkan_video_codec_h264std_decode.h"
        "vulkan_video_codec_h264std_encode.h"
        "vulkan_video_codec_h265std.h"
        "vulkan_video_codec_h265std_decode.h"
        "vulkan_video_codec_h265std_encode.h"
        "vulkan_video_codec_av1std.h"
        "vulkan_video_codec_av1std_decode.h"
        "vulkan_video_codecs_common.h"
      )
      
      for HEADER in "${VK_VIDEO_HEADERS[@]}"; do
        if [ ! -f "$VULKAN_INCLUDE_DIR/vk_video/$HEADER" ]; then
          if command -v wget &> /dev/null; then
            wget -q "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/main/include/vk_video/$HEADER" -O "$VULKAN_INCLUDE_DIR/vk_video/$HEADER"
          elif command -v curl &> /dev/null; then
            curl -s -o "$VULKAN_INCLUDE_DIR/vk_video/$HEADER" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/main/include/vk_video/$HEADER"
          fi
        fi
      done
    fi
    
    # Copy headers to NDK sysroot
    NDK_SYSROOT_INCLUDE="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include"
    if [ -d "$NDK_SYSROOT_INCLUDE" ]; then
      echo -e "${YELLOW}Copying Vulkan headers to NDK sysroot...${NC}"
      mkdir -p "$NDK_SYSROOT_INCLUDE/vulkan"
      cp -r "$VULKAN_INCLUDE_DIR/vulkan/"* "$NDK_SYSROOT_INCLUDE/vulkan/"
      
      # Also copy vk_video headers
      if [ -d "$VULKAN_INCLUDE_DIR/vk_video" ]; then
        mkdir -p "$NDK_SYSROOT_INCLUDE/vk_video"
        cp -r "$VULKAN_INCLUDE_DIR/vk_video/"* "$NDK_SYSROOT_INCLUDE/vk_video/"
      fi
    fi
    
    # Setup Vulkan shader compilation with GLSLC - crucial for cross-compilation
    echo -e "${YELLOW}Setting up Vulkan shader compiler (GLSLC)...${NC}"
    
    # First, check if a custom GLSLC path was provided
    if [ -n "$CUSTOM_GLSLC_PATH" ]; then
      if [ -f "$CUSTOM_GLSLC_PATH" ]; then
        echo -e "${GREEN}Using custom GLSLC: $CUSTOM_GLSLC_PATH${NC}"
        GLSLC_EXECUTABLE="$CUSTOM_GLSLC_PATH"
        chmod +x "$GLSLC_EXECUTABLE" || true
      else
        echo -e "${RED}Custom GLSLC path provided but file does not exist: $CUSTOM_GLSLC_PATH${NC}"
        GLSLC_EXECUTABLE=""
      fi
    # If not, try to find GLSLC in the environment
    elif [ -n "$GLSLC_EXECUTABLE" ] && [ -f "$GLSLC_EXECUTABLE" ]; then
      echo -e "${GREEN}Using GLSLC from environment: $GLSLC_EXECUTABLE${NC}"
      chmod +x "$GLSLC_EXECUTABLE" || true
    else
      # Try to find GLSLC in the NDK shader-tools
      GLSLC_EXECUTABLE=""
      NDK_SHADER_TOOLS="$NDK_PATH/shader-tools/linux-x86_64"
      
      if [ -d "$NDK_SHADER_TOOLS" ]; then
        GLSLC_EXECUTABLE="$NDK_SHADER_TOOLS/glslc"
        if [ -f "$GLSLC_EXECUTABLE" ]; then
          echo -e "${GREEN}Using GLSLC from NDK shader-tools: $GLSLC_EXECUTABLE${NC}"
          chmod +x "$GLSLC_EXECUTABLE" || true
        else
          echo -e "${YELLOW}GLSLC not found in expected NDK location: $GLSLC_EXECUTABLE${NC}"
          GLSLC_EXECUTABLE=""
        fi
      fi
      
      # If we didn't find GLSLC in the standard location, search more broadly
      if [ -z "$GLSLC_EXECUTABLE" ]; then
        echo -e "${YELLOW}Searching for GLSLC in the NDK...${NC}"
        
        # Use different find syntax based on OS
        if [[ "$OSTYPE" == "darwin"* ]]; then
          # macOS version - doesn't support -executable flag
          GLSLC_EXECUTABLE=$(find "$NDK_PATH" -name "glslc" -type f -perm +111 2>/dev/null | head -1)
        else
          # Linux version
          GLSLC_EXECUTABLE=$(find "$NDK_PATH" -name "glslc" -type f -executable 2>/dev/null | head -1)
        fi
        
        if [ -n "$GLSLC_EXECUTABLE" ]; then
          echo -e "${GREEN}Found GLSLC at: $GLSLC_EXECUTABLE${NC}"
          chmod +x "$GLSLC_EXECUTABLE" || true
        else
          echo -e "${YELLOW}GLSLC not found in NDK, will use system GLSLC if available${NC}"
          if [[ "$OSTYPE" == "darwin"* ]]; then
            # Try standard macOS locations
            for path in "/usr/local/bin/glslc" "/opt/homebrew/bin/glslc" "/usr/bin/glslc"; do
              if [ -f "$path" ]; then
                GLSLC_EXECUTABLE="$path"
                echo -e "${GREEN}Found system GLSLC at: $GLSLC_EXECUTABLE${NC}"
                break
              fi
            done
            
            if [ -z "$GLSLC_EXECUTABLE" ]; then
              GLSLC_EXECUTABLE=$(which glslc 2>/dev/null || echo "")
            fi
          else
            # Try which on Linux
            GLSLC_EXECUTABLE=$(which glslc 2>/dev/null || echo "")
          fi
          
          if [ -z "$GLSLC_EXECUTABLE" ]; then
            echo -e "${RED}GLSLC not found in system. This may cause issues with Vulkan shader compilation.${NC}"
          else
            echo -e "${GREEN}Using system GLSLC: $GLSLC_EXECUTABLE${NC}"
          fi
        fi
      fi
    fi
    
    # Verify GLSLC permissions and existence
    if [ -n "$GLSLC_EXECUTABLE" ]; then
      echo -e "${YELLOW}Checking GLSLC executable permissions...${NC}"
      ls -la "$GLSLC_EXECUTABLE" || echo "Cannot access GLSLC file"
      
      if [ -f "$GLSLC_EXECUTABLE" ] && [ ! -x "$GLSLC_EXECUTABLE" ]; then
        echo -e "${YELLOW}GLSLC is not executable, attempting to fix...${NC}"
        chmod +x "$GLSLC_EXECUTABLE" || true
      fi
    else
      echo -e "${RED}WARNING: No GLSLC compiler found! Vulkan shader compilation may fail.${NC}"
    fi
    
    return 0
  else
    echo -e "${RED}Vulkan headers not found in expected location. Disabling Vulkan.${NC}"
    return 1
  fi
}

# Setup dependencies if needed
if [ "$BUILD_VULKAN" = true ]; then
  echo -e "${YELLOW}Setting up Vulkan for build...${NC}"
  setup_vulkan || {
    echo -e "${RED}Failed to setup Vulkan dependencies. Disabling Vulkan support.${NC}"
    BUILD_VULKAN=false
  }
fi

# Function to build for a specific ABI
build_for_abi() {
  local ABI=$1
  echo -e "${YELLOW}Building for $ABI...${NC}"
  
  local BUILD_DIR="$PREBUILT_DIR/build-android-$ABI"
  
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
  CMAKE_FLAGS+=("-DBUILD_SHARED_LIBS=OFF")  # Build static libraries, we'll create the shared lib manually
  CMAKE_FLAGS+=("-DLLAMA_BUILD_TESTS=OFF")
  CMAKE_FLAGS+=("-DLLAMA_BUILD_EXAMPLES=OFF")
  CMAKE_FLAGS+=("-DLLAMA_CURL=OFF")  # Disable CURL to avoid dependency issues

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
  
  # Configure Vulkan
  if [ "$BUILD_VULKAN" = true ]; then
    # Configure Vulkan for CMake
    CMAKE_FLAGS+=("-DGGML_VULKAN=ON")
    CMAKE_FLAGS+=("-DLLAMA_VULKAN=ON")
    CMAKE_FLAGS+=("-DVK_USE_PLATFORM_ANDROID_KHR=ON")
    CMAKE_FLAGS+=("-DGGML_BUILD_VULKAN_BACKEND=ON")
    
    # Add Vulkan C/CXX flags for better integration
    CMAKE_FLAGS+=("-DCMAKE_C_FLAGS=-DVK_USE_PLATFORM_ANDROID_KHR=1 -DGGML_VULKAN=1 ${CMAKE_C_FLAGS}")
    CMAKE_FLAGS+=("-DCMAKE_CXX_FLAGS=-DVK_USE_PLATFORM_ANDROID_KHR=1 -DGGML_VULKAN=1 ${CMAKE_CXX_FLAGS}")
    
    # Add Vulkan include directories
    ABS_VULKAN_INCLUDE_DIR=$(readlink -f "$VULKAN_INCLUDE_DIR")
    CMAKE_FLAGS+=("-DVulkan_INCLUDE_DIR=$ABS_VULKAN_INCLUDE_DIR")
    
    # Enable Vulkan shader compilation with GLSLC
    if [ -n "$GLSLC_EXECUTABLE" ]; then
      CMAKE_FLAGS+=("-DGGML_VULKAN_SHADER_EMBED_GLSLC_PATH=$GLSLC_EXECUTABLE")
      CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
      
      # Add specific configuration for embedded shader compilation
      CMAKE_FLAGS+=("-DGGML_VULKAN_KEEP_SHADER_SOURCE=ON")  # For debugging purposes
      CMAKE_FLAGS+=("-DGGML_VULKAN_COOPMAT_GLSLC_SUPPORT=ON")  # Extended support
    fi
    
    # Find Vulkan library for this ABI in the NDK
    NDK_VULKAN_LIB=""
    if [ "$ABI" = "arm64-v8a" ]; then
      NDK_VULKAN_LIB="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/aarch64-linux-android/$ANDROID_MIN_SDK/libvulkan.so"
    elif [ "$ABI" = "x86_64" ]; then
      NDK_VULKAN_LIB="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/x86_64-linux-android/$ANDROID_MIN_SDK/libvulkan.so"
    fi
    
    # If not found in the primary location, search in the NDK
    if [ ! -f "$NDK_VULKAN_LIB" ]; then
      echo -e "${YELLOW}Vulkan library not found at expected location, searching...${NC}"
      if [ "$ABI" = "arm64-v8a" ]; then
        NDK_VULKAN_LIB=$(find "$NDK_PATH" -name "libvulkan.so" | grep "aarch64" | head -1)
      elif [ "$ABI" = "x86_64" ]; then
        NDK_VULKAN_LIB=$(find "$NDK_PATH" -name "libvulkan.so" | grep "x86_64" | head -1)
      fi
    fi
    
    # Set Vulkan library path if found
    if [ -f "$NDK_VULKAN_LIB" ]; then
      echo -e "${GREEN}Found Vulkan library: $NDK_VULKAN_LIB${NC}"
      CMAKE_FLAGS+=("-DVulkan_LIBRARY=$NDK_VULKAN_LIB")
    else
      echo -e "${YELLOW}Vulkan library not found, will try to link dynamically${NC}"
    fi
    
    echo -e "${GREEN}Vulkan configuration complete${NC}"
  else
    CMAKE_FLAGS+=("-DGGML_VULKAN=OFF")
    CMAKE_FLAGS+=("-DLLAMA_VULKAN=OFF")
    echo -e "${YELLOW}Building without Vulkan support${NC}"
  fi
  
  # Create a custom toolchain file to properly handle OpenCL/Vulkan detection
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

# Force Vulkan to be found if we're building with Vulkan
if(DEFINED Vulkan_INCLUDE_DIR AND DEFINED Vulkan_LIBRARY)
  set(Vulkan_FOUND TRUE CACHE BOOL "Vulkan Found" FORCE)
  set(VULKAN_FOUND TRUE CACHE BOOL "Vulkan Found" FORCE)
  set(VULKAN_INCLUDE_DIRS "\${Vulkan_INCLUDE_DIR}" CACHE PATH "Vulkan Include Dirs" FORCE)
  set(VULKAN_LIBRARIES "\${Vulkan_LIBRARY}" CACHE PATH "Vulkan Libraries" FORCE)
endif()
EOF
  
  # Use our custom toolchain file
  CMAKE_FLAGS=("${CMAKE_FLAGS[@]/-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE/-DCMAKE_TOOLCHAIN_FILE=$CUSTOM_TOOLCHAIN_FILE}")
  
  # Try to build with GPU acceleration
  if ! cmake -S "$LLAMA_CPP_DIR" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"; then
    echo -e "${RED}Failed to configure with OpenCL/Vulkan support.${NC}"
    echo -e "${RED}Please check the CMake error messages above.${NC}"
    exit 1
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
    
    # Set OpenCL library for linking
    OPENCL_LINK_ARG=""
    if [ "$BUILD_OPENCL" = true ]; then
      OPENCL_ABI_LIB_DIR="$OPENCL_LIB_DIR/$ABI"
      if [ -f "$OPENCL_ABI_LIB_DIR/libOpenCL.so" ]; then
        OPENCL_LINK_ARG="$OPENCL_ABI_LIB_DIR/libOpenCL.so"
        echo -e "${GREEN}Using OpenCL library for linking: $OPENCL_LINK_ARG${NC}"
      fi
    fi
    
    # Set Vulkan library for linking
    VULKAN_LINK_ARG=""
    if [ "$BUILD_VULKAN" = true ] && [ -n "$NDK_VULKAN_LIB" ]; then
      VULKAN_LINK_ARG="$NDK_VULKAN_LIB"
      echo -e "${GREEN}Using Vulkan library for linking: $VULKAN_LINK_ARG${NC}"
    fi
    
    # Collect all libraries for linking
    CORE_LIBS=("$BUILD_DIR/src/libllama.a" "$BUILD_DIR/ggml/src/libggml.a" "$BUILD_DIR/ggml/src/libggml-cpu.a" "$BUILD_DIR/common/libcommon.a")
    
    # Add OpenCL library if available
    if [ "$BUILD_OPENCL" = true ] && [ -f "$BUILD_DIR/ggml/src/ggml-opencl/libggml-opencl.a" ]; then
      echo -e "${GREEN}Adding OpenCL library: $BUILD_DIR/ggml/src/ggml-opencl/libggml-opencl.a${NC}"
      CORE_LIBS+=("$BUILD_DIR/ggml/src/ggml-opencl/libggml-opencl.a")
    fi
    
    # Add Vulkan library if available
    if [ "$BUILD_VULKAN" = true ] && [ -f "$BUILD_DIR/ggml/src/ggml-vulkan/libggml-vulkan.a" ]; then
      echo -e "${GREEN}Adding Vulkan library: $BUILD_DIR/ggml/src/ggml-vulkan/libggml-vulkan.a${NC}"
      CORE_LIBS+=("$BUILD_DIR/ggml/src/ggml-vulkan/libggml-vulkan.a")
    else
      # Try to find the Vulkan library elsewhere
      if [ "$BUILD_VULKAN" = true ]; then
        echo -e "${YELLOW}Searching for Vulkan library component...${NC}"
        VULKAN_LIB_PATH=$(find "$BUILD_DIR" -name "libggml-vulkan.a" | head -1)
        if [ -n "$VULKAN_LIB_PATH" ]; then
          echo -e "${GREEN}Found Vulkan library at: $VULKAN_LIB_PATH${NC}"
          CORE_LIBS+=("$VULKAN_LIB_PATH")
        fi
      fi
    fi
    
    # Join array elements with spaces
    LIBS_STRING="${CORE_LIBS[*]}"
    
    # Add necessary compilation flags for Vulkan and OpenCL
    COMPILER_FLAGS=""
    if [ "$BUILD_VULKAN" = true ]; then
      COMPILER_FLAGS="$COMPILER_FLAGS -DGGML_VULKAN=1 -DVK_USE_PLATFORM_ANDROID_KHR=1"
    fi
    if [ "$BUILD_OPENCL" = true ]; then
      COMPILER_FLAGS="$COMPILER_FLAGS -DGGML_OPENCL=1"
    fi
    
    # Create the shared library
    echo -e "${YELLOW}Creating shared library with: $CLANG_EXEC${NC}"
    # Place the intermediate shared library in the prebuilt directory
    if ! "$CLANG_EXEC" $COMPILER_FLAGS -shared -fPIC -o "$BUILD_DIR/libllama.so" \
      -Wl,--whole-archive $LIBS_STRING \
      -Wl,--no-whole-archive -landroid -llog $OPENCL_LINK_ARG $VULKAN_LINK_ARG; then
      
      echo -e "${RED}❌ Failed to create shared library for $ABI${NC}"
      exit 1
    fi
    
    # Copy library to JNI directory (keep final libraries in original location)
    cp "$BUILD_DIR/libllama.so" "$ANDROID_JNI_DIR/$ABI/"
    echo -e "${GREEN}✅ Successfully built libllama.so for $ABI${NC}"
    echo -e "${GREEN}✅ Final library copied to $ANDROID_JNI_DIR/$ABI/libllama.so${NC}"
  else
    echo -e "${RED}❌ Failed to build static library for $ABI${NC}"
    exit 1
  fi
}

# Clean prebuilt directory if requested
if [ "$CLEAN_PREBUILT" = true ]; then
  echo -e "${YELLOW}Cleaning entire prebuilt directory for a fresh start...${NC}"
  rm -rf "$PREBUILT_DIR"
  echo -e "${GREEN}Prebuilt directory cleaned${NC}"
  
  # Recreate essential directories
  mkdir -p "$PREBUILT_DIR"
  mkdir -p "$PREBUILT_LIBS_DIR"
  mkdir -p "$PREBUILT_EXTERNAL_DIR"
  mkdir -p "$OPENCL_INCLUDE_DIR"
  mkdir -p "$OPENCL_LIB_DIR"
  mkdir -p "$VULKAN_INCLUDE_DIR"
fi

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

# Print summary of enabled GPU backends
echo -e "${YELLOW}=== Build Summary ===${NC}"
echo -e "OpenCL support: $([ "$BUILD_OPENCL" = true ] && echo -e "${GREEN}Enabled${NC}" || echo -e "${RED}Disabled${NC}")"
echo -e "Vulkan support: $([ "$BUILD_VULKAN" = true ] && echo -e "${GREEN}Enabled${NC}" || echo -e "${RED}Disabled${NC}")"

# Check built libraries for GPU symbols
if [ -f "$ANDROID_JNI_DIR/arm64-v8a/libllama.so" ]; then
  echo -e "${YELLOW}Checking arm64-v8a library for GPU acceleration:${NC}"
  
  # Check for OpenCL symbols
  if nm -D "$ANDROID_JNI_DIR/arm64-v8a/libllama.so" 2>/dev/null | grep -i "opencl" >/dev/null; then
    echo -e "  OpenCL symbols: ${GREEN}Found${NC}"
    OPENCL_SYMBOLS_FOUND=true
  else
    echo -e "  OpenCL symbols: ${RED}Not found${NC}"
    OPENCL_SYMBOLS_FOUND=false
  fi
  
  # Check for Vulkan symbols
  if nm -D "$ANDROID_JNI_DIR/arm64-v8a/libllama.so" 2>/dev/null | grep -i "vulkan" >/dev/null; then
    echo -e "  Vulkan symbols: ${GREEN}Found${NC}"
    VULKAN_SYMBOLS_FOUND=true
  else
    echo -e "  Vulkan symbols: ${RED}Not found${NC}"
    VULKAN_SYMBOLS_FOUND=false
  fi
  
  # Overall GPU support status
  if [ "$OPENCL_SYMBOLS_FOUND" = true ] || [ "$VULKAN_SYMBOLS_FOUND" = true ]; then
    echo -e "${GREEN}✅ GPU acceleration is available in the built library${NC}"
  else
    echo -e "${RED}❌ WARNING: No GPU acceleration symbols found in the built library${NC}"
    if [ "$BUILD_OPENCL" = true ] || [ "$BUILD_VULKAN" = true ]; then
      echo -e "${RED}GPU support was requested but not found in the final binary. Check build errors.${NC}"
    fi
  fi
fi

# Information about directories
echo -e "${YELLOW}=== Directory Information ===${NC}"
echo -e "Final library files: ${GREEN}$ANDROID_JNI_DIR/${NC}"
echo -e "Build intermediates: ${GREEN}$PREBUILT_DIR/${NC}"
echo -e "External dependencies: ${GREEN}$PREBUILT_EXTERNAL_DIR/${NC}"

echo -e "${YELLOW}=======================${NC}"

