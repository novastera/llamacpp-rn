import LlamaCppRn from './specs/NativeLlamaCppRn';
import type {
  LlamaModelParams,
  LlamaCompletionParams,
  LlamaCompletionResult,
  LlamaMessage,
  LlamaTool,
  LlamaContextType,
  LlamaContextMethods,
} from './specs/NativeLlamaCppRn';

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
export function loadLlamaModel(params: {
  modelPath: string;
  contextSize?: number;
  batchSize?: number;
  threads?: number;
  gpuLayers?: number;
  verbose?: boolean;
}): Promise<LlamaModel> {
  return LlamaCppRn.initLlama({
    model: params.modelPath,
    n_ctx: params.contextSize || 2048,
    n_batch: params.batchSize || 512,
    n_threads: params.threads,
    n_gpu_layers: params.gpuLayers || 0,
    verbose: params.verbose ? 1 : 0,
  });
}

// Original function kept for backward compatibility
export function initLlama(params: LlamaModelParams): Promise<LlamaContextType & LlamaContextMethods> {
  return LlamaCppRn.initLlama(params);
}

/**
 * Get information about a model without loading it fully
 */
export function loadLlamaModelInfo(
  modelPath: string
): Promise<{
  n_params: number;
  n_vocab: number;
  n_context: number;
  n_embd: number;
  description: string;
  gpuSupported: boolean;
  optimalGpuLayers?: number;
  quant_type?: string;
  architecture?: string;
}> {
  return LlamaCppRn.loadLlamaModelInfo(modelPath);
}

// Export types for users of the module
export type {
  LlamaModelParams,
  LlamaCompletionParams,
  LlamaCompletionResult,
  LlamaMessage,
  LlamaTool,
  LlamaContextType,
  LlamaContextMethods,
};
