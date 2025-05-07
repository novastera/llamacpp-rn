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

## Tool Definitions

```typescript
interface LlamaTool {
  type: 'function';
  function: {
    name: string;              // Name of the function
    description?: string;      // Optional description of what the function does
    parameters: {              // JSON Schema object describing the parameters
      type: 'object';          // Must be 'object' type
      properties: {            // Map of parameter names to their schemas
        [key: string]: {
          type: string;        // Data type: string, number, boolean, etc.
          description?: string; // Parameter description
          enum?: string[];     // Possible values for enumerated types
        }
      };
      required?: string[];     // Array of required parameter names
    }
  }
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
  // Basic response fields
  text: string;                // The generated completion text
  tokens_predicted: number;    // Number of tokens generated
  timings: {
    predicted_n: number;      // Number of tokens predicted
    predicted_ms: number;     // Time spent generating tokens (ms)
    prompt_n: number;         // Number of tokens in the prompt
    prompt_ms: number;        // Time spent processing prompt (ms)
    total_ms: number;         // Total time spent (ms)
  };
  
  // OpenAI-compatible format - a structured format similar to OpenAI's API
  choices?: Array<{
    index: number;             // Index of the choice (usually 0)
    message: {
      role: string;            // Role of the message (e.g., 'assistant')
      content: string;         // Content of the message
      tool_calls?: Array<{     // Tool calls if any were generated
        id: string;            // Unique identifier for the tool call
        type: string;          // Type of tool (e.g., 'function')
        function: {
          name: string;        // Name of the function to call
          arguments: string;   // JSON string of arguments for the function
        }
      }>
    };
    finish_reason: 'stop' | 'length' | 'tool_calls'; // Why generation stopped
  }>;
  
  // Tool calls may also appear at the top level for simpler access
  tool_calls?: Array<{
    id: string;                // Unique identifier for the tool call
    type: string;              // Type of tool (e.g., 'function')
    function: {
      name: string;            // Name of the function to call
      arguments: string;       // JSON string of arguments for the function
    };
  }>;
}
```

When working with tool calls, you should check for tool calls in this order:
1. Look for `response.choices?.[0]?.finish_reason === 'tool_calls'` to determine if tool calling happened
2. Access tool calls from `response.choices?.[0]?.message?.tool_calls` (OpenAI-compatible format)
3. Or access from `response.tool_calls` (simplified top-level access)

### Embedding Response
```typescript
interface EmbeddingResponse {
  data: Array<{
    embedding: number[] | string;  // Array of floats or base64 string
    index: number;                // Index in the batch (usually 0)
    object: 'embedding';          // Object type
    encoding_format?: 'base64';   // Present when using base64 encoding
  }>;
  model: string;                  // Model identifier
  object: 'list';                 // Object type
  usage: {
    prompt_tokens: number;        // Tokens in the prompt
    total_tokens: number;         // Total tokens processed
  };
}
```

For more detailed TypeScript definitions, see [NativeLlamaCppRn.ts](./src/specs/NativeLlamaCppRn.ts). 

## Example: Processing Tool Calls

```typescript
// Perform a completion that might result in a tool call
const response = await context.completion({
  messages: [...],
  tools: [...],
  tool_choice: 'auto'
});

// Check if a tool call was generated
if (response.choices?.[0]?.finish_reason === 'tool_calls' || response.tool_calls?.length > 0) {
  // Get the tool calls from either location
  const toolCalls = response.choices?.[0]?.message?.tool_calls || response.tool_calls || [];
  
  // Process each tool call
  for (const toolCall of toolCalls) {
    if (toolCall.function) {
      // Extract function name and arguments
      const functionName = toolCall.function.name;
      const args = JSON.parse(toolCall.function.arguments);
      
      // Process the tool call based on the function name
      let result;
      if (functionName === 'get_weather') {
        result = await getWeatherData(args.location);
      } else if (functionName === 'search_database') {
        result = await searchDatabase(args.query);
      }
      
      // Send the result back to the model in a follow-up completion
      const finalResponse = await context.completion({
        messages: [
          ...previousMessages,
          { 
            role: 'assistant', 
            content: null, 
            tool_calls: [toolCall] 
          },
          {
            role: 'tool',
            tool_call_id: toolCall.id,
            name: functionName,
            content: JSON.stringify(result)
          }
        ]
      });
      
      // finalResponse now contains the model's response after processing the tool result
    }
  }
} 