# LlamaCppRn

> ⚠️ **WORK IN PROGRESS**: This package is currently under active development. Community help and feedback are greatly appreciated, especially in the areas mentioned in [What Needs Help](#what-needs-help).

A React Native wrapper for [llama.cpp](https://github.com/ggml-org/llama.cpp) focused on providing a simple, reliable way to run LLMs on mobile devices. This project was inspired by and builds upon the excellent work of [llama.rn](https://github.com/mybigday/llama.rn).

## Goals

- Provide a thin, reliable wrapper around llama.cpp for React Native
- Maintain compatibility with llama.cpp server API where possible
- Make it easy to run LLMs on mobile devices with automatic resource management
- Keep the codebase simple and maintainable

## Current Features

- Basic model loading and inference
- Metal support on iOS
- OpenCL/Vulkan support on Android (in progress)
- Automatic CPU/GPU detection
- Chat completion with templates
- Embeddings generation
- Grammar-based output constraints

## What Needs Help

We welcome contributions, especially in these areas:

1. **Tool Support**: The tool calling functionality needs improvement to better handle complex interactions
2. **Testing**: 
   - Automated testing using the example project
   - Testing on GPU-enabled Android devices
   - More comprehensive unit tests
3. **Documentation**: Improving examples and usage guides
4. **Performance**: Optimizing resource usage on different devices

If you're interested in helping with any of these areas, please check our [Contributing Guide](./CONTRIBUTING.md).

## Installation

```sh
npm install @novastera-oss/llamacpp-rn
```

## Basic Usage

```typescript
import { initLlama } from '@novastera-oss/llamacpp-rn';

// Initialize the model
const context = await initLlama({
  model: 'path/to/model.gguf',
  n_ctx: 2048,
  n_batch: 512
});

// Generate a completion
const result = await context.completion({
  prompt: 'What is artificial intelligence?',
  temperature: 0.7,
  top_p: 0.95
});

console.log('Response:', result.text);
```

## Documentation

- [Interface Documentation](./INTERFACE.md) - Detailed API interfaces
- [Example App](./example/) - Working example with common use cases
- [Contributing Guide](./CONTRIBUTING.md) - How to help improve the library

## Model Path Handling

The module accepts different path formats depending on the platform:

### iOS
- Bundle path: `models/model.gguf` (if added to Xcode project)
- Absolute path: `/path/to/model.gguf`

### Android
- Asset path: `asset:/models/model.gguf`
- File path: `file:///path/to/model.gguf`

## About

Part of [Novastera](https://novastera.com)'s suite of privacy-focused solutions, this package enables on-device LLM inference with no data leaving the user's device. We're committed to helping developers build AI-powered applications that respect user privacy.

## License

Apache 2.0 © [Novastera](https://novastera.com)
