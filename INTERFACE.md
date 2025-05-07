# Interface Documentation

This document details the API interfaces for llamacpp-rn. The library aims to be compatible with the [llama.cpp server](https://github.com/ggml-org/llama.cpp/tree/master/examples/server) API where possible.

## Model Initialization Parameters

```typescript
interface LlamaModelParams {
  // Required
  model: string;               // path to the model file

  // Context and Processing
  n_ctx?: number;             // context size (default: 2048)
  n_batch?: number;           // batch size (default: 512)
  n_ubatch?: number;          // micro batch size for prompt processing
  n_threads?: number;         // number of threads
  n_keep?: number;            // number of tokens to keep from initial prompt
  
  // GPU Acceleration
  n_gpu_layers?: number;      // number of layers to store in VRAM (default: 0)
  
  // Memory Management
  use_mmap?: boolean;         // use mmap for faster loading (default: true)
  use_mlock?: boolean;        // use mlock to keep model in memory (default: false)
  
  // Model Behavior
  vocab_only?: boolean;       // only load vocabulary
  embedding?: boolean;        // use embedding mode (default: false)
  seed?: number;              // RNG seed
  
  // RoPE Parameters
  rope_freq_base?: number;    // RoPE base frequency
  rope_freq_scale?: number;   // RoPE frequency scaling factor
  
  // YaRN Parameters
  yarn_ext_factor?: number;   // extrapolation mix factor
  yarn_attn_factor?: number;  // magnitude scaling factor
  yarn_beta_fast?: number;    // low correction dim
  yarn_beta_slow?: number;    // high correction dim
  
  // Additional Options
  logits_all?: boolean;       // return logits for all tokens
  chat_template?: string;     // override chat template
  use_jinja?: boolean;        // use Jinja template parser
  verbose?: number;           // verbosity level
  
  // LoRA Support
  lora_adapters?: Array<{
    path: string;             // adapter file path
    scale?: number;           // scaling factor (default: 1.0)
  }>;
}
```

## Completion Parameters

```typescript
interface LlamaCompletionParams {
  // Input Methods
  prompt?: string;            // text prompt
  system_prompt?: string;     // system prompt for chat mode
  messages?: LlamaMessage[];  // chat messages
  
  // Generation Control
  temperature?: number;       // sampling temperature (default: 0.8)
  top_p?: number;            // top-p sampling (default: 0.95)
  top_k?: number;            // top-k sampling (default: 40)
  n_predict?: number;        // max tokens to predict
  max_tokens?: number;       // alias for n_predict
  stop?: string[];          // stop sequences
  stream?: boolean;         // stream tokens (default: true)

  // Tool Support
  tool_choice?: string | 'auto' | 'none';
  tools?: LlamaTool[];

  // Advanced Parameters
  repeat_penalty?: number;   // repetition penalty (default: 1.1)
  repeat_last_n?: number;   // last n tokens for repetition penalty (default: 64)
  frequency_penalty?: number; // frequency penalty (default: 0.0)
  presence_penalty?: number; // presence penalty (default: 0.0)
  seed?: number;            // RNG seed (default: -1)
  grammar?: string;         // GBNF grammar for structured output
}
```

## Model Path Handling

The module accepts different path formats depending on the platform:

### iOS
- Bundle path: `models/model.gguf` (if added to Xcode project)
- Absolute path: `/path/to/model.gguf`

### Android
- Asset path: `asset:/models/model.gguf`
- File path: `file:///path/to/model.gguf`

## Response Formats

### Completion Result
```typescript
interface LlamaCompletionResult {
  text: string;
  tokens_predicted: number;
  timings: {
    predicted_n: number;
    predicted_ms: number;
    prompt_n: number;
    prompt_ms: number;
    total_ms: number;
  };
  tool_calls?: Array<{
    id: string;
    type: string;
    function: {
      name: string;
      arguments: string;
    };
  }>;
}
```

### Embedding Response
```typescript
interface EmbeddingResponse {
  data: Array<{
    embedding: number[] | string;
    index: number;
    object: 'embedding';
    encoding_format?: 'base64';
  }>;
  model: string;
  object: 'list';
  usage: {
    prompt_tokens: number;
    total_tokens: number;
  };
}
```

For more detailed TypeScript definitions, see [NativeLlamaCppRn.ts](./src/specs/NativeLlamaCppRn.ts). 