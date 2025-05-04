# RN-Llama: React Native Llama.cpp Library

This module provides a standalone implementation of the llama.cpp text completion and chat completion functionality, designed for direct integration into applications without requiring a server.

## Overview

The RN-Llama library consists of three main components:

1. `rn-utils.hpp`: Utility functions and data structures for the RN-Llama implementation
2. `rn-llama.cpp`: Core implementation of completion and chat completion functionality
3. `rn-main.cpp`: Example CLI application demonstrating usage

## Building

### Using the build script

The easiest way to build the RN-Llama library is to use the provided build script:

```bash
./tools/server/build-rn-main.sh
```

This will:
1. Apply necessary patches to the build system
2. Configure CMake with the appropriate options
3. Build the `rn-main` executable

The built binary will be located at `build/bin/rn-main`.

### Manual build

If you prefer to build manually, follow these steps:

1. Make sure the `server/rn-main` directory is included in the build:
   ```
   patch -p0 < tools/CMakeLists.txt.patch
   ```

2. Configure and build the project:
   ```bash
   mkdir -p build
   cd build
   cmake .. -DLLAMA_BUILD_SERVER=ON
   cmake --build . --target rn-main
   ```

## Usage

### Running the example application

Once built, you can run the example application with a model:

```bash
./build/bin/rn-main -m /path/to/your/model.gguf -c 2048
```

This will start an interactive shell where you can:
- Enter text to get completions
- Type `.chat` to switch to chat mode
- Type `.complete` to switch back to completion mode
- Type `.exit` to quit

### Using the library in your own code

To use the RN-Llama library in your own code:

1. Include the necessary headers:
   ```cpp
   #include "rn-utils.hpp"
   #include "rn-llama.cpp"
   ```

2. Initialize the model:
   ```cpp
   common_params params;
   // Set your parameters
   params.model.path = "/path/to/model.gguf";
   
   // Initialize the context
   rn_llama_context* ctx = rn_llama_init(params);
   ```

3. Set options and run completions:
   ```cpp
   // For text completion
   CompletionOptions options;
   options.prompt = "Complete this sentence:";
   auto result = run_completion(ctx, options);
   
   // For chat completion
   options.messages = json::array({
       json{{"role", "system"}, {"content", "You are a helpful assistant."}},
       json{{"role", "user"}, {"content", "Hello, who are you?"}}
   });
   auto result = run_chat_completion(ctx, options);
   ```

4. Clean up when done:
   ```cpp
   rn_llama_free(ctx);
   ```

## API Reference

### Core Functions

- `rn_llama_init(const common_params& params)`: Initialize the RN-Llama context
- `run_completion(rn_llama_context* ctx, const CompletionOptions& options, callback)`: Generate text completions
- `run_chat_completion(rn_llama_context* ctx, const CompletionOptions& options, callback)`: Generate chat completions
- `rn_llama_free(rn_llama_context* ctx)`: Free the RN-Llama context 