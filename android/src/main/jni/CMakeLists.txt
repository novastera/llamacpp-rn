cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define the library name - needs to match what's used in the module
project(llamacpprn)

# Print system info for debugging
message(STATUS "Building for Android with ABI: ${ANDROID_ABI}")
message(STATUS "Android SDK version: ${ANDROID_PLATFORM}")
message(STATUS "Android NDK: ${ANDROID_NDK}")

# Enable optimizations for Release builds - simplified for CI compatibility
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
endif()

# Define the RCT_NEW_ARCH_ENABLED flag
add_definitions(-DRCT_NEW_ARCH_ENABLED=1)

# Get the path to the module root - two parent directories up from the jni directory
get_filename_component(MODULE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# Get the cpp directory - it's directly in the module root
get_filename_component(CPP_DIR "${MODULE_ROOT}/../../cpp" ABSOLUTE)

# Double check if the CPP_DIR exists, if not try a direct path
if(NOT EXISTS "${CPP_DIR}/LlamaCppRnModule.cpp")
    # Try alternate path - assuming we're building from the main project directory
    get_filename_component(ALT_CPP_DIR "${MODULE_ROOT}/../cpp" ABSOLUTE)
    if(EXISTS "${ALT_CPP_DIR}/LlamaCppRnModule.cpp")
        set(CPP_DIR "${ALT_CPP_DIR}")
    endif()
endif()

# Print the paths for debugging
message(STATUS "Module root: ${MODULE_ROOT}")
message(STATUS "CPP directory: ${CPP_DIR}")

# IMPORTANT: Verify LlamaCppRnModule exists in the CPP_DIR
if(NOT EXISTS "${CPP_DIR}/LlamaCppRnModule.cpp")
    message(FATAL_ERROR "Could not find LlamaCppRnModule.cpp in ${CPP_DIR}")
endif()

# Options to enable GPU acceleration
option(LLAMACPPRN_OPENCL "Enable OpenCL GPU acceleration" ON)
option(LLAMACPPRN_VULKAN "Enable Vulkan GPU acceleration" ON)

# Define the path to llama.cpp directory
set(LLAMA_CPP_DIR "${CPP_DIR}/llama.cpp")

# Get the ABI from CMAKE_ANDROID_ARCH_ABI
if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
else()
    # Fallback to a default if not set
    set(ANDROID_ABI "arm64-v8a")
endif()
message(STATUS "Using ABI: ${ANDROID_ABI}")

# Check for GPU support flags in jniLibs directory
set(JNI_LIBS_DIR "${MODULE_ROOT}/src/main/jniLibs/${ANDROID_ABI}")
set(OPENCL_ENABLED FALSE)
set(VULKAN_ENABLED FALSE)

# Check for OpenCL flag
if(EXISTS "${JNI_LIBS_DIR}/.opencl_enabled")
    set(OPENCL_ENABLED TRUE)
    message(STATUS "OpenCL support enabled via flag in jniLibs")
endif()

# Check for Vulkan flag
if(EXISTS "${JNI_LIBS_DIR}/.vulkan_enabled")
    set(VULKAN_ENABLED TRUE)
    message(STATUS "Vulkan support enabled via flag in jniLibs")
endif()

# Set LLAMA_CPP options - simplified for CI compatibility
set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "llama.cpp: build examples" FORCE)
set(LLAMA_BUILD_SERVER OFF CACHE BOOL "llama.cpp: build server" FORCE)
set(BUILD_SHARED_LIBS ON CACHE BOOL "llama.cpp: build shared libs" FORCE)
set(LLAMA_NATIVE OFF CACHE BOOL "llama.cpp: optimize for native CPU" FORCE)

# Add basic optimizations for ARM64
if(${ANDROID_ABI} STREQUAL "arm64-v8a")
    set(LLAMA_BLAS ON CACHE BOOL "llama.cpp: use BLAS" FORCE)
    set(LLAMA_BLAS_VENDOR "OpenBLAS" CACHE STRING "llama.cpp: BLAS vendor" FORCE)
    add_definitions(-DGGML_USE_NEON=1)
endif()

# OpenCL support - simplified for CI compatibility
if(LLAMACPPRN_OPENCL AND OPENCL_ENABLED)
    # Set up for dynamic loading of OpenCL
    set(GGML_CLBLAST ON CACHE BOOL "llama.cpp: use CLBlast" FORCE)
    set(GGML_CLBLAST_DLOAD ON CACHE BOOL "llama.cpp: dynamically load CLBlast" FORCE)
    message(STATUS "OpenCL support enabled")
else()
    set(GGML_CLBLAST OFF CACHE BOOL "llama.cpp: use CLBlast" FORCE)
    message(STATUS "OpenCL support disabled")
endif()

# Vulkan support - simplified for CI compatibility
if(LLAMACPPRN_VULKAN AND VULKAN_ENABLED)
    set(GGML_VULKAN ON CACHE BOOL "llama.cpp: use Vulkan" FORCE)
    set(GGML_VULKAN_DYNAMIC_LOADING ON CACHE BOOL "llama.cpp: dynamically load Vulkan" FORCE)
    set(VK_USE_PLATFORM_ANDROID_KHR ON CACHE BOOL "Use Android Vulkan platform" FORCE)
    message(STATUS "Vulkan support enabled")
else()
    set(GGML_VULKAN OFF CACHE BOOL "llama.cpp: use Vulkan" FORCE)
    message(STATUS "Vulkan support disabled")
endif()

# Memory efficiency options
set(GGML_USE_TINYBLAS ON CACHE BOOL "llama.cpp: use tiny BLAS" FORCE)

# Include llama.cpp as a subdirectory
add_subdirectory(${LLAMA_CPP_DIR} llama.cpp)

# Source files
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/OnLoad.cpp
    ${CPP_DIR}/LlamaCppRnModule.cpp
    ${CPP_DIR}/LlamaCppModel.cpp
    ${CPP_DIR}/SystemUtils.cpp
    ${CPP_DIR}/rn-completion.cpp
)

# Additional llama.cpp common files
set(LLAMA_CPP_COMMON_SOURCES
    ${LLAMA_CPP_DIR}/common/build-info.cpp
    ${LLAMA_CPP_DIR}/common/json-schema-to-grammar.cpp
    ${LLAMA_CPP_DIR}/common/json-schema-to-grammar.h
    ${LLAMA_CPP_DIR}/common/common.cpp
    ${LLAMA_CPP_DIR}/common/common.h
    ${LLAMA_CPP_DIR}/common/chat.cpp
    ${LLAMA_CPP_DIR}/common/chat.h
    ${LLAMA_CPP_DIR}/common/log.cpp
    ${LLAMA_CPP_DIR}/common/log.h
    ${LLAMA_CPP_DIR}/common/sampling.cpp
    ${LLAMA_CPP_DIR}/common/sampling.h
    ${LLAMA_CPP_DIR}/common/ngram-cache.cpp
    ${LLAMA_CPP_DIR}/common/ngram-cache.h
    ${LLAMA_CPP_DIR}/common/base64.hpp
    ${LLAMA_CPP_DIR}/common/json.hpp
    ${LLAMA_CPP_DIR}/common/speculative.cpp
    ${LLAMA_CPP_DIR}/common/speculative.h
)

# Create the include paths
set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CPP_DIR}
    ${LLAMA_CPP_DIR}
    ${LLAMA_CPP_DIR}/src
    ${LLAMA_CPP_DIR}/common
    ${LLAMA_CPP_DIR}/common/minja
    ${LLAMA_CPP_DIR}/ggml/include
    ${LLAMA_CPP_DIR}/ggml/src
)

# Include all the directories
include_directories(${INCLUDE_DIRECTORIES})

# Define preprocessor flags for GPU features
if(OPENCL_ENABLED)
    add_definitions(-DLLAMACPPRN_OPENCL_ENABLED=1)
else()
    add_definitions(-DLLAMACPPRN_OPENCL_ENABLED=0)
endif()

if(VULKAN_ENABLED)
    add_definitions(-DLLAMACPPRN_VULKAN_ENABLED=1)
    add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR=1)
else()
    add_definitions(-DLLAMACPPRN_VULKAN_ENABLED=0)
endif()

# Create our module library
add_library(${CMAKE_PROJECT_NAME} SHARED 
    ${SOURCE_FILES}
    ${LLAMA_CPP_COMMON_SOURCES}
    # Add minja template files
    ${LLAMA_CPP_DIR}/common/minja/chat-template.hpp
    ${LLAMA_CPP_DIR}/common/minja/minja.hpp
)

# Required libraries for Android
find_library(log-lib log)
find_library(android-lib android)

# Link against llama library
target_link_libraries(${CMAKE_PROJECT_NAME} llama)

# Check for additional GPU-specific shared libraries in jniLibs
if(OPENCL_ENABLED)
    # Check for prebuilt OpenCL lib
    set(GGML_OPENCL_LIB "${JNI_LIBS_DIR}/libggml-opencl.so")
    if(EXISTS "${GGML_OPENCL_LIB}")
        message(STATUS "Found prebuilt OpenCL library: ${GGML_OPENCL_LIB}")
        add_library(ggml-opencl SHARED IMPORTED)
        set_target_properties(ggml-opencl PROPERTIES IMPORTED_LOCATION "${GGML_OPENCL_LIB}")
        target_link_libraries(${CMAKE_PROJECT_NAME} ggml-opencl)
    endif()
endif()

if(VULKAN_ENABLED)
    # Check for prebuilt Vulkan lib
    set(GGML_VULKAN_LIB "${JNI_LIBS_DIR}/libggml-vulkan.so")
    if(EXISTS "${GGML_VULKAN_LIB}")
        message(STATUS "Found prebuilt Vulkan library: ${GGML_VULKAN_LIB}")
        add_library(ggml-vulkan SHARED IMPORTED)
        set_target_properties(ggml-vulkan PROPERTIES IMPORTED_LOCATION "${GGML_VULKAN_LIB}")
        target_link_libraries(${CMAKE_PROJECT_NAME} ggml-vulkan)
    endif()
endif()

# Link required libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    ${log-lib}
    ${android-lib}
)

# Set compiler flags - simplified for CI compatibility
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -O3
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
)

# Set C++ standard
set_target_properties(
    ${CMAKE_PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)
