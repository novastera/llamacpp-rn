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

// Main function to initialize a Llama context
export function initLlama(params: LlamaModelParams): Promise<LlamaContextType & LlamaContextMethods> {
  return LlamaCppRn.initLlama(params);
}

// Function to load model information without creating a full context
export function loadLlamaModelInfo(
  modelPath: string
): Promise<{
  n_params: number;
  n_vocab: number;
  n_context: number;
  n_embd: number;
  description: string;
}> {
  return LlamaCppRn.loadLlamaModelInfo(modelPath);
}

// Function to convert JSON Schema to GBNF grammar
export function jsonSchemaToGbnf(schema: Record<string, any>): Promise<string> {
  return LlamaCppRn.jsonSchemaToGbnf(schema);
}

// Function to get GPU capabilities information
export function getGPUInfo(): Promise<{
  isSupported: boolean;
  available: boolean;
  deviceName: string;
  deviceVendor: string;
  deviceVersion: string;
  deviceComputeUnits: number;
  deviceMemorySize: number;
  implementation?: string;
  metalEnabled?: boolean;
}> {
  return LlamaCppRn.getGPUInfo();
}

// Function to get absolute path and file information
export function getAbsolutePath(
  relativePath: string
): Promise<{
  relativePath: string;
  path: string;
  exists: boolean;
  attributes?: {
    size: number;
  };
}> {
  return LlamaCppRn.getAbsolutePath(relativePath);
}

// Export types for users of the module
export type {
  LlamaModelParams,
  LlamaCompletionParams,
  LlamaCompletionResult,
  LlamaMessage,
  LlamaTool,
  LlamaContextType,
}; 