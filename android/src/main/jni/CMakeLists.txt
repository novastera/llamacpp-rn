cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define the library name
project(llamacpprn)

# Enable optimizations for Release builds
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -Ofast -ffast-math -DNDEBUG")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -Ofast -ffast-math -DNDEBUG")

# Define the RCT_NEW_ARCH_ENABLED flag
add_definitions(-DRCT_NEW_ARCH_ENABLED=1)

# Get the path to the module root - two parent directories up from the jni directory
get_filename_component(MODULE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
# Use a more reliable way to get the cpp directory - it's directly in the module root
get_filename_component(CPP_DIR "${MODULE_ROOT}/../../cpp" ABSOLUTE)

# Double check if the CPP_DIR exists, if not try a direct path
if(NOT EXISTS "${CPP_DIR}/LlamaCppRnModule.cpp")
    # Try alternate path - assuming we're building from the main project directory
    get_filename_component(ALT_CPP_DIR "${MODULE_ROOT}/../cpp" ABSOLUTE)
    if(EXISTS "${ALT_CPP_DIR}/LlamaCppRnModule.cpp")
        set(CPP_DIR "${ALT_CPP_DIR}")
    endif()
endif()

message(STATUS "Module root: ${MODULE_ROOT}")
message(STATUS "CPP directory: ${CPP_DIR}")

# Option to enable OpenCL support
option(LLAMACPPRN_OPENCL "Enable OpenCL GPU acceleration for compatible devices" ON)

# Option to enable Vulkan support
option(LLAMACPPRN_VULKAN "Enable Vulkan GPU acceleration for compatible devices" ON)

# We always build from source for Android now
set(rnllamaBuildFromSource "true")

# Define the path to llama.cpp directory
set(LLAMA_CPP_DIR "${CPP_DIR}/llama.cpp")

# Define React Native paths - using multiple possible locations
set(POSSIBLE_RN_DIRS
    "${MODULE_ROOT}/../../node_modules/react-native"
    "${MODULE_ROOT}/../node_modules/react-native"
    "${MODULE_ROOT}/node_modules/react-native"
)

# Try to find React Native directory
set(RN_DIR "")
foreach(dir ${POSSIBLE_RN_DIRS})
    if(EXISTS "${dir}")
        set(RN_DIR "${dir}")
        message(STATUS "Found React Native at: ${RN_DIR}")
        break()
    endif()
endforeach()

if("${RN_DIR}" STREQUAL "")
    message(FATAL_ERROR "Could not find React Native directory in any of the search paths")
endif()

set(REACT_ANDROID_DIR "${RN_DIR}/ReactAndroid")

# Various places React Native might place FBJNI headers
set(FBJNI_HEADERS_DIRS
    "${REACT_ANDROID_DIR}/src/main/jni/first-party/fbjni/headers"
    "${REACT_ANDROID_DIR}/src/main/java/com/facebook/react/turbomodule/core/jni"
    "${REACT_ANDROID_DIR}/../ReactCommon/jsi"
    "${REACT_ANDROID_DIR}/src/main/jni/react/turbomodule"
    "${REACT_ANDROID_DIR}/src/main/jni/react/jni"
)

# Get the ABI from the CMAKE_ANDROID_ARCH_ABI which is set by Android toolchain
if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
    message(STATUS "Using ABI from toolchain: ${ANDROID_ABI}")
else()
    # Fallback to a default if not set
    set(ANDROID_ABI "arm64-v8a")
    message(STATUS "ABI not defined in toolchain, using default: ${ANDROID_ABI}")
endif()

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

# Override OpenCL option based on flag
if(OPENCL_ENABLED)
    set(LLAMACPPRN_OPENCL ON)
    message(STATUS "OpenCL enabled based on flag in jniLibs")
endif()

# Override Vulkan option based on flag
if(VULKAN_ENABLED)
    set(LLAMACPPRN_VULKAN ON)
    message(STATUS "Vulkan enabled based on flag in jniLibs")
endif()

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
        "/system/lib64/libOpenCL.so",
        "/system/vendor/lib64/libOpenCL.so",
        "/system/vendor/lib64/egl/libGLES_mali.so",
        "/system/vendor/lib64/libPVROCL.so",
        "/data/data/org.pocl.libs/files/lib64/libpocl.so",
        "/system/lib/libOpenCL.so",
        "/system/vendor/lib/libOpenCL.so",
        "/system/vendor/lib/egl/libGLES_mali.so",
        "/system/vendor/lib/libPVROCL.so",
        "/data/data/org.pocl.libs/files/lib/libpocl.so",
        "/system/lib/egl/libq3dtools_adreno.so" # Qualcomm Adreno
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

# Define paths to search for the prebuilt llama.so library
set(PREBUILT_LLAMA_PATHS
    "${MODULE_ROOT}/jniLibs/${ANDROID_ABI}/libllama.so"
    "${MODULE_ROOT}/src/main/jniLibs/${ANDROID_ABI}/libllama.so"
    "${MODULE_ROOT}/../jniLibs/${ANDROID_ABI}/libllama.so"
)

# Find the prebuilt llama.so library
set(PREBUILT_LLAMA_LIB "")
foreach(path ${PREBUILT_LLAMA_PATHS})
    if(EXISTS "${path}")
        set(PREBUILT_LLAMA_LIB "${path}")
        message(STATUS "Found prebuilt llama library at: ${PREBUILT_LLAMA_LIB}")
        break()
    endif()
endforeach()

# Get the correct path to the include directory - corrected path
set(MAIN_INCLUDE_DIR "${MODULE_ROOT}/main/cpp/include")
if(NOT EXISTS "${MAIN_INCLUDE_DIR}")
    set(MAIN_INCLUDE_DIR "${MODULE_ROOT}/cpp/include")
    if(NOT EXISTS "${MAIN_INCLUDE_DIR}")
        set(MAIN_INCLUDE_DIR "${MODULE_ROOT}/src/main/cpp/include")
    endif()
endif()

message(STATUS "Using include directory: ${MAIN_INCLUDE_DIR}")

# Source files with explicit full paths to avoid resolution issues
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/OnLoad.cpp
    ${CPP_DIR}/LlamaCppRnModule.cpp
    ${CPP_DIR}/LlamaCppModel.cpp
    ${CPP_DIR}/SystemUtils.cpp
    ${CPP_DIR}/rn-completion.cpp
)

# Additional llama.cpp common files needed for json-schema-to-grammar and chat functionality
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

# Verify each source file exists
foreach(src_file ${SOURCE_FILES})
    if(NOT EXISTS "${src_file}")
        message(WARNING "Source file not found: ${src_file}")
    endif()
endforeach()

# Create the include paths
set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CPP_DIR}
    ${MAIN_INCLUDE_DIR}
    ${LLAMA_CPP_DIR}
    ${LLAMA_CPP_DIR}/src
    ${LLAMA_CPP_DIR}/common
    ${LLAMA_CPP_DIR}/common/minja
    ${LLAMA_CPP_DIR}/ggml/include
    ${LLAMA_CPP_DIR}/ggml/src
    ${RN_DIR}/ReactCommon
    ${RN_DIR}/ReactCommon/callinvoker
    ${RN_DIR}/ReactCommon/jsi
    ${RN_DIR}/ReactCommon/react/nativemodule/core
    ${RN_DIR}/ReactCommon/react/bridging
    ${REACT_ANDROID_DIR}/src/main/jni/react/turbomodule
    ${REACT_ANDROID_DIR}/src/main/jni/first-party/fbjni/headers
    ${REACT_ANDROID_DIR}/src/main/jni/react/jni
    ${REACT_ANDROID_DIR}/src/main/cpp/jni
    ${REACT_ANDROID_DIR}/src/main/java/com/facebook/react/turbomodule/core/jni
)

# Add OpenCL includes if enabled
if(LLAMACPPRN_OPENCL AND ${ANDROID_ABI} STREQUAL "arm64-v8a")
    list(APPEND INCLUDE_DIRECTORIES ${OPENCL_INCLUDE_DIRS})
    
    # Add definition to enable OpenCL-specific code
    add_definitions(-DLLAMACPPRN_OPENCL_ENABLED=1)
else()
    add_definitions(-DLLAMACPPRN_OPENCL_ENABLED=0)
endif()

# Add Vulkan includes if enabled
if(LLAMACPPRN_VULKAN AND (${ANDROID_ABI} STREQUAL "arm64-v8a" OR ${ANDROID_ABI} STREQUAL "x86_64"))
    list(APPEND INCLUDE_DIRECTORIES ${VULKAN_INCLUDE_DIRS})
    
    # Add definition to enable Vulkan-specific code
    add_definitions(-DLLAMACPPRN_VULKAN_ENABLED=1)
    add_definitions(-DVK_USE_PLATFORM_ANDROID_KHR=1)
else()
    add_definitions(-DLLAMACPPRN_VULKAN_ENABLED=0)
endif()

# Add all possible FBJNI header directories
foreach(fbjni_dir ${FBJNI_HEADERS_DIRS})
    if(EXISTS "${fbjni_dir}")
        list(APPEND INCLUDE_DIRECTORIES "${fbjni_dir}")
        message(STATUS "Adding FBJNI headers from: ${fbjni_dir}")
    endif()
endforeach()

# Include all the directories
include_directories(${INCLUDE_DIRECTORIES})

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

# Link against the prebuilt llama library if it exists
if(PREBUILT_LLAMA_LIB)
    message(STATUS "Found prebuilt llama library, linking against it: ${PREBUILT_LLAMA_LIB}")
    
    # Add the prebuilt library
    add_library(llama SHARED IMPORTED)
    set_target_properties(llama PROPERTIES IMPORTED_LOCATION "${PREBUILT_LLAMA_LIB}")
    
    # Define macros to ensure correct API usage
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE LLAMA_SHARED)
    
    # Link against the imported library
    target_link_libraries(${CMAKE_PROJECT_NAME} llama)
else()
    # We're building from source, so we don't need to link against a prebuilt library
    target_link_libraries(${CMAKE_PROJECT_NAME} llama)
endif()

# Check for additional GPU-specific shared libraries in jniLibs
if(LLAMACPPRN_OPENCL AND OPENCL_ENABLED)
    # Check for prebuilt OpenCL lib
    set(GGML_OPENCL_LIB "${JNI_LIBS_DIR}/libggml-opencl.so")
    if(EXISTS "${GGML_OPENCL_LIB}")
        message(STATUS "Found prebuilt OpenCL library: ${GGML_OPENCL_LIB}")
        add_library(ggml-opencl SHARED IMPORTED)
        set_target_properties(ggml-opencl PROPERTIES IMPORTED_LOCATION "${GGML_OPENCL_LIB}")
        target_link_libraries(${CMAKE_PROJECT_NAME} ggml-opencl)
    endif()
endif()

if(LLAMACPPRN_VULKAN AND VULKAN_ENABLED)
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

# Set compiler flags for maximum performance
target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE
    -O3
    -fvisibility=hidden
    -ffunction-sections
    -fdata-sections
    $<$<CONFIG:Release>:-Ofast -ffast-math>
)

# Set C++ standard
set_target_properties(
    ${CMAKE_PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

