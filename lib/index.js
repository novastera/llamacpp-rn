import LlamaCppRn from './specs/NativeLlamaCppRn';
// Main function to initialize a Llama context
export function initLlama(params) {
    return LlamaCppRn.initLlama(params);
}
// Function to load model information without creating a full context
export function loadLlamaModelInfo(modelPath) {
    return LlamaCppRn.loadLlamaModelInfo(modelPath);
}
// Function to convert JSON Schema to GBNF grammar
export function jsonSchemaToGbnf(schema) {
    return LlamaCppRn.jsonSchemaToGbnf(schema);
}
