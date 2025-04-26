"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.initLlama = initLlama;
exports.jsonSchemaToGbnf = jsonSchemaToGbnf;
exports.loadLlamaModelInfo = loadLlamaModelInfo;
var _LlamaCppRnSpec = _interopRequireDefault(require("./specs/LlamaCppRnSpec"));
function _interopRequireDefault(e) { return e && e.__esModule ? e : { default: e }; }
// Main function to initialize a Llama context
function initLlama(params) {
  return _LlamaCppRnSpec.default.initLlama(params);
}

// Function to load model information without creating a full context
function loadLlamaModelInfo(modelPath) {
  return _LlamaCppRnSpec.default.loadLlamaModelInfo(modelPath);
}

// Function to convert JSON Schema to GBNF grammar
function jsonSchemaToGbnf(schema) {
  return _LlamaCppRnSpec.default.jsonSchemaToGbnf(schema);
}

// Export types for users of the module
//# sourceMappingURL=index.js.map