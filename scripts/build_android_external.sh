#!/bin/bash
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
PREBUILT_EXTERNAL_DIR="$PREBUILT_LIBS_DIR/external"

# Define directories
THIRD_PARTY_DIR="$PREBUILT_DIR/third_party"
OPENCL_HEADERS_DIR="$THIRD_PARTY_DIR/OpenCL-Headers"
OPENCL_INCLUDE_DIR="$PREBUILT_EXTERNAL_DIR/opencl/include"
OPENCL_LIB_DIR="$PREBUILT_EXTERNAL_DIR/opencl/lib"
VULKAN_HEADERS_DIR="$THIRD_PARTY_DIR/Vulkan-Headers"
VULKAN_INCLUDE_DIR="$PREBUILT_EXTERNAL_DIR/vulkan/include"

# Define llama.cpp paths
LLAMA_CPP_DIR="$PROJECT_ROOT/cpp/llama.cpp"

echo -e "${YELLOW}Setting up external dependencies for Android builds...${NC}"

# Create necessary directories
mkdir -p "$PREBUILT_DIR"
mkdir -p "$PREBUILT_LIBS_DIR"
mkdir -p "$PREBUILT_EXTERNAL_DIR"
mkdir -p "$OPENCL_INCLUDE_DIR" "$OPENCL_LIB_DIR" "$VULKAN_INCLUDE_DIR"

# Verify llama.cpp exists as a git repository
if [ ! -d "$LLAMA_CPP_DIR/.git" ]; then
  echo -e "${YELLOW}llama.cpp not found as a git repository at $LLAMA_CPP_DIR${NC}"
  echo -e "${YELLOW}Running setupLlamaCpp.sh to initialize it...${NC}"
  "$SCRIPT_DIR/setupLlamaCpp.sh" init
  
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

# Ensure we have architecture-specific OpenCL libraries
mkdir -p "$OPENCL_LIB_DIR/arm64-v8a"
mkdir -p "$OPENCL_LIB_DIR/x86_64"

# Create stub libraries for each architecture
if [ ! -f "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so" ]; then
  echo -e "${YELLOW}Creating stub libOpenCL.so for arm64-v8a...${NC}"
  touch "$OPENCL_LIB_DIR/arm64-v8a/libOpenCL.so"
fi

if [ ! -f "$OPENCL_LIB_DIR/x86_64/libOpenCL.so" ]; then
  echo -e "${YELLOW}Creating stub libOpenCL.so for x86_64...${NC}"
  touch "$OPENCL_LIB_DIR/x86_64/libOpenCL.so"
fi

echo -e "${GREEN}✅ OpenCL dependencies setup complete${NC}"

# Setup Vulkan dependencies
echo -e "${YELLOW}Setting up Vulkan dependencies...${NC}"

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

# Make sure we have the vulkan.hpp C++ header
if [ ! -f "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" ]; then
  echo -e "${YELLOW}Vulkan-Hpp header not found. Downloading manually...${NC}"
  mkdir -p "$VULKAN_INCLUDE_DIR/vulkan"
  
  # Try to use wget or curl to download
  if command -v wget &> /dev/null; then
    wget -q "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Hpp/vulkan-sdk-$VULKAN_SDK_VERSION/vulkan/vulkan.hpp" -O "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp"
  elif command -v curl &> /dev/null; then
    curl -s -o "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Hpp/vulkan-sdk-$VULKAN_SDK_VERSION/vulkan/vulkan.hpp"
  else
    echo -e "${RED}❌ Neither wget nor curl found. Cannot download Vulkan-Hpp header.${NC}"
  fi
  
  if [ ! -f "$VULKAN_INCLUDE_DIR/vulkan/vulkan.hpp" ]; then
    echo -e "${RED}❌ Failed to download Vulkan-Hpp header. Build will likely fail.${NC}"
  else
    echo -e "${GREEN}✅ Successfully downloaded Vulkan-Hpp header${NC}"
  fi
fi

# Also download vk_platform.h if needed
if [ ! -f "$VULKAN_INCLUDE_DIR/vulkan/vk_platform.h" ] && [ ! -d "$VULKAN_HEADERS_DIR" ]; then
  echo -e "${YELLOW}Missing vk_platform.h. Downloading Vulkan-Headers repository...${NC}"
  git clone --depth 1 --branch "v$VULKAN_SDK_VERSION" https://github.com/KhronosGroup/Vulkan-Headers.git "$VULKAN_HEADERS_DIR"
  cp -r "$VULKAN_HEADERS_DIR/include/vulkan/"* "$VULKAN_INCLUDE_DIR/vulkan/"
fi

# Make sure we have the vk_video headers
if [ ! -d "$VULKAN_INCLUDE_DIR/vk_video" ]; then
  echo -e "${YELLOW}Missing vk_video directory. Adding vk_video headers...${NC}"
  
  # If we already have Vulkan-Headers repo, copy from there
  if [ -d "$VULKAN_HEADERS_DIR/include/vk_video" ]; then
    mkdir -p "$VULKAN_INCLUDE_DIR/vk_video"
    cp -r "$VULKAN_HEADERS_DIR/include/vk_video/"* "$VULKAN_INCLUDE_DIR/vk_video/"
    echo -e "${GREEN}✅ Copied vk_video headers from Vulkan-Headers repository${NC}"
  else
    # Otherwise download them individually
    echo -e "${YELLOW}Downloading vk_video headers individually...${NC}"
    mkdir -p "$VULKAN_INCLUDE_DIR/vk_video"
    
    # List of required video headers
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
    
    # Download each header
    for HEADER in "${VK_VIDEO_HEADERS[@]}"; do
      if command -v wget &> /dev/null; then
        wget -q "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/v$VULKAN_SDK_VERSION/include/vk_video/$HEADER" -O "$VULKAN_INCLUDE_DIR/vk_video/$HEADER"
      elif command -v curl &> /dev/null; then
        curl -s -o "$VULKAN_INCLUDE_DIR/vk_video/$HEADER" "https://raw.githubusercontent.com/KhronosGroup/Vulkan-Headers/v$VULKAN_SDK_VERSION/include/vk_video/$HEADER"
      fi
      
      if [ -f "$VULKAN_INCLUDE_DIR/vk_video/$HEADER" ]; then
        echo -e "${GREEN}✅ Downloaded $HEADER${NC}"
      else
        echo -e "${RED}❌ Failed to download $HEADER${NC}"
      fi
    done
  fi
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
