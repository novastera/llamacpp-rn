# LlamaCppRn API Reference

This document provides detailed information about the available functions and objects in the LlamaCppRn module.

## Core Functions

### `initLlama(options: LlamaOptions): Promise<LlamaContext>`

Initializes a Llama model and returns a context for generating text.

#### Parameters:

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `modelPath` | `string` | Yes | - | Path to the model file (.gguf) |
| `contextSize` | `number` | No | 512 | Token context window size |
| `seed` | `number` | No | -1 | RNG seed, -1 for random |
| `batchSize` | `number` | No | 512 | Batch size for prompt processing |
| `threads` | `number` | No | 4 | Number of CPU threads to use |
| `gpuLayers` | `number` | No | 0 | Number of layers to offload to GPU (Metal on iOS) |
| `f16Memory` | `boolean` | No | true | Use half-precision for model computation |
| `embeddings` | `boolean` | No | false | Whether to enable embeddings generation |
| `loraAdapter` | `string` | No | "" | Path to LoRA adapter file |
| `loraBase` | `string` | No | "" | Path to LoRA base model |
| `logPrompt` | `boolean` | No | false | Whether to log prompt to console |
| `logitBias` | `Record<number, number>` | No | {} | Token bias dictionary |

#### Returns:

`Promise<LlamaContext>` - A promise that resolves to a Llama context object.

### `loadLlamaModelInfo(modelPath: string): Promise<LlamaModelInfo>`

Loads and returns information about a model without creating a full context.

#### Parameters:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `modelPath` | `string` | Yes | Path to the model file (.gguf) |

#### Returns:

`Promise<LlamaModelInfo>` - A promise that resolves to an object containing model information.

### `jsonSchemaToGbnf(jsonSchema: object): string`

Converts a JSON schema to Grammar BNF (GBNF) format.

#### Parameters:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `jsonSchema` | `object` | Yes | JSON schema to convert |

#### Returns:

`string` - GBNF representation of the JSON schema.

## LlamaContext Methods

### `context.tokenize(text: string): Promise<number[]>`

Converts a string to an array of token IDs.

#### Parameters:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `text` | `string` | Yes | Text to tokenize |

#### Returns:

`Promise<number[]>` - An array of token IDs.

### `context.detokenize(tokens: number[]): Promise<string>`

Converts an array of token IDs back to a string.

#### Parameters:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `tokens` | `number[]` | Yes | Array of token IDs |

#### Returns:

`Promise<string>` - Reconstructed text.

### `context.embedding(text: string): Promise<number[]>`

Generates embeddings for the input text.

#### Parameters:

| Parameter | Type | Required | Description |
|-----------|------|----------|-------------|
| `text` | `string` | Yes | Text to generate embeddings for |

#### Returns:

`Promise<number[]>` - Vector embeddings.

### `context.completion(options: CompletionOptions): Promise<CompletionResult>`

Generates a text completion based on the prompt.

#### Parameters:

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `prompt` | `string` | Yes | - | The input prompt |
| `maxTokens` | `number` | No | 512 | Maximum tokens to generate |
| `temperature` | `number` | No | 0.8 | Sampling temperature |
| `topP` | `number` | No | 0.9 | Nucleus sampling probability threshold |
| `stop` | `string[]` | No | [] | Stop sequences to end generation |
| `frequencyPenalty` | `number` | No | 0.0 | Repetition penalty |
| `presencePenalty` | `number` | No | 0.0 | Presence penalty |
| `logitBias` | `Record<number, number>` | No | {} | Token bias dictionary |
| `grammar` | `string` | No | "" | GBNF grammar for structured output |

#### Returns:

`Promise<CompletionResult>` - An object containing the generated text and metadata.

### `context.chat(options: ChatOptions): Promise<ChatResult>`

Generates a chat response based on a conversation.

#### Parameters:

| Parameter | Type | Required | Default | Description |
|-----------|------|----------|---------|-------------|
| `messages` | `ChatMessage[]` | Yes | - | Array of chat messages |
| `maxTokens` | `number` | No | 512 | Maximum tokens to generate |
| `temperature` | `number` | No | 0.8 | Sampling temperature |
| `topP` | `number` | No | 0.9 | Nucleus sampling probability threshold |
| `stop` | `string[]` | No | [] | Stop sequences to end generation |
| `frequencyPenalty` | `number` | No | 0.0 | Repetition penalty |
| `presencePenalty` | `number` | No | 0.0 | Presence penalty |
| `logitBias` | `Record<number, number>` | No | {} | Token bias dictionary |
| `grammar` | `string` | No | "" | GBNF grammar for structured output |

#### Returns:

`Promise<ChatResult>` - An object containing the assistant's response message and metadata.

### `context.release(): Promise<void>`

Releases the model resources from memory.

#### Returns:

`Promise<void>` - A promise that resolves when the resources have been released.

## Type Definitions

### `LlamaModelInfo`

Information about a loaded model.

```typescript
interface LlamaModelInfo {
  n_params: number;   // Number of parameters
  n_ctx_train: number; // Training context size
  n_vocab: number;    // Vocabulary size
  n_embd: number;     // Embedding dimension
  description: string; // Model description if available
  vocab: string[];    // Model vocabulary (first 100 tokens)
}
```

### `ChatMessage`

A single message in a chat conversation.

```typescript
interface ChatMessage {
  role: 'system' | 'user' | 'assistant';
  content: string;
}
```

### `CompletionResult`

Result of a text completion.

```typescript
interface CompletionResult {
  text: string;             // Generated text
  tokens: number;           // Number of tokens generated
  tokenIds: number[];       // Array of generated token IDs
  logprobs?: LogProbs;      // Log probabilities if requested
  finishReason: 'stop' | 'length' | 'content_filter';
}
```

### `ChatResult`

Result of a chat completion.

```typescript
interface ChatResult {
  message: {
    role: 'assistant';
    content: string;
  };
  tokens: number;           // Number of tokens generated
  tokenIds: number[];       // Array of generated token IDs
  finishReason: 'stop' | 'length' | 'content_filter';
}
```

### `LogProbs`

Log probability information for generated tokens.

```typescript
interface LogProbs {
  tokens: string[];          // Generated tokens as strings
  token_logprobs: number[];  // Log probabilities for each token
  top_logprobs: Array<Record<string, number>>; // Top alternatives
}
``` 