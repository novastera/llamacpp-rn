cmake_minimum_required(VERSION 3.13)

# Define the library name here.
project(llamacpprn)

# Enable optimizations for Release builds
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -ffast-math -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast -ffast-math -DNDEBUG")

# Define the RCT_NEW_ARCH_ENABLED flag
add_definitions(-DRCT_NEW_ARCH_ENABLED=1)

# Option to enable OpenCL support
option(LLAMACPPRN_OPENCL "Enable OpenCL GPU acceleration for compatible devices" ON)

# Option to enable Vulkan support
option(LLAMACPPRN_VULKAN "Enable Vulkan GPU acceleration for compatible devices" ON)

# We always build from source for Android now
set(rnllamaBuildFromSource "true")

set(LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/llama.cpp")

# Build llama.cpp from source
message(STATUS "Building llama.cpp from source for Android")

# Set LLAMA_CPP options
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "llama.cpp: build examples" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "llama.cpp: build server" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "llama.cpp: build shared libs" FORCE)
set(LLAMA_NATIVE OFF CACHE BOOL "llama.cpp: optimize for native CPU" FORCE)

# Try to find device OpenCL library first
if(LLAMACPPRN_OPENCL AND ${ANDROID_ABI} STREQUAL "arm64-v8a")
    # Check for OpenCL on the device first - most Android devices with Adreno, Mali, or PowerVR GPUs
    # have their own vendor-specific OpenCL implementations
    
    message(STATUS "Looking for device OpenCL libraries...")
    
    # Common locations for vendor OpenCL libraries on Android devices
    set(DEVICE_OPENCL_LIB_PATHS
        "/system/vendor/lib64/libOpenCL.so"
        "/system/lib64/libOpenCL.so"
        "/vendor/lib64/libOpenCL.so"
        "/vendor/lib64/egl/libGLES_mali.so"  # Mali GPUs sometimes expose OpenCL here
        "/vendor/lib64/libPVROCL.so"         # PowerVR
        "/vendor/lib64/libq3dtools_adreno.so" # Qualcomm Adreno
    )
    
    # Cache the preferred OpenCL library path (if found)
    foreach(DEVICE_OPENCL_PATH ${DEVICE_OPENCL_LIB_PATHS})
        if(EXISTS "${DEVICE_OPENCL_PATH}")
            set(DEVICE_OPENCL_FOUND TRUE)
            set(DEVICE_OPENCL_LIB_PATH "${DEVICE_OPENCL_PATH}")
            message(STATUS "Found device OpenCL library: ${DEVICE_OPENCL_LIB_PATH}")
            break()
        endif()
    endforeach()
    
    # We'll set up for an implementation that will dynamically load OpenCL
    # This means we don't need to find the actual library at build time,
    # but will load it at runtime based on what's available on the device
    
    # Set up OpenCL header paths - check both locations
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/opencl")
        # CI/CD environment might have copied headers here
        set(OPENCL_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/opencl")
        message(STATUS "Using local OpenCL headers for CI/CD build")
    else
        # Development environment - use headers from llama.cpp
        set(OPENCL_INCLUDE_DIRS "${LLAMA_CPP_DIR}/extra/OpenCL-Headers")
        message(STATUS "Using llama.cpp OpenCL headers")
    endif()
    
    # Enable CLBlast for llama.cpp
    set(LLAMA_CLBLAST ON CACHE BOOL "llama.cpp: use CLBlast" FORCE)
    
    # Tell llama.cpp to dynamically load OpenCL libraries at runtime
    set(LLAMA_CLBLAST_DLOAD ON CACHE BOOL "llama.cpp: dynamically load CLBlast" FORCE)
    
    message(STATUS "Configured for dynamic OpenCL loading - will detect at runtime")
else()
    message(STATUS "OpenCL disabled or unsupported architecture, using CPU-only")
    set(LLAMA_CLBLAST OFF CACHE BOOL "llama.cpp: use CLBlast" FORCE)
endif()

# Try to find device Vulkan library
if(LLAMACPPRN_VULKAN AND (${ANDROID_ABI} STREQUAL "arm64-v8a" OR ${ANDROID_ABI} STREQUAL "x86_64"))
    message(STATUS "Looking for device Vulkan libraries...")
    
    # Common locations for Vulkan libraries on Android devices
    set(DEVICE_VULKAN_LIB_PATHS
        "/system/vendor/lib64/libvulkan.so"
        "/system/lib64/libvulkan.so"
        "/vendor/lib64/libvulkan.so"
        "/vendor/lib64/hw/vulkan.*.so"  # Some devices use this format
    )
    
    # Cache the preferred Vulkan library path (if found)
    foreach(DEVICE_VULKAN_PATH ${DEVICE_VULKAN_LIB_PATHS})
        if(EXISTS "${DEVICE_VULKAN_PATH}")
            set(DEVICE_VULKAN_FOUND TRUE)
            set(DEVICE_VULKAN_LIB_PATH "${DEVICE_VULKAN_PATH}")
            message(STATUS "Found device Vulkan library: ${DEVICE_VULKAN_LIB_PATH}")
            break()
        endif()
    endforeach()
    
    # Set up Vulkan header paths - check locations
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan")
        # CI/CD environment might have copied headers here
        set(VULKAN_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/vulkan")
        message(STATUS "Using local Vulkan headers for CI/CD build")
    elseif(EXISTS "${LLAMA_CPP_DIR}/extra/Vulkan-Headers")
        # Development environment - use headers from llama.cpp extras
        set(VULKAN_INCLUDE_DIRS "${LLAMA_CPP_DIR}/extra/Vulkan-Headers/include")
        message(STATUS "Using llama.cpp Vulkan headers")
    else
        # Try to use NDK Vulkan headers
        set(VULKAN_INCLUDE_DIRS "${ANDROID_NDK}/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include")
        message(STATUS "Using NDK Vulkan headers")
    endif()
    
    # Enable Vulkan for llama.cpp with Android platform flag
    set(GGML_VULKAN ON CACHE BOOL "llama.cpp: use Vulkan" FORCE)
    set(GGML_VULKAN_CHECK_RESULTS ON CACHE BOOL "llama.cpp: validate Vulkan results" FORCE)
    set(VK_USE_PLATFORM_ANDROID_KHR ON CACHE BOOL "Use Android Vulkan platform" FORCE)
    
    message(STATUS "Configured Vulkan support - will detect at runtime")
else()
    message(STATUS "Vulkan disabled or unsupported architecture")
    set(GGML_VULKAN OFF CACHE BOOL "llama.cpp: use Vulkan" FORCE)
endif()

# Optimizations for ARM64
if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    # Enable ARM-specific optimizations
    set(LLAMA_BLAS ON CACHE BOOL "llama.cpp: use BLAS" FORCE)
    set(LLAMA_BLAS_VENDOR "OpenBLAS" CACHE STRING "llama.cpp: BLAS vendor" FORCE)
    
    # Enable ARM NEON intrinsics for better performance
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -march=armv8-a+simd")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=armv8-a+simd")
endif()

# Include llama.cpp as a subdirectory
add_subdirectory(${LLAMA_CPP_DIR} llama.cpp)

# Find the React Native headers
find_package(ReactAndroid REQUIRED CONFIG)
find_package(fbjni REQUIRED CONFIG)

# Source files
file(GLOB_RECURSE LLAMACPPRN_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# Additional llama.cpp common files needed for json-schema-to-grammar and chat functionality
set(LLAMA_CPP_COMMON_SOURCES
    ${LLAMA_CPP_DIR}/common/json-schema-to-grammar.cpp
    ${LLAMA_CPP_DIR}/common/common.cpp
    ${LLAMA_CPP_DIR}/common/chat.cpp
    ${LLAMA_CPP_DIR}/common/log.cpp
    ${LLAMA_CPP_DIR}/common/sampling.cpp
)

# Set the source files for the library
set(SOURCE_FILES
    # Module implementation
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/LlamaCppRnModule.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/LlamaCppModel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/SystemUtils.h
    
    # Add RN-llama files
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/rn-utils.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/rn-llama.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp/rn-llama.cpp
    
    # Add the common sources
    ${LLAMA_CPP_COMMON_SOURCES}
)

# Include paths
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LLAMA_CPP_DIR}
    ${LLAMA_CPP_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../cpp
)

# Add OpenCL includes if enabled
if(LLAMACPPRN_OPENCL AND ${ANDROID_ABI} STREQUAL "arm64-v8a")
    include_directories(
        ${OPENCL_INCLUDE_DIRS}
    )
    
    # Add definition to enable OpenCL-specific code
    add_definitions(-DLLAMACPPRN_OPENCL_ENABLED=1)
else()
    add_definitions(-DLLAMACPPRN_OPENCL_ENABLED=0)
endif()

# Add Vulkan includes if enabled
if(LLAMACPPRN_VULKAN AND (${ANDROID_ABI} STREQUAL "arm64-v8a" OR ${ANDROID_ABI} STREQUAL "x86_64"))
    include_directories(
        ${VULKAN_INCLUDE_DIRS}
    )
    
    # Add definition to enable Vulkan-specific code
    add_definitions(-DLLAMACPPRN_VULKAN_ENABLED=1)
    add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR=1)
else()
    add_definitions(-DLLAMACPPRN_VULKAN_ENABLED=0)
endif()

# Build our native module
add_library(llamacpprn SHARED
    # C++ sources
    ${LLAMACPPRN_SOURCES}
    # Shared C++ implementation with iOS
    ${SOURCE_FILES}
)

# Link against dependencies
target_link_libraries(llamacpprn
    llama
    ReactAndroid::jsi
    fbjni::fbjni
    android
    log
)

# Set compiler flags for maximum performance
target_compile_options(llamacpprn PRIVATE
    -O3
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    $<$<CONFIG:Release>:-Ofast -ffast-math>
) 