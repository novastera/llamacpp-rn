#include "LlamaCppModel.h"
#include <chrono>
#include <stdexcept>

// Include llama.cpp headers directly
#include "llama.h"

namespace facebook::react {

LlamaCppModel::LlamaCppModel(llama_model* model, llama_context* ctx)
    : model_(model), ctx_(ctx), should_stop_completion_(false), is_predicting_(false) {
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
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), false, false);
  
  if (n_tokens < 0) {
    n_tokens = 0; // Handle error case
  }
  
  // Add special tokens if needed
  std::vector<llama_token> tokens_with_special;
  llama_token bos_token = llama_token_bos(vocab);
  
  // Add BOS token if it's not already there
  if (n_tokens > 0 && tokens[0] != bos_token) {
    tokens_with_special.push_back(bos_token);
  }
  
  // Add the regular tokens
  for (int i = 0; i < n_tokens; i++) {
    tokens_with_special.push_back(tokens[i]);
  }
  
  return std::vector<int32_t>(tokens_with_special.begin(), tokens_with_special.end());
}

std::vector<float> LlamaCppModel::embedding(const std::string& text) {
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  // Tokenize the text
  std::vector<llama_token> tokens(text.size() + 4);
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), false, false);
  
  if (n_tokens < 0) {
    n_tokens = 0; // Handle error case
  }
  
  // Add special tokens if needed
  std::vector<llama_token> tokens_with_special;
  llama_token bos_token = llama_token_bos(vocab);
  
  // Add BOS token if it's not already there
  if (n_tokens > 0 && tokens[0] != bos_token) {
    tokens_with_special.push_back(bos_token);
  }
  
  // Add the regular tokens
  for (int i = 0; i < n_tokens; i++) {
    tokens_with_special.push_back(tokens[i]);
  }
  
  // Update tokens and count for processing
  tokens = std::move(tokens_with_special);
  n_tokens = tokens.size();
  
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

// Then modify the function signature
CompletionResult LlamaCppModel::completion(const std::string& prompt, 
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
  
  // Start timing
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Create result to return
  CompletionResult result;
  result.text = "";
  
  // Clear the KV cache if needed
  bool is_first = llama_kv_self_used_cells(ctx_) == 0;
  
  std::string input_text;
  
  if (!prompt.empty()) {
    // Use prompt directly if provided
    input_text = prompt;
  } else if (!messages.empty()) {
    // Format chat messages with official template
    std::vector<llama_chat_message> llama_messages;
    
    // Convert messages to llama.cpp format
    for (const auto& msg : messages) {
      llama_chat_message lcm;
      lcm.role = strdup(msg.role.c_str());
      lcm.content = strdup(msg.content.c_str());
      llama_messages.push_back(lcm);
    }
    
    // Get the template to use - use model's default if none specified
    const char* tmpl = nullptr;
    if (!template_name.empty()) {
      tmpl = llama_model_chat_template(model_, template_name.c_str());
    } else {
      tmpl = llama_model_chat_template(model_, nullptr);
    }
    
    // Allocate a buffer that can hold the formatted template
    std::vector<char> formatted(llama_n_ctx(ctx_) * 4); // Plenty of space
    
    // Apply the template and format the messages
    int n_buf = llama_chat_apply_template(tmpl, llama_messages.data(), llama_messages.size(), true, formatted.data(), formatted.size());
    
    // Check for errors
    if (n_buf < 0) {
      // Fall back to a simple format if the template fails
      input_text = "";
      for (const auto& msg : messages) {
        if (msg.role == "system") {
          input_text += "System: " + msg.content + "\n\n";
        } else if (msg.role == "user") {
          input_text += "User: " + msg.content + "\n\n";
        } else if (msg.role == "assistant") {
          input_text += "Assistant: " + msg.content + "\n\n";
        } else {
          input_text += msg.role + ": " + msg.content + "\n\n";
        }
      }
      input_text += "Assistant:";
    } else {
      // Use the correctly formatted template
      input_text = std::string(formatted.data(), n_buf);
    }
    
    // Free allocated memory for messages
    for (auto& msg : llama_messages) {
      free(const_cast<char*>(msg.role));
      free(const_cast<char*>(msg.content));
    }
  } else {
    throw std::runtime_error("Either prompt or messages must be provided");
  }
  
  // Tokenize the input
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  
  // First get the number of tokens needed
  int n_tokens = llama_tokenize(vocab, input_text.c_str(), input_text.length(), nullptr, 0, is_first, true);
  if (n_tokens < 0) {
    n_tokens = -n_tokens; // Handle negative return value
  }
  
  // Allocate space for tokens
  std::vector<llama_token> tokens(n_tokens);
  
  // Do the actual tokenization
  n_tokens = llama_tokenize(vocab, input_text.c_str(), input_text.length(), tokens.data(), tokens.size(), is_first, true);
  if (n_tokens < 0) {
    n_tokens = -n_tokens; // Handle negative return value
  }
  
  // Prepare a batch for the prompt
  llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
  
  // Process the prompt
  if (llama_decode(ctx_, batch)) {
    throw std::runtime_error("Failed to decode prompt");
  }
  
  // Record prompt processing time
  auto prompt_end_time = std::chrono::high_resolution_clock::now();
  
  // Initialize token generation sampler
  struct llama_sampler * sampler = nullptr;
  
  // Create the sampler chain
  sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
  
  // Add sampling methods
  if (top_k > 0) {
    llama_sampler_chain_add(sampler, llama_sampler_init_top_k(top_k));
  }
  
  if (top_p > 0.0f && top_p < 1.0f) {
    // Add a minimum of tokens to keep to ensure we don't cut off everything
    llama_sampler_chain_add(sampler, llama_sampler_init_top_p(top_p, 1));
  }
  
  if (temperature > 0.0f) {
    llama_sampler_chain_add(sampler, llama_sampler_init_temp(temperature));
  }
  
  // Finally add the distribution sampler
  llama_sampler_chain_add(sampler, llama_sampler_init_dist(0)); // Fixed seed for now
  
  // Generate tokens
  llama_token new_token_id = 0;
  int n_ctx = llama_n_ctx(ctx_);
  int n_generated = 0;
  
  // Generate up to max_tokens tokens
  while ((n_generated < max_tokens || max_tokens <= 0) && !should_stop_completion_) {
    // Check if we have enough space in the context
    if (llama_kv_self_used_cells(ctx_) + 1 >= n_ctx) {
      break;
    }
    
    // Sample a token using our sampler
    new_token_id = llama_sampler_sample(sampler, ctx_, -1);
    
    // Check for end of generation
    if (new_token_id == llama_vocab_eos(vocab)) {
      break;
    }
    
    // Convert token to text
    char token_str[8] = {0};
    int n_chars = llama_token_to_piece(vocab, new_token_id, token_str, sizeof(token_str) - 1, false, true);
    if (n_chars < 0) n_chars = 0;
    token_str[n_chars] = '\0';
    
    // Add to result
    std::string token_text(token_str);
    result.text += token_text;
    
    // Call partial callback if provided
    if (partialCallback && rt) {
      partialCallback(*rt, token_str);
    }
    
    // Check for stop sequences
    bool should_stop = false;
    for (const auto& stop_seq : stop_sequences) {
      if (result.text.size() >= stop_seq.size() && 
          result.text.substr(result.text.size() - stop_seq.size()) == stop_seq) {
        // Remove the stop sequence from the result
        result.text = result.text.substr(0, result.text.size() - stop_seq.size());
        should_stop = true;
        break;
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
      break;
    }
    
    n_generated++;
  }
  
  // Calculate time taken
  auto end_time = std::chrono::high_resolution_clock::now();
  auto prompt_duration = std::chrono::duration<double, std::milli>(prompt_end_time - start_time).count();
  auto generation_duration = std::chrono::duration<double, std::milli>(end_time - prompt_end_time).count();
  
  // Clean up
  llama_sampler_free(sampler);
  
  // Update result
  result.prompt_tokens = n_tokens;
  result.generated_tokens = n_generated;
  result.prompt_duration_ms = prompt_duration;
  result.generation_duration_ms = generation_duration;
  
  return result;
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
  
  // Check if model is already predicting
  if (is_predicting_) {
    throw jsi::JSError(rt, "Model is currently busy with another prediction");
  }
  
  jsi::Object options = args[0].getObject(rt);
  
  try {
    // Set the predicting flag
    is_predicting_ = true;
    
    // Process input - either prompt or messages
    std::string prompt;
    std::vector<ChatMessage> chat_messages;
    
    // Get input type
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
    CompletionResult result;
    try {
      result = completion(prompt, chat_messages, temperature, top_p, top_k, max_tokens, stop_sequences, template_name, jinja, tool_choice, tools, partialCallback, &rt);
    } catch (const std::exception& e) {
      // Create a basic error message
      throw jsi::JSError(rt, std::string("Failed to complete: ") + e.what());
    }
    
    // Calculate time taken
    auto end_time = std::chrono::high_resolution_clock::now();
    double total_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Parse tool calls first and extract cleaned text
    std::string cleanedText;
    std::vector<ToolCall> toolCalls = parseToolCalls(result.text, &cleanedText);
    
    // Determine if there's actual text content or just tool calls
    bool hasToolCalls = !toolCalls.empty();
    bool hasText = !cleanedText.empty();
    
    // Create result object
    jsi::Object result_obj(rt);
    
    // Set text property (null if there's no meaningful text)
    if (hasText) {
      result_obj.setProperty(rt, "text", jsi::String::createFromUtf8(rt, cleanedText));
    } else {
      result_obj.setProperty(rt, "text", jsi::Value::null());
    }
    
    // Always include tool_calls property
    if (hasToolCalls) {
      jsi::Array toolCallsArray(rt, toolCalls.size());
      for (size_t i = 0; i < toolCalls.size(); i++) {
        toolCallsArray.setValueAtIndex(rt, i, convertToolCallToJsiObject(rt, toolCalls[i]));
      }
      result_obj.setProperty(rt, "tool_calls", toolCallsArray);
    } else {
      result_obj.setProperty(rt, "tool_calls", jsi::Value::null());
    }
    
    // Add basic timing info
    jsi::Object timings(rt);
    timings.setProperty(rt, "total_ms", jsi::Value(total_ms));
    timings.setProperty(rt, "prompt_n", jsi::Value(result.prompt_tokens));
    timings.setProperty(rt, "prompt_ms", jsi::Value(result.prompt_duration_ms));
    timings.setProperty(rt, "predicted_n", jsi::Value(result.generated_tokens));
    timings.setProperty(rt, "predicted_ms", jsi::Value(result.generation_duration_ms));
    
    result_obj.setProperty(rt, "timings", timings);
    result_obj.setProperty(rt, "tokens_predicted", jsi::Value(result.generated_tokens));
    
    // Reset prediction flag
    is_predicting_ = false;
    
    return result_obj;
  } catch (const jsi::JSError& e) {
    // Reset prediction flag
    is_predicting_ = false;
    // Re-throw JSError objects without wrapping
    throw e;
  } catch (const std::exception& e) {
    // Reset prediction flag
    is_predicting_ = false;
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
std::vector<ToolCall> LlamaCppModel::parseToolCalls(const std::string& text, std::string* remainingText) {
  std::vector<ToolCall> toolCalls;
  std::string processedText = text;
  
  // Basic implementation for extracting tool calls from model output
  // In a production environment, you'd want a more robust parser
  
  // Look for JSON blocks that might contain tool calls
  size_t pos = 0;
  bool hasTextContent = false;
  
  while ((pos = processedText.find("```json", pos)) != std::string::npos) {
    // Found a JSON code block
    size_t blockStart = pos;
    size_t start = processedText.find("{", pos);
    if (start == std::string::npos) {
      pos += 7; // Move past ```json
      continue;
    }
    
    // Find the closing code block
    size_t end = processedText.find("```", start);
    if (end == std::string::npos) {
      // Try to find the closing brace if no code block end marker
      int depth = 0;
      for (size_t i = start; i < processedText.length(); i++) {
        if (processedText[i] == '{') depth++;
        else if (processedText[i] == '}') {
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
      size_t jsonEnd = processedText.rfind("}", end);
      if (jsonEnd != std::string::npos && jsonEnd > start) {
        end = jsonEnd + 1;
      } else {
        pos = end + 3;
        continue;
      }
    }
    
    // Extract the JSON content
    std::string jsonContent = processedText.substr(start, end - start);
    
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
          
          // Remove the tool call from the text
          size_t blockEnd = processedText.find("```", end) + 3;
          if (blockEnd == std::string::npos) {
            blockEnd = end;
          }
          
          // Check if there's content before/after this tool call
          if (blockStart > 0) {
            hasTextContent = true;
          }
          
          if (blockEnd < processedText.length()) {
            hasTextContent = true;
          }
          
          // Remove this block from the processed text
          processedText.erase(blockStart, blockEnd - blockStart);
          
          // Adjust position to account for the removed text
          pos = blockStart;
          continue;
        }
      }
    }
    
    pos = end;
  }
  
  // If no tool calls found in code blocks, try the simple approach
  if (toolCalls.empty()) {
    pos = 0;
    while ((pos = processedText.find("\"name\":", pos)) != std::string::npos) {
      // Found a potential tool call
      size_t blockStart = pos;
      size_t start = processedText.rfind("{", pos);
      if (start == std::string::npos) {
        pos += 7; // Move past "name":
        continue;
      }
      
      // Look for a closing brace
      int depth = 1;
      size_t end = start + 1;
      
      while (depth > 0 && end < processedText.length()) {
        if (processedText[end] == '{') depth++;
        else if (processedText[end] == '}') depth--;
        end++;
      }
      
      if (depth != 0) {
        pos += 7;
        continue;
      }
      
      // Extract the tool call JSON
      std::string toolCallJson = processedText.substr(start, end - start);
      
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
          
          // Remove the tool call from the text
          // Check if there's content before/after this tool call
          if (start > 0) {
            hasTextContent = true;
          }
          
          if (end < processedText.length()) {
            hasTextContent = true;
          }
          
          // Remove this block from the processed text
          processedText.erase(start, end - start);
          
          // Adjust position to account for the removed text
          pos = start;
          continue;
        }
      }
      
      pos = end;
    }
  }
  
  // Clean up the remaining text
  if (remainingText != nullptr) {
    // Trim whitespace
    size_t first = processedText.find_first_not_of(" \t\n\r");
    size_t last = processedText.find_last_not_of(" \t\n\r");
    
    if (first != std::string::npos && last != std::string::npos) {
      *remainingText = processedText.substr(first, last - first + 1);
    } else {
      *remainingText = "";
    }
  }
  
  return toolCalls;
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
    
    // Tokenize the text
    std::vector<llama_token> tokens;
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // Create a buffer for tokenization
    tokens.resize(text.size() + 4);
    int n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), false, false);
    
    if (n_tokens < 0) {
      throw std::runtime_error("Tokenization failed: " + std::to_string(n_tokens));
    }
    
    // Add special tokens if needed
    std::vector<llama_token> tokens_with_special;
    llama_token bos_token = llama_token_bos(vocab);
    
    // Add BOS token if it's not already there
    if (n_tokens > 0 && tokens[0] != bos_token) {
      tokens_with_special.push_back(bos_token);
    }
    
    // Add the regular tokens
    for (int i = 0; i < n_tokens; i++) {
      tokens_with_special.push_back(tokens[i]);
    }
    
    // Update tokens and count
    tokens = std::move(tokens_with_special);
    n_tokens = tokens.size();
    
    // Create result object
    jsi::Object result(rt);
    
    // Add tokens to result
    jsi::Array tokensArray(rt, n_tokens);
    for (int i = 0; i < n_tokens; i++) {
      tokensArray.setValueAtIndex(rt, i, jsi::Value((double)tokens[i]));
    }
    result.setProperty(rt, "tokens", tokensArray);
    
    // Clear KV cache
    llama_kv_self_clear(ctx_);
    
    // Process each token individually for testing
    bool success = true;
    std::string errorMessage;
    jsi::Array processResults(rt, n_tokens);
    
    for (int i = 0; i < n_tokens; i++) {
      // Create a batch with a single token
      llama_batch batch = llama_batch_init(1, 0, 1);
      batch.token[0] = tokens[i];
      batch.pos[0] = i;
      batch.seq_id[0][0] = 0;
      batch.logits[0] = true;
      
      // Process the token and capture result
      jsi::Object tokenResult(rt);
      tokenResult.setProperty(rt, "index", jsi::Value(i));
      tokenResult.setProperty(rt, "id", jsi::Value((double)tokens[i]));
      
      // Try to get token text
      char token_str[8] = {0};
      int token_str_len = llama_token_to_piece(vocab, tokens[i], token_str, sizeof(token_str) - 1, 0, true);
      if (token_str_len > 0) {
        token_str[token_str_len] = '\0';
        tokenResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, token_str));
      } else {
        tokenResult.setProperty(rt, "text", jsi::String::createFromUtf8(rt, "<error>"));
      }
      
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
      
      // Free batch resources
      llama_batch_free(batch);
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

} // namespace facebook::react 