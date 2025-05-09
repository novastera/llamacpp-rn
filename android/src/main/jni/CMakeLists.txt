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
    # Expo modules locations
    "${MODULE_ROOT}/../../node_modules/expo-modules-core/android/src/main/jni/include"
    "${MODULE_ROOT}/../../node_modules/expo-modules-core/android/build/intermediates/cxx/Debug/fbjni"
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

# Create the include paths manually
set(INCLUDE_DIRECTORIES
    "${CPP_DIR}"
    "${MAIN_INCLUDE_DIR}"
    "${CPP_DIR}/llama.cpp/include"
    "${CPP_DIR}/llama.cpp/common"
    "${CPP_DIR}/llama.cpp/common/minja"
    "${CPP_DIR}/llama.cpp/ggml/include"
    "${CPP_DIR}/llama.cpp/ggml/src"
    "${RN_DIR}/ReactCommon"
    "${RN_DIR}/ReactCommon/callinvoker"
    "${RN_DIR}/ReactCommon/jsi"
    "${RN_DIR}/ReactCommon/react/nativemodule/core"
    "${RN_DIR}/ReactCommon/react/bridging"
    "${REACT_ANDROID_DIR}/src/main/jni/react/turbomodule"
    "${REACT_ANDROID_DIR}/src/main/jni/first-party/fbjni/headers"
    "${REACT_ANDROID_DIR}/src/main/jni/react/jni"
    "${REACT_ANDROID_DIR}/src/main/cpp/jni"
    "${REACT_ANDROID_DIR}/src/main/java/com/facebook/react/turbomodule/core/jni"
)

# Add all possible FBJNI header directories
foreach(fbjni_dir ${FBJNI_HEADERS_DIRS})
    if(EXISTS "${fbjni_dir}")
        list(APPEND INCLUDE_DIRECTORIES "${fbjni_dir}")
        message(STATUS "Adding FBJNI headers from: ${fbjni_dir}")
    endif()
endforeach()

# Include all the directories
include_directories(${INCLUDE_DIRECTORIES})

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
    message(WARNING "Prebuilt llama library not found in any of the search paths. The module will build but will not function correctly at runtime. Please provide libllama.so in one of these locations:\n- ${MODULE_ROOT}/jniLibs/${ANDROID_ABI}/libllama.so\n- ${MODULE_ROOT}/src/main/jniLibs/${ANDROID_ABI}/libllama.so\n- ${MODULE_ROOT}/../jniLibs/${ANDROID_ABI}/libllama.so")
    
    # Define a mock implementation when the library is missing
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE LLAMA_MISSING=1)
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
    POSITION_INDEPENDENT_CODE ON
)

