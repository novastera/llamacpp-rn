cmake_minimum_required(VERSION 3.13)
set(CMAKE_VERBOSE_MAKEFILE ON)

# Define the library name
project(llamacpprn)

# Print system info for debugging
message(STATUS "Building for Android with ABI: ${ANDROID_ABI}")
message(STATUS "Android SDK version: ${ANDROID_PLATFORM}")
message(STATUS "Android NDK: ${ANDROID_NDK}")

# Get the path to the tm directory
get_filename_component(MODULE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(TM_ROOT "${MODULE_ROOT}/../../tm" ABSOLUTE)

# Print path information for debugging
message(STATUS "Module root directory: ${MODULE_ROOT}")
message(STATUS "TM directory: ${TM_ROOT}")

# Make sure the ReactNative CMake includes are processed
if(DEFINED REACT_ANDROID_DIR)
  include(${REACT_ANDROID_DIR}/cmake-utils/ReactNative-application.cmake)
endif()

# Define the path to llama.cpp directory
set(LLAMA_CPP_DIR "${TM_ROOT}/llama.cpp")

# Make sure the llama.cpp submodule exists
if(NOT EXISTS "${LLAMA_CPP_DIR}/CMakeLists.txt")
    message(STATUS "llama.cpp submodule not found at ${LLAMA_CPP_DIR}")
    
    # Try alternate paths
    if(EXISTS "${TM_ROOT}/llama.cpp/CMakeLists.txt")
        set(LLAMA_CPP_DIR "${TM_ROOT}/llama.cpp")
        message(STATUS "Found llama.cpp at ${LLAMA_CPP_DIR}")
    else()
        message(FATAL_ERROR "Could not find llama.cpp submodule. Please run 'git submodule update --init --recursive'")
    endif()
endif()

# Add necessary include directories
include_directories(
  ${REACT_ANDROID_DIR}/ReactCommon
  ${REACT_ANDROID_DIR}/ReactCommon/callinvoker
  ${REACT_ANDROID_DIR}/ReactCommon/jsi
  ${TM_ROOT}
  ${LLAMA_CPP_DIR}
  ${LLAMA_CPP_DIR}/include
)

# Add our C++ module files from the tm directory
target_sources(${CMAKE_PROJECT_NAME} PRIVATE 
  ${TM_ROOT}/LlamaCppRnModule.cpp
  ${TM_ROOT}/LlamaCppModel.cpp
  ${TM_ROOT}/SystemUtils.cpp
  ${TM_ROOT}/rn-completion.cpp
)

# Look for the prebuilt llama library in jniLibs
find_library(llama-lib llama PATHS ${MODULE_ROOT}/jniLibs/${ANDROID_ABI})
if(llama-lib)
  message(STATUS "Found prebuilt llama library: ${llama-lib}")
  target_link_libraries(${CMAKE_PROJECT_NAME} ${llama-lib})
else()
  message(STATUS "Prebuilt llama library not found, will use dynamic loading")
  # We'll use dynamic loading via dlopen instead of direct linking
  add_definitions(-DLLAMA_DYNAMIC_LOADING=1)
endif()

# Enable optimizations for Release builds
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -DNDEBUG")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -O3 -DNDEBUG")
endif()

# Define the RCT_NEW_ARCH_ENABLED flag
add_definitions(-DRCT_NEW_ARCH_ENABLED=1)

# Link against required libraries
find_library(log-lib log)
target_link_libraries(${CMAKE_PROJECT_NAME} ${log-lib})

# Add Android specific library dependencies
find_library(android-lib android)
if(android-lib)
  target_link_libraries(${CMAKE_PROJECT_NAME} ${android-lib})
endif()

# Add dynamic linker library for dlopen
find_library(dl-lib dl)
if(dl-lib)
  target_link_libraries(${CMAKE_PROJECT_NAME} ${dl-lib})
endif()
