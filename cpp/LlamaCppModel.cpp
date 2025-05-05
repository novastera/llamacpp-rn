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

// Include rn-completion integration
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

LlamaCppModel::LlamaCppModel(rn_llama_context* rn_ctx)
    : rn_ctx_(rn_ctx), should_stop_completion_(false), is_predicting_(false) {
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
  if (rn_ctx_) {
    if (rn_ctx_->ctx) {
      llama_free(rn_ctx_->ctx);
      rn_ctx_->ctx = nullptr;
    }
    
    if (rn_ctx_->model) {
      llama_model_free(rn_ctx_->model);
      rn_ctx_->model = nullptr;
    }
    
    // Note: rn_ctx_ itself is owned by the module, so we don't delete it here
    rn_ctx_ = nullptr;
  }
}

int32_t LlamaCppModel::getVocabSize() const {
  if (!rn_ctx_ || !rn_ctx_->model) {
    throw std::runtime_error("Model not loaded");
  }
  
  return llama_vocab_n_tokens(rn_ctx_->vocab);
}

int32_t LlamaCppModel::getContextSize() const {
  if (!rn_ctx_ || !rn_ctx_->ctx) {
    throw std::runtime_error("Context not initialized");
  }
  
  return llama_n_ctx(rn_ctx_->ctx);
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
  if (!rn_ctx_ || !rn_ctx_->model || !rn_ctx_->ctx) {
    CompletionResult result;
    result.content = "";
    result.success = false;
    result.error_msg = "Model or context not initialized";
    result.error_type = RN_ERROR_MODEL_LOAD;
    return result;
  }
  // Clear the context KV cache
  llama_kv_self_clear(rn_ctx_->ctx);
  // Create a temporary rn_llama_context that wraps our model and context
  rn_llama_context rn_ctx;
  rn_ctx.model = rn_ctx_->model;
  rn_ctx.ctx = rn_ctx_->ctx;
  rn_ctx.model_loaded = true;
  rn_ctx.vocab = rn_ctx_->vocab;
  
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
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(rt, "tokenize requires an options object with 'content' field");
  }
  
  try {
    jsi::Object options = args[0].getObject(rt);
    
    // Extract required content parameter
    if (!options.hasProperty(rt, "content") || !options.getProperty(rt, "content").isString()) {
      throw jsi::JSError(rt, "tokenize requires a 'content' string field");
    }
    std::string content = options.getProperty(rt, "content").getString(rt).utf8(rt);
    
    // Extract optional parameters using SystemUtils helpers
    bool add_special = false;
    bool with_pieces = false;
    SystemUtils::setIfExists(rt, options, "add_special", add_special);
    SystemUtils::setIfExists(rt, options, "with_pieces", with_pieces);
    
    // Parameter for llama_tokenize
    bool parse_special = true;
    
    if (!rn_ctx_ || !rn_ctx_->model) {
      throw std::runtime_error("Model not loaded");
    }
    
    // First get the number of tokens needed
    int n_tokens = llama_tokenize(rn_ctx_->vocab, content.c_str(), content.length(), nullptr, 0, add_special, parse_special);
    if (n_tokens < 0) {
      n_tokens = -n_tokens; // Convert negative value (indicates insufficient buffer)
    }
    
    // Allocate space for tokens
    std::vector<llama_token> tokens(n_tokens);
    
    // Do the actual tokenization
    n_tokens = llama_tokenize(rn_ctx_->vocab, content.c_str(), content.length(), tokens.data(), tokens.size(), add_special, parse_special);
    if (n_tokens < 0) {
      n_tokens = -n_tokens; // Handle negative return value
      tokens.resize(n_tokens);
      
      // Retry with the correct size
      int retry_result = llama_tokenize(rn_ctx_->vocab, content.c_str(), content.length(), tokens.data(), tokens.size(), add_special, parse_special);
      if (retry_result != n_tokens) {
        throw std::runtime_error("Failed to tokenize text: inconsistent token count");
      }
    } else {
      tokens.resize(n_tokens);
    }
    
    // Create result object
    jsi::Object result(rt);
    
    // Create tokens array with appropriate size
    jsi::Array tokensArray(rt, tokens.size());
    
    // Process tokens in a single loop
    for (size_t i = 0; i < tokens.size(); i++) {
      if (with_pieces) {
        // Create token object with id and piece
        jsi::Object tokenObj(rt);
        tokenObj.setProperty(rt, "id", jsi::Value((double)tokens[i]));
        
        // Get token piece
        std::string piece = common_token_to_piece(rn_ctx_->ctx, tokens[i]);
        
        // Check if the piece is valid UTF-8, using the function from rn-utils.hpp
        if (is_valid_utf8(piece)) {
          tokenObj.setProperty(rt, "piece", jsi::String::createFromUtf8(rt, piece));
        } else {
          // Create an array of byte values for non-UTF8 pieces
          jsi::Array byteArray(rt, piece.size());
          for (size_t j = 0; j < piece.size(); j++) {
            byteArray.setValueAtIndex(rt, j, jsi::Value((double)(unsigned char)piece[j]));
          }
          tokenObj.setProperty(rt, "piece", byteArray);
        }
        tokensArray.setValueAtIndex(rt, i, tokenObj);
      } else {
        // Just use the token ID
        tokensArray.setValueAtIndex(rt, i, jsi::Value((double)tokens[i]));
      }
    }
    
    result.setProperty(rt, "tokens", tokensArray);
    return result;
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

jsi::Value LlamaCppModel::detokenizeJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(rt, "detokenize requires an options object with 'tokens' field");
  }
  
  try {
    jsi::Object options = args[0].getObject(rt);
    
    if (!options.hasProperty(rt, "tokens") || !options.getProperty(rt, "tokens").isObject()) {
      throw jsi::JSError(rt, "detokenize requires a 'tokens' array field");
    }
    
    jsi::Array tokensArray = options.getProperty(rt, "tokens").getObject(rt).getArray(rt);
    std::vector<llama_token> tokens;
    tokens.reserve(tokensArray.size(rt));
    
    for (size_t i = 0; i < tokensArray.size(rt); i++) {
      jsi::Value token = tokensArray.getValueAtIndex(rt, i);
      if (!token.isNumber()) {
        throw jsi::JSError(rt, "tokens array must contain only numbers");
      }
      tokens.push_back(static_cast<llama_token>(token.getNumber()));
    }
    
    if (!rn_ctx_ || !rn_ctx_->ctx) {
      throw std::runtime_error("Context not initialized");
    }
    
    // Pre-allocate content with a reasonable size estimate
    // Each token typically expands to 1-4 characters, but we'll use a conservative estimate of 4
    std::string content;
    content.reserve(tokens.size() * 4);
    
    // Perform detokenization
    for (const auto& token : tokens) {
      content += common_token_to_piece(rn_ctx_->ctx, token);
    }
    
    // Create result object
    jsi::Object result(rt);
    result.setProperty(rt, "content", jsi::String::createFromUtf8(rt, content));
    
    return result;
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

// Get embedding size for the model
int32_t LlamaCppModel::getEmbeddingSize() const {
  if (!rn_ctx_ || !rn_ctx_->model) {
    throw std::runtime_error("Model not loaded");
  }
  
  return llama_model_n_embd(rn_ctx_->model);
}

// Update the embeddingJsi method
jsi::Value LlamaCppModel::embeddingJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1) {
    throw jsi::JSError(rt, "embedding requires input text or options object");
  }
  
  // Parse input parameters
  std::string text;
  bool add_bos_token = true;
  std::string encoding_format = "float";
  
  // Check if the input is a string or an object
  if (args[0].isString()) {
    text = args[0].getString(rt).utf8(rt);
  } else if (args[0].isObject()) {
    jsi::Object options = args[0].getObject(rt);
    
    // Check for content or input properties (for OpenAI compatibility)
    if (options.hasProperty(rt, "content") && options.getProperty(rt, "content").isString()) {
      text = options.getProperty(rt, "content").getString(rt).utf8(rt);
    } else if (options.hasProperty(rt, "input") && options.getProperty(rt, "input").isString()) {
      text = options.getProperty(rt, "input").getString(rt).utf8(rt);
    } else {
      throw jsi::JSError(rt, "embedding requires a 'content' or 'input' string property");
    }
    
    // Check for additional options
    if (options.hasProperty(rt, "add_bos_token") && options.getProperty(rt, "add_bos_token").isBool()) {
      add_bos_token = options.getProperty(rt, "add_bos_token").getBool();
    }
    
    if (options.hasProperty(rt, "encoding_format") && options.getProperty(rt, "encoding_format").isString()) {
      encoding_format = options.getProperty(rt, "encoding_format").getString(rt).utf8(rt);
      // Validate encoding format options
      if (encoding_format != "float" && encoding_format != "base64") {
        throw jsi::JSError(rt, "encoding_format must be 'float' or 'base64'");
      }
    }
  } else {
    throw jsi::JSError(rt, "embedding requires a string or options object");
  }
  
  if (text.empty()) {
    throw jsi::JSError(rt, "Input text cannot be empty");
  }
  
  try {
    // Check model initialization
    if (!rn_ctx_ || !rn_ctx_->model || !rn_ctx_->ctx) {
      throw std::runtime_error("Model or context not initialized");
    }
    
    // Clear the KV cache
    llama_kv_self_clear(rn_ctx_->ctx);
    
    // Generate embedding directly here
    const llama_model* model = llama_get_model(rn_ctx_->ctx);
    const llama_vocab* vocab = llama_model_get_vocab(model);
    const int n_embd = llama_model_n_embd(model);
    
    // Tokenize the input text
    llama_tokens tokens = common_tokenize(vocab, text, add_bos_token, true);
    if (tokens.empty()) {
      throw std::runtime_error("Input content cannot be tokenized properly");
    }
    
    // Create and populate the batch
    llama_batch batch = llama_batch_init(tokens.size(), 0, 1);
    for (size_t i = 0; i < tokens.size(); i++) {
      common_batch_add(batch, tokens[i], i, {0}, true);
    }
    
    // Set embedding mode
    llama_set_embeddings(rn_ctx_->ctx, true);
    
    // Process the batch
    if (llama_decode(rn_ctx_->ctx, batch) != 0) {
      llama_batch_free(batch);
      throw std::runtime_error("Failed to process embeddings batch");
    }
    
    // Get the pooling type and extract embeddings
    enum llama_pooling_type pooling_type = llama_pooling_type(rn_ctx_->ctx);
    std::vector<float> embeddings;
    
    if (pooling_type == LLAMA_POOLING_TYPE_NONE) {
      // Without pooling, return embedding for the last token
      for (int i = 0; i < batch.n_tokens; ++i) {
        if (!batch.logits[i]) {
          continue;
        }
        
        const float* embd = llama_get_embeddings_ith(rn_ctx_->ctx, i);
        if (embd == NULL) {
          llama_batch_free(batch);
          throw std::runtime_error("Failed to get embeddings");
        }
        
        embeddings.assign(embd, embd + n_embd);
        break; // We only extract one embedding here
      }
    } else {
      // With pooling, get the pooled embedding
      const float* embd = llama_get_embeddings_seq(rn_ctx_->ctx, 0);
      if (embd == NULL) {
        llama_batch_free(batch);
        throw std::runtime_error("Failed to get pooled embeddings");
      }
      
      // Normalize the embedding
      embeddings.resize(n_embd);
      common_embd_normalize(embd, embeddings.data(), n_embd, 2);
    }
    
    llama_batch_free(batch);
    
    // Token count for usage reporting
    int token_count = tokens.size();
    
    // Determine if we should return OpenAI-like format
    bool openai_format = (count > 1 && args[1].isBool() && args[1].getBool());
    
    if (openai_format) {
      // Create OpenAI-compatible response format
      jsi::Object response(rt);
      jsi::Array data(rt, 1);
      jsi::Object embeddingObj(rt);
      
      // Handle base64 encoding if requested
      if (encoding_format == "base64") {
        // Convert embedding to base64
        const char* data_ptr = reinterpret_cast<const char*>(embeddings.data());
        size_t data_size = embeddings.size() * sizeof(float);
        std::string base64str = base64::encode(data_ptr, data_size);
        
        // Set the base64 encoded string
        embeddingObj.setProperty(rt, "embedding", jsi::String::createFromUtf8(rt, base64str));
        embeddingObj.setProperty(rt, "encoding_format", jsi::String::createFromUtf8(rt, "base64"));
      } else {
        // Create the embedding array for float format
        jsi::Array embeddingArray(rt, embeddings.size());
        for (size_t i = 0; i < embeddings.size(); i++) {
          embeddingArray.setValueAtIndex(rt, i, jsi::Value((double)embeddings[i]));
        }
        embeddingObj.setProperty(rt, "embedding", embeddingArray);
      }
      
      // Set common properties
      embeddingObj.setProperty(rt, "index", jsi::Value(0));
      embeddingObj.setProperty(rt, "object", jsi::String::createFromUtf8(rt, "embedding"));
      
      // Add embedding object to data array
      data.setValueAtIndex(rt, 0, embeddingObj);
      
      // Set up response properties
      response.setProperty(rt, "data", data);
      response.setProperty(rt, "model", jsi::String::createFromUtf8(rt, "llama"));
      response.setProperty(rt, "object", jsi::String::createFromUtf8(rt, "list"));
      
      // Add usage information
      jsi::Object usage(rt);
      usage.setProperty(rt, "prompt_tokens", jsi::Value(token_count));
      usage.setProperty(rt, "total_tokens", jsi::Value(token_count));
      response.setProperty(rt, "usage", usage);
      
      return response;
    } else {
      // For non-OpenAI format, base64 is not applicable
      if (encoding_format == "base64") {
        throw jsi::JSError(rt, "base64 encoding format is only supported with OpenAI format");
      }
      
      // Return simple embedding array
      jsi::Array result(rt, embeddings.size());
      for (size_t i = 0; i < embeddings.size(); i++) {
        result.setValueAtIndex(rt, i, jsi::Value((double)embeddings[i]));
      }
      return result;
    }
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

jsi::Value LlamaCppModel::get(jsi::Runtime& rt, const jsi::PropNameID& name) {
  auto nameStr = name.utf8(rt);

  if (nameStr == "tokenize") {
    return jsi::Function::createFromHostFunction(
      rt, name, 1,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->tokenizeJsi(runtime, args, count);
      });
  } 
  else if (nameStr == "detokenize") {
    return jsi::Function::createFromHostFunction(
      rt, name, 1,
      [this](jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
        return this->detokenizeJsi(runtime, args, count);
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
  result.push_back(jsi::PropNameID::forAscii(rt, "detokenize"));
  result.push_back(jsi::PropNameID::forAscii(rt, "completion"));
  result.push_back(jsi::PropNameID::forAscii(rt, "embedding"));
  result.push_back(jsi::PropNameID::forAscii(rt, "release"));
  result.push_back(jsi::PropNameID::forAscii(rt, "n_vocab"));
  result.push_back(jsi::PropNameID::forAscii(rt, "n_ctx"));
  result.push_back(jsi::PropNameID::forAscii(rt, "n_embd"));
  return result;
}

} // namespace facebook::react 