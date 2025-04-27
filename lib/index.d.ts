import type { LlamaModelParams, LlamaCompletionParams, LlamaCompletionResult, LlamaMessage, LlamaTool, LlamaContextType, LlamaContextMethods } from './specs/NativeLlamaCppRn';
export declare function initLlama(params: LlamaModelParams): Promise<LlamaContextType & LlamaContextMethods>;
export declare function loadLlamaModelInfo(modelPath: string): Promise<{
    n_params: number;
    n_vocab: number;
    n_context: number;
    n_embd: number;
    description: string;
}>;
export declare function jsonSchemaToGbnf(schema: Record<string, any>): Promise<string>;
export type { LlamaModelParams, LlamaCompletionParams, LlamaCompletionResult, LlamaMessage, LlamaTool, LlamaContextType, };
