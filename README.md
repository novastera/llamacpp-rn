# LlamaCppRn

> ⚠️ **WORK IN PROGRESS**: This package is currently under active development and not ready for production use. The implementation is incomplete and the package is not yet published to NPM.

This library was greatly inspired by [llama.rn](https://github.com/mybigday/llama.rn) and aims to provide a thin wrapper around [llama.cpp](https://github.com/ggerganov/llama.cpp) to make it work seamlessly on mobile devices. The goal is to maintain a minimal abstraction layer while providing automatic CPU/GPU detection capabilities to simplify mobile development.

## Features

- Run Llama-based models directly on device using React Native
- Fast native C++ implementation using TurboModules and JSI
- Metal/GPU acceleration on iOS
- OpenCL and Vulkan GPU acceleration on Android
- Simple JavaScript API
- Chat completion with prompt templates
- Embeddings generation
- JSON mode with grammar constraints
- **Opinionated defaults** for thread count and GPU layers

## Compatibility with llama.cpp Server

This library is designed to be compatible with the [llama.cpp server](https://github.com/ggml-org/llama.cpp/tree/master/examples/server) API. This means:

- Similar parameter naming and behavior
- Support for most llama.cpp server options

If you're already using llama.cpp server in your projects, good for you :D

```typescript
// Server-compatible initialization
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_ctx: 2048,
  n_batch: 512,
  n_gpu_layers: 32,
  use_mlock: true
});

// Server-compatible completion parameters
const result = await context.completion({
  prompt: 'What is artificial intelligence?',
  temperature: 0.7,
  top_p: 0.95,
  top_k: 40,
  min_p: 0.05,
  n_predict: 256,
  stop: ['\n\n'],
  repeat_penalty: 1.1,
  repeat_last_n: 64,
  frequency_penalty: 0.0,
  presence_penalty: 0.0
});
```

For a complete list of supported parameters, see the [API.md](./API.md) file or check the TypeScript definitions.

## Opinionated Resource Handling

Given the wide diversity of mobile devices, LlamaCppRn takes an opinionated approach to resource management:

- **Thread Count**: If `n_threads` is not provided, the library attempts to select a reasonable thread count based on the device's CPU cores.
- **User Control**: When you explicitly provide values for `n_threads` or `n_gpu_layers`, your values always take precedence.
- **Graceful Fallback**: If GPU acceleration is requested but not available, the library silently falls back to CPU-only mode.

### Getting Optimal GPU Layer Information

To make informed decisions about GPU acceleration, you can use the `loadLlamaModelInfo` function to get the recommended GPU layer count:

```typescript
// Get model info including optimal GPU layer count
const modelInfo = await loadLlamaModelInfo('path/to/model.gguf');

console.log(`GPU supported: ${modelInfo.gpuSupported}`);
console.log(`Optimal GPU layers: ${modelInfo.optimalGpuLayers}`);
console.log(`Quantization type: ${modelInfo.quant_type}`);

// Use the optimal GPU layers in model initialization
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_gpu_layers: modelInfo.optimalGpuLayers,
  // other parameters...
});
```

This approach allows your application to:
1. Check if GPU acceleration is supported
2. Get a model-specific recommended value for GPU layers
3. Make an informed decision based on model size and quantization type

This approach aims to simplify development across different mobile devices while still giving you full control when needed.

## Known Issues

### iOS Simulator GPU Detection

When running on iOS simulators, the library may detect GPU support but fail when attempting to use it. This is a known limitation of Metal in iOS simulators:

```typescript
// For iOS simulator testing, explicitly disable GPU acceleration:
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_gpu_layers: 0, // Force CPU-only mode on simulators
  // other parameters...
});
```

### Android GPU Acceleration

Android devices have varying GPU capabilities. The library supports both OpenCL and Vulkan:

- OpenCL is prioritized and used when available (most common on Mali, Adreno, and PowerVR GPUs)
- Vulkan is used as a fallback when OpenCL is not available
- The library automatically detects which technologies are supported at runtime
- For optimal performance, target Android 10+ (API level 29+)

For debugging GPU issues on Android, you can explicitly control the acceleration:

```typescript
// Force CPU-only mode:
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_gpu_layers: 0,
  // other parameters...
});

// Force a specific number of GPU layers:
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_gpu_layers: 24,  // Use up to 24 GPU layers
  // other parameters...
});
```

Always test GPU acceleration on real devices. For simulator development, explicitly set `n_gpu_layers: 0` to avoid potential crashes.

## Installation

```sh
npm install @novastera-oss/llamacpp-rn
```

## Quick Start (Example App)

1. Clone the repository:
   ```sh
   git clone https://github.com/novastera/llamacpp-rn.git
   cd llamacpp-rn
   ```

2. Install dependencies:
   ```sh
   npm install
   cd example
   npm install
   ```

3. Download a model file (e.g., Mistral-7B-Instruct-v0.3.Q4_K_M.gguf) and place it in the `example/assets` directory.

4. Run the setup script to prepare the model for use:
   ```sh
   cd example
   npm run setup-model
   ```

5. For iOS, open the Xcode project and add the model file:
   - Open `example/ios/example.xcworkspace` in Xcode
   - Right-click on the "example" project in the Navigator
   - Select "Add Files to example..."
   - Navigate to `example/ios/example/models` and select the model file
   - Make sure "Create folder references" is selected
   - Click "Add"

6. Run the example app:
   ```sh
   # iOS
   npm run ios
   
   # Android
   npm run android
   ```

## Basic Usage

```typescript
import { initLlama, loadLlamaModelInfo } from '@novastera-oss/llamacpp-rn';

// Load model info to check capabilities without creating a context
const modelInfo = await loadLlamaModelInfo('path/to/model.gguf');
console.log(`Model has ${modelInfo.n_params} parameters`);

// Initialize the model
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_ctx: 2048,
  n_batch: 512,
  // Optionally specify GPU layers (if omitted, defaults will be applied)
  // n_gpu_layers: 32,
  // Optionally specify thread count (if omitted, defaults will be applied)
  // n_threads: 4,
});

// Generate a completion
const result = await context.completion({
  prompt: 'What is artificial intelligence?',
  n_predict: 256,
  temperature: 0.7,
  top_p: 0.9
});

console.log('Response:', result.text);

// Chat completion
const messages = [
  { role: 'system', content: 'You are a helpful assistant.' },
  { role: 'user', content: 'Tell me about React Native.' }
];

const chatResult = await context.completion({
  messages,
  n_predict: 512,
  temperature: 0.7,
  stop: ['USER:']
});

console.log('Chat response:', chatResult.text);
```

## Model Path Handling

The module accepts different types of paths depending on the platform:

### iOS
- Bundle path: `models/model.gguf` (if added to the Xcode project)
- Absolute path: `/path/to/model.gguf`

### Android
- Asset path: `asset:/models/model.gguf`
- File path: `file:///path/to/model.gguf`

## API Reference

Full API documentation can be found in the [API.md](./API.md) file.

## Prebuilt Binaries

This package is distributed with prebuilt binaries to make installation easier. See [PREBUILT.md](./PREBUILT.md) for details on how these work and how to package them for distribution.

## About

Part of [Novastera](https://novastera.com)'s suite of privacy-focused solutions, this package enables on-device LLM inference with no data leaving the user's device. We're committed to helping developers build AI-powered applications that respect user privacy and provide complete control over sensitive data. By running models locally rather than relying on cloud APIs, your application gains independence from external services while ensuring user data remains private.

Visit our website to learn more about our approach to privacy-first AI solutions.

## License

Apache 2.0 © [Novastera](https://novastera.com)
