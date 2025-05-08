# Android Native Setup

This directory contains the Android-specific configuration for the `@novastera-oss/llamacpp-rn` module.

## Directory Structure

```
android/
├── build.gradle                   # Android build configuration
└── src/
    └── main/
        ├── jni/                   # Native code integration
        │   ├── CMakeLists.txt     # CMake build file
        │   └── OnLoad.cpp         # Native module registration
        ├── cpp/                   # Header includes
        │   └── include/           # For prefab publishing
        └── jniLibs/               # Prebuilt native libraries
            ├── arm64-v8a/         # ARM64 libraries
            └── x86_64/            # x86_64 libraries for emulators
```

## Build Process

The Android native module is built using:

1. **CMake** - For native C++ code compilation
2. **Prefab** - For publishing C++ headers to consuming modules

The build.gradle file configures the Android Library with the necessary settings to support the native code integration.

## Native Integration

The module follows the [Pure C++ Modules](https://reactnative.dev/docs/the-new-architecture/pure-cxx-modules) approach from React Native documentation, where TurboModules are implemented directly in C++ without Java/Kotlin bindings.

The native module is registered in `OnLoad.cpp` and made available to JS through the TurboModule system. 