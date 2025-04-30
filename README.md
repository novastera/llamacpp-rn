# LlamaCppRn

> ⚠️ **WORK IN PROGRESS**: This package is currently under active development and not ready for production use. The implementation is incomplete and the package is not yet published to NPM.

This library was greatly inspired by [llama.rn](https://github.com/mybigday/llama.rn) and aims to provide a thin wrapper around [llama.cpp](https://github.com/ggerganov/llama.cpp) to make it work seamlessly on mobile devices. The goal is to maintain a minimal abstraction layer while providing automatic CPU/GPU detection capabilities to simplify mobile development.

## Features

- Run Llama-based models directly on device using React Native
- Fast native C++ implementation using TurboModules and JSI
- Metal/GPU acceleration on iOS
- Simple JavaScript API
- Chat completion with prompt templates
- Embeddings generation
- JSON mode with grammar constraints
- **Opinionated defaults** for thread count and GPU layers

## Opinionated Resource Handling

Given the wide diversity of mobile devices, LlamaCppRn takes an opinionated approach to resource management:

- **Thread Count**: If `n_threads` is not provided, the library attempts to select a reasonable thread count based on the device's CPU cores.
- **GPU Layers**: If `n_gpu_layers` is not specified, the library makes a best-effort estimate based on the device's capabilities and model parameters.
- **User Control**: When you explicitly provide values for `n_threads` or `n_gpu_layers`, your values always take precedence.
- **Graceful Fallback**: If GPU acceleration is requested but not available, the library silently falls back to CPU-only mode.

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
