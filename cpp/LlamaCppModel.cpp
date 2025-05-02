#include "LlamaCppModel.h"
#include <chrono>
#include <stdexcept>

// Include llama.cpp headers directly
#include "llama.h"
#include "chat.h"
#include "json-schema-to-grammar.h"
#include "sampling.h"

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
  std::lock_guard<std::mutex> lock(mutex_);
  
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  // Check if it's the first run (empty context)
  const bool is_first = llama_kv_self_used_cells(ctx_) == 0;
  
  const llama_vocab* vocab = llama_model_get_vocab(model_);
  
  // First get the number of tokens
  int n_tokens = -llama_tokenize(vocab, text.c_str(), text.length(), nullptr, 0, is_first, true);
  if (n_tokens < 0) {
    n_tokens = -n_tokens; // Handle negative return value
  }
  
  // Allocate space for tokens
  std::vector<llama_token> tokens(n_tokens);
  
  // Do the actual tokenization
  n_tokens = llama_tokenize(vocab, text.c_str(), text.length(), tokens.data(), tokens.size(), is_first, true);
  if (n_tokens < 0) {
    n_tokens = -n_tokens; // Handle negative return value
  }
  
  // Convert to int32_t for JS compatibility
  std::vector<int32_t> result(tokens.begin(), tokens.end());
  return result;
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
  llama_token bos_token = llama_vocab_bos(vocab);
  
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

// Convert CompletionOptions to common_params_sampling
common_params_sampling LlamaCppModel::convertToSamplingParams(const CompletionOptions& options) {
    common_params_sampling params;
    
    // Set parameters from options
    params.n_prev = 64;
    params.temp = options.temperature;
    params.penalty_repeat = options.repeat_penalty;
    params.penalty_freq = options.frequency_penalty;
    params.penalty_present = options.presence_penalty;
    params.top_k = options.top_k;
    params.top_p = options.top_p;
    params.min_p = options.min_p;
    params.typ_p = options.typical_p;
    params.penalty_last_n = options.repeat_last_n;
    params.seed = options.seed; // Add seed parameter
    
    // Set sampling strategy - use a reasonable default chain
    // Order matters for samplers!
    if (options.min_p > 0.0f && options.min_p < 1.0f) {
        params.samplers.push_back(COMMON_SAMPLER_TYPE_MIN_P);
    }
    
    if (options.top_k > 0) {
        params.samplers.push_back(COMMON_SAMPLER_TYPE_TOP_K);
    }
    
    if (options.top_p > 0.0f && options.top_p < 1.0f) {
        params.samplers.push_back(COMMON_SAMPLER_TYPE_TOP_P);
    }
    
    if (options.typical_p > 0.0f && options.typical_p < 1.0f) {
        params.samplers.push_back(COMMON_SAMPLER_TYPE_TYPICAL_P);
    }
    
    // Apply frequency and presence penalties if set
    if (options.frequency_penalty != 0.0f || options.presence_penalty != 0.0f) {
        params.samplers.push_back(COMMON_SAMPLER_TYPE_PENALTIES);
    }
    
    // Temperature always comes last before distribution
    params.samplers.push_back(COMMON_SAMPLER_TYPE_TEMPERATURE);
    
    return params;
}

// Main completion method
CompletionResult LlamaCppModel::completion(const CompletionOptions& options) {
  if (!model_ || !ctx_) {
    throw std::runtime_error("Model or context not initialized");
  }
  
  // Start timing
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Create result to return
  CompletionResult result;
  result.text = "";
  result.truncated = false;
  result.finish_reason = "length"; // Default finish reason
  
  // Clear the KV cache if needed
  bool is_first = llama_kv_self_used_cells(ctx_) == 0;
  
  // Prepare the prompt using chat template
  std::string input_text;
  
  if (!options.prompt.empty()) {
    // Use prompt directly if provided
    input_text = options.prompt;
    fprintf(stderr, "Completion using prompt (first 50 chars): %.50s%s\n", 
            input_text.c_str(), input_text.length() > 50 ? "..." : "");
  } else if (!options.messages.empty()) {
    try {
      // Validate messages
      fprintf(stderr, "Completion using %zu messages\n", options.messages.size());
      for (size_t i = 0; i < options.messages.size(); i++) {
        const auto& msg = options.messages[i];
        fprintf(stderr, "  Message %zu: role='%s', content length=%zu%s\n", 
                i, msg.role.c_str(), msg.content.length(),
                !msg.name.empty() ? (", name='" + msg.name + "'").c_str() : "");
      }
      
      // Initialize the chat template using llama.cpp's chat utilities
      auto chat_templates = common_chat_templates_init(model_, "");
      if (!chat_templates) {
        throw std::runtime_error("Failed to initialize chat templates");
      }
      
      // Convert our messages to common_chat_msg format
      std::vector<common_chat_msg> common_msgs;
      for (const auto& msg : options.messages) {
        common_chat_msg common_msg;
        common_msg.role = msg.role;
        common_msg.content = msg.content;
        
        if (!msg.name.empty()) {
          common_msg.tool_name = msg.name;
        }
        
        common_msgs.push_back(common_msg);
      }
      
      // Convert tools to common_chat_tool format if provided
      std::vector<common_chat_tool> common_tools;
      if (!options.tools.empty()) {
        fprintf(stderr, "Completion with %zu tools\n", options.tools.size());
        for (const auto& tool : options.tools) {
          common_chat_tool common_tool;
          common_tool.name = tool.function.name;
          common_tool.description = tool.function.description;
          
          // Convert parameters to JSON schema string
          nlohmann::ordered_json params_obj;
          params_obj["type"] = "object";
          params_obj["properties"] = nlohmann::ordered_json::object();
          params_obj["required"] = nlohmann::ordered_json::array();
          
          for (const auto& param : tool.function.parameters) {
            nlohmann::ordered_json param_obj;
            param_obj["type"] = param.type;
            if (!param.description.empty()) {
              param_obj["description"] = param.description;
            }
            
            params_obj["properties"][param.name] = param_obj;
            
            if (param.required) {
              params_obj["required"].push_back(param.name);
            }
          }
          
          common_tool.parameters = params_obj.dump();
          fprintf(stderr, "  Tool: name='%s', parameters=%zu bytes\n", 
                  common_tool.name.c_str(), common_tool.parameters.length());
          common_tools.push_back(common_tool);
        }
      }
      
      // Set up inputs for the template
      common_chat_templates_inputs inputs;
      inputs.messages = common_msgs;
      inputs.add_generation_prompt = true;
      inputs.use_jinja = options.jinja;
      inputs.tools = common_tools;
      
      // Convert tool_choice to the enum
      if (!options.tool_choice.empty()) {
        fprintf(stderr, "Tool choice: %s\n", options.tool_choice.c_str());
        inputs.tool_choice = common_chat_tool_choice_parse_oaicompat(options.tool_choice);
      } else {
        inputs.tool_choice = COMMON_CHAT_TOOL_CHOICE_AUTO;
      }
      
      // Set grammar if provided
      if (!options.grammar.empty()) {
        fprintf(stderr, "Using grammar: %zu bytes\n", options.grammar.length());
        inputs.grammar = options.grammar;
      }
      
      // Apply the template to get the prompt and grammar
      try {
        common_chat_params params = common_chat_templates_apply(chat_templates.get(), inputs);
        input_text = params.prompt;
        
        fprintf(stderr, "Template applied successfully, prompt length=%zu\n", input_text.length());
        
        // Use the grammar from the template if we don't have one
        if (options.grammar.empty() && !params.grammar.empty()) {
          fprintf(stderr, "Using grammar from template: %zu bytes\n", params.grammar.length());
          const_cast<CompletionOptions&>(options).grammar = params.grammar;
        }
      } catch (const std::exception& e) {
        fprintf(stderr, "Error applying chat template: %s\n", e.what());
        throw std::runtime_error(std::string("Error applying chat template: ") + e.what());
      }
    } catch (const std::exception& e) {
      fprintf(stderr, "Error in chat template processing: %s\n", e.what());
      throw;
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
  
  // Convert our options to common_params_sampling
  common_params_sampling sampling_params = convertToSamplingParams(options);
  
  // Create common_sampler for token generation
  struct common_sampler* sampler = common_sampler_init(model_, sampling_params);
  
  if (!sampler) {
    throw std::runtime_error("Failed to initialize sampler");
  }
  
  // Generate tokens
  llama_token token_id = 0;
  int n_generated = 0;
  
  // Generate up to max_tokens tokens
  while ((n_generated < options.max_tokens || options.max_tokens <= 0) && !should_stop_completion_) {
    // Check if we have enough space in the context
    uint32_t n_ctx = llama_n_ctx(ctx_);
    if (llama_kv_self_used_cells(ctx_) + 1 >= n_ctx) {
      result.truncated = true;
      result.finish_reason = "context_length_exceeded";
      break;
    }
    
    // Sample a token using our common_sampler
    token_id = common_sampler_sample(sampler, ctx_, 0, true);
    
    // Check for end of generation
    if (token_id == llama_vocab_eos(vocab)) {
      result.finish_reason = "stop";
      break;
    }
    
    // Convert token to text
    char token_buf[8] = {0};
    int n_chars = llama_token_to_piece(vocab, token_id, token_buf, sizeof(token_buf) - 1, 0, true);
    if (n_chars < 0) n_chars = 0;
    token_buf[n_chars] = '\0';
    
    std::string token_text(token_buf);
    
    // Add to result
    result.text += token_text;
    
    // Call partial callback if provided
    if (options.partial_callback && options.runtime) {
      options.partial_callback(*options.runtime, token_text.c_str());
    }
    
    // Check for stop sequences
    bool should_stop = false;
    for (const auto& stop_seq : options.stop_sequences) {
      if (result.text.size() >= stop_seq.size() && 
          result.text.substr(result.text.size() - stop_seq.size()) == stop_seq) {
        // Remove the stop sequence from the result
        result.text = result.text.substr(0, result.text.size() - stop_seq.size());
        should_stop = true;
        result.finish_reason = "stop";
        break;
      }
    }
    
    if (should_stop) {
      break;
    }
    
    // Accept the token
    common_sampler_accept(sampler, token_id, true);
    
    // Prepare the batch with the new token
    batch = llama_batch_get_one(&token_id, 1);
    
    // Continue decoding with new token
    if (llama_decode(ctx_, batch)) {
      break;
    }
    
    n_generated++;
  }
  
  // Handle interrupted generation
  if (should_stop_completion_) {
    result.finish_reason = "interrupted";
  }
  
  // Calculate time taken
  auto end_time = std::chrono::high_resolution_clock::now();
  auto prompt_duration = std::chrono::duration<double, std::milli>(prompt_end_time - start_time).count();
  auto generation_duration = std::chrono::duration<double, std::milli>(end_time - prompt_end_time).count();
  auto total_duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();
  
  // Clean up
  common_sampler_free(sampler);
  
  // Parse tool calls if needed
  if (!options.tools.empty() && !result.text.empty()) {
    std::string remainingText;
    std::vector<ToolCall> toolCalls = parseToolCalls(result.text, &remainingText);
    
    // If we found tool calls, only keep the remaining text
    if (!toolCalls.empty()) {
      result.text = remainingText;
    }
  }
  
  // Update result
  result.prompt_tokens = n_tokens;
  result.generated_tokens = n_generated;
  result.prompt_duration_ms = prompt_duration;
  result.generation_duration_ms = generation_duration;
  result.total_duration_ms = total_duration;
  
  return result;
}

// Parse tool calls from generated text using llama.cpp common chat functionality
std::vector<ToolCall> LlamaCppModel::parseToolCalls(const std::string& text, std::string* remainingText) {
  std::vector<ToolCall> result;
  
  // Add debug logging
  fprintf(stderr, "parseToolCalls: Attempting to parse text: %.100s%s\n", 
          text.c_str(), text.length() > 100 ? "..." : "");
  
  try {
    // Use common_chat_parse to extract tool calls based on format
    // Try multiple formats, starting with Llama-3, falling back to more generic parsing
    common_chat_msg parsed;
    std::vector<common_chat_format> formats_to_try = {
      COMMON_CHAT_FORMAT_LLAMA_3_X,
      COMMON_CHAT_FORMAT_GENERIC,
      COMMON_CHAT_FORMAT_CONTENT_ONLY
    };
    
    bool parsing_succeeded = false;
    for (auto format : formats_to_try) {
      try {
        fprintf(stderr, "parseToolCalls: Trying format: %d\n", format);
        parsed = common_chat_parse(text, format);
        parsing_succeeded = true;
        break;
      } catch (const std::exception& e) {
        fprintf(stderr, "parseToolCalls: Format %d failed: %s\n", format, e.what());
        // Continue to next format
      }
    }
    
    if (!parsing_succeeded) {
      fprintf(stderr, "parseToolCalls: All parsing formats failed\n");
      // Fall back to returning the original text
      if (remainingText) {
        *remainingText = text;
      }
      return result;
    }
    
    // Set the remaining text if requested
    if (remainingText) {
      *remainingText = parsed.content;
    }
    
    // Convert common_chat_tool_call to our ToolCall format
    for (const auto& tc : parsed.tool_calls) {
      ToolCall toolCall;
      toolCall.id = tc.id.empty() ? "call_" + std::to_string(result.size()) : tc.id;
      toolCall.type = "function";  // Default to function type
      toolCall.name = tc.name;
      toolCall.arguments = tc.arguments;
      
      fprintf(stderr, "parseToolCalls: Found tool call - name: %s, args: %.50s%s\n", 
              toolCall.name.c_str(), 
              toolCall.arguments.c_str(), 
              toolCall.arguments.length() > 50 ? "..." : "");
      
      result.push_back(toolCall);
    }
  } catch (const std::exception& e) {
    // Log the error but don't crash
    fprintf(stderr, "Error parsing tool calls: %s\n", e.what());
    
    // Set the original text as remaining if requested
    if (remainingText) {
      *remainingText = text;
    }
  }
  
  return result;
}

// JSI method for completions
jsi::Value LlamaCppModel::completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  try {
    if (count < 1 || !args[0].isString()) {
      throw std::runtime_error("First argument (prompt) must be a string");
    }
    
    std::string prompt = args[0].asString(rt).utf8(rt);
    
    // Setup options with defaults
    CompletionOptions options;
    options.prompt = prompt;
    options.max_tokens = 128;     // Default max tokens to generate
    options.temperature = 0.8f;   // Default temperature
    options.top_p = 0.95f;        // Default top_p
    options.top_k = 40;           // Default top_k
    options.stop_sequences = {};  // Default stop words
    options.repeat_penalty = 1.1f; // Default repeat penalty
    options.seed = LLAMA_DEFAULT_SEED; // Default to random seed (-1)
    
    // Use second argument for options if provided
    if (count > 1 && args[1].isObject()) {
      jsi::Object jsOptions = args[1].getObject(rt);
      
      if (jsOptions.hasProperty(rt, "max_tokens")) {
        options.max_tokens = jsOptions.getProperty(rt, "max_tokens").asNumber();
      }
      
      if (jsOptions.hasProperty(rt, "temperature")) {
        options.temperature = jsOptions.getProperty(rt, "temperature").asNumber();
      }
      
      if (jsOptions.hasProperty(rt, "top_p")) {
        options.top_p = jsOptions.getProperty(rt, "top_p").asNumber();
      }
      
      if (jsOptions.hasProperty(rt, "top_k")) {
        options.top_k = jsOptions.getProperty(rt, "top_k").asNumber();
      }
      
      if (jsOptions.hasProperty(rt, "repeat_penalty")) {
        options.repeat_penalty = jsOptions.getProperty(rt, "repeat_penalty").asNumber();
      }
      
      // Add seed parameter
      if (jsOptions.hasProperty(rt, "seed")) {
        options.seed = jsOptions.getProperty(rt, "seed").asNumber();
      }
      
      // Handle stop words array
      if (jsOptions.hasProperty(rt, "stop")) {
        jsi::Value stop_val = jsOptions.getProperty(rt, "stop");
        if (stop_val.isObject() && stop_val.getObject(rt).isArray(rt)) {
          jsi::Array stop_arr = stop_val.getObject(rt).asArray(rt);
          for (size_t i = 0; i < stop_arr.size(rt); i++) {
            jsi::Value item = stop_arr.getValueAtIndex(rt, i);
            if (item.isString()) {
              options.stop_sequences.push_back(item.asString(rt).utf8(rt));
            }
          }
        } else if (stop_val.isString()) {
          options.stop_sequences.push_back(stop_val.asString(rt).utf8(rt));
        }
      }
    }
    
    // Get vocab for tokenization
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // Check if context is empty (first run)
    const bool is_first = llama_kv_self_used_cells(ctx_) == 0;
    
    // Tokenize prompt
    std::vector<llama_token> tokens;
    int n_tokens = -llama_tokenize(vocab, prompt.c_str(), prompt.size(), NULL, 0, is_first, true);
    if (n_tokens < 0) {
      n_tokens = -n_tokens; // Handle negative return value
    }
    tokens.resize(n_tokens);
    
    if (llama_tokenize(vocab, prompt.c_str(), prompt.size(), tokens.data(), tokens.size(), is_first, true) < 0) {
      throw std::runtime_error("Failed to tokenize prompt");
    }
    
    // Check for sufficient context space
    int n_ctx = llama_n_ctx(ctx_);
    int n_ctx_used = llama_kv_self_used_cells(ctx_);
    if (n_ctx_used + n_tokens > n_ctx) {
      throw std::runtime_error("Context size exceeded");
    }
    
    // Initialize the sampler chain
    llama_sampler* sampler = llama_sampler_chain_init(llama_sampler_chain_default_params());
    if (options.min_p > 0.0f && options.min_p < 1.0f) {
      llama_sampler_chain_add(sampler, llama_sampler_init_min_p(options.min_p, 1));
    }
    if (options.repeat_penalty > 1.0f) {
      llama_sampler_chain_add(sampler, llama_sampler_init_penalties(64, options.repeat_penalty, 0.0f, 0.0f));
    }
    if (options.temperature > 0.0f) {
      llama_sampler_chain_add(sampler, llama_sampler_init_temp(options.temperature));
    }
    llama_sampler_chain_add(sampler, llama_sampler_init_dist(options.seed));
    
    if (!sampler) {
      throw std::runtime_error("Failed to initialize sampler");
    }
    
    // Create batch for the prompt
    llama_batch batch = llama_batch_get_one(tokens.data(), tokens.size());
    
    // Decode the prompt
    if (llama_decode(ctx_, batch)) {
      llama_sampler_free(sampler);
      throw std::runtime_error("Failed to decode prompt");
    }
    
    // Generate response
    std::string completion;
    std::vector<llama_token> result_tokens;
    llama_token new_token_id;
    
    for (int i = 0; i < options.max_tokens; i++) {
      // Sample the next token
      new_token_id = llama_sampler_sample(sampler, ctx_, -1);
      
      // Is it an end of generation token?
      if (llama_vocab_is_eog(vocab, new_token_id)) {
        break;
      }
      
      // Add token to result
      result_tokens.push_back(new_token_id);
      
      // Convert token to text
      char token_buf[8] = {0};
      int n_chars = llama_token_to_piece(vocab, new_token_id, token_buf, sizeof(token_buf) - 1, 0, true);
      if (n_chars < 0) n_chars = 0;
      token_buf[n_chars] = '\0';
      
      std::string new_piece(token_buf);
      completion += new_piece;
      
      // Call partial callback if provided
      if (options.partial_callback && options.runtime) {
        options.partial_callback(*options.runtime, new_piece.c_str());
      }
      
      // Check for custom stop sequences
      bool should_stop = false;
      for (const auto& stop_seq : options.stop_sequences) {
        if (completion.find(stop_seq) != std::string::npos) {
          should_stop = true;
          break;
        }
      }
      
      if (should_stop) {
        break;
      }
      
      // Prepare batch for next token prediction
      batch = llama_batch_get_one(&new_token_id, 1);
      
      // Process the batch
      if (llama_decode(ctx_, batch)) {
        break;
      }
    }
    
    // Clean up resources
    llama_sampler_free(sampler);
    
    // Create result object
    jsi::Object result_obj(rt);
    result_obj.setProperty(rt, "text", jsi::String::createFromUtf8(rt, completion));
    
    // Create an array for token ids if needed
    jsi::Array token_array(rt, result_tokens.size());
    for (size_t i = 0; i < result_tokens.size(); i++) {
      token_array.setValueAtIndex(rt, i, jsi::Value((double)result_tokens[i]));
    }
    result_obj.setProperty(rt, "token_ids", token_array);
    
    return result_obj;
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
        
        // Get property names
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
    llama_token bos_token = llama_vocab_bos(vocab);
    
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
      char token_buf[8] = {0};
      int n_chars = llama_token_to_piece(vocab, tokens[i], token_buf, sizeof(token_buf) - 1, 0, true);
      if (n_chars < 0) n_chars = 0;
      token_buf[n_chars] = '\0';
      
      std::string token_text(token_buf);
      
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