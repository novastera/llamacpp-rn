#include "LlamaCppModel.h"
#include <chrono>
#include <stdexcept>
#include "SystemUtils.h"
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
  // Start timing
  auto start_time = std::chrono::high_resolution_clock::now();
  
  // Create result to return
  CompletionResult result;
  result.text = "";
  result.truncated = false;
  result.finish_reason = "length"; // Default finish reason
  result.prompt_tokens = 0;
  result.generated_tokens = 0;
  
  try {
    if (!model_ || !ctx_) {
      throw std::runtime_error("Model or context not initialized");
    }
    
    fprintf(stderr, "Starting completion with model at %p, context at %p\n", (void*)model_, (void*)ctx_);
    
    // Clear the KV cache if needed
    bool is_first = llama_kv_self_used_cells(ctx_) == 0;
    fprintf(stderr, "KV cache status: is_first=%d, used_cells=%zu\n", is_first, llama_kv_self_used_cells(ctx_));
    
    // Prepare the prompt 
    std::string input_text;
    
    // Set is_predicting flag
    is_predicting_ = true;
    
    if (!options.prompt.empty()) {
      // Use prompt directly if provided
      input_text = options.prompt;
      fprintf(stderr, "Using direct prompt: %.50s%s\n", 
              input_text.c_str(), input_text.length() > 50 ? "..." : "");
    } else if (!options.messages.empty()) {
      try {
        // Initialize chat template
        fprintf(stderr, "Initializing chat template with %zu messages\n", options.messages.size());
        
        auto chat_templates = common_chat_templates_init(model_, "");
        if (!chat_templates) {
          throw std::runtime_error("Failed to initialize chat templates");
        }
        
        // Convert our messages to common_chat_msg format
        std::vector<common_chat_msg> common_msgs;
        common_msgs.reserve(options.messages.size());
        
        for (const auto& msg : options.messages) {
          common_chat_msg common_msg;
          common_msg.role = msg.role;
          common_msg.content = msg.content;
          
          if (!msg.name.empty()) {
            common_msg.tool_name = msg.name;
          }
          
          common_msgs.push_back(common_msg);
        }
        
        // Convert tools if present
        std::vector<common_chat_tool> common_tools;
        if (!options.tools.empty()) {
          fprintf(stderr, "Setting up %zu tools\n", options.tools.size());
          for (const auto& tool : options.tools) {
            common_chat_tool common_tool;
            common_tool.name = tool.function.name;
            common_tool.description = tool.function.description;
            
            // Convert parameters to simple JSON schema
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
            common_tools.push_back(common_tool);
          }
        }
        
        // Set up template inputs
        common_chat_templates_inputs inputs;
        inputs.messages = common_msgs;
        inputs.add_generation_prompt = true;
        inputs.use_jinja = options.jinja;
        inputs.tools = common_tools;
        
        // Convert tool_choice
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
        
        // Apply template to get prompt and grammar
        fprintf(stderr, "Applying chat template\n");
        common_chat_params params = common_chat_templates_apply(chat_templates.get(), inputs);
        input_text = params.prompt;
        
        fprintf(stderr, "Template applied, prompt length: %zu\n", input_text.length());
        
        // Use grammar from template if needed
        if (options.grammar.empty() && !params.grammar.empty()) {
          fprintf(stderr, "Using grammar from template: %zu bytes\n", params.grammar.length());
          const_cast<CompletionOptions&>(options).grammar = params.grammar;
        }
      } catch (const std::exception& e) {
        fprintf(stderr, "Error in chat template processing: %s\n", e.what());
        throw std::runtime_error(std::string("Template error: ") + e.what());
      }
    } else {
      throw std::runtime_error("Either prompt or messages must be provided");
    }
    
    // Tokenize the input
    fprintf(stderr, "Tokenizing input text\n");
    const llama_vocab* vocab = llama_model_get_vocab(model_);
    
    // First get the number of tokens
    int n_tokens = llama_tokenize(vocab, input_text.c_str(), input_text.length(), nullptr, 0, is_first, true);
    if (n_tokens <= 0) {
      n_tokens = -n_tokens; // Handle negative return value
      fprintf(stderr, "Tokenization required negation, new count: %d\n", n_tokens);
    }
    
    fprintf(stderr, "Prompt requires %d tokens\n", n_tokens);
    
    // Allocate space for tokens
    std::vector<llama_token> tokens(n_tokens);
    
    // Do the actual tokenization
    n_tokens = llama_tokenize(vocab, input_text.c_str(), input_text.length(), tokens.data(), tokens.size(), is_first, true);
    if (n_tokens <= 0) {
      n_tokens = -n_tokens; // Handle negative return value
      fprintf(stderr, "Final tokenization required negation, final count: %d\n", n_tokens);
    }
    
    if (n_tokens == 0) {
      throw std::runtime_error("Failed to tokenize input text");
    }
    
    fprintf(stderr, "Successfully tokenized to %d tokens\n", n_tokens);
    
    // Process the prompt
    fprintf(stderr, "Creating batch for prompt tokens\n");
    llama_batch batch = llama_batch_get_one(tokens.data(), n_tokens);
    
    fprintf(stderr, "Decoding prompt tokens\n");
    if (llama_decode(ctx_, batch)) {
      throw std::runtime_error("Failed to decode prompt");
    }
    
    fprintf(stderr, "Prompt tokens processed successfully\n");
    
    // Record prompt processing time and set prompt tokens in result
    auto prompt_end_time = std::chrono::high_resolution_clock::now();
    result.prompt_tokens = n_tokens;
    
    // Set up sampling parameters
    fprintf(stderr, "Setting up sampling parameters\n");
    common_params_sampling sampling_params = convertToSamplingParams(options);
    
    // Create sampler for token generation
    fprintf(stderr, "Initializing sampler\n");
    struct common_sampler* sampler = common_sampler_init(model_, sampling_params);
    
    if (!sampler) {
      throw std::runtime_error("Failed to initialize sampler");
    }
    
    fprintf(stderr, "Starting token generation, max_tokens: %d\n", options.max_tokens);
    
    // Generate tokens
    llama_token token_id = 0;
    int n_generated = 0;
    
    // Generate up to max_tokens tokens
    while ((n_generated < options.max_tokens || options.max_tokens <= 0) && !should_stop_completion_) {
      // Safety check for array bounds and context length
      uint32_t n_ctx = llama_n_ctx(ctx_);
      if (llama_kv_self_used_cells(ctx_) + 1 >= n_ctx) {
        fprintf(stderr, "Context length exceeded, truncating\n");
        result.truncated = true;
        result.finish_reason = "context_length_exceeded";
        break;
      }
      
      // Sample next token
      fprintf(stderr, "Sampling token %d\n", n_generated + 1);
      token_id = common_sampler_sample(sampler, ctx_, 0, false);
      
      // Check for end of generation
      if (token_id == llama_vocab_eos(vocab)) {
        fprintf(stderr, "Generated EOS token, stopping\n");
        result.finish_reason = "stop";
        break;
      }
      
      // Convert token to text
      char token_buf[8] = {0};
      int n_chars = llama_token_to_piece(vocab, token_id, token_buf, sizeof(token_buf) - 1, 0, true);
      if (n_chars < 0) n_chars = 0;
      token_buf[n_chars] = '\0';
      
      std::string token_text(token_buf);
      fprintf(stderr, "Generated token: '%s'\n", token_text.c_str());
      
      // Add to result
      result.text += token_text;
      
      // Call partial callback if provided - safely
      if (options.partial_callback && options.runtime) {
        try {
          fprintf(stderr, "Calling partial callback\n");
          options.partial_callback(*options.runtime, token_text.c_str());
        } catch (const std::exception& e) {
          // Log but continue - don't fail the entire completion
          fprintf(stderr, "Error in partial callback: %s\n", e.what());
        }
      }
      
      // Check for stop sequences
      bool should_stop = false;
      for (const auto& stop_seq : options.stop_sequences) {
        if (result.text.size() >= stop_seq.size() && 
            result.text.substr(result.text.size() - stop_seq.size()) == stop_seq) {
          // Remove the stop sequence from the result
          fprintf(stderr, "Stop sequence found: '%s'\n", stop_seq.c_str());
          result.text = result.text.substr(0, result.text.size() - stop_seq.size());
          should_stop = true;
          result.finish_reason = "stop";
          break;
        }
      }
      
      if (should_stop) {
        break;
      }
      
      // Accept the token and prepare for next iteration
      fprintf(stderr, "Accepting token\n");
      common_sampler_accept(sampler, token_id, true);
      
      // Prepare the batch with the new token
      fprintf(stderr, "Preparing batch with new token\n");
      batch = llama_batch_get_one(&token_id, 1);
      
      // Continue decoding with new token
      fprintf(stderr, "Decoding with new token\n");
      if (llama_decode(ctx_, batch)) {
        fprintf(stderr, "Error decoding token, stopping generation\n");
        break;
      }
      
      n_generated++;
      fprintf(stderr, "Generated %d tokens so far\n", n_generated);
    }
    
    // Handle interrupted generation
    if (should_stop_completion_) {
      fprintf(stderr, "Completion was interrupted\n");
      result.finish_reason = "interrupted";
    }
    
    // Calculate time taken
    auto end_time = std::chrono::high_resolution_clock::now();
    auto prompt_duration = std::chrono::duration<double, std::milli>(prompt_end_time - start_time).count();
    auto generation_duration = std::chrono::duration<double, std::milli>(end_time - prompt_end_time).count();
    auto total_duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    // Clean up
    fprintf(stderr, "Completion finished, cleaning up\n");
    common_sampler_free(sampler);
    
    // Reset prediction flag
    is_predicting_ = false;
    
    // Parse tool calls if needed
    if (!options.tools.empty() && !result.text.empty()) {
      fprintf(stderr, "Checking for tool calls in generated text\n");
      std::string remainingText;
      std::vector<ToolCall> toolCalls = parseToolCalls(result.text, &remainingText);
      
      // If we found tool calls, store them in the result
      if (!toolCalls.empty()) {
        fprintf(stderr, "Found %zu tool calls\n", toolCalls.size());
        result.tool_calls = std::move(toolCalls);
        result.text = remainingText;
        result.finish_reason = "tool_calls";
      }
    }
    
    // Update result
    result.generated_tokens = n_generated;
    result.prompt_duration_ms = prompt_duration;
    result.generation_duration_ms = generation_duration;
    result.total_duration_ms = total_duration;
    
    fprintf(stderr, "Completion successful: %d prompt tokens, %d generated tokens\n", 
            result.prompt_tokens, result.generated_tokens);
    
    return result;
  } catch (const std::exception& e) {
    // Reset prediction flag on error
    is_predicting_ = false;
    
    // Log the error
    fprintf(stderr, "Error in completion: %s\n", e.what());
    
    // Return a result with the error information
    result.text = "";
    result.finish_reason = "error";
    // Add error message to result for troubleshooting
    result.text = std::string("ERROR: ") + e.what();
    
    return result;
  }
}

// Parse tool calls from generated text using llama.cpp common chat functionality
std::vector<ToolCall> LlamaCppModel::parseToolCalls(const std::string& text, std::string* remainingText) {
  std::vector<ToolCall> result;
  
  // Add detailed logging
  fprintf(stderr, "parseToolCalls: Starting parse of text (length=%zu): %.100s%s\n", 
          text.length(), text.c_str(), text.length() > 100 ? "..." : "");
  
  try {
    // First check if the text is empty or too short to contain a tool call
    if (text.empty() || text.length() < 10) {
      fprintf(stderr, "parseToolCalls: Text too short to contain tool calls\n");
      if (remainingText) {
        *remainingText = text;
      }
      return result;
    }

    // Use common_chat_parse to extract tool calls - try multiple formats
    fprintf(stderr, "parseToolCalls: Trying common_chat_parse\n");
    std::vector<common_chat_format> formats_to_try = {
      COMMON_CHAT_FORMAT_LLAMA_3_X,
      COMMON_CHAT_FORMAT_GENERIC,
      COMMON_CHAT_FORMAT_CONTENT_ONLY
    };
    
    bool parsing_succeeded = false;
    common_chat_msg parsed;
    
    for (auto format : formats_to_try) {
      try {
        fprintf(stderr, "parseToolCalls: Trying format: %d\n", format);
        parsed = common_chat_parse(text, format);
        parsing_succeeded = true;
        fprintf(stderr, "parseToolCalls: Format %d succeeded\n", format);
        break;
      } catch (const std::exception& e) {
        fprintf(stderr, "parseToolCalls: Format %d failed: %s\n", format, e.what());
        // Continue to next format
      }
    }
    
    if (!parsing_succeeded) {
      fprintf(stderr, "parseToolCalls: All parsing formats failed\n");
      
      // Try to manually detect JSON in the content as a fallback
      size_t jsonStart = text.find("{\"");
      size_t jsonEnd = text.rfind("}");
      
      if (jsonStart != std::string::npos && jsonEnd != std::string::npos && jsonEnd > jsonStart) {
        fprintf(stderr, "parseToolCalls: Trying manual JSON extraction\n");
        try {
          std::string jsonStr = text.substr(jsonStart, jsonEnd - jsonStart + 1);
          nlohmann::json parsedJson = nlohmann::json::parse(jsonStr);
          
          // Check if this looks like a function call
          if (parsedJson.contains("name") && parsedJson.contains("arguments")) {
            fprintf(stderr, "parseToolCalls: Found potential tool call via manual extraction\n");
            
            ToolCall toolCall;
            toolCall.id = "call_" + std::to_string(result.size());
            toolCall.type = "function";
            toolCall.name = parsedJson["name"].get<std::string>();
            
            // Handle arguments as either string or object
            if (parsedJson["arguments"].is_string()) {
              toolCall.arguments = parsedJson["arguments"].get<std::string>();
            } else {
              toolCall.arguments = parsedJson["arguments"].dump();
            }
            
            result.push_back(toolCall);
            
            // Set remaining text
            if (remainingText) {
              // Return text before JSON if possible
              if (jsonStart > 0) {
                *remainingText = text.substr(0, jsonStart);
              } else {
                *remainingText = "";
              }
            }
            
            fprintf(stderr, "parseToolCalls: Successfully extracted tool call manually\n");
            return result;
          }
        } catch (const std::exception& e) {
          fprintf(stderr, "parseToolCalls: Manual JSON extraction failed: %s\n", e.what());
        }
      }
      
      // Fall back to returning the original text
      if (remainingText) {
        *remainingText = text;
      }
      return result;
    }
    
    // Set the remaining text if we have a successful parse
    if (remainingText) {
      *remainingText = parsed.content;
      fprintf(stderr, "parseToolCalls: Remaining text length=%zu\n", parsed.content.length());
    }
    
    // Convert tool calls
    if (!parsed.tool_calls.empty()) {
      fprintf(stderr, "parseToolCalls: Found %zu tool calls\n", parsed.tool_calls.size());
      
      for (const auto& tc : parsed.tool_calls) {
        ToolCall toolCall;
        toolCall.id = tc.id.empty() ? "call_" + std::to_string(result.size()) : tc.id;
        toolCall.type = "function";  // Default to function type
        toolCall.name = tc.name;
        toolCall.arguments = tc.arguments;
        
        fprintf(stderr, "parseToolCalls: Tool call - name: %s, args: %.50s%s\n", 
                toolCall.name.c_str(), 
                toolCall.arguments.c_str(), 
                toolCall.arguments.length() > 50 ? "..." : "");
        
        result.push_back(toolCall);
      }
    } else {
      fprintf(stderr, "parseToolCalls: No tool calls found in parsed message\n");
    }
  } catch (const std::exception& e) {
    // Log the error but don't crash
    fprintf(stderr, "parseToolCalls: Error: %s\n", e.what());
    
    // Set the original text as remaining if requested
    if (remainingText) {
      *remainingText = text;
    }
  }
  
  fprintf(stderr, "parseToolCalls: Returning %zu tool calls\n", result.size());
  return result;
}

// JSI method for completions
jsi::Value LlamaCppModel::completionJsi(jsi::Runtime& rt, const jsi::Value* args, size_t count) {
  try {
    if (count < 1 || !args[0].isObject()) {
      throw std::runtime_error("First argument must be a completion options object");
    }
    
    // Get the options object
    jsi::Object options = args[0].getObject(rt);
    
    // Setup completion options - defaults already defined in the struct
    CompletionOptions completionOpts;
    
    // Check if options has either prompt or messages
    bool hasPrompt = options.hasProperty(rt, "prompt");
    bool hasMessages = options.hasProperty(rt, "messages");
    
    if (!hasPrompt && !hasMessages) {
      throw std::runtime_error("Either 'prompt' or 'messages' must be provided");
    }
    
    // Process prompt if available
    if (hasPrompt) {
      jsi::Value promptValue = options.getProperty(rt, "prompt");
      if (promptValue.isString()) {
        completionOpts.prompt = promptValue.asString(rt).utf8(rt);
      } else {
        throw std::runtime_error("'prompt' must be a string");
      }
    }
    
    // Process messages if available
    if (hasMessages) {
      jsi::Value messagesValue = options.getProperty(rt, "messages");
      if (messagesValue.isObject() && messagesValue.getObject(rt).isArray(rt)) {
        jsi::Array messagesArray = messagesValue.getObject(rt).asArray(rt);
        
        for (size_t i = 0; i < messagesArray.size(rt); i++) {
          jsi::Value messageValue = messagesArray.getValueAtIndex(rt, i);
          
          if (!messageValue.isObject()) {
            throw std::runtime_error("Each message must be an object");
          }
          
          jsi::Object messageObj = messageValue.getObject(rt);
          
          if (!messageObj.hasProperty(rt, "role") || !messageObj.hasProperty(rt, "content")) {
            throw std::runtime_error("Each message must have 'role' and 'content' properties");
          }
          
          Message message;
          message.role = messageObj.getProperty(rt, "role").asString(rt).utf8(rt);
          message.content = messageObj.getProperty(rt, "content").asString(rt).utf8(rt);
          
          // Optional name property for tool responses
          if (messageObj.hasProperty(rt, "name") && messageObj.getProperty(rt, "name").isString()) {
            message.name = messageObj.getProperty(rt, "name").asString(rt).utf8(rt);
          }
          
          completionOpts.messages.push_back(message);
        }
      } else {
        throw std::runtime_error("'messages' must be an array");
      }
    }
    
    // Use SystemUtils to set remaining options with proper type checking
    SystemUtils::setIfExists(rt, options, "temperature", completionOpts.temperature);
    SystemUtils::setIfExists(rt, options, "top_p", completionOpts.top_p);
    SystemUtils::setIfExists(rt, options, "top_k", completionOpts.top_k);
    SystemUtils::setIfExists(rt, options, "min_p", completionOpts.min_p);
    SystemUtils::setIfExists(rt, options, "typical_p", completionOpts.typical_p);
    SystemUtils::setIfExists(rt, options, "repeat_penalty", completionOpts.repeat_penalty);
    SystemUtils::setIfExists(rt, options, "repeat_last_n", completionOpts.repeat_last_n);
    SystemUtils::setIfExists(rt, options, "frequency_penalty", completionOpts.frequency_penalty);
    SystemUtils::setIfExists(rt, options, "presence_penalty", completionOpts.presence_penalty);
    SystemUtils::setIfExists(rt, options, "seed", completionOpts.seed);
    SystemUtils::setIfExists(rt, options, "jinja", completionOpts.jinja);
    
    // Support both n_predict and max_tokens (with max_tokens taking precedence)
    if (options.hasProperty(rt, "n_predict")) {
      int n_predict = 0;
      if (SystemUtils::setIfExists(rt, options, "n_predict", n_predict)) {
        completionOpts.max_tokens = n_predict;
      }
    }
    
    if (options.hasProperty(rt, "max_tokens")) {
      SystemUtils::setIfExists(rt, options, "max_tokens", completionOpts.max_tokens);
    }
    
    // Handle optional string parameters
    SystemUtils::setIfExists(rt, options, "grammar", completionOpts.grammar);
    SystemUtils::setIfExists(rt, options, "tool_choice", completionOpts.tool_choice);
    
    // Handle the stop sequences
    if (options.hasProperty(rt, "stop")) {
      jsi::Value stopVal = options.getProperty(rt, "stop");
      if (stopVal.isString()) {
        // Single stop sequence
        completionOpts.stop_sequences.push_back(stopVal.asString(rt).utf8(rt));
      } else if (stopVal.isObject() && stopVal.getObject(rt).isArray(rt)) {
        // Array of stop sequences
        jsi::Array stopArr = stopVal.getObject(rt).asArray(rt);
        for (size_t i = 0; i < stopArr.size(rt); i++) {
          jsi::Value item = stopArr.getValueAtIndex(rt, i);
          if (item.isString()) {
            completionOpts.stop_sequences.push_back(item.asString(rt).utf8(rt));
          }
        }
      }
    }
    
    // Handle tools if provided
    if (options.hasProperty(rt, "tools") && options.getProperty(rt, "tools").isObject() && 
        options.getProperty(rt, "tools").getObject(rt).isArray(rt)) {
      jsi::Array toolsArr = options.getProperty(rt, "tools").getObject(rt).asArray(rt);
      
      for (size_t i = 0; i < toolsArr.size(rt); i++) {
        if (toolsArr.getValueAtIndex(rt, i).isObject()) {
          try {
            auto tool = convertJsiToolToTool(rt, toolsArr.getValueAtIndex(rt, i).getObject(rt));
            completionOpts.tools.push_back(tool);
          } catch (const std::exception& e) {
            fprintf(stderr, "Error converting tool at index %zu: %s\n", i, e.what());
            // Continue with other tools
          }
        }
      }
    }
    
    // Check for partial callback (second argument)
    if (count > 1 && args[1].isObject() && args[1].getObject(rt).isFunction(rt)) {
      // Set up the runtime pointer only - this will be used to access the correct runtime
      completionOpts.runtime = &rt;
      
      // Create a persistent copy of the function using a shared_ptr to avoid copy issues
      auto jsFunction = std::make_shared<jsi::Function>(args[1].getObject(rt).asFunction(rt));
      
      // Store a simple lambda that doesn't capture by copy, avoiding the copy constructor issue
      completionOpts.partial_callback = [jsFunction](jsi::Runtime& runtime, const char* token) {
        // Safely create the result object and call the function
        try {
          jsi::Object result(runtime);
          result.setProperty(runtime, "token", jsi::String::createFromUtf8(runtime, token));
          jsFunction->call(runtime, result);
        } catch (const std::exception& e) {
          // Log error but don't crash
          fprintf(stderr, "Error in callback: %s\n", e.what());
        }
      };
    }
    
    try {
      // Execute the completion
      CompletionResult result = completion(completionOpts);
      
      // Reset the prediction status
      is_predicting_ = false;
      
      // Create return object
      jsi::Object resultObj(rt);
      resultObj.setProperty(rt, "text", jsi::String::createFromUtf8(rt, result.text));
      resultObj.setProperty(rt, "truncated", jsi::Value(result.truncated));
      resultObj.setProperty(rt, "finish_reason", jsi::String::createFromUtf8(rt, result.finish_reason));
      resultObj.setProperty(rt, "prompt_tokens", jsi::Value(result.prompt_tokens));
      resultObj.setProperty(rt, "generated_tokens", jsi::Value(result.generated_tokens));
      
      // Add timing information
      jsi::Object timings(rt);
      timings.setProperty(rt, "prompt_ms", jsi::Value(result.prompt_duration_ms));
      timings.setProperty(rt, "predicted_ms", jsi::Value(result.generation_duration_ms));
      timings.setProperty(rt, "total_ms", jsi::Value(result.total_duration_ms));
      timings.setProperty(rt, "prompt_n", jsi::Value(result.prompt_tokens));
      timings.setProperty(rt, "predicted_n", jsi::Value(result.generated_tokens));
      resultObj.setProperty(rt, "timings", timings);
      
      // If there are tool calls, add them to the result
      if (!result.tool_calls.empty()) {
        jsi::Array toolCallsArray(rt, result.tool_calls.size());
        
        for (size_t i = 0; i < result.tool_calls.size(); i++) {
          jsi::Object toolCallObj = convertToolCallToJsiObject(rt, result.tool_calls[i]);
          toolCallsArray.setValueAtIndex(rt, i, toolCallObj);
        }
        
        resultObj.setProperty(rt, "tool_calls", toolCallsArray);
      }
      
      return resultObj;
    } catch (const std::exception& e) {
      // Add more context to the error
      throw std::runtime_error(std::string("Error during completion: ") + e.what());
    }
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
  
  try {
    // Check if the tool has a type field
    if (!jsiTool.hasProperty(rt, "type")) {
      throw std::runtime_error("Missing tool type");
    }
    
    // Get the type
    tool.type = jsiTool.getProperty(rt, "type").asString(rt).utf8(rt);
    
    // Currently we only support function tools
    if (tool.type != "function") {
      throw std::runtime_error("Unsupported tool type: " + tool.type);
    }
    
    // Check if the tool has a function field
    if (!jsiTool.hasProperty(rt, "function") || 
        !jsiTool.getProperty(rt, "function").isObject()) {
      throw std::runtime_error("Missing tool function");
    }
    
    // Get the function object
    jsi::Object funcObj = jsiTool.getProperty(rt, "function").asObject(rt);
    
    // Get required name field
    if (!funcObj.hasProperty(rt, "name") || !funcObj.getProperty(rt, "name").isString()) {
      throw std::runtime_error("Missing function name");
    }
    tool.function.name = funcObj.getProperty(rt, "name").asString(rt).utf8(rt);
    
    // Get optional description field
    if (funcObj.hasProperty(rt, "description") && funcObj.getProperty(rt, "description").isString()) {
      tool.function.description = funcObj.getProperty(rt, "description").asString(rt).utf8(rt);
    }
    
    // Get the parameters object (required)
    if (!funcObj.hasProperty(rt, "parameters") || !funcObj.getProperty(rt, "parameters").isObject()) {
      throw std::runtime_error("Missing function parameters");
    }
    
    jsi::Object paramsObj = funcObj.getProperty(rt, "parameters").asObject(rt);
    
    // Build the function parameters
    if (paramsObj.hasProperty(rt, "properties") && paramsObj.getProperty(rt, "properties").isObject()) {
      jsi::Object propsObj = paramsObj.getProperty(rt, "properties").asObject(rt);
      
      // Get required array if it exists
      std::vector<std::string> requiredProps;
      if (paramsObj.hasProperty(rt, "required") && 
          paramsObj.getProperty(rt, "required").isObject() &&
          paramsObj.getProperty(rt, "required").asObject(rt).isArray(rt)) {
          
        jsi::Array reqArray = paramsObj.getProperty(rt, "required").asObject(rt).asArray(rt);
        for (size_t i = 0; i < reqArray.size(rt); i++) {
          requiredProps.push_back(reqArray.getValueAtIndex(rt, i).asString(rt).utf8(rt));
        }
      }
      
      // Get property names
      jsi::Array propNames = propsObj.getPropertyNames(rt);
      for (size_t i = 0; i < propNames.size(rt); i++) {
        std::string propName = propNames.getValueAtIndex(rt, i).asString(rt).utf8(rt);
        
        if (propsObj.hasProperty(rt, propName.c_str()) && 
            propsObj.getProperty(rt, propName.c_str()).isObject()) {
            
          jsi::Object propObj = propsObj.getProperty(rt, propName.c_str()).asObject(rt);
          
          FunctionParameter param;
          param.name = propName;
          
          // Get type
          if (propObj.hasProperty(rt, "type") && propObj.getProperty(rt, "type").isString()) {
            param.type = propObj.getProperty(rt, "type").asString(rt).utf8(rt);
          } else {
            param.type = "string"; // Default type
          }
          
          // Get description
          if (propObj.hasProperty(rt, "description") && propObj.getProperty(rt, "description").isString()) {
            param.description = propObj.getProperty(rt, "description").asString(rt).utf8(rt);
          }
          
          // Check if required
          param.required = std::find(requiredProps.begin(), requiredProps.end(), propName) != requiredProps.end();
          
          tool.function.parameters.push_back(param);
        }
      }
    }
    
    return tool;
  } catch (const std::exception& e) {
    // Log the error for debugging
    fprintf(stderr, "Error converting JSI tool: %s\n", e.what());
    throw; // Rethrow to be handled by caller
  }
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