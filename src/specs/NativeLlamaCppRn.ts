import type { TurboModule } from 'react-native';
import { TurboModuleRegistry } from 'react-native';

/**
 * Native LlamaCppRn Module
 * 
 * The API is designed to be compatible with the llama.cpp server API:
 * https://github.com/ggml-org/llama.cpp/tree/master/examples/server
 * 
 * However, since we are in the context of a mobile app, we need to make some adjustments.
 */

export interface LlamaContextType {
  // This will be a native object reference that maps to
  // a pointer to the llama_context C++ object
}

export interface LlamaModelParams {
  // Model loading parameters
  model: string;               // path to the model file
  model_alias?: string;        // alias for the model name
  n_ctx?: number;              // context size (default: 512)
  n_batch?: number;            // batch size (default: 512)
  n_threads?: number;          // number of threads (default: number of physical CPU cores)
  n_gpu_layers?: number;       // number of layers to store in VRAM (default: 0)
  use_mmap?: boolean;          // use mmap for faster loading (default: true)
  use_mlock?: boolean;         // use mlock to keep model in memory (default: false)
  vocab_only?: boolean;        // only load the vocabulary, no weights
  embedding?: boolean;         // use embedding mode (default: false)
  rope_freq_base?: number;     // RoPE base frequency (default: 10000.0)
  rope_freq_scale?: number;    // RoPE frequency scaling factor (default: 1.0)
  logits_all?: boolean;        // return logits for all tokens (needed for perplexity calculation)
  grammar?: string;            // GBNF grammar for grammar-based sampling
}

export interface LlamaCompletionParams {
  // Basic completion parameters
  prompt?: string;             // text prompt
  system_prompt?: string;      // system prompt for chat mode
  messages?: LlamaMessage[];   // chat messages
  temperature?: number;        // sampling temperature (default: 0.8)
  top_p?: number;              // top-p sampling (default: 0.95)
  top_k?: number;              // top-k sampling (default: 40)
  min_p?: number;              // min-p sampling (default: 0.05)
  typical_p?: number;          // locally typical sampling (default: 1.0)
  n_predict?: number;          // max tokens to predict (default: -1, infinite)
  n_keep?: number;             // number of tokens to keep from initial prompt
  stop?: string[];             // stop sequences
  stream?: boolean;            // stream tokens as they're generated (default: true)
  tfs_z?: number;              // tail free sampling value (default: 1.0)
  chat_template?: string;      // optional chat template name to use
  cache_prompt?: boolean;      // cache the prompt for faster reuse

  // Tool calling parameters
  jinja?: boolean;             // Enable Jinja template parser
  tool_choice?: string | 'auto' | 'none'; // Tool choice mode
  tools?: LlamaTool[];         // Available tools

  // Advanced parameters
  frequency_penalty?: number;  // frequency penalty (default: 0.0)
  presence_penalty?: number;   // presence penalty (default: 0.0)
  repeat_penalty?: number;     // repetition penalty (default: 1.1)
  repeat_last_n?: number;      // last n tokens to consider for repetition penalty (default: 64)
  penalize_nl?: boolean;       // penalize newlines (default: true)
  mirostat?: number;           // use Mirostat sampling (default: 0)
  mirostat_tau?: number;       // Mirostat target entropy (default: 5.0)
  mirostat_eta?: number;       // Mirostat learning rate (default: 0.1)
  seed?: number;               // RNG seed (default: -1, random)
  ignore_eos?: boolean;        // ignore end of sequence token (default: false)
  logit_bias?: Record<number, number>; // token biases for sampling

  // JSON response format parameters
  response_format?: {
    type: 'json_object';
    json_schema?: Record<string, any>;
  };
}

export interface LlamaMessage {
  role: 'system' | 'user' | 'assistant' | 'tool';
  content: string;
  tool_call_id?: string;
  name?: string;
}

export interface LlamaTool {
  type: 'function';
  function: {
    name: string;
    description?: string;
    parameters: Record<string, any>;
  };
}

export interface LlamaCompletionResult {
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

export interface LlamaContextMethods {
  completion(params: LlamaCompletionParams, partialCallback?: (data: {token: string}) => void): Promise<LlamaCompletionResult>;
  tokenize(content: string): Promise<number[]>;
  detokenize(tokens: number[]): Promise<string>;
  embedding(content: string): Promise<number[]>;
  detectTemplate(messages: LlamaMessage[]): Promise<string>;
  loadSession(path: string): Promise<boolean>;
  saveSession(path: string): Promise<boolean>;
  stopCompletion(): Promise<void>;
  release(): Promise<void>;
}

export interface Spec extends TurboModule {
  // Initialize a Llama context with the given model parameters
  initLlama(params: LlamaModelParams): Promise<LlamaContextType & LlamaContextMethods>;

  // Load model info without creating a full context
  loadLlamaModelInfo(modelPath: string): Promise<{
    n_params: number;
    n_vocab: number;
    n_context: number;
    n_embd: number;
    description: string;
    gpuSupported: boolean;
    optimalGpuLayers: number;
    quant_type?: string;
    architecture?: string;
  }>;

  // Convert JSON schema to GBNF grammar
  jsonSchemaToGbnf(schema: Record<string, any>): Promise<string>;
}

export default TurboModuleRegistry.getEnforcing<Spec>('LlamaCppRn');
