import type { TurboModule } from 'react-native';
/**
 * Native LlamaCppRn Module
 *
 * The API is designed to be compatible with the llama.cpp server API:
 * https://github.com/ggml-org/llama.cpp/tree/master/examples/server
 *
 * However, since we are in the context of a mobile app, we need to make some adjustments.
 */
export interface LlamaContextType {
}
export interface LlamaModelParams {
    model: string;
    n_ctx?: number;
    n_batch?: number;
    n_ubatch?: number;
    n_threads?: number;
    n_keep?: number;
    n_gpu_layers?: number;
    use_mmap?: boolean;
    use_mlock?: boolean;
    vocab_only?: boolean;
    embedding?: boolean;
    seed?: number;
    rope_freq_base?: number;
    rope_freq_scale?: number;
    yarn_ext_factor?: number;
    yarn_attn_factor?: number;
    yarn_beta_fast?: number;
    yarn_beta_slow?: number;
    logits_all?: boolean;
    chat_template?: string;
    use_jinja?: boolean;
    verbose?: number;
    lora_adapters?: Array<{
        path: string;
        scale?: number;
    }>;
    grammar?: string;
}
export interface LlamaCompletionParams {
    prompt?: string;
    system_prompt?: string;
    messages?: LlamaMessage[];
    temperature?: number;
    top_p?: number;
    top_k?: number;
    n_predict?: number;
    max_tokens?: number;
    stop?: string[];
    stream?: boolean;
    chat_template?: string;
    tool_choice?: string | 'auto' | 'none';
    tools?: LlamaTool[];
    repeat_penalty?: number;
    repeat_last_n?: number;
    frequency_penalty?: number;
    presence_penalty?: number;
    seed?: number;
    grammar?: string;
}
export interface LlamaMessage {
    role: 'system' | 'user' | 'assistant' | 'tool';
    content: string;
    tool_call_id?: string;
    name?: string;
}
export interface JsonSchemaObject {
    type: 'object';
    properties: {
        [key: string]: JsonSchemaProperty;
    };
    required?: string[];
    description?: string;
}
export interface JsonSchemaArray {
    type: 'array';
    items: JsonSchemaProperty;
    description?: string;
}
export type JsonSchemaScalar = {
    type: 'string' | 'number' | 'boolean' | 'null';
    enum?: string[];
    description?: string;
};
export type JsonSchemaProperty = JsonSchemaObject | JsonSchemaArray | JsonSchemaScalar;
export interface LlamaTool {
    type: 'function';
    function: {
        name: string;
        description?: string;
        parameters: JsonSchemaObject;
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
    choices?: Array<{
        index: number;
        message: {
            role: string;
            content: string;
            tool_calls?: Array<{
                id: string;
                type: string;
                function: {
                    name: string;
                    arguments: string;
                };
            }>;
        };
        finish_reason: 'stop' | 'length' | 'tool_calls';
    }>;
    tool_calls?: Array<{
        id: string;
        type: string;
        function: {
            name: string;
            arguments: string;
        };
    }>;
}
export interface EmbeddingOptions {
    input?: string | string[];
    content?: string | string[];
    add_bos_token?: boolean;
    encoding_format?: 'float' | 'base64';
    model?: string;
}
export interface EmbeddingResponse {
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
export interface LlamaContextMethods {
    completion(params: LlamaCompletionParams, partialCallback?: (data: {
        token: string;
    }) => void): Promise<LlamaCompletionResult>;
    tokenize(options: {
        content: string;
        add_special?: boolean;
        with_pieces?: boolean;
    }): Promise<{
        tokens: (number | {
            id: number;
            piece: string | number[];
        })[];
    }>;
    detokenize(options: {
        tokens: number[];
    }): Promise<{
        content: string;
    }>;
    /**
     * Generate embeddings for input tex
     *
     * @param options Embedding options matching server.cpp forma
     * @returns Array of embedding values or OpenAI-compatible embedding response
     */
    embedding(options: EmbeddingOptions): Promise<EmbeddingResponse>;
    detectTemplate(messages: LlamaMessage[]): Promise<string>;
    loadSession(path: string): Promise<boolean>;
    saveSession(path: string): Promise<boolean>;
    stopCompletion(): Promise<void>;
    release(): Promise<void>;
}
export interface Spec extends TurboModule {
    initLlama(params: LlamaModelParams): Promise<LlamaContextType & LlamaContextMethods>;
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
}
declare const LlamaCppRn: Spec;
/**
 * LlamaModel type representing a loaded model instance
 */
export type LlamaModel = LlamaContextType & LlamaContextMethods;
/**
 * Load a Llama model with simplified parameters
 *
 * @param params Configuration for loading the model
 * @returns Promise that resolves to a LlamaModel instance
 */
export declare function loadLlamaModel(params: {
    modelPath: string;
    contextSize?: number;
    batchSize?: number;
    threads?: number;
    gpuLayers?: number;
    verbose?: boolean;
}): Promise<LlamaModel>;
export declare function initLlama(params: LlamaModelParams): Promise<LlamaContextType & LlamaContextMethods>;
/**
 * Get information about a model without loading it fully
 */
export declare function loadLlamaModelInfo(modelPath: string): Promise<{
    n_params: number;
    n_vocab: number;
    n_context: number;
    n_embd: number;
    description: string;
    gpuSupported: boolean;
    optimalGpuLayers?: number;
    quant_type?: string;
    architecture?: string;
}>;
export default LlamaCppRn;
