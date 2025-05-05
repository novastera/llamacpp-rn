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
#include "rn-llama.hpp"

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

// Model instance that encapsulates a llama.cpp model context
class LlamaCppModel : public jsi::HostObject {
public:
  LlamaCppModel(rn_llama_context* rn_ctx);
  virtual ~LlamaCppModel();

  // Clean up resources (should be called explicitly)
  void release();

  // Model introspection
  int32_t getVocabSize() const;
  int32_t getContextSize() const;
  int32_t getEmbeddingSize() const;

  // Completion state control
  bool shouldStopCompletion() const;
  void setShouldStopCompletion(bool value);
  
  // Completion method (for internal use)
  CompletionResult completion(
      const CompletionOptions& options,
      std::function<void(jsi::Runtime&, const char*)> partialCallback = nullptr,
      jsi::Runtime* runtime = nullptr);

  // JSI interface implementation
  jsi::Value get(jsi::Runtime& rt, const jsi::PropNameID& name) override;
  void set(jsi::Runtime& rt, const jsi::PropNameID& name, const jsi::Value& value) override;
  std::vector<jsi::PropNameID> getPropertyNames(jsi::Runtime& rt) override;

private:
  // JSI method implementations
  jsi::Value completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value tokenizeJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value detokenizeJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value embeddingJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value releaseJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);

  // Helper to parse completion options from JS object
  CompletionOptions parseCompletionOptions(jsi::Runtime& rt, const jsi::Object& obj);

  // Helper to convert completion result to JSI object
  jsi::Object completionResultToJsi(jsi::Runtime& rt, const CompletionResult& result);

  // Initialize utility functions and handlers
  void initHelpers();

  // LLAMA context pointer (owned by the module)
  rn_llama_context* rn_ctx_;

  // Completion state
  bool should_stop_completion_;
  bool is_predicting_;
};

} // namespace facebook::react 