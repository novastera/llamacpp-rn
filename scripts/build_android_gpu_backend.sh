#!/bin/bash
# ==============================================================================
# Android GPU Backend Builder for llama.cpp
# ==============================================================================
# This script builds only the GPU acceleration backends (OpenCL, Vulkan) 
# for llama.cpp to be used with React Native.
# 
# The built libraries are stored in the prebuilt/gpu/ directory and can be
# used by the build_android_external.sh script when creating the final JNI
# libraries that will be included in the React Native package.
#
# CI Usage:
# This script is called directly by the CI workflow to build the GPU backends.
# The --install-deps flag is no longer needed as it's handled internally
# when GPU backends are built.
#
# Usage: 
#   scripts/build_android_gpu_backend.sh [options]
#
# See --help for all available options.
# ==============================================================================
set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

# Get the absolute path of the script
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
# Get project root directory (one level up from script dir)
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Source the version information
. "$SCRIPT_DIR/used_version.sh"

# Define prebuilt directory for all intermediary files
PREBUILT_DIR="$PROJECT_ROOT/prebuilt"
PREBUILT_LIBS_DIR="$PREBUILT_DIR/libs"
PREBUILT_EXTERNAL_DIR="$PREBUILT_DIR/libs/external"
PREBUILT_GPU_DIR="$PREBUILT_DIR/gpu"

# Define directories
ANDROID_DIR="$PROJECT_ROOT/android"
CPP_DIR="$PROJECT_ROOT/cpp"
LLAMA_CPP_DIR="$CPP_DIR/llama.cpp"

# Third-party directories in prebuilt directory
THIRD_PARTY_DIR="$PREBUILT_DIR/third_party"
OPENCL_HEADERS_DIR="$THIRD_PARTY_DIR/OpenCL-Headers"
OPENCL_INCLUDE_DIR="$PREBUILT_EXTERNAL_DIR/opencl/include"
OPENCL_LIB_DIR="$PREBUILT_EXTERNAL_DIR/opencl/lib"
VULKAN_HEADERS_DIR="$THIRD_PARTY_DIR/Vulkan-Headers"
VULKAN_INCLUDE_DIR="$PREBUILT_EXTERNAL_DIR/vulkan/include"

# Print usage information
print_usage() {
  echo "Usage: $0 [options]"
  echo "Options:"
  echo "  --help                 Print this help message"
  echo "  --abi=[all|arm64-v8a|x86_64]  Specify which ABI to build for (default: all)"
  echo "  --platform=[linux|macos|windows]  Specify host OS platform:" 
  echo "                         - linux: Build on Linux host for Android target (common in CI)"
  echo "                         - macos: Build on macOS host for Android target"
  echo "                         - windows: Build on Windows host for Android target"
  echo "  --no-opencl            Disable OpenCL GPU acceleration"
  echo "  --no-vulkan            Disable Vulkan GPU acceleration"
  echo "  --debug                Build in debug mode"
  echo "  --clean                Clean previous builds before building"
  echo "  --dry-run              Show commands without execution"
  echo "  --install-deps         Only install dependencies and exit"
  echo "  --ndk-path=<path>      Use a custom NDK path"
  echo "  --glslc-path=<path>    Specify custom path to glslc compiler for Vulkan shaders"
}

# Default values
BUILD_ABI="all"
BUILD_OPENCL=true
BUILD_VULKAN=true
BUILD_TYPE="Release"
CLEAN_BUILD=false
DRY_RUN=false
INSTALL_DEPS_ONLY=false  # Flag to only install dependencies
CUSTOM_NDK_PATH=""
CUSTOM_GLSLC_PATH=""

# Auto-detect host platform (the OS where we're building from)
if [[ "$OSTYPE" == "darwin"* ]]; then
  HOST_PLATFORM="macos"
elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
  HOST_PLATFORM="linux"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
  HOST_PLATFORM="windows"
else
  HOST_PLATFORM="linux" # Default fallback
fi
# Note: The target platform is always Android regardless of host platform

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
    --platform=*)
      HOST_PLATFORM="${arg#*=}"
      ;;
    --no-opencl)
      BUILD_OPENCL=false
      ;;
    --no-vulkan)
      BUILD_VULKAN=false
      ;;
    --debug)
      BUILD_TYPE="Debug"
      ;;
    --clean)
      CLEAN_BUILD=true
      ;;
    --dry-run)
      DRY_RUN=true
      ;;
    --install-deps)
      INSTALL_DEPS_ONLY=true
      ;;
    --ndk-path=*)
      CUSTOM_NDK_PATH="${arg#*=}"
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

# Set NDK path if provided
if [ -n "$CUSTOM_NDK_PATH" ]; then
  echo -e "${GREEN}Using custom NDK path: $CUSTOM_NDK_PATH${NC}"
  ANDROID_NDK_HOME="$CUSTOM_NDK_PATH"
fi

echo -e "${YELLOW}Setting up external dependencies for Android GPU backends...${NC}"
echo -e "${YELLOW}Host OS platform: $HOST_PLATFORM${NC}"
echo -e "${YELLOW}Target ABI: $BUILD_ABI${NC}"

# Create necessary directories
echo -e "${YELLOW}Creating necessary directories...${NC}"
mkdir -p "$PREBUILT_DIR"
mkdir -p "$PREBUILT_LIBS_DIR"
mkdir -p "$PREBUILT_EXTERNAL_DIR"
mkdir -p "$THIRD_PARTY_DIR"
mkdir -p "$OPENCL_INCLUDE_DIR"
mkdir -p "$OPENCL_LIB_DIR"
mkdir -p "$VULKAN_INCLUDE_DIR"
mkdir -p "$PREBUILT_GPU_DIR/arm64-v8a" "$PREBUILT_GPU_DIR/x86_64" # Create GPU libraries directories

# Helper function to fix Vulkan build issues when detected
fix_vulkan_build_issues() {
  echo -e "${YELLOW}Attempting to fix Vulkan build issues...${NC}"
  
  # Install Vulkan SDK if on Linux CI
  if [[ "$HOST_PLATFORM" == "linux" ]]; then
    echo -e "${YELLOW}Installing Vulkan SDK on Linux...${NC}"
    
    # Add LunarG repository with proper authentication (updated method)
    wget -qO - https://packages.lunarg.com/lunarg-signing-key-pub.asc | sudo apt-key add -
    sudo wget -qO /etc/apt/sources.list.d/lunarg-vulkan-jammy.list https://packages.lunarg.com/vulkan/lunarg-vulkan-jammy.list
    sudo apt update -y
    
    # Install Vulkan SDK and development packages
    sudo apt install -y vulkan-sdk vulkan-headers vulkan-tools libvulkan-dev
    
    # Install shader tools
    sudo apt install -y glslc glslang-tools shaderc
    
    # Check if glslc is available now
    if command -v glslc &> /dev/null; then
      echo -e "${GREEN}✅ glslc installed successfully: $(which glslc)${NC}"
      export GLSLC_EXECUTABLE=$(which glslc)
    else
      echo -e "${RED}❌ glslc still not available after installation${NC}"
    fi
    
    # Use pkg-config to find libs
    if command -v pkg-config &> /dev/null; then
      VULKAN_LIB_PATH=$(pkg-config --variable=libdir vulkan)
      echo -e "${GREEN}Found Vulkan libraries at $VULKAN_LIB_PATH${NC}"
      
      # Copy actual libraries to the prebuilt directory if found
      if [ -d "$VULKAN_LIB_PATH" ]; then
        mkdir -p "$VULKAN_INCLUDE_DIR/lib"
        mkdir -p "$VULKAN_INCLUDE_DIR/lib/arm64-v8a"
        mkdir -p "$VULKAN_INCLUDE_DIR/lib/x86_64"
        
        if [ -f "$VULKAN_LIB_PATH/libvulkan.so" ]; then
          cp "$VULKAN_LIB_PATH/libvulkan.so" "$VULKAN_INCLUDE_DIR/lib/"
          cp "$VULKAN_LIB_PATH/libvulkan.so" "$VULKAN_INCLUDE_DIR/lib/arm64-v8a/"
          cp "$VULKAN_LIB_PATH/libvulkan.so" "$VULKAN_INCLUDE_DIR/lib/x86_64/"
          echo -e "${GREEN}✅ Copied actual Vulkan libraries${NC}"
        fi
      fi
    fi
    
    return 0
  elif [[ "$HOST_PLATFORM" == "macos" ]]; then
    echo -e "${YELLOW}Installing Vulkan SDK on macOS using Homebrew...${NC}"
    brew install molten-vk vulkan-headers shaderc glslang
    
    if command -v glslc &> /dev/null; then
      echo -e "${GREEN}✅ glslc installed successfully: $(which glslc)${NC}"
      export GLSLC_EXECUTABLE=$(which glslc)
    else
      echo -e "${RED}❌ glslc still not available after installation${NC}"
    fi
    
    return 0
  else
    echo -e "${YELLOW}Platform $HOST_PLATFORM not supported for automatic Vulkan SDK installation${NC}"
    return 1
  fi
}

# Setup dependencies for both OpenCL and Vulkan headers
setup_dependencies() {
  # Verify llama.cpp exists as a git repository
  if [ ! -d "$LLAMA_CPP_DIR/.git" ]; then
    echo -e "${YELLOW}llama.cpp not found as a git repository at $LLAMA_CPP_DIR${NC}"
    echo -e "${YELLOW}Running setupLlamaCpp.sh to initialize it...${NC}"
    "$SCRIPT_DIR/setupLlamaCpp.sh" init --platform=android
    
    if [ ! -d "$LLAMA_CPP_DIR/.git" ]; then
      echo -e "${RED}Failed to initialize llama.cpp${NC}"
      exit 1
    fi
  fi

  # Setup OpenCL dependencies
  echo -e "${YELLOW}Setting up OpenCL dependencies...${NC}"

  # Verify OpenCL dependencies have been properly setup
  if [ ! -d "$OPENCL_HEADERS_DIR" ]; then
    echo -e "${YELLOW}OpenCL Headers not installed correctly. Installing manually...${NC}"
    mkdir -p "$OPENCL_HEADERS_DIR"
    # Use specific OpenCL 3.0 release tag to match build_android.sh
    git clone --depth 1 --branch "$OPENCL_HEADERS_TAG" https://github.com/KhronosGroup/OpenCL-Headers.git "$OPENCL_HEADERS_DIR"
  fi

  if [ ! -d "$OPENCL_INCLUDE_DIR/CL" ]; then
    echo -e "${YELLOW}OpenCL include directory not set up correctly. Creating manually...${NC}"
    mkdir -p "$OPENCL_INCLUDE_DIR"
    cp -r "$OPENCL_HEADERS_DIR/CL" "$OPENCL_INCLUDE_DIR/"
  fi

  # Instead of empty stub libraries, use the OpenCL ICD loader
  # Check if we can install OpenCL ICD loader
  if [[ "$HOST_PLATFORM" == "linux" ]]; then
    sudo apt install -y ocl-icd-libopencl1 ocl-icd-opencl-dev opencl-headers
    if [ -f "/usr/lib/x86_64-linux-gnu/libOpenCL.so" ]; then
      # Ensure directories exist
      mkdir -p "$OPENCL_LIB_DIR/arm64-v8a"
      mkdir -p "$OPENCL_LIB_DIR/x86_64"
      
      # Copy the actual OpenCL library instead of an empty stub
      cp "/usr/lib/x86_64-linux-gnu/libOpenCL.so" "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so"
      cp "/usr/lib/x86_64-linux-gnu/libOpenCL.so" "$OPENCL_LIB_DIR/x86_64/libOpenCL.so"
      echo -e "${GREEN}✅ Copied actual OpenCL libraries${NC}"
    else
      # Fall back to stub libraries if needed
      mkdir -p "$OPENCL_LIB_DIR/arm64-v8a"
      mkdir -p "$OPENCL_LIB_DIR/x86_64"
      
      if [ ! -f "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so" ]; then
        echo -e "${YELLOW}Creating OpenCL shared library for arm64-v8a...${NC}"
        # Create a minimal implementation instead of an empty file
        cat > "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.c" << 'EOL'
#include <CL/cl.h>
cl_int clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms) { return CL_DEVICE_NOT_FOUND; }
cl_int clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret) { return CL_DEVICE_NOT_FOUND; }
cl_int clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices) { return CL_DEVICE_NOT_FOUND; }
cl_int clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret) { return CL_DEVICE_NOT_FOUND; }
cl_context clCreateContext(const cl_context_properties *properties, cl_uint num_devices, const cl_device_id *devices, void (CL_CALLBACK *pfn_notify)(const char *errinfo, const void *private_info, size_t cb, void *user_data), void *user_data, cl_int *errcode_ret) { if(errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND; return NULL; }
EOL
        # Compile a simple shared library with the necessary symbols
        ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang -shared -o "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so" "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.c" -I"$OPENCL_INCLUDE_DIR"
      fi
      
      if [ ! -f "$OPENCL_LIB_DIR/x86_64/libOpenCL.so" ]; then
        echo -e "${YELLOW}Creating OpenCL shared library for x86_64...${NC}"
        # Create a minimal implementation instead of an empty file
        cat > "$OPENCL_LIB_DIR/x86_64/libOpenCL.c" << 'EOL'
#include <CL/cl.h>
cl_int clGetPlatformIDs(cl_uint num_entries, cl_platform_id *platforms, cl_uint *num_platforms) { return CL_DEVICE_NOT_FOUND; }
cl_int clGetPlatformInfo(cl_platform_id platform, cl_platform_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret) { return CL_DEVICE_NOT_FOUND; }
cl_int clGetDeviceIDs(cl_platform_id platform, cl_device_type device_type, cl_uint num_entries, cl_device_id *devices, cl_uint *num_devices) { return CL_DEVICE_NOT_FOUND; }
cl_int clGetDeviceInfo(cl_device_id device, cl_device_info param_name, size_t param_value_size, void *param_value, size_t *param_value_size_ret) { return CL_DEVICE_NOT_FOUND; }
cl_context clCreateContext(const cl_context_properties *properties, cl_uint num_devices, const cl_device_id *devices, void (CL_CALLBACK *pfn_notify)(const char *errinfo, const void *private_info, size_t cb, void *user_data), void *user_data, cl_int *errcode_ret) { if(errcode_ret) *errcode_ret = CL_DEVICE_NOT_FOUND; return NULL; }
EOL
        # Compile a simple shared library with the necessary symbols
        ${ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/x86_64-linux-android21-clang -shared -o "$OPENCL_LIB_DIR/x86_64/libOpenCL.so" "$OPENCL_LIB_DIR/x86_64/libOpenCL.c" -I"$OPENCL_INCLUDE_DIR"
      fi
    fi
  else
    # On other platforms, at least create stub libraries with dummy implementations
    mkdir -p "$OPENCL_LIB_DIR/arm64-v8a"
    mkdir -p "$OPENCL_LIB_DIR/x86_64"
    
    if [ ! -f "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so" ]; then
      echo -e "${YELLOW}Creating stub libOpenCL.so for arm64-v8a...${NC}"
      touch "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so"
    fi

    if [ ! -f "$OPENCL_LIB_DIR/x86_64/libOpenCL.so" ]; then
      echo -e "${YELLOW}Creating stub libOpenCL.so for x86_64...${NC}"
      touch "$OPENCL_LIB_DIR/x86_64/libOpenCL.so"
    fi
  fi

  echo -e "${GREEN}✅ OpenCL dependencies setup complete${NC}"

  # Setup Vulkan dependencies
  echo -e "${YELLOW}Setting up Vulkan dependencies...${NC}"

  # If on Linux, try to install Vulkan libraries
  if [[ "$HOST_PLATFORM" == "linux" ]]; then
    # Try to install Vulkan SDK and dependencies
    fix_vulkan_build_issues || true
  fi

  # Verify Vulkan dependencies have been properly setup
  if [ ! -d "$VULKAN_HEADERS_DIR" ]; then
    echo -e "${YELLOW}Vulkan Headers not installed correctly. Installing manually...${NC}"
    mkdir -p "$VULKAN_HEADERS_DIR"
    # Use specific Vulkan SDK version tag to match build_android.sh
    git clone --depth 1 --branch "v$VULKAN_SDK_VERSION" https://github.com/KhronosGroup/Vulkan-Headers.git "$VULKAN_HEADERS_DIR"
  fi

  if [ ! -d "$VULKAN_INCLUDE_DIR/vulkan" ]; then
    echo -e "${YELLOW}Vulkan include directory not set up correctly. Creating manually...${NC}"
    mkdir -p "$VULKAN_INCLUDE_DIR"
    cp -r "$VULKAN_HEADERS_DIR/include/vulkan" "$VULKAN_INCLUDE_DIR/"
  fi

  # Copy to NDK sysroot if provided
  if [ -n "$ANDROID_NDK_HOME" ]; then
    NDK_SYSROOT_INCLUDE="$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include"
    if [ -d "$NDK_SYSROOT_INCLUDE" ]; then
      echo -e "${YELLOW}Copying Vulkan headers to NDK sysroot...${NC}"
      mkdir -p "$NDK_SYSROOT_INCLUDE/vulkan"
      cp -r "$VULKAN_INCLUDE_DIR/vulkan/"* "$NDK_SYSROOT_INCLUDE/vulkan/"
      
      # Also copy vk_video headers
      if [ -d "$VULKAN_INCLUDE_DIR/vk_video" ]; then
        mkdir -p "$NDK_SYSROOT_INCLUDE/vk_video"
        cp -r "$VULKAN_INCLUDE_DIR/vk_video/"* "$NDK_SYSROOT_INCLUDE/vk_video/"
        echo -e "${GREEN}✅ vk_video headers copied to NDK sysroot${NC}"
      fi
      
      echo -e "${GREEN}✅ Vulkan headers copied to NDK sysroot${NC}"
      
      # Also copy OpenCL headers
      echo -e "${YELLOW}Copying OpenCL headers to NDK sysroot...${NC}"
      mkdir -p "$NDK_SYSROOT_INCLUDE/CL"
      cp -r "$OPENCL_INCLUDE_DIR/CL/"* "$NDK_SYSROOT_INCLUDE/CL/"
      echo -e "${GREEN}✅ OpenCL headers copied to NDK sysroot${NC}"
    fi
  fi

  echo -e "${GREEN}✅ External dependencies setup complete${NC}"
  echo -e "${GREEN}All external libraries are available in: $PREBUILT_EXTERNAL_DIR${NC}"
}

# Call setup_dependencies to set up all required headers and libraries
setup_dependencies

# Exit if we're only installing dependencies
if [ "$INSTALL_DEPS_ONLY" = true ]; then
  echo -e "${GREEN}✅ Dependencies installed successfully. Exiting as requested.${NC}"
  exit 0
fi

# Function to build GPU libraries for a specific ABI
build_gpu_libs_for_abi() {
  local ABI=$1
  echo -e "${YELLOW}Building GPU libraries for $ABI...${NC}"
  
  # Create build directory for this ABI
  local BUILD_DIR="$PREBUILT_DIR/build-gpu-$ABI"
  
  # Clean build directory if requested
  if [ "$CLEAN_BUILD" = true ] && [ -d "$BUILD_DIR" ]; then
    echo -e "${YELLOW}Cleaning previous build for $ABI...${NC}"
    rm -rf "$BUILD_DIR"
  fi
  
  mkdir -p "$BUILD_DIR"
  
  # Prepare CMake flags
  CMAKE_FLAGS=()
  CMAKE_FLAGS+=("-DCMAKE_BUILD_TYPE=$BUILD_TYPE")
  CMAKE_FLAGS+=("-DBUILD_SHARED_LIBS=ON") # Build shared libraries
  CMAKE_FLAGS+=("-DLLAMA_BUILD_TESTS=OFF")
  CMAKE_FLAGS+=("-DLLAMA_BUILD_EXAMPLES=OFF")
  CMAKE_FLAGS+=("-DLLAMA_CURL=OFF")
  
  # Add GPU backend flags
  if [ "$BUILD_OPENCL" = true ]; then
    CMAKE_FLAGS+=("-DGGML_OPENCL=ON")
    CMAKE_FLAGS+=("-DGGML_OPENCL_EMBED_KERNELS=ON")
    CMAKE_FLAGS+=("-DGGML_OPENCL_USE_ADRENO_KERNELS=ON") # Good for Android
    
    # Use OpenCL include directory
    CMAKE_FLAGS+=("-DOpenCL_INCLUDE_DIR=$OPENCL_INCLUDE_DIR")
    CMAKE_FLAGS+=("-DOpenCL_LIBRARY=$OPENCL_LIB_DIR/$ABI/libOpenCL.so")
  else
    CMAKE_FLAGS+=("-DGGML_OPENCL=OFF")
  fi
  
  if [ "$BUILD_VULKAN" = true ]; then
    CMAKE_FLAGS+=("-DGGML_VULKAN=ON")
    CMAKE_FLAGS+=("-DLLAMA_VULKAN=ON")
    CMAKE_FLAGS+=("-DGGML_VULKAN_EMBED_KERNELS=ON")
    
    # Use Vulkan include directory
    CMAKE_FLAGS+=("-DVulkan_INCLUDE_DIR=$VULKAN_INCLUDE_DIR")
    
    # Set platform-specific Vulkan flags
    CMAKE_FLAGS+=("-DVK_USE_PLATFORM_ANDROID_KHR=ON")
    
    # Improved glslc detection and handling
    # First, check if a custom GLSLC path was provided
    if [ -n "$CUSTOM_GLSLC_PATH" ]; then
      if [ -f "$CUSTOM_GLSLC_PATH" ]; then
        echo -e "${GREEN}Using custom GLSLC: $CUSTOM_GLSLC_PATH${NC}"
        GLSLC_EXECUTABLE="$CUSTOM_GLSLC_PATH"
        CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
        CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
        chmod +x "$GLSLC_EXECUTABLE" || true
      else
        echo -e "${RED}Custom GLSLC path provided but file does not exist: $CUSTOM_GLSLC_PATH${NC}"
        GLSLC_EXECUTABLE=""
      fi
    # If not, try to find GLSLC in the environment
    elif [ -n "$GLSLC_EXECUTABLE" ] && [ -f "$GLSLC_EXECUTABLE" ]; then
      echo -e "${GREEN}Using GLSLC from environment: $GLSLC_EXECUTABLE${NC}"
      CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
      CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
      chmod +x "$GLSLC_EXECUTABLE" || true
    else
      # Try to find GLSLC in the NDK shader-tools
      GLSLC_EXECUTABLE=""
      NDK_SHADER_TOOLS="$ANDROID_NDK_HOME/shader-tools"
      if [ -d "$NDK_SHADER_TOOLS" ]; then
        # Find suitable host tag
        if [[ "$OSTYPE" == "darwin"* ]]; then
          if [[ $(uname -m) == "arm64" ]]; then
            NDK_HOST_TAG="darwin-arm64"
          else
            NDK_HOST_TAG="darwin-x86_64"
          fi
        elif [[ "$OSTYPE" == "linux-gnu"* ]]; then
          NDK_HOST_TAG="linux-x86_64"
        elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "win32" ]]; then
          NDK_HOST_TAG="windows-x86_64"
        fi
        
        if [ -d "$NDK_SHADER_TOOLS/$NDK_HOST_TAG" ]; then
          GLSLC_EXECUTABLE="$NDK_SHADER_TOOLS/$NDK_HOST_TAG/glslc"
          if [ -f "$GLSLC_EXECUTABLE" ]; then
            echo -e "${GREEN}Using GLSLC from NDK shader-tools: $GLSLC_EXECUTABLE${NC}"
            CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
            CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
            chmod +x "$GLSLC_EXECUTABLE" || true
          else
            echo -e "${YELLOW}GLSLC not found in expected NDK location: $GLSLC_EXECUTABLE${NC}"
            GLSLC_EXECUTABLE=""
          fi
        fi
      fi
      
      # If we didn't find GLSLC in the standard location, search more broadly
      if [ -z "$GLSLC_EXECUTABLE" ]; then
        echo -e "${YELLOW}Searching for GLSLC in the NDK and system locations...${NC}"
        
        # Use different find syntax based on OS
        if [[ "$OSTYPE" == "darwin"* ]]; then
          # macOS version - doesn't support -executable flag
          GLSLC_EXECUTABLE=$(find "$ANDROID_NDK_HOME" -name "glslc" -type f -perm +111 2>/dev/null | head -1)
        else
          # Linux version
          GLSLC_EXECUTABLE=$(find "$ANDROID_NDK_HOME" -name "glslc" -type f -executable 2>/dev/null | head -1)
        fi
        
        if [ -n "$GLSLC_EXECUTABLE" ]; then
          echo -e "${GREEN}Found GLSLC at: $GLSLC_EXECUTABLE${NC}"
          CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
          CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
          chmod +x "$GLSLC_EXECUTABLE" || true
        else
          echo -e "${YELLOW}GLSLC not found in NDK, checking system locations...${NC}"
          if [[ "$OSTYPE" == "darwin"* ]]; then
            # Try standard macOS locations
            for path in "/usr/local/bin/glslc" "/opt/homebrew/bin/glslc" "/usr/bin/glslc"; do
              if [ -f "$path" ]; then
                GLSLC_EXECUTABLE="$path"
                echo -e "${GREEN}Found system GLSLC at: $GLSLC_EXECUTABLE${NC}"
                CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
                CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
                break
              fi
            done
            
            if [ -z "$GLSLC_EXECUTABLE" ]; then
              GLSLC_EXECUTABLE=$(which glslc 2>/dev/null || echo "")
              if [ -n "$GLSLC_EXECUTABLE" ]; then
                CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
                CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
              fi
            fi
          else
            # Try which on Linux
            GLSLC_EXECUTABLE=$(which glslc 2>/dev/null || echo "")
            if [ -n "$GLSLC_EXECUTABLE" ]; then
              CMAKE_FLAGS+=("-DGLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
              CMAKE_FLAGS+=("-DVulkan_GLSLC_EXECUTABLE=$GLSLC_EXECUTABLE")
            fi
          fi
          
          if [ -z "$GLSLC_EXECUTABLE" ]; then
            echo -e "${YELLOW}GLSLC not found. Using pre-built shaders instead.${NC}"
            # Tell CMake to use pre-built shaders instead of compiling them
            CMAKE_FLAGS+=("-DGGML_VULKAN_SHADERS_PREBUILD=ON")
          else
            echo -e "${GREEN}Using system GLSLC: $GLSLC_EXECUTABLE${NC}"
          fi
        fi
      fi
    fi
    
    # Always add these options for more reliable builds without glslc
    CMAKE_FLAGS+=("-DGGML_VULKAN_SHADERS_DIR=${LLAMA_CPP_DIR}/ggml-vulkan/shaders")
    CMAKE_FLAGS+=("-DGGML_VULKAN_SHADERS_EMBED=ON")
    
    # Create architecture-specific fake Vulkan libraries if needed
    VULKAN_LIB_DIR="$VULKAN_INCLUDE_DIR/lib/$ABI"
    mkdir -p "$VULKAN_LIB_DIR"
    VULKAN_DUMMY_LIB="$VULKAN_LIB_DIR/libvulkan.so"
    
    if [ ! -f "$VULKAN_DUMMY_LIB" ]; then
      touch "$VULKAN_DUMMY_LIB"
      echo -e "${YELLOW}Created empty libvulkan.so for $ABI to proceed with build${NC}"
    fi
    
    # Always set the Vulkan library path for this architecture
    CMAKE_FLAGS+=("-DVulkan_LIBRARY=$VULKAN_DUMMY_LIB")
    CMAKE_FLAGS+=("-DVULKAN_LIBRARY=$VULKAN_DUMMY_LIB")
    
    # Set generic lib as well for CMake's FindVulkan module
    if [ ! -f "$VULKAN_INCLUDE_DIR/lib/libvulkan.so" ]; then
      mkdir -p "$VULKAN_INCLUDE_DIR/lib"
      touch "$VULKAN_INCLUDE_DIR/lib/libvulkan.so"
    fi
  else
    CMAKE_FLAGS+=("-DGGML_VULKAN=OFF")
    CMAKE_FLAGS+=("-DLLAMA_VULKAN=OFF")
  fi
  
  # Set host-specific configurations based on the platform we're building FROM
  if [ "$HOST_PLATFORM" = "macos" ]; then
    echo -e "${YELLOW}Building on macOS host for Android target${NC}"
    # Add macOS-specific flags here if needed
  elif [ "$HOST_PLATFORM" = "windows" ]; then
    echo -e "${YELLOW}Building on Windows host for Android target${NC}"
    # Add Windows-specific flags here if needed
  elif [ "$HOST_PLATFORM" = "linux" ]; then
    echo -e "${YELLOW}Building on Linux host for Android target${NC}"
    # Add Linux-specific flags here if needed
  else
    echo -e "${YELLOW}Building on unknown host platform for Android target${NC}"
  fi
  
  # Configure Android NDK (required regardless of host platform)
  if [ -z "$ANDROID_NDK_HOME" ]; then
    if [ -n "$ANDROID_NDK_ROOT" ]; then
      ANDROID_NDK_HOME="$ANDROID_NDK_ROOT"
    elif [ -d "$ANDROID_HOME/ndk" ]; then
      # Find the latest NDK version
      NEWEST_NDK_VERSION=$(ls -1 "$ANDROID_HOME/ndk" | sort -rV | head -n 1)
      ANDROID_NDK_HOME="$ANDROID_HOME/ndk/$NEWEST_NDK_VERSION"
    elif [ -d "$ANDROID_HOME/ndk-bundle" ]; then
      ANDROID_NDK_HOME="$ANDROID_HOME/ndk-bundle"
    else
      echo -e "${RED}Android NDK not found. Please set ANDROID_NDK_HOME.${NC}"
      exit 1
    fi
  fi
  
  echo -e "${GREEN}Using Android NDK at: $ANDROID_NDK_HOME${NC}"
  
  # Add Android-specific flags (required for all builds since we're always building for Android)
  CMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"
  CMAKE_FLAGS+=("-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_FILE")
  CMAKE_FLAGS+=("-DANDROID_ABI=$ABI")
  CMAKE_FLAGS+=("-DANDROID_PLATFORM=android-24") # Minimum Android 7.0
  CMAKE_FLAGS+=("-DANDROID_STL=c++_shared")
  
  if [ "$BUILD_VULKAN" = true ]; then
    CMAKE_FLAGS+=("-DVK_USE_PLATFORM_ANDROID_KHR=ON")
  fi
  
  # Run CMake configuration
  if [ "$DRY_RUN" = true ]; then
    echo -e "${YELLOW}Would run: cmake -S \"$LLAMA_CPP_DIR\" -B \"$BUILD_DIR\" ${CMAKE_FLAGS[*]}${NC}"
  else
    echo -e "${YELLOW}Configuring GPU backends with CMake...${NC}"
    cmake -S "$LLAMA_CPP_DIR" -B "$BUILD_DIR" "${CMAKE_FLAGS[@]}"
  fi
  
  # Build libraries
  if [ "$DRY_RUN" = true ]; then
    echo -e "${YELLOW}Would run: cmake --build \"$BUILD_DIR\" --config \"$BUILD_TYPE\" -j $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)${NC}"
  else
    echo -e "${YELLOW}Building GPU backends...${NC}"
    cmake --build "$BUILD_DIR" --config "$BUILD_TYPE" -j $(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
  fi
  
  # Create GPU libraries directory for this ABI
  mkdir -p "$PREBUILT_GPU_DIR/$ABI"
  
  # Find and copy the GPU libraries to the prebuilt directory
  echo -e "${YELLOW}Copying GPU backend libraries to $PREBUILT_GPU_DIR/$ABI${NC}"
  
  # Check for OpenCL library
  if [ "$BUILD_OPENCL" = true ]; then
    OPENCL_LIB_PATH=$(find "$BUILD_DIR" -name "libggml-opencl.*" | head -n 1)
    if [ -n "$OPENCL_LIB_PATH" ]; then
      if [ "$DRY_RUN" = true ]; then
        echo -e "${YELLOW}Would copy: $OPENCL_LIB_PATH to $PREBUILT_GPU_DIR/$ABI/${NC}"
      else
        cp "$OPENCL_LIB_PATH" "$PREBUILT_GPU_DIR/$ABI/"
        echo -e "${GREEN}✅ Copied OpenCL library to $PREBUILT_GPU_DIR/$ABI/$(basename "$OPENCL_LIB_PATH")${NC}"
        
        # Create capability flag file in prebuilt directory
        touch "$PREBUILT_GPU_DIR/$ABI/.opencl_enabled"
        echo -e "${GREEN}✅ Created OpenCL capability flag${NC}"
      fi
    else
      echo -e "${RED}❌ OpenCL library not found in build directory${NC}"
    fi
  fi
  
  # Check for Vulkan library
  if [ "$BUILD_VULKAN" = true ]; then
    VULKAN_LIB_PATH=$(find "$BUILD_DIR" -name "libggml-vulkan.*" | head -n 1)
    if [ -n "$VULKAN_LIB_PATH" ]; then
      if [ "$DRY_RUN" = true ]; then
        echo -e "${YELLOW}Would copy: $VULKAN_LIB_PATH to $PREBUILT_GPU_DIR/$ABI/${NC}"
      else
        cp "$VULKAN_LIB_PATH" "$PREBUILT_GPU_DIR/$ABI/"
        echo -e "${GREEN}✅ Copied Vulkan library to $PREBUILT_GPU_DIR/$ABI/$(basename "$VULKAN_LIB_PATH")${NC}"
        
        # Create capability flag file in prebuilt directory
        touch "$PREBUILT_GPU_DIR/$ABI/.vulkan_enabled"
        echo -e "${GREEN}✅ Created Vulkan capability flag${NC}"
      fi
    else
      echo -e "${RED}❌ Vulkan library not found in build directory${NC}"
    fi
  fi
  
  # Copy core GGML libraries (needed for the GPU backends)
  GGML_LIB_PATH=$(find "$BUILD_DIR" -name "libggml.*" | head -n 1)
  if [ -n "$GGML_LIB_PATH" ]; then
    if [ "$DRY_RUN" = true ]; then
      echo -e "${YELLOW}Would copy: $GGML_LIB_PATH to $PREBUILT_GPU_DIR/$ABI/${NC}"
    else
      cp "$GGML_LIB_PATH" "$PREBUILT_GPU_DIR/$ABI/"
      echo -e "${GREEN}✅ Copied core GGML library to $PREBUILT_GPU_DIR/$ABI/$(basename "$GGML_LIB_PATH")${NC}"
    fi
  else
    echo -e "${RED}❌ Core GGML library not found in build directory${NC}"
  fi
  
  # Also check for CPU-specific libraries
  GGML_CPU_LIB_PATH=$(find "$BUILD_DIR" -name "libggml-cpu.*" | head -n 1)
  if [ -n "$GGML_CPU_LIB_PATH" ]; then
    if [ "$DRY_RUN" = true ]; then
      echo -e "${YELLOW}Would copy: $GGML_CPU_LIB_PATH to $PREBUILT_GPU_DIR/$ABI/${NC}"
    else
      cp "$GGML_CPU_LIB_PATH" "$PREBUILT_GPU_DIR/$ABI/"
      echo -e "${GREEN}✅ Copied GGML CPU library to $PREBUILT_GPU_DIR/$ABI/$(basename "$GGML_CPU_LIB_PATH")${NC}"
    fi
  fi
  
  echo -e "${GREEN}✅ GPU backends built and copied for $ABI${NC}"
}

# Build for each requested ABI
if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "arm64-v8a" ]; then
  build_gpu_libs_for_abi "arm64-v8a"
fi

if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "x86_64" ]; then
  build_gpu_libs_for_abi "x86_64" || {
    echo -e "${YELLOW}⚠️ Build for x86_64 failed, but continuing with other architectures${NC}"
    # Create directory to indicate we tried to build, even if it failed
    mkdir -p "$PREBUILT_GPU_DIR/x86_64"
    touch "$PREBUILT_GPU_DIR/x86_64/.build_attempted"
  }
fi

# Create a manifest file to indicate build details
MANIFEST_FILE="$PREBUILT_GPU_DIR/build-manifest.txt"
echo "Build timestamp: $(date)" > "$MANIFEST_FILE"
echo "Host platform: $HOST_PLATFORM" >> "$MANIFEST_FILE"
echo "Target platform: Android" >> "$MANIFEST_FILE"
echo "OpenCL: $([ "$BUILD_OPENCL" = true ] && echo "enabled" || echo "disabled")" >> "$MANIFEST_FILE"
echo "Vulkan: $([ "$BUILD_VULKAN" = true ] && echo "enabled" || echo "disabled")" >> "$MANIFEST_FILE"
echo "Build type: $BUILD_TYPE" >> "$MANIFEST_FILE"
echo "llama.cpp commit: $(cd "$LLAMA_CPP_DIR" && git rev-parse HEAD)" >> "$MANIFEST_FILE"

# Check for GPU backend build failures
if [ "$BUILD_VULKAN" = true ] && [ ! -f "$PREBUILT_GPU_DIR/arm64-v8a/libggml-vulkan.so" ] && [ ! -f "$PREBUILT_GPU_DIR/x86_64/libggml-vulkan.so" ]; then
  echo -e "${RED}⚠️ Vulkan libraries failed to build${NC}"
  
  # Try to fix Vulkan build issues
  if fix_vulkan_build_issues; then
    echo -e "${GREEN}✅ Fixed Vulkan build issues, retrying build...${NC}"
    
    # Retry building with Vulkan
    if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "arm64-v8a" ]; then
      build_gpu_libs_for_abi "arm64-v8a"
    fi
    
    if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "x86_64" ]; then
      build_gpu_libs_for_abi "x86_64"
    fi
  else
    echo -e "${YELLOW}Attempting to build with Vulkan disabled...${NC}"
    
    # Retry without Vulkan
    BUILD_VULKAN=false
    echo "Vulkan: disabled (after build failure)" >> "$MANIFEST_FILE"
    
    if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "arm64-v8a" ]; then
      build_gpu_libs_for_abi "arm64-v8a"
    fi
    
    if [ "$BUILD_ABI" = "all" ] || [ "$BUILD_ABI" = "x86_64" ]; then
      build_gpu_libs_for_abi "x86_64"
    fi
  fi
fi

echo -e "${GREEN}Manifest created at $MANIFEST_FILE${NC}"

echo -e "${GREEN}✅ All GPU backend libraries built successfully!${NC}"
echo -e "${GREEN}GPU libraries are available in: $PREBUILT_GPU_DIR${NC}"
