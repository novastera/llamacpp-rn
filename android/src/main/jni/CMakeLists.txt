cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define the library name
project(llamacpprn)

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

# Define React Native paths
set(RN_DIR "${MODULE_ROOT}/../../node_modules/react-native")
set(REACT_ANDROID_DIR "${RN_DIR}/ReactAndroid")
set(FBJNI_DIR "${REACT_ANDROID_DIR}/src/main/jni/first-party/fbjni/headers")
set(TURBOMODULE_DIR "${REACT_ANDROID_DIR}/src/main/java/com/facebook/react/turbomodule/core/jni")

# Get the ABI from the CMAKE_ANDROID_ARCH_ABI which is set by Android toolchain
if(DEFINED CMAKE_ANDROID_ARCH_ABI)
    set(ANDROID_ABI ${CMAKE_ANDROID_ARCH_ABI})
    message(STATUS "Using ABI from toolchain: ${ANDROID_ABI}")
else()
    # Fallback to a default if not set
    set(ANDROID_ABI "arm64-v8a")
    message(STATUS "ABI not defined in toolchain, using default: ${ANDROID_ABI}")
endif()

# Define path to the prebuilt llama.so library (use the direct path in the module)
set(PREBUILT_LLAMA_LIB "${MODULE_ROOT}/jniLibs/${ANDROID_ABI}/libllama.so")
message(STATUS "Looking for prebuilt llama library at: ${PREBUILT_LLAMA_LIB}")

# Try alternate path if not found
if(NOT EXISTS "${PREBUILT_LLAMA_LIB}")
    set(PREBUILT_LLAMA_LIB "${MODULE_ROOT}/src/main/jniLibs/${ANDROID_ABI}/libllama.so")
    message(STATUS "Trying alternate path for prebuilt library: ${PREBUILT_LLAMA_LIB}")
endif()

# Include path for llama.cpp headers
set(LLAMA_CPP_INCLUDE_DIR "${CPP_DIR}/llama.cpp")
set(ANDROID_CPP_INCLUDE_DIR "${MODULE_ROOT}/cpp/include")

# Get the correct path to the include directory - corrected path
set(MAIN_INCLUDE_DIR "${MODULE_ROOT}/main/cpp/include")
if(NOT EXISTS "${MAIN_INCLUDE_DIR}")
    set(MAIN_INCLUDE_DIR "${MODULE_ROOT}/cpp/include")
    if(NOT EXISTS "${MAIN_INCLUDE_DIR}")
        set(MAIN_INCLUDE_DIR "${MODULE_ROOT}/src/main/cpp/include")
    endif()
endif()

message(STATUS "Using include directory: ${MAIN_INCLUDE_DIR}")

# Check if the paths exist
if(NOT EXISTS "${FBJNI_DIR}")
    message(WARNING "FBJNI headers directory not found: ${FBJNI_DIR}")
endif()

if(NOT EXISTS "${TURBOMODULE_DIR}")
    message(WARNING "TurboModule directory not found: ${TURBOMODULE_DIR}")
endif()

# Warn if prebuilt llama library is not found
if(NOT EXISTS "${PREBUILT_LLAMA_LIB}")
    message(WARNING "Prebuilt llama library not found at ${PREBUILT_LLAMA_LIB}, build may fail!")
endif()

# Basic include paths
include_directories(
    "${CPP_DIR}"
    "${MAIN_INCLUDE_DIR}"
    # Add direct paths to llama.cpp headers
    "${CPP_DIR}/llama.cpp/include"
    "${CPP_DIR}/llama.cpp/common"
    "${CPP_DIR}/llama.cpp/common/minja"
    "${CPP_DIR}/llama.cpp/ggml/include"
    "${CPP_DIR}/llama.cpp/ggml/src"
    # React Native paths
    "${RN_DIR}/ReactAndroid/src/main/jni/react/turbomodule"
    "${RN_DIR}/ReactCommon"
    "${RN_DIR}/ReactCommon/callinvoker"
    "${RN_DIR}/ReactCommon/jsi"
    "${RN_DIR}/ReactCommon/react/nativemodule/core"
    "${RN_DIR}/ReactCommon/react/bridging"
)

# Check if source files exist
if(NOT EXISTS "${CPP_DIR}/LlamaCppRnModule.cpp")
    message(FATAL_ERROR "Cannot find ${CPP_DIR}/LlamaCppRnModule.cpp - check paths")
endif()

# Source files with explicit full paths to avoid resolution issues
set(SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/OnLoad.cpp
    ${CPP_DIR}/LlamaCppRnModule.cpp
    ${CPP_DIR}/LlamaCppModel.cpp
    ${CPP_DIR}/SystemUtils.cpp
    ${CPP_DIR}/rn-completion.cpp
)

# Verify each source file exists
foreach(src_file ${SOURCE_FILES})
    if(NOT EXISTS "${src_file}")
        message(WARNING "Source file not found: ${src_file}")
    endif()
endforeach()

# Create our module library
add_library(${CMAKE_PROJECT_NAME} SHARED ${SOURCE_FILES})

# Link against the prebuilt llama library if it exists
if(EXISTS "${PREBUILT_LLAMA_LIB}")
    message(STATUS "Found prebuilt llama library, linking against it: ${PREBUILT_LLAMA_LIB}")
    
    # Add the prebuilt library
    add_library(llama SHARED IMPORTED)
    set_target_properties(llama PROPERTIES IMPORTED_LOCATION "${PREBUILT_LLAMA_LIB}")
    
    # Link against the imported library
    target_link_libraries(${CMAKE_PROJECT_NAME} llama)
else()
    message(WARNING "Prebuilt llama library not found at ${PREBUILT_LLAMA_LIB}, build may fail!")
endif()

# Required libraries for Android
find_library(log-lib log)
find_library(android-lib android)

# Link required libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    ${log-lib}
    ${android-lib}
)

# Add preprocessor definitions
target_compile_definitions(${CMAKE_PROJECT_NAME}
    PRIVATE
    -DRCT_NEW_ARCH_ENABLED=1
)

# Set C++ standard
set_target_properties(
    ${CMAKE_PROJECT_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
) 

# Include the React Native application CMake utils
include(${REACT_ANDROID_DIR}/cmake-utils/ReactNative-application.cmake)
