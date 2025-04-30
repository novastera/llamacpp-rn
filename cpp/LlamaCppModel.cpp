#include "LlamaCppModel.h"
#include <chrono>
#include <iostream>
#include <stdexcept>

// Include llama.cpp headers directly
#include "llama.h"

// We will implement our own template detection using the functions available in llama.h
// instead of directly using llama_chat_detect_template which isn't exposed

namespace facebook::react {

LlamaCppModel::LlamaCppModel(llama_model* model, llama_context* ctx)
    : model_(model), ctx_(ctx), should_stop_completion_(false) {
}

LlamaCppModel::~LlamaCppModel() {
  // Note: We don't automatically release resources here
  // as the user should call release() explicitly
}

void LlamaCppModel::release() {
  std::lock_guard<std::mutex> lock(mutex_);
  
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
}

std::vector<int32_t> LlamaCppModel::tokenize(const std::string& text) {
  if (!model_) {
    throw std::runtime_error("Model not loaded");
  }
  
  std::vector<llama_token> tokens(text.size() + 4);
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), true, true);
  
  if (n_tokens < 0) {
    n_tokens = 0; // Handle error case
  }
  
  tokens.resize(n_tokens);
  return std::vector<int32_t>(tokens.begin(), tokens.end());
}

std::vector<float> LlamaCppModel::embedding(const std::string& text) {
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  // Tokenize the text
  std::vector<llama_token> tokens(text.size() + 4);
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), true, true);
  
  if (n_tokens < 0) {
    n_tokens = 0; // Handle error case
  }
  
  // Evaluate the prompt if we have tokens
  if (n_tokens > 0) {
    llama_batch batch = llama_batch_init(n_tokens, 0, 1);
    for (int i = 0; i < n_tokens; i++) {
      batch.token[i] = tokens[i];
      batch.pos[i] = i;
      batch.seq_id[i][0] = 0;
      batch.logits[i] = false;
    }
    batch.logits[n_tokens - 1] = true; // Get embeddings for the last token
    
    if (llama_decode(ctx_, batch) != 0) {
      llama_batch_free(batch);
      throw std::runtime_error("Failed to decode embedding");
    }
    
    llama_batch_free(batch);
  }
  
  // Get embedding dimension
  int n_embd = llama_model_n_embd(model_);
  
  // Get embeddings
  std::vector<float> embeddings(n_embd);const float* embeddings_data = llama_get_embeddings(ctx_);
  if (embeddings_data) {
    std::copy(embeddings_data, embeddings_data + n_embd, embeddings.data());
  }
  
  return embeddings;
}

std::string LlamaCppModel::completion(const std::string& prompt, 
                                     const std::vector<ChatMessage>& messages,
                                     float temperature, float top_p, int top_k, 
                                     int max_tokens,
                                     const std::vector<std::string>& stop_sequences,
                                     const std::string& template_name,
                                     bool jinja,
                                     const std::string& tool_choice,
                                     const std::vector<Tool>& tools,
                                     std::function<void(jsi::Runtime&, const char*)> partialCallback,
                                     jsi::Runtime* rt) {
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  // We can't directly manipulate JSI objects here since we don't have access to the runtime
  // We'll need to modify our approach to tool handling
  
  // For JSI objects, we should extract necessary information in the JSI wrapper method
  // and pass simple C++ data structures to this method
  
  // Determine text to process (either prompt or messages)
  std::string input_text;
  if (!prompt.empty()) {
    input_text = prompt;
  } else if (!messages.empty()) {
    // Convert our ChatMessage to the llama.cpp chat message format
    std::vector<llama_chat_message> llama_messages;
    
    // Reserve space to avoid reallocations
    llama_messages.reserve(messages.size());
    
    // Store C-style strings for role and content
    std::vector<std::string> role_strings;
    std::vector<std::string> content_strings;
    role_strings.reserve(messages.size());
    content_strings.reserve(messages.size());
    
    // Start collecting debug information
    std::string debug_info = "Messages: ";
    debug_info += std::to_string(messages.size()) + " total (";
    
    for (const auto& msg : messages) {
      // Store strings to keep them alive
      role_strings.push_back(msg.role);
      content_strings.push_back(msg.content);
      
      debug_info += msg.role + ":" + std::to_string(msg.content.size()) + "b, ";
      
      // Create the message struct
      llama_chat_message lcm = {};
      lcm.role = role_strings.back().c_str();
      lcm.content = content_strings.back().c_str();
      
      // Keep the message data alive
      llama_messages.push_back(lcm);
    }
    
    debug_info += ")";
    
    // Apply the chat template 
    bool add_generation_prompt = true;  // Add assistant prompt at the end
    
    // Use the template name provided by the user or get it from the model if not specified
    const char* template_name_cstr = nullptr;
    
    if (template_name.empty()) {
      // No template specified, use the model's default template
      debug_info += " | Template: using model default";
      template_name_cstr = nullptr; // nullptr will use the model's default template
    } else {
      // Use user-provided template
      debug_info += " | Template: user-specified '" + template_name + "'";
      template_name_cstr = template_name.c_str();
    }
    
    // Handle tool setup if tools are provided
    if (!tools.empty()) {
      // Currently, tool calling in llama.cpp is handled through the chat template system
      debug_info += " | Tools: " + std::to_string(tools.size()) + " provided";
    }
    
    // Estimate buffer size needed (twice the total message length plus some overhead)
    size_t estimated_size = 0;
    for (const auto& msg : messages) {
      estimated_size += msg.content.size() * 2;
    }
    estimated_size = std::max(estimated_size, size_t(16384)); // At least 16KB
    debug_info += " | Buffer: " + std::to_string(estimated_size) + "b initial";
    
    // Create buffer for formatted chat
    std::vector<char> chat_buf(estimated_size);
    
    // Apply the template
    int32_t required_size;
    
    if (template_name_cstr == nullptr) {
      // Use the model's default template
      const char* default_template = llama_model_chat_template(model_, nullptr);
      if (default_template) {
        debug_info += ", using model's default template";
        required_size = llama_chat_apply_template(
                        default_template,
                        llama_messages.data(),
                        llama_messages.size(),
                        add_generation_prompt,
                        chat_buf.data(),
                        chat_buf.size());
      } else {
        // If the model doesn't have a built-in template, try a default one
        debug_info += ", model has no default template, trying chatml fallback";
        const char* fallback_template = llama_model_chat_template(model_, "chatml");
        debug_info += fallback_template ? ", found chatml template" : ", chatml template not found, using empty template";
        required_size = llama_chat_apply_template(
                        fallback_template ? fallback_template : "", 
                        llama_messages.data(),
                        llama_messages.size(),
                        add_generation_prompt,
                        chat_buf.data(),
                        chat_buf.size());
      }
    } else {
      // Use the specified template
      const char* specified_template = llama_model_chat_template(model_, template_name_cstr);
      if (specified_template) {
        debug_info += ", using specified template: " + std::string(template_name_cstr);
        required_size = llama_chat_apply_template(
                        specified_template,
                        llama_messages.data(),
                        llama_messages.size(),
                        add_generation_prompt,
                        chat_buf.data(),
                        chat_buf.size());
      } else {
        // Template name was specified but not found
        debug_info += ", specified template '" + std::string(template_name_cstr) + "' not found";
        throw std::runtime_error("Specified chat template '" + std::string(template_name_cstr) + "' not found");
      }
    }
    
    if (required_size < 0) {
      throw std::runtime_error("Failed to apply chat template: error code " + std::to_string(required_size) + ". " + debug_info);
    }
    
    // If the buffer wasn't big enough, resize and try again
    if (static_cast<size_t>(required_size) > chat_buf.size()) {
      chat_buf.resize(required_size + 1); // +1 for null terminator
      debug_info += ", buffer resized to " + std::to_string(required_size + 1) + " bytes";
      
      if (template_name_cstr == nullptr) {
        // Use the model's default template
        const char* default_template = llama_model_chat_template(model_, nullptr);
        if (default_template) {
          required_size = llama_chat_apply_template(
                          default_template,
                          llama_messages.data(),
                          llama_messages.size(),
                          add_generation_prompt,
                          chat_buf.data(),
                          chat_buf.size());
        } else {
          // If the model doesn't have a built-in template, try a default one
          const char* fallback_template = llama_model_chat_template(model_, "chatml");
          required_size = llama_chat_apply_template(
                          fallback_template ? fallback_template : "", 
                          llama_messages.data(),
                          llama_messages.size(),
                          add_generation_prompt,
                          chat_buf.data(),
                          chat_buf.size());
        }
      } else {
        // Use the specified template
        const char* specified_template = llama_model_chat_template(model_, template_name_cstr);
        if (specified_template) {
          required_size = llama_chat_apply_template(
                          specified_template,
                          llama_messages.data(),
                          llama_messages.size(),
                          add_generation_prompt,
                          chat_buf.data(),
                          chat_buf.size());
        } else {
          // Template name was specified but not found
          throw std::runtime_error("Specified chat template '" + std::string(template_name_cstr) + "' not found");
        }
      }
      
      if (required_size < 0) {
        throw std::runtime_error("Failed to apply chat template with resized buffer");
      }
    }
    
    // Convert to string
    input_text = std::string(chat_buf.data(), required_size);
  } else {
    throw std::runtime_error("Either prompt or messages must be provided");
  }
  
  // Create batch for the input tokens
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  std::vector<llama_token> tokens(input_text.size() + 4);
  
  int n_tokens = llama_tokenize(vocab, input_text.c_str(), input_text.length(), tokens.data(), tokens.size(), true, true);
  
  if (n_tokens < 0) {
    throw std::runtime_error("Failed to tokenize input: error code " + std::to_string(n_tokens) + 
                             ". Input text length: " + std::to_string(input_text.length()));
  }
  
  // Resize tokens vector to actual size
  tokens.resize(n_tokens);
  
  // Track timings
  auto start_time = std::chrono::high_resolution_clock::now();
  auto prompt_eval_start = start_time;
  
  // Feed the prompt to the model - following llama.cpp examples
  int n_past = 0;
  
  // Create a proper batch following the examples pattern
  llama_batch batch = llama_batch_init(n_tokens, 0, 1);
  
  // Add all tokens to the batch
  for (int i = 0; i < n_tokens; i++) {
    batch.token[i] = tokens[i];
    batch.pos[i] = i;
    batch.seq_id[i][0] = 0;  // Use sequence 0
    batch.logits[i] = false; // Only need logits for the last token
  }
  
  // Only set logits for the last token as shown in the examples
  if (n_tokens > 0) {
    batch.logits[n_tokens - 1] = true;
  }
  
  bool decode_success = false;
  std::string debug_info = "Initial batch: " + std::to_string(n_tokens) + " tokens";
  
  try {
    // Try full batch decode first, following the examples
    if (llama_decode(ctx_, batch) == 0) {
      decode_success = true;
      debug_info += " | Full batch decode succeeded";
    }
  } catch (...) {
    // Catch any C++ exceptions
    debug_info += " | Exception during full batch decode";
  }
  
  // Free the initial batch before trying fallback
  llama_batch_free(batch);
  
  // If full batch decode failed, try token-by-token fallback
  if (!decode_success) {
    // Reset context state to be safe
    llama_kv_cache_clear(ctx_);
    debug_info += " | Trying token-by-token approach";
    
    // Try token-by-token approach instead (for robustness)
    for (int i = 0; i < n_tokens; i++) {
      llama_batch single_batch = llama_batch_init(1, 0, 1);
      single_batch.token[0] = tokens[i];
      single_batch.pos[0] = i;
      single_batch.seq_id[0][0] = 0;
      single_batch.logits[0] = (i == n_tokens - 1); // Only get logits for last token
      
      if (llama_decode(ctx_, single_batch) != 0) {
        llama_batch_free(single_batch);
        throw std::runtime_error("Failed to decode prompt (token-by-token fallback also failed). Number of tokens: " + 
                                std::to_string(n_tokens) + " | Failed at token " + std::to_string(i) + " | " + debug_info);
      }
      
      llama_batch_free(single_batch);
    }
    
    // If we get here, token-by-token approach worked
    decode_success = true;
    debug_info += " | Token-by-token approach succeeded";
  }
  
  if (!decode_success) {
    throw std::runtime_error("Failed to decode prompt. Number of tokens: " + std::to_string(n_tokens) + " | " + debug_info);
  }
  
  n_past = n_tokens;
  
  auto prompt_eval_end = std::chrono::high_resolution_clock::now();
  auto generation_start = prompt_eval_end;
  
  // Setup for generation
  std::string result_text;
  bool completed = false;
  int n_predict = 0;
  llama_token id = 0;
  
  // Main generation loop
  while (!completed && (max_tokens < 0 || n_predict < max_tokens)) {
    // Check if we should stop completion
    if (should_stop_completion_) {
      should_stop_completion_ = false;
      break;
    }
    
    // Sample next token using logits from last eval
    const float* logits = llama_get_logits(ctx_);
    const int n_vocab = llama_vocab_n_tokens(vocab);

    // Simple temperature/top-p/top-k sampling implementation
    std::vector<std::pair<float, llama_token>> candidates;
    candidates.reserve(n_vocab);
    
    // Create candidates
    for (int i = 0; i < n_vocab; i++) {
      candidates.push_back(std::make_pair(logits[i], i));
    }
    
    // Apply temperature if specified
    if (temperature > 0.0f) {
      for (auto& candidate : candidates) {
        candidate.first /= temperature;
      }
    }
    
    // Sort by logits in descending order
    std::sort(candidates.begin(), candidates.end(), [](const auto& a, const auto& b) {
      return a.first > b.first;
    });
    
    // Apply top-k if specified
    if (top_k > 0 && top_k < (int)candidates.size()) {
      candidates.resize(top_k);
    }
    
    // Apply softmax
    {
      float max_logit = candidates[0].first;
      float sum = 0.0f;
      
      for (auto& candidate : candidates) {
        candidate.first = std::exp(candidate.first - max_logit);
        sum += candidate.first;
      }
      
      for (auto& candidate : candidates) {
        candidate.first /= sum;
      }
    }
    
    // Apply top-p if specified (cumulative probability)
    if (top_p > 0.0f && top_p < 1.0f) {
      float cumulative_prob = 0.0f;
      
      for (size_t i = 0; i < candidates.size(); i++) {
        cumulative_prob += candidates[i].first;
        
        if (cumulative_prob >= top_p) {
          candidates.resize(i + 1);
          break;
        }
      }
      
      // Renormalize probabilities
      float sum = 0.0f;
      for (auto& candidate : candidates) {
        sum += candidate.first;
      }
      
      for (auto& candidate : candidates) {
        candidate.first /= sum;
      }
    }
    
    // Sample from the distribution
    float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
    float cdf = 0.0f;
    
    id = candidates[0].second; // In case we don't sample anything
    for (const auto& candidate : candidates) {
      cdf += candidate.first;
      if (r < cdf) {
        id = candidate.second;
        break;
      }
    }
    
    // Check for end condition
    if (id == llama_token_eos(vocab)) {
      completed = true;
      break;
    }
    
    // Check for stop sequences (simple implementation)
    bool is_stop_sequence = false;
    
    // Convert token to string
    char token_str[8];
    int token_str_len = llama_token_to_piece(vocab, id, token_str, sizeof(token_str) - 1, 0, true);
    if (token_str_len < 0) {
      token_str_len = 0;
    }
    token_str[token_str_len] = '\0';
    
    // Append to result text
    result_text += token_str;
    
    // Check stop sequences after appending
    for (const auto& stop_seq : stop_sequences) {
      if (result_text.size() >= stop_seq.size() && 
          result_text.substr(result_text.size() - stop_seq.size()) == stop_seq) {
        is_stop_sequence = true;
        // Remove the stop sequence from the result
        result_text = result_text.substr(0, result_text.size() - stop_seq.size());
        break;
      }
    }
    
    if (is_stop_sequence) {
      completed = true;
      break;
    }
    
    // Call the partial callback if provided
    if (partialCallback && rt) {
      // Pass the token to the callback
      partialCallback(*rt, token_str);
    }
    
    n_predict++;
    
    // Prepare next batch - following the examples style
    llama_batch next_batch = llama_batch_init(1, 0, 1);
    next_batch.token[0] = id;
    next_batch.pos[0] = n_past;
    next_batch.seq_id[0][0] = 0;
    next_batch.logits[0] = true;  // We need logits for the next token
    
    // Decode next token
    if (llama_decode(ctx_, next_batch) != 0) {
      llama_batch_free(next_batch);
      break;
    }
    
    // Free batch resources
    llama_batch_free(next_batch);
    
    // Advance context
    n_past += 1;
  }
  
  return result_text;
}

// JSI wrapper functions
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

jsi::Value LlamaCppModel::completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(rt, "completion requires an options object");
  }
  
  jsi::Object options = args[0].getObject(rt);
  
  try {
    // Process input - either prompt or messages
    std::string prompt;
    std::vector<ChatMessage> chat_messages;
    
    if (options.hasProperty(rt, "prompt") && options.getProperty(rt, "prompt").isString()) {
      prompt = options.getProperty(rt, "prompt").getString(rt).utf8(rt);
    } else if (options.hasProperty(rt, "messages") && options.getProperty(rt, "messages").isObject()) {
      // Get messages array
      jsi::Array messages = options.getProperty(rt, "messages").getObject(rt).asArray(rt);
      
      // Convert to chat messages
      for (size_t i = 0; i < messages.size(rt); i++) {
        jsi::Object msg = messages.getValueAtIndex(rt, i).getObject(rt);
        
        ChatMessage chat_msg;
        if (msg.hasProperty(rt, "role")) {
          chat_msg.role = msg.getProperty(rt, "role").getString(rt).utf8(rt);
        }
        
        if (msg.hasProperty(rt, "content")) {
          chat_msg.content = msg.getProperty(rt, "content").getString(rt).utf8(rt);
        }
        
        if (msg.hasProperty(rt, "name")) {
          chat_msg.name = msg.getProperty(rt, "name").getString(rt).utf8(rt);
        }
        
        chat_messages.push_back(chat_msg);
      }
    } else {
      throw jsi::JSError(rt, "completion requires either 'prompt' or 'messages'");
    }
    
    // Extract parameters
    float temperature = 0.8f;
    float top_p = 0.9f;
    int top_k = 40;
    int max_tokens = -1;
    std::vector<std::string> stop_sequences;
    std::string template_name = "";
    bool jinja = false;
    std::string tool_choice = "";
    std::vector<Tool> tools;
    
    // Get temperature 
    if (options.hasProperty(rt, "temperature")) {
      temperature = (float)options.getProperty(rt, "temperature").asNumber();
    }
    
    // Get top_p
    if (options.hasProperty(rt, "top_p")) {
      top_p = (float)options.getProperty(rt, "top_p").asNumber();
    }
    
    // Get top_k
    if (options.hasProperty(rt, "top_k")) {
      top_k = (int)options.getProperty(rt, "top_k").asNumber();
    }
    
    // Get token limit
    if (options.hasProperty(rt, "max_tokens")) {
      max_tokens = (int)options.getProperty(rt, "max_tokens").asNumber();
    } else if (options.hasProperty(rt, "n_predict")) {
      max_tokens = (int)options.getProperty(rt, "n_predict").asNumber();
    }
    
    // Get stop sequences
    if (options.hasProperty(rt, "stop") && options.getProperty(rt, "stop").isObject()) {
      jsi::Array stopArray = options.getProperty(rt, "stop").getObject(rt).asArray(rt);
      
      for (size_t i = 0; i < stopArray.size(rt); i++) {
        std::string stop_str = stopArray.getValueAtIndex(rt, i).getString(rt).utf8(rt);
        stop_sequences.push_back(stop_str);
      }
    }
    
    // Get chat template name (if specified)
    if (options.hasProperty(rt, "chat_template") && options.getProperty(rt, "chat_template").isString()) {
      template_name = options.getProperty(rt, "chat_template").getString(rt).utf8(rt);
    }
    
    // Get jinja flag
    if (options.hasProperty(rt, "jinja") && options.getProperty(rt, "jinja").isBool()) {
      jinja = options.getProperty(rt, "jinja").getBool();
    }
    
    // Get tool choice
    if (options.hasProperty(rt, "tool_choice") && options.getProperty(rt, "tool_choice").isString()) {
      tool_choice = options.getProperty(rt, "tool_choice").getString(rt).utf8(rt);
    }
    
    // Get tools
    if (options.hasProperty(rt, "tools") && options.getProperty(rt, "tools").isObject()) {
      jsi::Array toolsArray = options.getProperty(rt, "tools").getObject(rt).asArray(rt);
      
      for (size_t i = 0; i < toolsArray.size(rt); i++) {
        jsi::Object jsiTool = toolsArray.getValueAtIndex(rt, i).getObject(rt);
        tools.push_back(convertJsiToolToTool(rt, jsiTool));
      }
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
    
    // Record start time
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Get completion
    std::string result_text;
    try {
      result_text = completion(prompt, chat_messages, temperature, top_p, top_k, max_tokens, stop_sequences, template_name, jinja, tool_choice, tools, partialCallback, &rt);
    } catch (const std::exception& e) {
      // Create a detailed error object
      jsi::Object errorObj(rt);
      errorObj.setProperty(rt, "message", jsi::String::createFromUtf8(rt, e.what()));
      
      // Add detailed diagnostics
      jsi::Object diagObj(rt);
      diagObj.setProperty(rt, "messages_count", jsi::Value((double)chat_messages.size()));
      
      // Include message roles
      jsi::Array roles(rt, chat_messages.size());
      for (size_t i = 0; i < chat_messages.size(); i++) {
        roles.setValueAtIndex(rt, i, jsi::String::createFromUtf8(rt, chat_messages[i].role));
      }
      diagObj.setProperty(rt, "message_roles", roles);
      
      // Include parameter info
      diagObj.setProperty(rt, "temperature", jsi::Value(temperature));
      diagObj.setProperty(rt, "top_p", jsi::Value(top_p));
      diagObj.setProperty(rt, "top_k", jsi::Value(top_k));
      diagObj.setProperty(rt, "max_tokens", jsi::Value(max_tokens));
      
      if (!template_name.empty()) {
        diagObj.setProperty(rt, "template_name", jsi::String::createFromUtf8(rt, template_name));
      }
      
      errorObj.setProperty(rt, "diagnostics", diagObj);
      
      throw jsi::JSError(rt, std::move(errorObj));
    }
    
    // Calculate time taken
    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Create result object
    jsi::Object result_obj(rt);
    result_obj.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result_text));
    
    // Check for tool calls
    std::vector<ToolCall> toolCalls = parseToolCalls(result_text);
    if (!toolCalls.empty()) {
      jsi::Array toolCallsArray(rt, toolCalls.size());
      for (size_t i = 0; i < toolCalls.size(); i++) {
        toolCallsArray.setValueAtIndex(rt, i, convertToolCallToJsiObject(rt, toolCalls[i]));
      }
      result_obj.setProperty(rt, "tool_calls", toolCallsArray);
    }
    
    // Add basic timing info
    jsi::Object timings(rt);
    timings.setProperty(rt, "total_ms", jsi::Value(total_ms));
    timings.setProperty(rt, "prompt_n", jsi::Value(0));  // Placeholder
    timings.setProperty(rt, "prompt_ms", jsi::Value(0)); // Placeholder
    timings.setProperty(rt, "predicted_n", jsi::Value(0));  // Placeholder
    timings.setProperty(rt, "predicted_ms", jsi::Value(0)); // Placeholder
    
    result_obj.setProperty(rt, "timings", timings);
    result_obj.setProperty(rt, "tokens_predicted", jsi::Value(0)); // Placeholder
    
    return result_obj;
  } catch (const jsi::JSError& e) {
    // Re-throw JSError objects without wrapping
    throw e;
  } catch (const std::exception& e) {
    // Create a basic error message for other exceptions
    throw jsi::JSError(rt, "Completion error: " + std::string(e.what()));
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

jsi::Value LlamaCppModel::detectTemplateJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  if (count < 1 || !args[0].isObject()) {
    throw jsi::JSError(rt, "detectTemplate requires a messages array");
  }
  
  try {
    if (!model_) {
      return jsi::String::createFromUtf8(rt, "chatml"); // Default if no model is loaded
    }
    
    // First, check if model has a built-in template
    const char* default_template = llama_model_chat_template(model_, nullptr);
    if (default_template) {
      // Model has a default template, return a special identifier
      return jsi::String::createFromUtf8(rt, "auto");
    }
    
    // If no built-in template is available, return the default
    return jsi::String::createFromUtf8(rt, "chatml");
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

// Function to get all built-in chat templates
jsi::Value LlamaCppModel::getBuiltinTemplatesJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  try {
    // Use llama.cpp's API to get built-in templates
    const int max_templates = 50;
    const char* templates[max_templates] = {nullptr};
    
    int n_templates = llama_chat_builtin_templates(templates, max_templates);
    
    // Create array to return the template names
    jsi::Array result(rt, n_templates);
    for (int i = 0; i < n_templates; i++) {
      if (templates[i]) {
        result.setValueAtIndex(rt, i, jsi::String::createFromUtf8(rt, templates[i]));
      } else {
        result.setValueAtIndex(rt, i, jsi::String::createFromUtf8(rt, ""));
      }
    }
    
    return result;
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

// Helper to convert JSI tool objects to our Tool structure
Tool LlamaCppModel::convertJsiToolToTool(jsi::Runtime& rt, const jsi::Object& jsiTool) {
  Tool tool;
  
  // Get the type
  if (jsiTool.hasProperty(rt, "type")) {
    tool.type = jsiTool.getProperty(rt, "type").asString(rt).utf8(rt);
  } else {
    tool.type = "function"; // Default type
  }
  
  // Get the function
  if (jsiTool.hasProperty(rt, "function") && jsiTool.getProperty(rt, "function").isObject()) {
    jsi::Object funcObj = jsiTool.getProperty(rt, "function").asObject(rt);
    
    // Get the name
    if (funcObj.hasProperty(rt, "name")) {
      tool.function.name = funcObj.getProperty(rt, "name").asString(rt).utf8(rt);
    }
    
    // Get the description
    if (funcObj.hasProperty(rt, "description")) {
      tool.function.description = funcObj.getProperty(rt, "description").asString(rt).utf8(rt);
    }
    
    // Get the parameters
    if (funcObj.hasProperty(rt, "parameters") && funcObj.getProperty(rt, "parameters").isObject()) {
      jsi::Object paramsObj = funcObj.getProperty(rt, "parameters").asObject(rt);
      
      // For simplicity, we'll only process the first level of parameters
      if (paramsObj.hasProperty(rt, "properties") && paramsObj.getProperty(rt, "properties").isObject()) {
        jsi::Object propsObj = paramsObj.getProperty(rt, "properties").asObject(rt);
        
        // Get required array if it exists
        std::vector<std::string> requiredProps;
        if (paramsObj.hasProperty(rt, "required")) {
          jsi::Value reqValue = paramsObj.getProperty(rt, "required");
          if (reqValue.isObject() && reqValue.getObject(rt).isArray(rt)) {
            jsi::Array reqArray = reqValue.getObject(rt).asArray(rt);
            for (size_t i = 0; i < reqArray.size(rt); i++) {
              requiredProps.push_back(reqArray.getValueAtIndex(rt, i).asString(rt).utf8(rt));
            }
          }
        }
        
        // Get property names (non-standard but works for our case)
        jsi::Array propNames = propsObj.getPropertyNames(rt);
        for (size_t i = 0; i < propNames.size(rt); i++) {
          std::string propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
          
          if (propsObj.hasProperty(rt, propName.c_str()) && propsObj.getProperty(rt, propName.c_str()).isObject()) {
            jsi::Object propObj = propsObj.getProperty(rt, propName.c_str()).asObject(rt);
            
            FunctionParameter param;
            param.name = propName;
            
            // Get type
            if (propObj.hasProperty(rt, "type")) {
              param.type = propObj.getProperty(rt, "type").asString(rt).utf8(rt);
            }
            
            // Get description
            if (propObj.hasProperty(rt, "description")) {
              param.description = propObj.getProperty(rt, "description").asString(rt).utf8(rt);
            }
            
            // Check if required
            param.required = std::find(requiredProps.begin(), requiredProps.end(), propName) != requiredProps.end();
            
            tool.function.parameters.push_back(param);
          }
        }
      }
    }
  }
  
  return tool;
}

// Helper to convert our ToolCall to JSI object
jsi::Object LlamaCppModel::convertToolCallToJsiObject(jsi::Runtime& rt, const ToolCall& toolCall) {
  jsi::Object jsiToolCall(rt);
  
  jsiToolCall.setProperty(rt, "id", jsi::String::createFromUtf8(rt, toolCall.id));
  jsiToolCall.setProperty(rt, "type", jsi::String::createFromUtf8(rt, toolCall.type));
  
  jsi::Object function(rt);
  function.setProperty(rt, "name", jsi::String::createFromUtf8(rt, toolCall.name));
  function.setProperty(rt, "arguments", jsi::String::createFromUtf8(rt, toolCall.arguments));
  
  jsiToolCall.setProperty(rt, "function", function);
  
  return jsiToolCall;
}

// Helper to parse tool calls from model output
std::vector<ToolCall> LlamaCppModel::parseToolCalls(const std::string& text) {
  std::vector<ToolCall> toolCalls;
  
  // Basic implementation for extracting tool calls from model output
  // In a production environment, you'd want a more robust parser
  
  // Look for JSON blocks that might contain tool calls
  size_t pos = 0;
  while ((pos = text.find("```json", pos)) != std::string::npos) {
    // Found a JSON code block
    size_t start = text.find("{", pos);
    if (start == std::string::npos) {
      pos += 7; // Move past ```json
      continue;
    }
    
    // Find the closing code block
    size_t end = text.find("```", start);
    if (end == std::string::npos) {
      // Try to find the closing brace if no code block end marker
      int depth = 0;
      for (size_t i = start; i < text.length(); i++) {
        if (text[i] == '{') depth++;
        else if (text[i] == '}') {
          depth--;
          if (depth == 0) {
            end = i + 1;
            break;
          }
        }
      }
      
      if (end == std::string::npos) {
        pos = start + 1;
        continue;
      }
    } else {
      // Adjust end to point to the last character of the JSON
      size_t jsonEnd = text.rfind("}", end);
      if (jsonEnd != std::string::npos && jsonEnd > start) {
        end = jsonEnd + 1;
      } else {
        pos = end + 3;
        continue;
      }
    }
    
    // Extract the JSON content
    std::string jsonContent = text.substr(start, end - start);
    
    // Check if it contains "name" and "arguments" properties
    if (jsonContent.find("\"name\"") != std::string::npos && 
        jsonContent.find("\"arguments\"") != std::string::npos) {
      
      // Create a tool call
      ToolCall toolCall;
      toolCall.id = "call_" + std::to_string(toolCalls.size() + 1);
      toolCall.type = "function";
      
      // Extract function name
      size_t nameStart = jsonContent.find("\"name\"");
      size_t nameValueStart = jsonContent.find("\"", nameStart + 7) + 1;
      size_t nameValueEnd = jsonContent.find("\"", nameValueStart);
      
      if (nameValueStart != std::string::npos && nameValueEnd != std::string::npos) {
        toolCall.name = jsonContent.substr(nameValueStart, nameValueEnd - nameValueStart);
        
        // Extract arguments - this part is tricky because arguments could be a complex JSON
        size_t argsStart = jsonContent.find("\"arguments\"");
        if (argsStart != std::string::npos) {
          // Check if arguments is a JSON object
          size_t argsValueStart = jsonContent.find("{", argsStart);
          if (argsValueStart != std::string::npos) {
            // Find the matching closing brace for the arguments object
            int depth = 1;
            size_t argsValueEnd = argsValueStart + 1;
            
            while (depth > 0 && argsValueEnd < jsonContent.length()) {
              if (jsonContent[argsValueEnd] == '{') depth++;
              else if (jsonContent[argsValueEnd] == '}') depth--;
              argsValueEnd++;
            }
            
            if (depth == 0) {
              toolCall.arguments = jsonContent.substr(argsValueStart, argsValueEnd - argsValueStart);
            }
          } else {
            // Try for string arguments (surrounded by quotes)
            argsValueStart = jsonContent.find("\"", argsStart + 12) + 1;
            size_t argsValueEnd = jsonContent.find("\"", argsValueStart);
            
            if (argsValueStart != std::string::npos && argsValueEnd != std::string::npos) {
              toolCall.arguments = jsonContent.substr(argsValueStart, argsValueEnd - argsValueStart);
            }
          }
        }
        
        // Only add if we have a valid name and arguments
        if (!toolCall.name.empty()) {
          // Initialize empty arguments if not set
          if (toolCall.arguments.empty()) {
            toolCall.arguments = "{}";
          }
          toolCalls.push_back(toolCall);
        }
      }
    }
    
    pos = end;
  }
  
  // If no tool calls found in code blocks, try the simple approach
  if (toolCalls.empty()) {
    pos = 0;
    while ((pos = text.find("\"name\":", pos)) != std::string::npos) {
      // Found a potential tool call
      size_t start = text.rfind("{", pos);
      if (start == std::string::npos) {
        pos += 7; // Move past "name":
        continue;
      }
      
      // Look for a closing brace
      int depth = 1;
      size_t end = start + 1;
      
      while (depth > 0 && end < text.length()) {
        if (text[end] == '{') depth++;
        else if (text[end] == '}') depth--;
        end++;
      }
      
      if (depth != 0) {
        pos += 7;
        continue;
      }
      
      // Extract the tool call JSON
      std::string toolCallJson = text.substr(start, end - start);
      
      // Create a basic tool call
      ToolCall toolCall;
      toolCall.id = "call_" + std::to_string(toolCalls.size() + 1);
      toolCall.type = "function";
      
      // Extract function name
      size_t nameStart = toolCallJson.find("\"name\":");
      size_t nameValueStart = toolCallJson.find("\"", nameStart + 7) + 1;
      size_t nameValueEnd = toolCallJson.find("\"", nameValueStart);
      
      if (nameValueStart != std::string::npos && nameValueEnd != std::string::npos) {
        toolCall.name = toolCallJson.substr(nameValueStart, nameValueEnd - nameValueStart);
        
        // Extract arguments
        size_t argsStart = toolCallJson.find("\"arguments\":");
        if (argsStart != std::string::npos) {
          // Check if arguments is a JSON object
          size_t argsValueStart = toolCallJson.find("{", argsStart + 12);
          if (argsValueStart != std::string::npos) {
            // Find the matching closing brace for the arguments object
            int depth = 1;
            size_t argsValueEnd = argsValueStart + 1;
            
            while (depth > 0 && argsValueEnd < toolCallJson.length()) {
              if (toolCallJson[argsValueEnd] == '{') depth++;
              else if (toolCallJson[argsValueEnd] == '}') depth--;
              argsValueEnd++;
            }
            
            if (depth == 0) {
              toolCall.arguments = toolCallJson.substr(argsValueStart, argsValueEnd - argsValueStart);
            }
          } else {
            // Try for string arguments (surrounded by quotes)
            argsValueStart = toolCallJson.find("\"", argsStart + 12) + 1;
            size_t argsValueEnd = toolCallJson.find("\"", argsValueStart);
            
            if (argsValueStart != std::string::npos && argsValueEnd != std::string::npos) {
              toolCall.arguments = toolCallJson.substr(argsValueStart, argsValueEnd - argsValueStart);
            }
          }
        }
        
        // Only add if we have a valid name
        if (!toolCall.name.empty()) {
          // Initialize empty arguments if not set
          if (toolCall.arguments.empty()) {
            toolCall.arguments = "{}";
          }
          toolCalls.push_back(toolCall);
        }
      }
      
      pos = end;
    }
  }
  
  return toolCalls;
}

// Function to get the content of a specific template
jsi::Value LlamaCppModel::getTemplateContentJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  try {
    if (!model_) {
      throw jsi::JSError(rt, "Model not loaded");
    }
    
    // Get template name from args or use null for default
    const char* template_name = nullptr;
    if (count > 0 && args[0].isString()) {
      std::string name = args[0].getString(rt).utf8(rt);
      if (name != "auto" && name != "default") {
        template_name = name.c_str();
      }
    }
    
    // Get the template content
    const char* template_content = llama_model_chat_template(model_, template_name);
    
    if (template_content) {
      return jsi::String::createFromUtf8(rt, template_content);
    } else {
      return jsi::Value::null();
    }
  } catch (const std::exception& e) {
    throw jsi::JSError(rt, e.what());
  }
}

} // namespace facebook::react 