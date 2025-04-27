# LlamaCppRn

> ⚠️ **WORK IN PROGRESS**: This package is currently under active development and not ready for production use. The implementation is incomplete and the package is not yet published to NPM.

## Features

- Run Llama-based models directly on device using React Native
- Fast native C++ implementation using TurboModules and JSI
- Metal/GPU acceleration on iOS
- Simple JavaScript API
- Chat completion with prompt templates
- Embeddings generation
- JSON mode with grammar constraints

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
  modelPath: 'path/to/model.gguf',
  contextSize: 2048,
  seed: 42,
  batchSize: 512,
  gpuLayers: 42, // Set to 0 for CPU only
});

// Generate a completion
const result = await context.completion({
  prompt: 'What is artificial intelligence?',
  maxTokens: 256,
  temperature: 0.7,
  topP: 0.9
});

console.log('Response:', result.text);

// Chat completion
const messages = [
  { role: 'system', content: 'You are a helpful assistant.' },
  { role: 'user', content: 'Tell me about React Native.' }
];

const chatResult = await context.chat({
  messages,
  maxTokens: 512,
  temperature: 0.7,
  stop: ['USER:']
});

console.log('Chat response:', chatResult.message.content);
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

## Building from Source

See [BUILDING.md](./BUILDING.md) for instructions on building the native modules.

## License

MIT
