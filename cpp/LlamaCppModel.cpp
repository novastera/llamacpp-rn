#include "LlamaCppModel.h"
#include <jsi/jsi.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>

// Include RN-llama integration
#include "rn-utils.hpp"
#include "rn-llama.hpp"

// Include llama.cpp headers
#include "llama.h"
#include "chat.h"
#include "common.h"
#include "json-schema-to-grammar.h"
#include "sampling.h"

// System utilities
#include "SystemUtils.h"

// Remove the global 'using namespace' to avoid namespace conflicts
// using namespace facebook::react;

namespace facebook::react {

LlamaCppModel::LlamaCppModel(llama_model* model, llama_context* ctx)
    : model_(model), ctx_(ctx), should_stop_completion_(false), is_predicting_(false) {
    
    // Initialize helpers if needed
    initHelpers();
}

void LlamaCppModel::initHelpers() {
    // No longer need tool handler initialization
}

LlamaCppModel::~LlamaCppModel() {
  // Note: We don't automatically release resources here
  // as the user should call release() explicitly
}

void LlamaCppModel::release() {
  
  // Cancel any ongoing predictions
  if (is_predicting_) {
    should_stop_completion_ = true;
    
    // Optionally wait a bit for completion to stop
    int retry = 0;
    while (is_predicting_ && retry < 10) {
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
      retry++;
    }
  }

  // Clean up our resources
  if (ctx_) {
    llama_free(ctx_);
    ctx_ = nullptr;
  }
  
  if (model_) {
    llama_model_free(model_);
    model_ = nullptr;
  }
}

int32_t LlamaCppModel::getVocabSize() const {
  if (!model_) {
    throw std::runtime_error("Model not loaded");
  }
  
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  return llama_vocab_n_tokens(vocab);
}

int32_t LlamaCppModel::getContextSize() const {
  if (!ctx_) {
    throw std::runtime_error("Context not initialized");
  }
  
  return llama_n_ctx(ctx_);
}

int32_t LlamaCppModel::getEmbeddingSize() const {
  if (!model_) {
    throw std::runtime_error("Model not loaded");
  }
  
  return llama_model_n_embd(model_);
}

bool LlamaCppModel::shouldStopCompletion() const {
  return should_stop_completion_;
}

void LlamaCppModel::setShouldStopCompletion(bool value) {
  should_stop_completion_ = value;
  if (value) {
    // Reset the predicting flag when stopping completion
    is_predicting_ = false;
  }
}

std::vector<int32_t> LlamaCppModel::tokenize(const std::string& text) {
  if (!model_) {
    throw std::runtime_error("Model not loaded");
  }
  
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  
  // Parameters for tokenization
  bool add_bos = false;
  bool parse_special = false;
  
  // First get the number of tokens needed
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), nullptr, 0, add_bos, parse_special);
  if (n_tokens < 0) {
    n_tokens = -n_tokens; // Convert negative value (indicates insufficient buffer)
  }
  
  // Allocate space for tokens
  std::vector<llama_token> tokens(n_tokens);
  
  // Do the actual tokenization
  n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), add_bos, parse_special);
  if (n_tokens < 0) {
    n_tokens = -n_tokens; // Handle negative return value
    tokens.resize(n_tokens);
    
    // Retry with the correct size
    int retry_result = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), add_bos, parse_special);
    if (retry_result != n_tokens) {
      throw std::runtime_error("Failed to tokenize text: inconsistent token count");
    }
  } else {
    tokens.resize(n_tokens);
  }
  
  // Convert to int32_t for JSI
  return std::vector<int32_t>(tokens.begin(), tokens.end());
}

std::vector<float> LlamaCppModel::embedding(const std::string& text) {
  
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  // Set the context parameters to compute embeddings
  llama_set_embeddings(ctx_, true);
  
  // Ensure we're using the right pooling method
  // Prefer MEAN pooling for embeddings
  if (llama_pooling_type(ctx_) == LLAMA_POOLING_TYPE_UNSPECIFIED) {
    llama_set_causal_attn(ctx_, false);
  }

  // Get the vocab
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  
  // Clear the KV cache
  llama_kv_self_clear(ctx_);
  
  // Same tokenization parameters as the tokenize method for consistency
  bool add_bos = false;
  bool parse_special = false;
  
  // Tokenize the text
  std::vector<llama_token> tokens(text.size() + 4); // Allocate a reasonable initial size
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), add_bos, parse_special);
  
  // Handle negative return value (token buffer too small)
  if (n_tokens < 0) {
    tokens.resize(-n_tokens);
    n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), add_bos, parse_special);
    if (n_tokens < 0) {
      throw std::runtime_error("Tokenization failed: " + std::to_string(n_tokens));
    }
  }
  
  // Resize to actual size
  tokens.resize(n_tokens);
  
  // Process the tokens
  llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
  batch.logits[n_tokens - 1] = true; // Only need logits for the last token
  
  if (llama_decode(ctx_, batch)) {
    throw std::runtime_error("Failed to decode text for embedding");
  }
  
  // Get embedding size
  const int32_t embd_size = getEmbeddingSize();
  
  // Get the embeddings
  float* embeddings_data = llama_get_embeddings(ctx_);
  
  if (embeddings_data == nullptr) {
    throw std::runtime_error("Failed to get embeddings");
  }
  
  // Copy the embeddings to a vector
  std::vector<float> result(embeddings_data, embeddings_data + embd_size);
  
  // Normalize the embeddings (L2 norm)
  float norm = 0.0f;
  for (float val : result) {
    norm += val * val;
  }
  
  if (norm > 0.0f) {
    norm = std::sqrt(norm);
    for (float& val : result) {
      val /= norm;
    }
  }
  
  return result;
}

// Parse the CompletionOptions from a JS object
CompletionOptions LlamaCppModel::parseCompletionOptions(jsi::Runtime& rt, const jsi::Object& obj) {
  CompletionOptions options;
  
  // Extract basic options
  if (obj.hasProperty(rt, "prompt") && !obj.getProperty(rt, "prompt").isUndefined()) {
    options.prompt = obj.getProperty(rt, "prompt").asString(rt).utf8(rt);
  }
  
  // Parse sampling parameters
  if (obj.hasProperty(rt, "temperature") && !obj.getProperty(rt, "temperature").isUndefined()) {
    options.temperature = obj.getProperty(rt, "temperature").asNumber();
  }
  
  if (obj.hasProperty(rt, "top_p") && !obj.getProperty(rt, "top_p").isUndefined()) {
    options.top_p = obj.getProperty(rt, "top_p").asNumber();
  }
  
  if (obj.hasProperty(rt, "top_k") && !obj.getProperty(rt, "top_k").isUndefined()) {
    options.top_k = obj.getProperty(rt, "top_k").asNumber();
  }
  
  if (obj.hasProperty(rt, "min_p") && !obj.getProperty(rt, "min_p").isUndefined()) {
    options.min_p = obj.getProperty(rt, "min_p").asNumber();
  }
  
  if (obj.hasProperty(rt, "n_predict") && !obj.getProperty(rt, "n_predict").isUndefined()) {
    options.n_predict = obj.getProperty(rt, "n_predict").asNumber();
  } else if (obj.hasProperty(rt, "max_tokens") && !obj.getProperty(rt, "max_tokens").isUndefined()) {
    options.n_predict = obj.getProperty(rt, "max_tokens").asNumber();
  }
  
  if (obj.hasProperty(rt, "n_keep") && !obj.getProperty(rt, "n_keep").isUndefined()) {
    options.n_keep = obj.getProperty(rt, "n_keep").asNumber();
  }
  
  // Extract seed
  if (obj.hasProperty(rt, "seed") && !obj.getProperty(rt, "seed").isUndefined()) {
    options.seed = obj.getProperty(rt, "seed").asNumber();
  }
  
  // Extract stop sequences
  if (obj.hasProperty(rt, "stop") && !obj.getProperty(rt, "stop").isUndefined()) {
    auto stopVal = obj.getProperty(rt, "stop");
    if (stopVal.isString()) {
      options.stop.push_back(stopVal.asString(rt).utf8(rt));
    } else if (stopVal.isObject() && stopVal.getObject(rt).isArray(rt)) {
      auto stopArr = stopVal.getObject(rt).getArray(rt);
      for (size_t i = 0; i < stopArr.size(rt); i++) {
        auto item = stopArr.getValueAtIndex(rt, i);
        if (item.isString()) {
          options.stop.push_back(item.asString(rt).utf8(rt));
        }
      }
    }
  }
  
  // Extract grammar
  if (obj.hasProperty(rt, "grammar") && !obj.getProperty(rt, "grammar").isUndefined()) {
    options.grammar = obj.getProperty(rt, "grammar").asString(rt).utf8(rt);
  }
  
  // Extract chat template options
  if (obj.hasProperty(rt, "jinja") && !obj.getProperty(rt, "jinja").isUndefined()) {
    options.use_jinja = obj.getProperty(rt, "jinja").asBool();
  } else if (obj.hasProperty(rt, "use_jinja") && !obj.getProperty(rt, "use_jinja").isUndefined()) {
    options.use_jinja = obj.getProperty(rt, "use_jinja").asBool();
  }
  
  if (obj.hasProperty(rt, "chat_template") && !obj.getProperty(rt, "chat_template").isUndefined()) {
    options.chat_template = obj.getProperty(rt, "chat_template").asString(rt).utf8(rt);
  } else if (obj.hasProperty(rt, "template_name") && !obj.getProperty(rt, "template_name").isUndefined()) {
    options.chat_template = obj.getProperty(rt, "template_name").asString(rt).utf8(rt);
  }
  
  if (obj.hasProperty(rt, "ignore_eos") && !obj.getProperty(rt, "ignore_eos").isUndefined()) {
    options.ignore_eos = obj.getProperty(rt, "ignore_eos").asBool();
  }
  
  if (obj.hasProperty(rt, "stream") && !obj.getProperty(rt, "stream").isUndefined()) {
    options.stream = obj.getProperty(rt, "stream").asBool();
  }
  
  // Extract and parse messages if present (for chat completion)
  if (obj.hasProperty(rt, "messages") && obj.getProperty(rt, "messages").isObject()) {
    auto messagesVal = obj.getProperty(rt, "messages").getObject(rt);
    if (messagesVal.isArray(rt)) {
      json messagesJson = json::array();
      auto messagesArr = messagesVal.getArray(rt);
      
      // Convert JSI messages to JSON format
      for (size_t i = 0; i < messagesArr.size(rt); i++) {
        auto msgVal = messagesArr.getValueAtIndex(rt, i);
        if (msgVal.isObject()) {
          auto msgObj = msgVal.getObject(rt);
          
          json msgJson = json::object();
          if (msgObj.hasProperty(rt, "role")) {
            msgJson["role"] = msgObj.getProperty(rt, "role").asString(rt).utf8(rt);
          }
          
          if (msgObj.hasProperty(rt, "content")) {
            auto contentVal = msgObj.getProperty(rt, "content");
            if (contentVal.isString()) {
              msgJson["content"] = contentVal.asString(rt).utf8(rt);
            }
          }
          
          if (msgObj.hasProperty(rt, "name")) {
            msgJson["name"] = msgObj.getProperty(rt, "name").asString(rt).utf8(rt);
          }
          
          messagesJson.push_back(msgJson);
        }
      }
      
      options.messages = messagesJson;
    }
  }
  
  // Helper function to convert JSI objects to nlohmann::json
  std::function<nlohmann::json(const jsi::Object&)> jsiObjectToJson;
  
  jsiObjectToJson = [&rt, &jsiObjectToJson](const jsi::Object& jsObject) -> nlohmann::json {
    try {
      if (jsObject.isArray(rt)) {
        nlohmann::json jsonArray = nlohmann::json::array();
        jsi::Array jsArray = jsObject.asArray(rt);
        size_t size = jsArray.size(rt);
        
        for (size_t i = 0; i < size; i++) {
          jsi::Value value = jsArray.getValueAtIndex(rt, i);
          if (value.isObject()) {
            jsonArray.push_back(jsiObjectToJson(value.asObject(rt)));
          } else if (value.isString()) {
            jsonArray.push_back(value.asString(rt).utf8(rt));
          } else if (value.isNumber()) {
            jsonArray.push_back(value.asNumber());
          } else if (value.isBool()) {
            jsonArray.push_back(value.asBool());
          } else if (value.isNull()) {
            jsonArray.push_back(nullptr);
          } else {
            // Skip unknown types
            jsonArray.push_back(nullptr);
          }
        }
        return jsonArray;
      } else {
        nlohmann::json jsonObj = nlohmann::json::object();
        jsi::Array propNames = jsObject.getPropertyNames(rt);
        size_t size = propNames.size(rt);
        
        for (size_t i = 0; i < size; i++) {
          std::string propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
          
          // Skip if property name is empty
          if (propName.empty()) {
            continue;
          }
          
          // Get the property value safely
          jsi::Value value;
          try {
            value = jsObject.getProperty(rt, propName.c_str());
          } catch (...) {
            // Skip properties that can't be accessed
            continue;
          }
          
          // Convert the value based on its type
          if (value.isObject()) {
            jsonObj[propName] = jsiObjectToJson(value.asObject(rt));
          } else if (value.isString()) {
            jsonObj[propName] = value.asString(rt).utf8(rt);
          } else if (value.isNumber()) {
            jsonObj[propName] = value.asNumber();
          } else if (value.isBool()) {
            jsonObj[propName] = value.asBool();
          } else if (value.isNull()) {
            jsonObj[propName] = nullptr;
          } else {
            // Skip unknown types
            jsonObj[propName] = nullptr;
          }
        }
        return jsonObj;
      }
    } catch (const std::exception& e) {
      // Return empty object on error to avoid crashing
      if (jsObject.isArray(rt)) {
        return nlohmann::json::array();
      } else {
        return nlohmann::json::object();
      }
    }
  };
  
  // Extract and parse tools if present
  if (obj.hasProperty(rt, "tools") && obj.getProperty(rt, "tools").isObject()) {
    auto toolsVal = obj.getProperty(rt, "tools").getObject(rt);
    options.tools = jsiObjectToJson(toolsVal);
  }
  
  return options;
}

// Modify the completion function to use this helper
CompletionResult LlamaCppModel::completion(const CompletionOptions& options, std::function<void(jsi::Runtime&, const char*)> partialCallback, jsi::Runtime* runtime) {  
  if (!model_ || !ctx_) {
    CompletionResult result;
    result.content = "";
    result.success = false;
    result.error_msg = "Model or context not initialized";
    result.error_type = RN_ERROR_MODEL_LOAD;
    return result;
  }
  // Clear the context KV cache
  llama_kv_self_clear(ctx_);
  // Create a temporary rn_llama_context that wraps our model and context
  rn_llama_context rn_ctx;
  rn_ctx.model = model_;
  rn_ctx.ctx = ctx_;
  rn_ctx.model_loaded = true;
  rn_ctx.vocab = llama_model_get_vocab(model_);
  
  // Initialize sampler params (used by run_completion)
  rn_ctx.params.sampling.temp = options.temperature;
  rn_ctx.params.sampling.top_p = options.top_p;
  rn_ctx.params.sampling.top_k = options.top_k;
  rn_ctx.params.sampling.min_p = options.min_p;
  rn_ctx.params.n_predict = options.n_predict;
  
  // Check for a partial callback
  auto callback_adapter = [&partialCallback, runtime](const std::string& token, bool is_done) -> bool {
    if (partialCallback && runtime && !is_done) {
      partialCallback(*runtime, token.c_str());
    }
    return true;
  };
  
  // Run the completion based on whether we have messages or prompt
  CompletionResult result;
  
  try {
    if (!options.messages.empty()) {
      // Chat completion (with messages)
      result = run_chat_completion(&rn_ctx, options, callback_adapter);
    } else {
      // Regular completion (with prompt)
      result = run_completion(&rn_ctx, options, callback_adapter);
    }
  } catch (const std::exception& e) {
    result.success = false;
    result.error_msg = std::string("Completion failed: ") + e.what();
    result.error_type = RN_ERROR_INFERENCE;
  }
  
  return result;
}

// Helper to convert from the rn-utils CompletionResult to a JSI object
jsi::Object LlamaCppModel::completionResultToJsi(jsi::Runtime& rt, const CompletionResult& result, bool withJinja) {
  jsi::Object jsResult(rt);
  /**

export type NativeCompletionResult = {
  //Original text (Ignored reasoning_content / tool_calls)
  text: string
  //Reasoning content (parsed for reasoning model)
  reasoning_content: string
  //Tool calls
  tool_calls: Array<{
    type: 'function'
    function: {
      name: string
      arguments: string
    }
    id?: string
  }>
   // Content text (Filtered text by reasoning_content / tool_calls)
   
  content: string

  tokens_predicted: number
  tokens_evaluated: number
  truncated: boolean
  stopped_eos: boolean
  stopped_word: string
  stopped_limit: number
  stopping_word: string
  tokens_cached: number
  timings: NativeCompletionResultTimings

  completion_probabilities?: Array<NativeCompletionTokenProb>
}
   */

  if (withJinja) {
    // should parse the result.content as a json object
    try {
      // Check if the content might be JSON
      if (!result.content.empty() && result.content[0] == '{') {
        // Try to parse as JSON
        json content_json = json::parse(result.content);
        
        // Convert parsed JSON to JSI object
        // Option 1: Add all properties from the JSON to the result object
        for (auto& [key, value] : content_json.items()) {
          if (value.is_string()) {
            jsResult.setProperty(rt, key.c_str(), jsi::String::createFromUtf8(rt, value.get<std::string>()));
          } else if (value.is_number_integer()) {
            jsResult.setProperty(rt, key.c_str(), jsi::Value(value.get<int>()));
          } else if (value.is_number_float()) {
            jsResult.setProperty(rt, key.c_str(), jsi::Value(value.get<double>()));
          } else if (value.is_boolean()) {
            jsResult.setProperty(rt, key.c_str(), jsi::Value(value.get<bool>()));
          } else if (value.is_object() || value.is_array()) {
            // For nested objects/arrays, convert to string
            jsResult.setProperty(rt, key.c_str(), jsi::String::createFromUtf8(rt, value.dump()));
          }
        }
        
        // Always include the original text for compatibility
        if (!jsResult.hasProperty(rt, "text")) {
          jsResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.content));
        }
      } else {
        // Not JSON, just use the content directly
        jsResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.content));
      }
    } catch (const std::exception& e) {
      // Not valid JSON, just use the content directly
      jsResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.content));
    }
  } else {
    jsResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.content));
  }

    // Add status related fields
  if (!result.success) {
    jsResult.setProperty(rt, "error", jsi::String::createFromUtf8(rt, result.error_msg));
  }
  
  // Set token count fields
  jsResult.setProperty(rt, "prompt_tokens", jsi::Value(result.n_prompt_tokens));
  jsResult.setProperty(rt, "generated_tokens", jsi::Value(result.n_predicted_tokens));
  
  
  return jsResult;
}

// JSI method for completions
jsi::Value LlamaCppModel::completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(rt, "completion requires an options object");
  }

  // Create partial callback function for token streaming
  std::function<void(jsi::Runtime&, const char*)> partialCallback = nullptr;

  if (count > 1 && args[1].isObject() && args[1].getObject(rt).isFunction(rt)) {
    auto callbackFn = std::make_shared<jsi::Function>(args[1].getObject(rt).getFunction(rt));
    partialCallback = [callbackFn](jsi::Runtime& rt, const char* token) {
      jsi::Object data(rt);
      data.setProperty(rt, "token", jsi::String::createFromUtf8(rt, token));
      callbackFn->call(rt, data);
    };
  }

  try {
    // Parse options from JSI object
    CompletionOptions options = parseCompletionOptions(rt, args[0].getObject(rt));
    
    // Set streaming flag based on callback presence
    options.stream = (partialCallback != nullptr);
    
    // Call our C++ completion method which properly initializes rn_llama_context
    CompletionResult result = completion(options, partialCallback, &rt);
    
    // Convert the result to a JSI object using our helper
    return completionResultToJsi(rt, result, options.use_jinja);
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

jsi::Value LlamaCppModel::tokenizeJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isString()) {
    throw jsi::JSError(rt, "tokenize requires a string argument");
  }
  
  std::string text = args[0].getString(rt).utf8(rt);
  
  try {
    // Get tokenization result
    auto tokens = tokenize(text);
    
    // Create array to hold the tokens
    jsi::Array result(rt, tokens.size());
    for (size_t i = 0; i < tokens.size(); i++) {
      result.setValueAtIndex(rt, i, jsi::Value((double)tokens[i]));
    }
    
    return result;
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

jsi::Value LlamaCppModel::embeddingJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isString()) {
    throw jsi::JSError(rt, "embedding requires a string argument");
  }
  
  std::string text = args[0].getString(rt).utf8(rt);
  
  try {
    // Get embedding vector
    auto embeddings = embedding(text);
    
    // Create array to hold the embeddings
    jsi::Array result(rt, embeddings.size());
    for (size_t i = 0; i < embeddings.size(); i++) {
      result.setValueAtIndex(rt, i, jsi::Value((double)embeddings[i]));
    }
    
    return result;
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

jsi::Value LlamaCppModel::releaseJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  try {
    release();
    return jsi::Value(true);
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

// Function to test token-by-token processing in a simplified manner
jsi::Value LlamaCppModel::testProcessTokensJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  try {
    if (!model_ || !ctx_) {
      throw std::runtime_error("Model or context not initialized");
    }
    
    // Get the text to test
    std::string text;
    if (count > 0 && args[0].isString()) {
      text = args[0].getString(rt).utf8(rt);
    } else {
      text = "Hello, world!"; // Default test text
    }
    
    // Clear KV cache
    llama_kv_self_clear(ctx_);
    
    // Tokenize the text
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // Create a buffer for tokenization - with clearing the KV cache, is_first is true
    std::vector<llama_token> tokens(text.size() + 4);
    int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), true, true);
    
    if (n_tokens < 0) {
      throw std::runtime_error("Tokenization failed: " + std::to_string(n_tokens));
    }
    
    // Resize to actual size
    tokens.resize(n_tokens);
    
    // Create result object
    jsi::Object result(rt);
    
    // Add tokens to result
    jsi::Array tokensArray(rt, n_tokens);
    for (int i = 0; i < n_tokens; i++) {
      tokensArray.setValueAtIndex(rt, i, jsi::Value((double)tokens[i]));
    }
    result.setProperty(rt, "tokens", tokensArray);
    
    // Process each token individually for testing
    bool success = true;
    std::string errorMessage;
    jsi::Array processResults(rt, n_tokens);
    
    for (int i = 0; i < n_tokens; i++) {
      // Create a batch for a single token
      llama_batch batch = llama_batch_get_one(&tokens[i], 1);
      batch.pos[0] = i; // Set position
      
      // Process the token and capture result
      jsi::Object tokenResult(rt);
      tokenResult.setProperty(rt, "index", jsi::Value(i));
      tokenResult.setProperty(rt, "id", jsi::Value((double)tokens[i]));
      
      // Try to get token text
      char token_buf[128] = {0}; // Larger buffer for safety
      int token_str_len = llama_token_to_piece(vocab, tokens[i], token_buf, sizeof(token_buf) - 1, false, true);
      std::string token_text;
      if (token_str_len > 0) {
        token_buf[token_str_len] = '\0';
        token_text = std::string(token_buf);
      }
      
      tokenResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, token_text));
      
      // Process the token
      int decode_result = llama_decode(ctx_, batch);
      
      tokenResult.setProperty(rt, "success", jsi::Value(decode_result == 0));
      tokenResult.setProperty(rt, "result", jsi::Value(decode_result));
      
      if (decode_result != 0) {
        success = false;
        errorMessage = "Failed at token " + std::to_string(i) + " (ID: " + std::to_string(tokens[i]) + ")";
        tokenResult.setProperty(rt, "error", jsi::String::createFromUtf8(rt, errorMessage));
      }
      
      processResults.setValueAtIndex(rt, i, tokenResult);
    }
    
    // Add overall results
    result.setProperty(rt, "success", jsi::Value(success));
    result.setProperty(rt, "processResults", processResults);
    
    if (!success) {
      result.setProperty(rt, "error", jsi::String::createFromUtf8(rt, errorMessage));
    }
    
    return result;
  } catch (const std::exception& e) {
    jsi::Object error(rt);
    error.setProperty(rt, "success", jsi::Value(false));
    error.setProperty(rt, "error", jsi::String::createFromUtf8(rt, e.what()));
    return error;
  }
}

jsi::Value LlamaCppModel::get(jsi::Runtime& rt, const jsi::PropNameID& name) {
  auto nameStr = name.utf8(rt);

  if (nameStr == "tokenize") {
    return jsi::Function::createFromHostFunction(
      rt, name, 1,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->tokenizeJsi(runtime, args, count);
      });
  } 
  else if (nameStr == "completion") {
    return jsi::Function::createFromHostFunction(
      rt, name, 2,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->completionJsi(runtime, args, count);
      });
  }
  else if (nameStr == "embedding") {
    return jsi::Function::createFromHostFunction(
      rt, name, 1,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->embeddingJsi(runtime, args, count);
      });
  }
  else if (nameStr == "testProcessTokens") {
    return jsi::Function::createFromHostFunction(
      rt, name, 1,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->testProcessTokensJsi(runtime, args, count);
      });
  }
  else if (nameStr == "release") {
    return jsi::Function::createFromHostFunction(
      rt, name, 0,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->releaseJsi(runtime, args, count);
      });
  }
  else if (nameStr == "n_vocab") {
    return jsi::Value(getVocabSize());
  }
  else if (nameStr == "n_ctx") {
    return jsi::Value(getContextSize());
  }
  else if (nameStr == "n_embd") {
    return jsi::Value(getEmbeddingSize());
  }
  
  return jsi::Value::undefined();
}

void LlamaCppModel::set(jsi::Runtime& rt, const jsi::PropNameID& name, const jsi::Value& value) {
  // Currently we don't support setting properties
  throw jsi::JSError(rt, "Cannot modify llama model properties");
}

std::vector<jsi::PropNameID> LlamaCppModel::getPropertyNames(jsi::Runtime& rt) {
  std::vector<jsi::PropNameID> result;
  result.push_back(jsi::PropNameID::forAscii(rt, "tokenize"));
  result.push_back(jsi::PropNameID::forAscii(rt, "completion"));
  result.push_back(jsi::PropNameID::forAscii(rt, "embedding"));
  result.push_back(jsi::PropNameID::forAscii(rt, "testProcessTokens"));
  result.push_back(jsi::PropNameID::forAscii(rt, "release"));
  result.push_back(jsi::PropNameID::forAscii(rt, "n_vocab"));
  result.push_back(jsi::PropNameID::forAscii(rt, "n_ctx"));
  result.push_back(jsi::PropNameID::forAscii(rt, "n_embd"));
  return result;
}


} // namespace facebook::react 