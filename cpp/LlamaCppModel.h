#pragma once

#include <jsi/jsi.h>
#include <string>
#include <vector>
#include <mutex>
#include <functional>

// Forward declarations
struct llama_model;
struct llama_context;
struct llama_chat_message;

namespace facebook::react {

// Structure to represent a chat message within our model interaction
// Note: This is different from the global ChatMessage struct in ChatTemplates.h
struct ChatMessage {
  std::string role;
  std::string content;
  std::string name;
};

// Structure for completion results
struct CompletionResult {
  std::string text;
  int prompt_tokens;
  int generated_tokens;
  double prompt_duration_ms;
  double generation_duration_ms;
};

// Structure to represent a function parameter for tool calling
struct FunctionParameter {
  std::string name;
  std::string type;
  std::string description;
  bool required;
};

// Structure to represent a function for tool calling
struct ToolFunction {
  std::string name;
  std::string description;
  std::vector<FunctionParameter> parameters;
};

// Structure to represent a tool
struct Tool {
  std::string type;
  ToolFunction function;
};

// Structure to represent a tool call in response
struct ToolCall {
  std::string id;
  std::string type;
  std::string name;
  std::string arguments;
};

class LlamaCppModel {
public:
  LlamaCppModel(llama_model* model, llama_context* ctx);
  ~LlamaCppModel();

  // Model operations
  std::vector<int32_t> tokenize(const std::string& text);
  CompletionResult completion(const std::string& prompt, const std::vector<ChatMessage>& messages, 
                         float temperature, float top_p, int top_k, int max_tokens, 
                         const std::vector<std::string>& stop_sequences,
                         const std::string& template_name = "",
                         bool jinja = false,
                         const std::string& tool_choice = "", 
                         const std::vector<Tool>& tools = std::vector<Tool>(),
                         std::function<void(jsi::Runtime&, const char*)> partialCallback = nullptr,
                         jsi::Runtime* rt = nullptr);
  
  std::vector<float> embedding(const std::string& text);
  void release();

  // JSI function implementations
  jsi::Value tokenizeJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value embeddingJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value releaseJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);
  jsi::Value testProcessTokensJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count);

  // Model info
  int32_t getVocabSize() const;
  int32_t getContextSize() const;
  int32_t getEmbeddingSize() const;

  bool shouldStopCompletion() const;
  void setShouldStopCompletion(bool value);
  
  // Check if model is currently busy with prediction
  bool isPredicting() const { return is_predicting_; }
  void setIsPredicting(bool value) { is_predicting_ = value; }

  // Helper to parse tool calls from model output
  std::vector<ToolCall> parseToolCalls(const std::string& text, std::string* remainingText = nullptr);
  
  // Helper to convert JSI tool objects to our Tool structure
  static Tool convertJsiToolToTool(jsi::Runtime& rt, const jsi::Object& jsiTool);
  
  // Helper to convert our ToolCall to JSI object
  jsi::Object convertToolCallToJsiObject(jsi::Runtime& rt, const ToolCall& toolCall);

private:
  llama_model* model_;
  llama_context* ctx_;
  std::mutex mutex_;
  bool should_stop_completion_ = false;
  bool is_predicting_ = false;
};

} // namespace facebook::react 