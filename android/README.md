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

# Android GPU Support for llamacpp-rn

This document provides information about GPU acceleration on Android using llamacpp-rn.

## Supported GPU Backends

llamacpp-rn supports two GPU backends on Android:

1. **OpenCL** - Works on most Qualcomm Snapdragon devices and other Android devices with OpenCL support
2. **Vulkan** - More modern graphics API with broader support

## Device Requirements

To use GPU acceleration, your Android device needs:

- A GPU that supports OpenCL and/or Vulkan
- Android 7.0 (API level 24) or higher
- For best performance:
  - Qualcomm Snapdragon with Adreno GPU (for OpenCL optimizations)
  - At least 6GB RAM

## How to Enable GPU Acceleration

GPU acceleration is automatically enabled in llamacpp-rn if the required libraries are detected on the device. Use the `n_gpu_layers` parameter to control how many layers are offloaded to the GPU:

```javascript
import { initLlama } from '@novastera-oss/llamacpp-rn';

// Initialize the model with GPU acceleration
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_ctx: 2048,
  n_gpu_layers: 33  // Number of layers to offload to GPU, use large number like 33 to offload all
});
```

## Troubleshooting GPU Support

If your app is having trouble with GPU acceleration on Android:

1. Check if the device has OpenCL libraries:
   - `/vendor/lib64/libOpenCL.so`
   - `/vendor/lib64/egl/libGLES_mali.so` (for Mali GPUs)
   - `/system/vendor/lib64/libOpenCL.so`

2. Check if GPU acceleration is actually being used with `logs` option:
   ```javascript
   const context = await initLlama({
     model: 'path/to/model.gguf',
     n_gpu_layers: 33,
     logs: true  // Enable detailed logging
   });
   ```
   
   Look for messages indicating GPU usage like:
   ```
   ggml_opencl: selecting platform: 'QUALCOMM Snapdragon(TM)'
   ggml_opencl: selecting device: 'QUALCOMM Adreno(TM)'
   ```

3. If no GPU is detected, or you get errors, try setting `n_gpu_layers: 0` to fall back to CPU-only mode.

## Testing GPU Compatibility

You can test if your device supports GPU acceleration by running the example app included with this library. The performance metrics will help you determine if GPU acceleration is working properly.

## Known Issues

- Some older devices may have incomplete OpenCL implementations that can cause crashes
- Mobile GPUs may throttle under extended load, leading to performance variations
- Memory limitations can cause issues with larger models
- Some Mali GPUs might report inaccurate memory usage

## Best Practices

- **Lower Precision Models**: Use Q4_0, Q4_K, or Q5_K quantized models for better GPU performance
- **Memory Management**: Set appropriate context size (`n_ctx`) to avoid OOM errors
- **Battery Considerations**: GPU acceleration uses more power, so monitor battery impact in your app
- **Fallback**: Always implement a fallback to CPU if GPU acceleration fails
- **Model Size**: For mobile devices, models under 7B parameters are recommended 