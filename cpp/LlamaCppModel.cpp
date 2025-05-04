#include "LlamaCppModel.h"
#include "llama.cpp/common/json.hpp"
#include <ctime>
#include <chrono>
#include <random>
#include <algorithm>
#include <sstream>

#include <iostream>
#include <vector>
#include <string>
#include <stdexcept>
#include <memory>
#include <cmath>
#include <cstring>
#include <mutex>
#include <condition_variable>

#include "SystemUtils.h"
#include "llama.cpp/common/common.h"
#include "llama.cpp/common/sampling.h"
#include "llama.cpp/common/chat.h"
#include "llama.cpp/common/speculative.h"
#include "llama.cpp/common/json-schema-to-grammar.h"
#include "llama.cpp/src/llama-grammar.h"
#include "llama.h"

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
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Reset flags
  should_stop_completion_ = false;
  is_predicting_ = false;
  
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
  std::lock_guard<std::mutex> lock(mutex_);
  
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

// Add a helper function to create a penalties sampler
struct llama_sampler* create_penalties_sampler(float repeat_penalty, int repeat_last_n) {
  if (repeat_penalty <= 1.0f) {
    return nullptr; // No need for penalties
  }
  
  // Create a penalties sampler with the given parameters
  // This replaces the non-existent llama_sampler_init_rp function
  return llama_sampler_init_penalties(
    repeat_last_n,      // penalty_last_n
    repeat_penalty,     // penalty_repeat
    0.0f,               // penalty_freq
    0.0f                // penalty_present
  );
}

// Modify the completion function to use this helper
CompletionResult LlamaCppModel::completion(const CompletionOptions& options, std::function<void(jsi::Runtime&, const char*)> partialCallback, jsi::Runtime* runtime) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  CompletionResult result;
  auto t_start = std::chrono::high_resolution_clock::now();
  
  try {
    // Clear the KV cache before processing a new prompt to ensure clean context
    llama_kv_self_clear(ctx_);
    
    // We'll use the precomputed chat params prompt that was applied in buildOptionsAndPrompt
    std::string prompt = options.chat_params.prompt;
    if (prompt.empty() && !options.prompt.empty()) {
      // Fallback to raw prompt if chat prompt is empty
      prompt = options.prompt;
    }
    
    // If we still don't have a prompt, try to build it from messages
    if (prompt.empty() && !options.messages.empty()) {
      // Format conversation for simple models without chat template support
      prompt = ""; // Start with empty prompt
      for (const auto& msg : options.messages) {
        if (msg.role == "system") {
          prompt += "System: " + msg.content + "\n\n";
        } else if (msg.role == "user") {
          prompt += "User: " + msg.content + "\n";
        } else if (msg.role == "assistant") {
          prompt += "Assistant: " + msg.content + "\n";
        } else if (msg.role == "tool") {
          prompt += "Tool: " + msg.content + "\n";
        }
      }
      // Add the final assistant marker
      prompt += "Assistant: ";
    }
    
    if (prompt.empty()) {
      throw std::runtime_error("No valid input provided: prompt, messages, or chat_params must be set");
    }
    
    // Tokenize the prompt with consistent parameters (same as tokenize method)
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // Same parameters as the tokenize method for consistency
    bool add_bos = false;
    bool parse_special = false;
    
    // First get the number of tokens needed
    int n_prompt = llama_tokenize(vocab, prompt.c_str(), prompt.length(), nullptr, 0, add_bos, parse_special);
    if (n_prompt < 0) {
      n_prompt = -n_prompt; // Convert negative value (indicates insufficient buffer)
    }
    
    // Check if the prompt will fit in context
    if (n_prompt > llama_n_ctx(ctx_)) {
      throw std::runtime_error("Prompt too long for context window (" + std::to_string(n_prompt) + 
                              " tokens, max context is " + std::to_string(llama_n_ctx(ctx_)) + ")");
    }
    
    // Allocate space for tokens
    std::vector<llama_token> tokens(n_prompt);
    
    // Do the actual tokenization
    n_prompt = llama_tokenize(vocab, prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), add_bos, parse_special);
    if (n_prompt < 0) {
      n_prompt = -n_prompt; // Handle negative return value
      tokens.resize(n_prompt);
      
      // Retry with the correct size
      n_prompt = llama_tokenize(vocab, prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), add_bos, parse_special);
      if (n_prompt < 0) {
        throw std::runtime_error("Failed to tokenize prompt: " + std::to_string(n_prompt));
      }
    } else {
      tokens.resize(n_prompt);
    }
    
    // Process tokens in batches
    int batch_size = 512; // Default batch size, can be adjusted
    if (options.n_batch > 0) {
      batch_size = options.n_batch;
    }
    
    // Process the prompt tokens in batches
    auto prompt_start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < n_prompt; i += batch_size) {
      int n_batch = std::min(batch_size, n_prompt - i);
      
      // Create a batch for this chunk of tokens
      llama_batch batch = llama_batch_init(n_batch, 0, 1);
      
      // Fill the batch
      for (int j = 0; j < n_batch; j++) {
        batch.token[j] = tokens[i + j];
        batch.pos[j] = i + j;
        batch.n_seq_id[j] = 1;
        batch.seq_id[j][0] = 0;
        batch.logits[j] = (i + j == n_prompt - 1); // Only need logits for last token
      }
      batch.n_tokens = n_batch;
      
      // Process this batch
      if (llama_decode(ctx_, batch) != 0) {
        llama_batch_free(batch);
        throw std::runtime_error("Failed to process prompt tokens (batch starting at token " + 
                                std::to_string(i) + ")");
      }
      
      // Clean up the batch
      llama_batch_free(batch);
    }
    
    auto prompt_end = std::chrono::high_resolution_clock::now();
    double prompt_duration = std::chrono::duration<double, std::milli>(prompt_end - prompt_start).count();
    
    // Capture prompt stats
    result.prompt_tokens = n_prompt;
    result.prompt_duration_ms = prompt_duration;
    
    // Prepare for sampling
    is_predicting_ = true;
    should_stop_completion_ = false;
    
    // Store token generation data
    std::string generated_text;
    int n_gen = 0;
    
    // Setup max tokens to generate
    int max_tokens = options.n_predict > 0 ? options.n_predict : 512;
    if (options.max_tokens > 0) {
      max_tokens = options.max_tokens;
    }
    
    // Combine all stop sequences
    std::vector<std::string> stop_sequences = options.stop_prompts;
    if (!options.chat_params.additional_stops.empty()) {
      stop_sequences.insert(stop_sequences.end(), 
                           options.chat_params.additional_stops.begin(), 
                           options.chat_params.additional_stops.end());
    }
    
    // Initialize sampling parameters
    auto gen_start = std::chrono::high_resolution_clock::now();
    
    struct llama_sampler* sampler = nullptr;
    
    // Create the sampler chain
    sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    
    // Add sampling methods in appropriate order
    // First penalties (frequency, presence, repeat)
    if (options.repeat_penalty > 1.0f) {
      int last_n = options.repeat_last_n;
      if (last_n <= 0) {
        last_n = 64; // Default value
      }
      // Use our helper function instead of the non-existent function
      struct llama_sampler* penalties_sampler = create_penalties_sampler(options.repeat_penalty, last_n);
      if (penalties_sampler) {
        llama_sampler_chain_add(sampler, penalties_sampler);
      }
    }
    
    // Then other sampling modifiers
    if (options.top_k > 0) {
      llama_sampler_chain_add(sampler, llama_sampler_init_top_k(options.top_k));
    }
    
    if (options.typical_p > 0.0f && options.typical_p < 1.0f) {
      // Use llama_sampler_init_typical instead of the non-existent llama_sampler_init_typ
      llama_sampler_chain_add(sampler, llama_sampler_init_typical(options.typical_p, 1));
    }
    
    if (options.top_p > 0.0f && options.top_p < 1.0f) {
      llama_sampler_chain_add(sampler, llama_sampler_init_top_p(options.top_p, 1));
    }
    
    if (options.min_p > 0.0f && options.min_p < 1.0f) {
      llama_sampler_chain_add(sampler, llama_sampler_init_min_p(options.min_p, 0));
    }
    
    if (options.temperature > 0.0f) {
      llama_sampler_chain_add(sampler, llama_sampler_init_temp(options.temperature));
    }
    
    // Set random seed
    int seed = options.seed;
    if (seed < 0) {
      // Generate random seed
      std::random_device rd;
      seed = rd();
    }
    
    // Finally add distribution sampler
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(seed));
    
    // Start generating tokens
    llama_token id = 0;
    bool is_tool_response = false;
    std::string tool_call_json = "";
    
    // Check if we need to look for tool calls
    bool detect_tools = !options.tools.empty();
    
    while (n_gen < max_tokens && !should_stop_completion_) {
      // Check if we've reached context limit
      if (llama_kv_self_used_cells(ctx_) + 1 >= llama_n_ctx(ctx_)) {
        result.finish_reason = "context_length_exceeded";
        result.truncated = true;
        break;
      }
      
      // Sample next token
      id = llama_sampler_sample(sampler, ctx_, -1);
      
      // Check for EOS
      if (id == llama_vocab_eos(vocab)) {
        result.finish_reason = "stop";
        break;
      }
      
      // Convert token to text
      std::string piece;
      piece.resize(64);  // Initial size
      int piece_len = llama_token_to_piece(vocab, id, piece.data(), piece.size(), false, true);
      if (piece_len < 0) {
        piece.resize(-piece_len);
        piece_len = llama_token_to_piece(vocab, id, piece.data(), piece.size(), false, true);
      }
      piece.resize(piece_len);
      
      // Add to generated text
      generated_text += piece;
      
      // Check for stop sequences
      bool should_stop = false;
      for (const auto& stop : stop_sequences) {
        if (!stop.empty() && generated_text.length() >= stop.length()) {
          if (generated_text.substr(generated_text.length() - stop.length()) == stop) {
            // Remove the stop sequence from the result
            generated_text = generated_text.substr(0, generated_text.length() - stop.length());
            should_stop = true;
            result.finish_reason = "stop";
            break;
          }
        }
      }
      
      if (should_stop) {
        break;
      }
      
      // Check for tool calls if enabled
      if (detect_tools && !is_tool_response) {
        if (generated_text.find("\"tool_calls\"") != std::string::npos) {
          // Potential tool call detected, look for tool JSON format
          size_t tool_start = generated_text.find("{\"tool_calls\"");
          if (tool_start != std::string::npos) {
            // Try to find the end of the JSON
            size_t brace_count = 0;
            bool in_string = false;
            bool escaped = false;
            size_t i = tool_start;
            
            for (; i < generated_text.length(); i++) {
              char c = generated_text[i];
              
              if (escaped) {
                escaped = false;
                continue;
              }
              
              if (c == '\\') {
                escaped = true;
                continue;
              }
              
              if (c == '"' && !escaped) {
                in_string = !in_string;
                continue;
              }
              
              if (!in_string) {
                if (c == '{') {
                  brace_count++;
                } else if (c == '}') {
                  brace_count--;
                  if (brace_count == 0) {
                    // Found the end of the JSON object
                    i++;
                    break;
                  }
                }
              }
            }
            
            if (i < generated_text.length() && brace_count == 0) {
              // We found a complete JSON object
              tool_call_json = generated_text.substr(tool_start, i - tool_start);
              
              try {
                // Parse the JSON to extract tool calls
                nlohmann::json json = nlohmann::json::parse(tool_call_json);
                if (json.contains("tool_calls") && json["tool_calls"].is_array()) {
                  auto tool_calls = json["tool_calls"];
                  for (const auto& tc : tool_calls) {
                    ToolCall tool_call;
                    
                    if (tc.contains("id")) {
                      tool_call.id = tc["id"];
                    }
                    
                    if (tc.contains("type")) {
                      tool_call.type = tc["type"];
                    } else {
                      tool_call.type = "function";
                    }
                    
                    if (tc.contains("function")) {
                      auto function = tc["function"];
                      if (function.contains("name")) {
                        tool_call.name = function["name"];
                      }
                      if (function.contains("arguments")) {
                        tool_call.arguments = function["arguments"];
                      }
                    }
                    
                    result.tool_calls.push_back(tool_call);
                  }
                  
                  // We found tool calls, so stop generation
                  result.finish_reason = "tool_calls";
                  break;
                }
              } catch (const std::exception& e) {
                // Failed to parse JSON, continue generation
              }
            }
          }
        }
      }
      
      // Accept the token
      llama_sampler_accept(sampler, id);
      
      // Create a batch for single token
      llama_batch batch = llama_batch_init(1, 0, 1);
      batch.token[0] = id;
      batch.pos[0] = n_prompt + n_gen;
      batch.n_seq_id[0] = 1;
      batch.seq_id[0][0] = 0;
      batch.logits[0] = true;
      batch.n_tokens = 1;
      
      // Process the token
      if (llama_decode(ctx_, batch) != 0) {
        llama_batch_free(batch);
        llama_sampler_free(sampler);
        throw std::runtime_error("Failed to process generation token");
      }
      
      // Clean up the batch
      llama_batch_free(batch);
      
      // Increment token count
      n_gen++;
      
      // If we have a partial callback, invoke it
      if (partialCallback && runtime) {
        partialCallback(*runtime, piece.c_str());
      }
    }
    
    // Set return values
    result.text = generated_text;
    result.generated_tokens = n_gen;
    result.truncated = result.finish_reason == "context_length_exceeded" || 
                      (n_gen >= max_tokens && result.finish_reason.empty());
    
    if (result.finish_reason.empty()) {
      if (n_gen >= max_tokens) {
        result.finish_reason = "length";
      } else if (should_stop_completion_) {
        result.finish_reason = "user_cancelled";
      } else {
        result.finish_reason = "stop";
      }
    }
    
    // Clean up and calculate duration
    auto gen_end = std::chrono::high_resolution_clock::now();
    result.generation_duration_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
    result.total_duration_ms = std::chrono::duration<double, std::milli>(gen_end - t_start).count();
    
    // Clean up sampler
    llama_sampler_free(sampler);
  } catch (const std::exception& e) {
    // Reset prediction state
    is_predicting_ = false;
    throw;
  }
  
  // Reset prediction state
  is_predicting_ = false;
  
  return result;
}

// JSI method for completions
// initialise completion parameters
jsi::Value LlamaCppModel::completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  // Check for required arguments
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(rt, "completion requires options object");
  }
  
  // Check for a partial callback
  std::function<void(jsi::Runtime&, const char*)> partialCallback = nullptr;
  if (count > 1 && args[1].isObject() && args[1].getObject(rt).isFunction(rt)) {
    // Create a shared copy of the callback function
    auto jsCallback = std::make_shared<jsi::Function>(args[1].getObject(rt).getFunction(rt));
    
    // Create our C++ callback that invokes the JS callback
    partialCallback = [jsCallback](jsi::Runtime& runtime, const char* token) {
      // Safely invoke the callback with the token
      try {
        jsi::Object dataObj(runtime);
        dataObj.setProperty(runtime, "token", jsi::String::createFromUtf8(runtime, token));
        jsCallback->call(runtime, dataObj);
      } catch (std::exception& e) {
        // Ignore errors in the callback
      }
    };
  }
  
  try {
    // Parse options and generate the prompt
    CompletionOptions options = buildOptionsAndPrompt(rt, args[0].getObject(rt));
    
    // Save these for the actual completion call
    options.partial_callback = partialCallback;
    options.runtime = &rt;
    
    // Perform the completion (don't try to extract prompt again)
    CompletionResult result = completion(options, partialCallback, &rt);
    
    // Convert the result to a JSI object
    jsi::Object resultObj(rt);
    resultObj.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.text));
    resultObj.setProperty(rt, "truncated", jsi::Value(result.truncated));
    resultObj.setProperty(rt, "finish_reason", jsi::String::createFromUtf8(rt, result.finish_reason));
    resultObj.setProperty(rt, "prompt_tokens", jsi::Value(result.prompt_tokens));
    resultObj.setProperty(rt, "generated_tokens", jsi::Value(result.generated_tokens));
    resultObj.setProperty(rt, "prompt_duration_ms", jsi::Value(result.prompt_duration_ms));
    resultObj.setProperty(rt, "generation_duration_ms", jsi::Value(result.generation_duration_ms));
    resultObj.setProperty(rt, "total_duration_ms", jsi::Value(result.total_duration_ms));
    
    return resultObj;
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

// Parse the CompletionOptions from a JS object
CompletionOptions LlamaCppModel::buildOptionsAndPrompt(jsi::Runtime& rt, const jsi::Object& obj) {
  CompletionOptions options;
  
  // Parse basic options
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
  
  if (obj.hasProperty(rt, "typical_p") && !obj.getProperty(rt, "typical_p").isUndefined()) {
    options.typical_p = obj.getProperty(rt, "typical_p").asNumber();
  }
  
  // Generation control
  if (obj.hasProperty(rt, "n_predict") && !obj.getProperty(rt, "n_predict").isUndefined()) {
    options.n_predict = static_cast<int>(obj.getProperty(rt, "n_predict").asNumber());
  } else if (obj.hasProperty(rt, "max_tokens") && !obj.getProperty(rt, "max_tokens").isUndefined()) {
    options.n_predict = static_cast<int>(obj.getProperty(rt, "max_tokens").asNumber());
  }
  
  if (obj.hasProperty(rt, "stop") && obj.getProperty(rt, "stop").isObject()) {
    jsi::Object stopObj = obj.getProperty(rt, "stop").asObject(rt);
    if (stopObj.isArray(rt)) {
      jsi::Array stopArray = stopObj.asArray(rt);
      for (size_t i = 0; i < stopArray.size(rt); i++) {
        options.stop_prompts.push_back(stopArray.getValueAtIndex(rt, i).asString(rt).utf8(rt));
      }
    } else {
      // Get the value directly which might be a string
      jsi::Value stopValue(rt, stopObj);
      if (stopValue.isString()) {
        options.stop_prompts.push_back(stopValue.asString(rt).utf8(rt));
      } else {
        // For other object types, we can't convert them to strings directly
        // Just log a warning and ignore
        std::cerr << "Warning: Unexpected type for stop sequence, expected string or array." << std::endl;
      }
    }
  }
  
  // Repetition control
  if (obj.hasProperty(rt, "repeat_penalty") && !obj.getProperty(rt, "repeat_penalty").isUndefined()) {
    options.repeat_penalty = obj.getProperty(rt, "repeat_penalty").asNumber();
  }
  
  if (obj.hasProperty(rt, "repeat_last_n") && !obj.getProperty(rt, "repeat_last_n").isUndefined()) {
    options.repeat_last_n = static_cast<int>(obj.getProperty(rt, "repeat_last_n").asNumber());
  }
  
  if (obj.hasProperty(rt, "frequency_penalty") && !obj.getProperty(rt, "frequency_penalty").isUndefined()) {
    options.frequency_penalty = obj.getProperty(rt, "frequency_penalty").asNumber();
  }
  
  if (obj.hasProperty(rt, "presence_penalty") && !obj.getProperty(rt, "presence_penalty").isUndefined()) {
    options.presence_penalty = obj.getProperty(rt, "presence_penalty").asNumber();
  }
  
  // Advanced options
  if (obj.hasProperty(rt, "jinja") && !obj.getProperty(rt, "jinja").isUndefined()) {
    options.jinja = obj.getProperty(rt, "jinja").asBool();
  }
  
  if (obj.hasProperty(rt, "chat_template") && !obj.getProperty(rt, "chat_template").isUndefined()) {
    options.template_name = obj.getProperty(rt, "chat_template").asString(rt).utf8(rt);
  }
  
  if (obj.hasProperty(rt, "tool_choice") && !obj.getProperty(rt, "tool_choice").isUndefined()) {
    options.tool_choice = obj.getProperty(rt, "tool_choice").asString(rt).utf8(rt);
  }
  
  if (obj.hasProperty(rt, "grammar") && !obj.getProperty(rt, "grammar").isUndefined()) {
    options.grammar = obj.getProperty(rt, "grammar").asString(rt).utf8(rt);
  }
  
  // Helper function to convert JSI objects to nlohmann::json
  std::function<nlohmann::json(const jsi::Object&)> jsiObjectToJson;
  
  jsiObjectToJson = [&rt, &jsiObjectToJson](const jsi::Object& jsObject) -> nlohmann::json {
    if (jsObject.isArray(rt)) {
      nlohmann::json jsonArray = nlohmann::json::array();
      jsi::Array jsArray = jsObject.asArray(rt);
      for (size_t i = 0; i < jsArray.size(rt); i++) {
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
        }
      }
      return jsonArray;
    } else {
      nlohmann::json jsonObj = nlohmann::json::object();
      jsi::Array propNames = jsObject.getPropertyNames(rt);
      for (size_t i = 0; i < propNames.size(rt); i++) {
        std::string propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
        jsi::Value value = jsObject.getProperty(rt, propName.c_str());
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
        }
      }
      return jsonObj;
    }
  };
  
  // Now generate the prompt using chat templates
  try {
    // Check if we have messages to process
    if (obj.hasProperty(rt, "messages") && obj.getProperty(rt, "messages").isObject()) {
      // Extract the messages directly to JSON
      nlohmann::json messagesJson = jsiObjectToJson(obj.getProperty(rt, "messages").asObject(rt));
      
      // Use built-in OAI parser to convert to common_chat_msg objects
      std::vector<common_chat_msg> chat_msgs = parse_chat_messages_from_json(messagesJson);
      
      // Set up chat template inputs
      common_chat_templates_inputs inputs;
      inputs.messages = chat_msgs;
      inputs.use_jinja = options.jinja;
      
      // Parse tools if available
      if (obj.hasProperty(rt, "tools") && obj.getProperty(rt, "tools").isObject()) {
        nlohmann::json toolsJson = jsiObjectToJson(obj.getProperty(rt, "tools").asObject(rt));
        inputs.tools = parse_chat_tools_from_json(toolsJson);
      }
      
      // Parse tool choice
      if (options.tool_choice == "none") {
        inputs.tool_choice = COMMON_CHAT_TOOL_CHOICE_NONE;
      } else if (options.tool_choice == "required") {
        inputs.tool_choice = COMMON_CHAT_TOOL_CHOICE_REQUIRED;
      } else {
        inputs.tool_choice = COMMON_CHAT_TOOL_CHOICE_AUTO;
      }
      
      // Get chat templates from the model
      common_chat_templates_ptr tmpls;
      
      // Try with provided template name first if available
      if (!options.template_name.empty()) {
        tmpls = common_chat_templates_init(model_, options.template_name);
        if (!tmpls) {
          options.generated_prompt = "WARNING: Template '" + options.template_name + "' initialization failed, falling back to auto-detect";
        }
      }
      
      // If no template specified or initialization failed, try auto-detect (empty string)
      if (!tmpls) {
        tmpls = common_chat_templates_init(model_, "");
      }
      
      // If that fails too, fallback to chatml as last resort
      if (!tmpls) {
        tmpls = common_chat_templates_init(model_, "chatml");
        if (!tmpls) {
          throw std::runtime_error("Failed to initialize chat templates, even with chatml fallback");
        }
      }
      
      // Log what template source we're using
      const char* template_source = common_chat_templates_source(tmpls.get());
      options.generated_prompt = "Using template: " + std::string(template_source ? template_source : "unknown") + "\n\n";
      
      try {
        // Apply the template to get the formatted prompt
        common_chat_params chat_params = common_chat_templates_apply(tmpls.get(), inputs);
        
        // Store the template and related structures for reuse during completion
        options.chat_template = std::move(tmpls);
        options.chat_inputs = inputs;
        options.chat_params = chat_params;
        
        // Append the actual formatted prompt
        options.generated_prompt += chat_params.prompt;
        
        // Add information about additional stops and grammar
        if (!chat_params.additional_stops.empty()) {
          options.generated_prompt += "\n\n--- Additional Stop Sequences ---\n";
          for (const auto& stop : chat_params.additional_stops) {
            options.generated_prompt += "- \"" + stop + "\"\n";
          }
        }
        
        // If there's a grammar specified in the chat params, use it
        if (!chat_params.grammar.empty()) {
          if (options.grammar.empty()) {
            options.grammar = chat_params.grammar;
            options.generated_prompt += "\n\n--- Grammar Generated by Template ---\n";
            options.generated_prompt += chat_params.grammar.substr(0, 500);
            if (chat_params.grammar.length() > 500) {
              options.generated_prompt += "...\n[Grammar truncated, total length: " + 
                                        std::to_string(chat_params.grammar.length()) + " chars]";
            }
          } else {
            options.generated_prompt += "\n\nNOTE: Grammar in template was ignored since options.grammar was already set.";
          }
        }
      } catch (const std::exception& e) {
        // If template application fails, try with chatml
        try {
          options.generated_prompt += "\nERROR applying template: " + std::string(e.what()) + "\nTrying chatml fallback...\n";
          
          tmpls = common_chat_templates_init(model_, "chatml");
          if (!tmpls) {
            throw std::runtime_error("Failed to initialize chat templates with chatml fallback");
          }
          
          common_chat_params chat_params = common_chat_templates_apply(tmpls.get(), inputs);
          options.generated_prompt += "\n--- ChatML Template Fallback ---\n" + chat_params.prompt;
        } catch (const std::exception& inner_e) {
          options.generated_prompt += "\nERROR with chatml fallback: " + std::string(inner_e.what());
          throw; // Re-throw the inner exception
        }
      }
    } else if (!options.prompt.empty()) {
      // Just use the raw prompt directly
      options.generated_prompt = options.prompt;
    } else {
      throw std::runtime_error("Either 'prompt' or 'messages' is required");
    }
  } catch (const std::exception& e) {
    // Add detailed error information
    options.generated_prompt = "ERROR generating prompt: " + std::string(e.what());
    throw;
  }
  
  return options;
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

// Custom helper to parse JSON messages without relying on template specialization
std::vector<common_chat_msg> parse_chat_messages_from_json(const nlohmann::json& messages) {
  std::vector<common_chat_msg> msgs;
  
  try {
    if (!messages.is_array()) {
      throw std::runtime_error("Expected 'messages' to be an array");
    }
    
    for (const auto& message : messages) {
      if (!message.is_object()) {
        throw std::runtime_error("Expected 'message' to be an object");
      }

      common_chat_msg msg;
      if (!message.contains("role")) {
        throw std::runtime_error("Missing 'role' in message");
      }
      msg.role = message.at("role");

      auto has_content = message.contains("content");
      auto has_tool_calls = message.contains("tool_calls");
      
      if (has_content) {
        const auto& content = message.at("content");
        if (content.is_string()) {
          msg.content = content;
        } else if (content.is_array()) {
          for (const auto& part : content) {
            if (!part.contains("type")) {
              throw std::runtime_error("Missing content part type");
            }
            const auto& type = part.at("type");
            if (type != "text") {
              throw std::runtime_error("Unsupported content part type");
            }
            common_chat_msg_content_part msg_part;
            msg_part.type = type;
            msg_part.text = part.at("text");
            msg.content_parts.push_back(msg_part);
          }
        } else if (!content.is_null()) {
          throw std::runtime_error("Invalid 'content' type");
        }
      }
      
      if (has_tool_calls) {
        for (const auto& tool_call : message.at("tool_calls")) {
          common_chat_tool_call tc;
          if (!tool_call.contains("type")) {
            throw std::runtime_error("Missing tool call type");
          }
          const auto& type = tool_call.at("type");
          if (type != "function") {
            throw std::runtime_error("Unsupported tool call type");
          }
          if (!tool_call.contains("function")) {
            throw std::runtime_error("Missing tool call function");
          }
          const auto& fc = tool_call.at("function");
          if (!fc.contains("name")) {
            throw std::runtime_error("Missing tool call name");
          }
          tc.name = fc.at("name");
          tc.arguments = fc.at("arguments");
          if (tool_call.contains("id")) {
            tc.id = tool_call.at("id");
          }
          msg.tool_calls.push_back(tc);
        }
      }
      
      if (!has_content && !has_tool_calls) {
        throw std::runtime_error("Expected 'content' or 'tool_calls'");
      }
      
      if (message.contains("reasoning_content")) {
        msg.reasoning_content = message.at("reasoning_content");
      }
      if (message.contains("name")) {
        msg.tool_name = message.at("name");
      }
      if (message.contains("tool_call_id")) {
        msg.tool_call_id = message.at("tool_call_id");
      }
      
      msgs.push_back(msg);
    }
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to parse messages: " + std::string(e.what()));
  }
  
  return msgs;
}

// Custom helper to parse tool JSON without relying on template specialization
std::vector<common_chat_tool> parse_chat_tools_from_json(const nlohmann::json& tools) {
  std::vector<common_chat_tool> result;
  
  try {
    if (!tools.is_null()) {
      if (!tools.is_array()) {
        throw std::runtime_error("Expected 'tools' to be an array");
      }
      for (const auto& tool : tools) {
        if (!tool.contains("type")) {
          throw std::runtime_error("Missing tool type");
        }
        const auto& type = tool.at("type");
        if (!type.is_string() || type != "function") {
          throw std::runtime_error("Unsupported tool type");
        }
        if (!tool.contains("function")) {
          throw std::runtime_error("Missing tool function");
        }

        const auto& function = tool.at("function");
        result.push_back({
          /* .name = */ function.at("name"),
          /* .description = */ function.at("description"),
          /* .parameters = */ function.at("parameters").dump(),
        });
      }
    }
  } catch (const std::exception& e) {
    throw std::runtime_error("Failed to parse tools: " + std::string(e.what()));
  }
  
  return result;
}

// Convert GenerationResult to JSI object
jsi::Object resultToJsi(jsi::Runtime& rt, const CompletionResult& result) {
  jsi::Object jsResult(rt);
  jsResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.text));
  jsResult.setProperty(rt, "truncated", jsi::Value(result.truncated));
  jsResult.setProperty(rt, "finish_reason", jsi::String::createFromUtf8(rt, result.finish_reason));
  jsResult.setProperty(rt, "prompt_tokens", jsi::Value(result.prompt_tokens));
  jsResult.setProperty(rt, "generated_tokens", jsi::Value(result.generated_tokens));
  jsResult.setProperty(rt, "prompt_duration_ms", jsi::Value(result.prompt_duration_ms));
  jsResult.setProperty(rt, "generation_duration_ms", jsi::Value(result.generation_duration_ms));
  jsResult.setProperty(rt, "total_duration_ms", jsi::Value(result.total_duration_ms));
  
  // Add tool calls if any
  if (!result.tool_calls.empty()) {
    jsi::Array toolCalls(rt, result.tool_calls.size());
    for (size_t i = 0; i < result.tool_calls.size(); i++) {
      const auto& tc = result.tool_calls[i];
      jsi::Object tcObj(rt);
      tcObj.setProperty(rt, "name", jsi::String::createFromUtf8(rt, tc.name));
      tcObj.setProperty(rt, "arguments", jsi::String::createFromUtf8(rt, tc.arguments));
      if (!tc.id.empty()) {
        tcObj.setProperty(rt, "id", jsi::String::createFromUtf8(rt, tc.id));
      }
      tcObj.setProperty(rt, "type", jsi::String::createFromUtf8(rt, tc.type));
      toolCalls.setValueAtIndex(rt, i, tcObj);
    }
    jsResult.setProperty(rt, "tool_calls", toolCalls);
  }
  
  return jsResult;
}

jsi::Value LlamaCppModel::processPrompt(jsi::Runtime& rt, const std::string& prompt, const CompletionOptions& options) {
    CompletionResult result;
    result.text = "";
    result.finish_reason = "";

    try {
      // Track timings
      const auto t_start = std::chrono::high_resolution_clock::now();
      
      // Clear the KV cache before processing
      llama_kv_self_clear(ctx_);
      
      // Same tokenization parameters as other methods for consistency
      bool add_bos = false;
      bool parse_special = false;
      
      // Tokenize the prompt (using llama's core API)
      const llama_vocab* vocab = llama_model_get_vocab(model_);
      
      // First get the number of tokens needed
      int n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.length(), nullptr, 0, add_bos, parse_special);
      if (n_tokens < 0) {
        n_tokens = -n_tokens; // Handle negative return value
      }
      
      if (n_tokens == 0) {
        throw std::runtime_error("Failed to process prompt tokens (empty input)");
      }
      
      // Allocate space for tokens
      std::vector<llama_token> tokens(n_tokens);
      
      // Do the actual tokenization
      n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), add_bos, parse_special);
      if (n_tokens < 0) {
        n_tokens = -n_tokens; // Handle negative return value
        tokens.resize(n_tokens);
        
        // Retry with the correct size
        n_tokens = llama_tokenize(vocab, prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), add_bos, parse_special);
        if (n_tokens < 0) {
          throw std::runtime_error("Failed to process prompt tokens: " + std::to_string(n_tokens));
        }
      } else {
        tokens.resize(n_tokens);
      }
      
      // Process the tokens
      llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
      batch.logits[batch.n_tokens - 1] = true; // Only need logits for the last token
      
      if (llama_decode(ctx_, batch)) {
        throw std::runtime_error("Failed to process prompt tokens (n_tokens=" + std::to_string(n_tokens) +
                                ", batch_size=" + std::to_string(batch.n_tokens) + 
                                ", ctx_size=" + std::to_string(llama_n_ctx(ctx_)) + 
                                ", vocab_size=" + std::to_string(llama_vocab_n_tokens(vocab)) + ")");
      }
      
      // Get the logits for the last token
      const float* logits = llama_get_logits_ith(ctx_, -1);
      if (!logits) {
        throw std::runtime_error("Failed to get logits after processing prompt");
      }
      
      // Track time spent
      const auto t_end = std::chrono::high_resolution_clock::now();
      const double elapsed = std::chrono::duration<double, std::milli>(t_end - t_start).count();
      
      result.prompt_tokens = n_tokens;
      result.prompt_duration_ms = elapsed;
      
      // Convert to JSI object
      return resultToJsi(rt, result);
      
    } catch (const std::exception& e) {
      jsi::Object error(rt);
      error.setProperty(rt, "error", jsi::String::createFromUtf8(rt, e.what()));
      return error;
    }
}

} // namespace facebook::react 