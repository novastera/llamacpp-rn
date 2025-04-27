import type { TurboModule } from 'react-native';
export interface LlamaContextType {
}
export interface LlamaModelParams {
    model: string;
    model_alias?: string;
    n_ctx?: number;
    n_batch?: number;
    n_threads?: number;
    n_gpu_layers?: number;
    use_mlock?: boolean;
    embedding?: boolean;
    cache_capacity?: number;
    rope_freq_base?: number;
    rope_freq_scale?: number;
    grammar?: string;
}
export interface LlamaCompletionParams {
    prompt?: string;
    messages?: LlamaMessage[];
    temperature?: number;
    top_p?: number;
    top_k?: number;
    n_predict?: number;
    stop?: string[];
    frequency_penalty?: number;
    presence_penalty?: number;
    mirostat?: number;
    mirostat_tau?: number;
    mirostat_eta?: number;
    penalize_nl?: boolean;
    seed?: number;
    logit_bias?: Record<number, number>;
    jinja?: boolean;
    tool_choice?: string | 'auto' | 'none';
    tools?: LlamaTool[];
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
    completion(params: LlamaCompletionParams, partialCallback?: (data: {
        token: string;
    }) => void): Promise<LlamaCompletionResult>;
    tokenize(content: string): Promise<number[]>;
    detokenize(tokens: number[]): Promise<string>;
    embedding(content: string): Promise<number[]>;
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
    }>;
    jsonSchemaToGbnf(schema: Record<string, any>): Promise<string>;
    getAbsolutePath(relativePath: string): Promise<{
        relativePath: string;
        path: string;
        exists: boolean;
        attributes?: {
            size: number;
        };
    }>;
    getGPUInfo(): Promise<{
        isSupported: boolean;
        available: boolean;
        deviceName: string;
        deviceVendor: string;
        deviceVersion: string;
        deviceComputeUnits: number;
        deviceMemorySize: number;
        implementation?: string;
        metalEnabled?: boolean;
    }>;
}
declare const _default: Spec;
export default _default;
