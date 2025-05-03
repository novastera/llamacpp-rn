#pragma once

#include <jsi/jsi.h>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

// Include all necessary common headers from llama.cpp
#include "json-schema-to-grammar.h"
#include "chat.h"
#include "sampling.h"

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

// Options for completion
struct CompletionOptions {
  // Input - either prompt or messages
  std::string prompt;
  std::vector<Message> messages;
  
  // Sampling parameters
  float temperature = 0.8f;
  float top_p = 0.9f;
  int top_k = 40;
  float min_p = 0.05f;
  float typical_p = 1.0f;
  
  // Generation control
  int max_tokens = 512;
  std::vector<std::string> stop_sequences;
  
  // Repetition penalties
  float repeat_penalty = 1.1f;
  int repeat_last_n = 64;
  float frequency_penalty = 0.0f;
  float presence_penalty = 0.0f;
  
  // Mirostat settings
  int mirostat = 0;       // 0 = disabled, 1 = mirostat, 2 = mirostat 2.0
  float mirostat_tau = 5.0f;
  float mirostat_eta = 0.1f;
  
  // Miscellaneous
  int seed = -1;
  std::string template_name;
  
  // Tool related
  std::vector<Tool> tools;
  std::string tool_choice = "auto";
  bool jinja = false;
  std::string grammar;
  
  // Callbacks
  std::function<void(jsi::Runtime&, const char*)> partial_callback;
  jsi::Runtime* runtime = nullptr;
};

// Result from completion
struct CompletionResult {
  std::string text;
  bool truncated;
  std::string finish_reason;
  
  // Stats
  int prompt_tokens;
  int generated_tokens;
  
  // Timing
  double prompt_duration_ms;
  double generation_duration_ms;
  double total_duration_ms;
  
  // Tool calls (if any)
  std::vector<ToolCall> tool_calls;
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
  
  // Main completion method
  CompletionResult completion(const CompletionOptions& options);
  
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
  std::vector<ToolCall> parseToolCalls(const std::string& text, std::string* remainingText);
  
  // Convert between JSI objects and our structs
  Tool convertJsiToolToTool(jsi::Runtime& rt, const jsi::Object& jsiTool);
  jsi::Object convertToolCallToJsiObject(jsi::Runtime& rt, const ToolCall& toolCall);
  
  // Convert options to sampling parameters
  common_params_sampling convertToSamplingParams(const CompletionOptions& options);
};

} // namespace facebook::react 