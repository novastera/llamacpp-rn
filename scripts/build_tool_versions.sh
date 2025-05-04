#!/bin/bash
# Central location for all build tool version information
# This file should be sourced by other build scripts

# Vulkan and OpenCL versions
VULKAN_SDK_VERSION="1.4.309"
OPENCL_VERSION="3.0"
OPENCL_HEADERS_TAG="opencl-3.0.17"

# Android SDK/NDK configuration
NDK_VERSION="27.2.12479018"  # Specific NDK version
ANDROID_MIN_SDK="33"
ANDROID_TARGET_SDK="35"
ANDROID_PLATFORM="android-$ANDROID_MIN_SDK"

# Export all variables
export VULKAN_SDK_VERSION
export OPENCL_VERSION
export OPENCL_HEADERS_TAG
export NDK_VERSION
export ANDROID_MIN_SDK
export ANDROID_TARGET_SDK
export ANDROID_PLATFORM
