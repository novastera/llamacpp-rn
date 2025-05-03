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
  
  std::vector<llama_token> tokens(text.size() + 4);
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  
  // Check if this is the first call (KV cache is empty)
  bool is_first = llama_kv_self_used_cells(ctx_) == 0;
  
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), is_first, true);
  
  if (n_tokens < 0) {
    n_tokens = 0; // Handle error case
  }
  
  // No need to add special tokens as llama_tokenize with is_first=true and add_bos=true already does that
  
  // Trim to actual size
  tokens.resize(n_tokens);
  
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
  
  // Check if this is the first call (KV cache is empty)
  bool is_first = true; // After clearing KV cache, this is true
  
  // Tokenize the text
  std::vector<llama_token> tokens(text.size() + 4);
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), is_first, true);
  
  if (n_tokens < 0) {
    throw std::runtime_error("Tokenization failed: " + std::to_string(n_tokens));
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

// generate completion
CompletionResult LlamaCppModel::completion(const CompletionOptions& options, std::function<void(jsi::Runtime&, const char*)> partialCallback, jsi::Runtime* runtime) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  CompletionResult result;
  auto t_start = std::chrono::high_resolution_clock::now();
  
  try {
    // We'll use the precomputed chat params prompt that was applied in buildOptionsAndPrompt
    std::string prompt = options.chat_params.prompt;
    if (prompt.empty() && !options.prompt.empty()) {
      // Fallback to raw prompt if chat prompt is empty
      prompt = options.prompt;
    }
    
    // Debugging info
    if (prompt.empty()) {
      throw std::runtime_error("No prompt available for completion");
    }
    
    // Tokenize the prompt (using llama's core API)
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // Use the common_tokenize function for robust tokenization
    std::vector<llama_token> tokens = common_tokenize(vocab, prompt, false, true);
    
    if (tokens.empty() && !prompt.empty()) {
      throw std::runtime_error("Failed to tokenize prompt");
    }
    
    const int n_prompt = tokens.size();
    
    // Create batch directly like the simple.cpp example
    llama_batch batch = llama_batch_init(n_prompt, 0, 1);
    
    // Fill the batch with prompt tokens
    for (int i = 0; i < n_prompt; i++) {
      batch.token[i] = tokens[i];
      batch.pos[i] = i;
      batch.n_seq_id[i] = 1;
      batch.seq_id[i][0] = 0;
      batch.logits[i] = (i == n_prompt - 1);
    }
    
    // Process the batch
    if (llama_decode(ctx_, batch) != 0) {
      std::string err = "Failed to process prompt tokens (n_tokens=" + std::to_string(n_prompt) + 
                       ", batch_size=" + std::to_string(batch.n_tokens) + 
                       ", ctx_size=" + std::to_string(llama_n_ctx(ctx_)) +
                       ", vocab_size=" + std::to_string(llama_vocab_n_tokens(vocab)) + ")";
      llama_batch_free(batch);
      throw std::runtime_error(err);
    }
    
    // Free the batch
    llama_batch_free(batch);
    
    // Now get ready for generation
    auto eval_end = std::chrono::high_resolution_clock::now();
    result.prompt_tokens = n_prompt;
    result.prompt_duration_ms = std::chrono::duration<double, std::milli>(eval_end - t_start).count();
    
    // Process stop sequences from options and chat params
    std::vector<std::string> stop_prompts = options.stop_prompts;
    for (const auto& stop : options.chat_params.additional_stops) {
      stop_prompts.push_back(stop);
    }
    
    // Main generation loop
    int n_gen = 0;
    std::string generated_text;
    const int max_generated_tokens = options.n_predict > 0 ? options.n_predict : 512;
    
    auto gen_start = std::chrono::high_resolution_clock::now();
    
    // Prepare for generation
    is_predicting_ = true;
    should_stop_completion_ = false;
    
    // Keep track of tokens for penalty calculation
    std::vector<llama_token> last_tokens(std::max(0, options.repeat_last_n));
    size_t last_tokens_index = 0;
    
    // Simplified implementation that uses only the basic llama API
    while (n_gen < max_generated_tokens && !should_stop_completion_) {
      // Get the logits for the most recent token
      float* logits = llama_get_logits(ctx_);
      
      // Prepare token data array for sampling
      const int n_vocab = llama_vocab_n_tokens(vocab);
      std::vector<llama_token_data> candidates;
      candidates.reserve(n_vocab);
      
      for (int token_id = 0; token_id < n_vocab; token_id++) {
        candidates.push_back({ token_id, logits[token_id], 0.0f });
      }
      
      // Create token data array for sampling functions
      llama_token_data_array candidates_array = { candidates.data(), candidates.size(), false };
      
      // Apply penalties if needed
      if (options.repeat_last_n > 0 && options.repeat_penalty > 1.0f) {
        // Get last tokens to penalize
        std::vector<llama_token> penalty_tokens;
        penalty_tokens.reserve(last_tokens.size());
        for (size_t i = 0; i < last_tokens.size(); i++) {
          if (last_tokens[i] != 0) {
            penalty_tokens.push_back(last_tokens[i]);
          }
        }
        
        // Apply repetition penalty
        for (size_t i = 0; i < candidates.size(); i++) {
          const auto token_id = candidates[i].id;
          
          // Check if this token appears in the last n tokens
          for (const auto& t : penalty_tokens) {
            if (token_id == t) {
              // Apply repetition penalty: if logit > 0, divide by penalty; if logit < 0, multiply by penalty
              if (candidates[i].logit > 0.0f) {
                candidates[i].logit /= options.repeat_penalty;
              } else {
                candidates[i].logit *= options.repeat_penalty;
              }
              break;
            }
          }
        }
      }
      
      // Apply frequency/presence penalties if needed
      if (options.frequency_penalty != 0.0f || options.presence_penalty != 0.0f) {
        // Count token frequencies from history
        std::unordered_map<llama_token, int> token_counts;
        for (const auto& t : last_tokens) {
          if (t != 0) {
            token_counts[t]++;
          }
        }
        
        // Apply penalties to the logits
        for (size_t i = 0; i < candidates.size(); i++) {
          const auto token_id = candidates[i].id;
          
          if (token_counts.find(token_id) != token_counts.end()) {
            // Token exists in history
            const int count = token_counts[token_id];
            
            // Apply frequency penalty (scales with frequency)
            candidates[i].logit -= static_cast<float>(count) * options.frequency_penalty;
            
            // Apply presence penalty (applies once if token appears at all)
            candidates[i].logit -= options.presence_penalty;
          }
        }
      }
      
      // Choose a token based on sampling strategy
      llama_token new_token_id;
      
      if (options.temperature <= 0.0f) {
        // Greedy sampling - just pick the token with the highest probability
        new_token_id = candidates[std::max_element(candidates.begin(), candidates.end(),
          [](const llama_token_data& a, const llama_token_data& b) { return a.logit < b.logit; }) - candidates.begin()].id;
      } else {
        // Temperature sampling
        
        // Convert logits to probabilities via softmax
        float sum_exp = 0.0f;
        float max_logit = -INFINITY;
        
        // Find max logit for numerical stability
        for (const auto& candidate : candidates) {
          max_logit = std::max(max_logit, candidate.logit);
        }
        
        // Compute softmax
        for (auto& candidate : candidates) {
          candidate.p = exp(candidate.logit - max_logit);
          sum_exp += candidate.p;
        }
        
        // Normalize
        for (auto& candidate : candidates) {
          candidate.p /= sum_exp;
        }
        
        // Apply top_k if enabled
        if (options.top_k > 0 && options.top_k < static_cast<int>(candidates.size())) {
          // Sort candidates by probability in descending order
          std::partial_sort(candidates.begin(), candidates.begin() + options.top_k, candidates.end(),
            [](const llama_token_data& a, const llama_token_data& b) { return a.p > b.p; });
          
          // Keep only top_k candidates
          candidates.resize(options.top_k);
          candidates_array.size = options.top_k;
          
          // Renormalize
          sum_exp = 0.0f;
          for (const auto& candidate : candidates) {
            sum_exp += candidate.p;
          }
          for (auto& candidate : candidates) {
            candidate.p /= sum_exp;
          }
        }
        
        // Apply top_p if enabled
        if (options.top_p > 0.0f && options.top_p < 1.0f) {
          // Sort by probability in descending order
          std::sort(candidates.begin(), candidates.end(),
            [](const llama_token_data& a, const llama_token_data& b) { return a.p > b.p; });
          
          float cumsum = 0.0f;
          size_t i;
          
          for (i = 0; i < candidates.size(); i++) {
            cumsum += candidates[i].p;
            if (cumsum >= options.top_p) {
              i++;  // Include this token
              break;
            }
          }
          
          // Resize to include only tokens up to the cumulative probability threshold
          candidates.resize(i);
          candidates_array.size = i;
          
          // Renormalize
          sum_exp = 0.0f;
          for (const auto& candidate : candidates) {
            sum_exp += candidate.p;
          }
          for (auto& candidate : candidates) {
            candidate.p /= sum_exp;
          }
        }
        
        // Apply temperature to adjust distribution entropy
        if (options.temperature != 1.0f) {
          for (auto& candidate : candidates) {
            candidate.p = pow(candidate.p, 1.0f / options.temperature);
          }
          
          // Renormalize
          sum_exp = 0.0f;
          for (const auto& candidate : candidates) {
            sum_exp += candidate.p;
          }
          for (auto& candidate : candidates) {
            candidate.p /= sum_exp;
          }
        }
        
        // Random sampling from the distribution
        std::random_device rd;
        std::mt19937 gen(rd());
        std::discrete_distribution<> dist(candidates.size(), 0.0, 0.0,
          [&candidates](size_t i) { return candidates[i].p; });
        
        new_token_id = candidates[dist(gen)].id;
      }
      
      // Update historical tokens for penalty calculation
      if (options.repeat_last_n > 0) {
        last_tokens[last_tokens_index] = new_token_id;
        last_tokens_index = (last_tokens_index + 1) % last_tokens.size();
      }
      
      // Check for EOS token
      if (new_token_id == llama_vocab_eos(vocab)) {
        result.finish_reason = "stop";
        break;
      }
      
      // First convert to piece (string)
      char token_buf[128] = {0}; // Larger buffer for safety
      int token_str_len = llama_token_to_piece(vocab, new_token_id, token_buf, sizeof(token_buf) - 1, false, true);
      std::string token_text;
      if (token_str_len > 0) {
        token_buf[token_str_len] = '\0';
        token_text = std::string(token_buf);
      }
      
      // Add the text to the output
      generated_text += token_text;
      
      // Check if we should stop generation based on stop sequences
      bool should_stop = false;
      
      for (const auto& stop : stop_prompts) {
        if (generated_text.length() >= stop.length()) {
          if (generated_text.substr(generated_text.length() - stop.length()) == stop) {
            should_stop = true;
            result.finish_reason = "stop";
            // Remove the stop sequence from the result
            generated_text = generated_text.substr(0, generated_text.length() - stop.length());
            break;
          }
        }
      }
      
      if (should_stop) {
        break;
      }
      
      // Prepare the next batch with the sampled token - using the same pattern as in the prompt
      llama_batch next_batch = llama_batch_init(1, 0, 1);
      next_batch.token[0] = new_token_id;
      next_batch.pos[0] = n_prompt + n_gen; // Position is after the prompt plus generated tokens so far
      next_batch.n_seq_id[0] = 1;
      next_batch.seq_id[0][0] = 0;
      next_batch.logits[0] = true;
      
      // Process the new token
      if (llama_decode(ctx_, next_batch)) {
        llama_batch_free(next_batch);
        throw std::runtime_error("Failed to process generation token");
      }
      llama_batch_free(next_batch);
      
      // Report progress if a callback is provided
      if (partialCallback && runtime) {
        partialCallback(*runtime, token_text.c_str());
      }
      
      n_gen++;
    }
    
    // Clean up after generation
    is_predicting_ = false;
    
    // Calculate duration
    auto gen_end = std::chrono::high_resolution_clock::now();
    
    // Set up result
    result.text = generated_text;
    result.generated_tokens = n_gen;
    
    // If we reached the max tokens, mark as truncated
    if (n_gen >= max_generated_tokens) {
      result.truncated = true;
      result.finish_reason = "length";
    }
    
    // Calculate durations
    result.generation_duration_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
    result.total_duration_ms = std::chrono::duration<double, std::milli>(gen_end - t_start).count();
    
  } catch (const std::exception& e) {
    // Ensure we clean up state even if there's an error
    is_predicting_ = false;
    throw;
  }
  
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
    
    // Set up callback and runtime if provided
    if (partialCallback) {
      options.partial_callback = partialCallback;
      options.runtime = &rt;
    }
    
    // Use the prompt from the options
    return processPrompt(rt, options.chat_params.prompt, options);
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
      
      // Check if KV cache is empty
      bool is_first = llama_kv_self_used_cells(ctx_) == 0;
      
      // Tokenize the prompt (using llama's core API)
      const llama_vocab* vocab = llama_model_get_vocab(model_);
      
      // First get the number of tokens needed
      int n_prompt = llama_tokenize(vocab, prompt.c_str(), prompt.length(), nullptr, 0, is_first, true);
      if (n_prompt < 0) {
        n_prompt = -n_prompt; // Handle negative return value
      }
      
      // Allocate space for tokens
      std::vector<llama_token> tokens(n_prompt);
      
      // Do the actual tokenization
      n_prompt = llama_tokenize(vocab, prompt.c_str(), prompt.length(), tokens.data(), tokens.size(), is_first, true);
      if (n_prompt < 0) {
        n_prompt = -n_prompt; // Handle negative return value
      }
      
      // Prepare a batch for the prompt using the helper function from llama.cpp
      llama_batch batch = llama_batch_get_one(tokens.data(), n_prompt);
      
      // Process the batch
      if (llama_decode(ctx_, batch) != 0) {
        std::string err = "Failed to process prompt tokens (n_tokens=" + std::to_string(n_prompt) + 
                         ", batch_size=" + std::to_string(batch.n_tokens) + 
                         ", ctx_size=" + std::to_string(llama_n_ctx(ctx_)) +
                         ", vocab_size=" + std::to_string(llama_vocab_n_tokens(vocab)) + 
                         ", is_first=" + (is_first ? "true" : "false") + ")";
        throw std::runtime_error(err);
      }
      
      // Now get ready for generation
      auto eval_end = std::chrono::high_resolution_clock::now();
      result.prompt_tokens = n_prompt;
      result.prompt_duration_ms = std::chrono::duration<double, std::milli>(eval_end - t_start).count();
      
      // Process stop sequences from options and chat params
      std::vector<std::string> stop_prompts = options.stop_prompts;
      for (const auto& stop : options.chat_params.additional_stops) {
        stop_prompts.push_back(stop);
      }
      
      // Main generation loop
      int n_gen = 0;
      std::string generated_text;
      const int max_generated_tokens = options.n_predict > 0 ? options.n_predict : 512;
      
      auto gen_start = std::chrono::high_resolution_clock::now();
      
      // Prepare for generation
      is_predicting_ = true;
      should_stop_completion_ = false;
      
      // Initialize token generation sampler
      struct llama_sampler* sampler = nullptr;
      
      // Create the sampler chain
      sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
      
      // Add sampling methods in the correct order
      if (options.top_k > 0) {
        llama_sampler_chain_add(sampler, llama_sampler_init_top_k(options.top_k));
      }
      
      if (options.top_p > 0.0f && options.top_p < 1.0f) {
        // Add a minimum of tokens to keep to ensure we don't cut off everything
        llama_sampler_chain_add(sampler, llama_sampler_init_top_p(options.top_p, 1));
      }
      
      if (options.temperature > 0.0f) {
        llama_sampler_chain_add(sampler, llama_sampler_init_temp(options.temperature));
      }
      
      // Set seed for reproducibility
      int seed = options.seed;
      if (seed < 0) {
        // Random seed based on time
        seed = (int)std::chrono::high_resolution_clock::now().time_since_epoch().count();
      }
      
      // Finally add the distribution sampler
      llama_sampler_chain_add(sampler, llama_sampler_init_dist(seed));
      
      // Generate tokens
      llama_token new_token_id = 0;
      
      // Generate up to max_tokens tokens
      while (n_gen < max_generated_tokens && !should_stop_completion_) {
        // Check if we have enough space in the context
        uint32_t n_ctx = llama_n_ctx(ctx_);
        if (llama_kv_self_used_cells(ctx_) + 1 >= n_ctx) {
          result.truncated = true;
          result.finish_reason = "context_length_exceeded";
          break;
        }
        
        // Sample a token using our sampler
        new_token_id = llama_sampler_sample(sampler, ctx_, -1);
        
        // Check for end of generation
        if (new_token_id == llama_vocab_eos(vocab)) {
          result.finish_reason = "stop";
          break;
        }
        
        // Convert token to text
        char token_buf[128] = {0}; // Larger buffer for safety
        int token_str_len = llama_token_to_piece(vocab, new_token_id, token_buf, sizeof(token_buf) - 1, false, true);
        std::string token_text;
        if (token_str_len > 0) {
          token_buf[token_str_len] = '\0';
          token_text = std::string(token_buf);
        }
        
        // Add the text to the output
        generated_text += token_text;
        
        // Check if we should stop generation based on stop sequences
        bool should_stop = false;
        for (const auto& stop : stop_prompts) {
          if (generated_text.length() >= stop.length()) {
            if (generated_text.substr(generated_text.length() - stop.length()) == stop) {
              should_stop = true;
              result.finish_reason = "stop";
              // Remove the stop sequence from the result
              generated_text = generated_text.substr(0, generated_text.length() - stop.length());
              break;
            }
          }
        }
        
        if (should_stop) {
          break;
        }
        
        // Accept the token
        llama_sampler_accept(sampler, new_token_id);
        
        // Continue decoding with new token
        batch = llama_batch_get_one(&new_token_id, 1);
        if (llama_decode(ctx_, batch)) {
          llama_sampler_free(sampler);
          throw std::runtime_error("Failed to process generation token");
        }
        
        // Report progress if we have a callback and runtime
        if (options.partial_callback) {
          options.partial_callback(*options.runtime, token_text.c_str());
        }
        
        n_gen++;
      }
      
      // Clean up after generation
      is_predicting_ = false;
      llama_sampler_free(sampler);
      
      // Calculate duration
      auto gen_end = std::chrono::high_resolution_clock::now();
      
      // Set up result
      result.text = generated_text;
      result.generated_tokens = n_gen;
      
      // If we reached the max tokens, mark as truncated
      if (n_gen >= max_generated_tokens) {
        result.truncated = true;
        result.finish_reason = "length";
      }
      
      // Calculate durations
      result.generation_duration_ms = std::chrono::duration<double, std::milli>(gen_end - gen_start).count();
      result.total_duration_ms = std::chrono::duration<double, std::milli>(gen_end - t_start).count();
      
    } catch (const std::exception& e) {
      // Ensure we clean up state even if there's an error
      is_predicting_ = false;
      throw;
    }
    
    return resultToJsi(rt, result);
}

} // namespace facebook::react 