import { TurboModuleRegistry } from 'react-native';
const LlamaCppRn = TurboModuleRegistry.getEnforcing('LlamaCppRn');
/**
 * Load a Llama model with simplified parameters
 *
 * @param params Configuration for loading the model
 * @returns Promise that resolves to a LlamaModel instance
 */
export function loadLlamaModel(params) {
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
export function initLlama(params) {
    return LlamaCppRn.initLlama(params);
}
/**
 * Get information about a model without loading it fully
 */
export function loadLlamaModelInfo(modelPath) {
    return LlamaCppRn.loadLlamaModelInfo(modelPath);
}
export default LlamaCppRn;
