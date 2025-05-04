#pragma once

#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// Include all necessary common headers from llama.cpp
#include "common.h"
#include "sampling.h"
#include "chat.h"
#include "json-schema-to-grammar.h"

// Include rn-utils.hpp which has the CompletionResult definition
#include "rn-utils.hpp"

// Forward declarations for llama.cpp types
struct llama_model;
struct llama_context;
struct common_sampler;
struct common_speculative;

namespace facebook::react {

// Chat message structure
struct Message {
  std::string role;
  std::string content;
  std::string name;
};

// Function parameter for tool calls
struct FunctionParameter {
  std::string name;
  std::string type;
  std::string description;
  bool required;
};

// Function definition for tool calls
struct Function {
  std::string name;
  std::string description;
  std::vector<FunctionParameter> parameters;
};

// Tool for completion
struct Tool {
  std::string type;
  Function function;
};

// Tool call parsed from completion
struct ToolCall {
  std::string id;
  std::string type;
  std::string name;
  std::string arguments;
};

// Main model class that wraps llama.cpp
class LlamaCppModel : public jsi::HostObject {
public:
  LlamaCppModel(llama_model* model, llama_context* ctx);
  ~LlamaCppModel();
  
  // Implement HostObject methods
  jsi::Value get(jsi::Runtime& rt, const jsi::PropNameID& name) override;
  void set(jsi::Runtime& rt, const jsi::PropNameID& name, const jsi::Value& value) override;
  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime& rt) override;
  
  // Resource management
  void release();
  
  // Public methods
  int32_t getVocabSize() const;
  int32_t getContextSize() const;
  int32_t getEmbeddingSize() const;
  
  bool shouldStopCompletion() const;
  void setShouldStopCompletion(bool value);
  
  std::vector<int32_t> tokenize(const std::string& text);
  std::vector<float> embedding(const std::string& text);
  
  // Main completion method - uses CompletionResult from rn-utils.hpp
  CompletionResult completion(const CompletionOptions& options, std::function<void(jsi::Runtime&, const char*)> partialCallback = nullptr, jsi::Runtime* runtime = nullptr);

  // JSI bindings
  jsi::Value completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value tokenizeJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value embeddingJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value releaseJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value testProcessTokensJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  
private:
  // Core llama.cpp model and context
  llama_model* model_;
  llama_context* ctx_;
  
  // State management
  std::mutex mutex_;
  bool should_stop_completion_;
  bool is_predicting_;
  
  // Helper methods
  void initHelpers();
  
  // Helper method to parse completion options from JSI objects
  CompletionOptions parseCompletionOptions(jsi::Runtime& rt, const jsi::Object& obj);
  
  // Helper to convert CompletionResult to JSI object
  jsi::Object completionResultToJsi(jsi::Runtime& rt, const CompletionResult& result, bool withJinja);
};

} // namespace facebook::react 