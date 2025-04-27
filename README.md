# llamacpp-rn

> ⚠️ **WORK IN PROGRESS**: This package is currently under active development and not ready for production use. The implementation is incomplete and the package is not yet published to NPM.

A high-performance React Native Turbo Module implementation of [llama.rn](https://github.com/mybigday/llama.rn) that itself depend on [llama.cpp](https://github.com/ggml-org/llama.cpp), which provides llama.cpp integration for React Native applications.

⚠️ **Note**: This library requires the React Native New Architecture and will not work with the legacy architecture.

## Project Goals

- Convert the existing llama.rn library to use React Native's Turbo Module system
- Optimize performance for on-device LLM inference with direct C++ integration
- Provide a seamless API for React Native developers to use llama.cpp

## Features

- Run llama.cpp models directly in React Native applications
- Utilize the performance benefits of Turbo Modules with direct C++ access
- Cross-platform support for iOS and Android
- Optimized for speed with the New Architecture

## Performance Benefits

This library offers significant performance improvements over the original llama.rn implementation:

- **Direct C++ Access**: Uses JSI to communicate directly with llama.cpp without serialization
- **Reduced Memory Overhead**: No redundant copies of data between JavaScript and native code
- **Thread Safety**: Can run operations on background threads without blocking the JS thread
- **Efficient Memory Management**: Better control of memory allocation and caching

## Installation

```sh
npm install @novastera-oss/llamacpp-rn
# or
yarn add @novastera-oss/llamacpp-rn
```

## Usage

```javascript
import { LlamaCpp } from '@novastera-oss/llamacpp-rn';

// Initialize the model
const model = await LlamaCpp.loadModel({
  modelPath: 'path/to/model.gguf',
  contextSize: 2048,
  // other options
});

// Generate text
const response = await model.generate("What is React Native?");
console.log(response);
```

## API Reference

### LlamaCpp.loadModel(options)

Loads a GGUF model file with the specified options.

```typescript
interface ModelOptions {
  modelPath: string;      // Path to the .gguf model file
  contextSize?: number;   // Context size (default: 512)
  threads?: number;       // Number of threads to use (default: 4)
  gpuLayers?: number;     // Number of layers to offload to GPU (default: 0)
}
```

### model.generate(prompt, options?)

Generates text based on the provided prompt.

```typescript
interface GenerationOptions {
  maxTokens?: number;     // Maximum tokens to generate (default: 256)
  temperature?: number;   // Sampling temperature (default: 0.8)
  topP?: number;          // Top-p sampling (default: 0.9)
  stopSequences?: string[]; // Sequences that will stop generation (default: [])
}
```

## Architecture

This project uses React Native's Turbo Module architecture with an optimized design for direct C++ access.

### New Architecture Only

Unlike the original llama.rn, this library **only supports** the New Architecture to maximize performance. Key advantages:

- **Zero Overhead JSI**: Direct JavaScript to C++ calls
- **No Legacy Compatibility**: Eliminates branching code paths
- **Smaller Binary Size**: No need for legacy bridge code

### Integration with llama.cpp

The package uses llama.cpp as a Git submodule with these optimizations:

1. JavaScript calls directly access the C++ implementation
2. No intermediate Java/Objective-C++ translation of parameters
3. C++ code directly interacts with llama.cpp for model operations
4. Results are returned to JavaScript without serialization/deserialization
