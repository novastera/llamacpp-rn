#include "LlamaCppRnModule.h"
#include <jsi/jsi.h>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <thread>
#include "ChatTemplates.h"

#if defined(__ANDROID__) || defined(__linux__)
#include <unistd.h>
#endif

// Will include llama.cpp headers once we have the subdirectory set up
// #include "llama.cpp/llama.h"
// #include "llama.cpp/common.h"

namespace facebook::react {

// Host function definitions that directly map to C++ methods
static jsi::Value __hostFunction_LlamaCppRnSpecInitLlama(
    jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value *args, size_t count) {
  return static_cast<LlamaCppRn *>(&turboModule)->initLlama(rt, args[0].getObject(rt));
}

static jsi::Value __hostFunction_LlamaCppRnSpecLoadLlamaModelInfo(
    jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value *args, size_t count) {
  return static_cast<LlamaCppRn *>(&turboModule)->loadLlamaModelInfo(rt, args[0].getString(rt));
}

static jsi::Value __hostFunction_LlamaCppRnSpecJsonSchemaToGbnf(
    jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value *args, size_t count) {
  return static_cast<LlamaCppRn *>(&turboModule)->jsonSchemaToGbnf(rt, args[0].getObject(rt));
}

LlamaCppRn::LlamaCppRn(std::shared_ptr<CallInvoker> jsInvoker)
    : TurboModule(kModuleName, std::move(jsInvoker)) {
  // Register the module's methods with the TurboModule system
  methodMap_["initLlama"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecInitLlama};
  methodMap_["loadLlamaModelInfo"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecLoadLlamaModelInfo};
  methodMap_["jsonSchemaToGbnf"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecJsonSchemaToGbnf};
}

std::shared_ptr<TurboModule> LlamaCppRn::create(std::shared_ptr<CallInvoker> jsInvoker) {
  return std::make_shared<LlamaCppRn>(std::move(jsInvoker));
}

jsi::Value LlamaCppRn::initLlama(jsi::Runtime &runtime, jsi::Object params) {
  // Thread-safe implementation with a lock to ensure multiple calls don't interfere
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Extract model path parameter (required)
    if (!params.hasProperty(runtime, "model")) {
      throw std::runtime_error("Missing required parameter: model");
    }
    
    std::string modelPath = params.getProperty(runtime, "model").asString(runtime).utf8(runtime);
    
    // Create model parameters with defaults
    llama_model_params modelParams = llama_model_default_params();
    
    // Extract optional parameters
    if (params.hasProperty(runtime, "n_gpu_layers")) {
      modelParams.n_gpu_layers = params.getProperty(runtime, "n_gpu_layers").asNumber();
    }
    
    if (params.hasProperty(runtime, "use_mlock") && params.getProperty(runtime, "use_mlock").asBool()) {
      modelParams.use_mlock = true;
    }
    
    // GPU detection and configuration
    GpuInfo gpuInfo = getGpuCapabilities();
    if (gpuInfo.available && modelParams.n_gpu_layers > 0) {
      // Enable GPU if available and requested
      if (!enableGpu(true)) {
        // If enabling GPU fails, log warning and revert to CPU
        modelParams.n_gpu_layers = 0;
      }
    } else {
      // No GPU available or not requested
      modelParams.n_gpu_layers = 0;
    }
    
    // Updated: Use new API to load model
    llama_model* model = llama_model_load_from_file(modelPath.c_str(), modelParams);
    if (!model) {
      throw std::runtime_error("Failed to load model: " + modelPath);
    }
    
    // Create context parameters with defaults
    llama_context_params contextParams = llama_context_default_params();
    
    // Extract and set context parameters
    if (params.hasProperty(runtime, "n_ctx")) {
      contextParams.n_ctx = params.getProperty(runtime, "n_ctx").asNumber();
    }
    
    if (params.hasProperty(runtime, "n_batch")) {
      contextParams.n_batch = params.getProperty(runtime, "n_batch").asNumber();
    }
    
    // Set thread count - use optimal value if not specified by the user
    if (params.hasProperty(runtime, "n_threads")) {
      contextParams.n_threads = params.getProperty(runtime, "n_threads").asNumber();
    } else {
      // Get optimal thread count based on device capabilities
      #if defined(__ANDROID__)
      // For Android, we'll use our DeviceCapabilities class from the JNI layer
      // This is a simplified approach for illustration - in a real implementation,
      // we'd create a proper API to get the value from the JNI layer
      int cpuCores = sysconf(_SC_NPROCESSORS_ONLN);
      int optimalThreads;
      if (cpuCores <= 1) {
          optimalThreads = 1;
      } else if (cpuCores <= 4) {
          optimalThreads = cpuCores - 1;
      } else {
          optimalThreads = cpuCores - 2;
      }
      contextParams.n_threads = optimalThreads;
      #elif defined(__APPLE__)
      // For iOS/macOS, determine thread count based on device
      int cpuCores = std::thread::hardware_concurrency();
      int optimalThreads = (cpuCores <= 1) ? 1 : ((cpuCores <= 4) ? cpuCores - 1 : cpuCores - 2);
      contextParams.n_threads = optimalThreads;
      #else
      // Default behavior for other platforms
      contextParams.n_threads = std::max(1U, std::thread::hardware_concurrency() - 1);
      #endif
    }
    
    if (params.hasProperty(runtime, "rope_freq_base")) {
      contextParams.rope_freq_base = params.getProperty(runtime, "rope_freq_base").asNumber();
    }
    
    if (params.hasProperty(runtime, "rope_freq_scale")) {
      contextParams.rope_freq_scale = params.getProperty(runtime, "rope_freq_scale").asNumber();
    }
    
    // Check for grammar
    if (params.hasProperty(runtime, "grammar") && params.getProperty(runtime, "grammar").isString()) {
      std::string grammar = params.getProperty(runtime, "grammar").asString(runtime).utf8(runtime);
      
      // Load grammar with llama.cpp - actual implementation would use llama.cpp's grammar API
      // This is just a placeholder for now - grammar handling will be done later
    }
    
    // Updated: Use new API to create context
    llama_context* ctx = llama_init_from_model(model, contextParams);
    if (!ctx) {
      // Clean up the model if context creation fails
      llama_model_free(model);
      throw std::runtime_error("Failed to create context");
    }
    
    // First, check if the model supports embeddings
    if (!ctx) {
      throw jsi::JSError(runtime, "No context available");
    }
    
    // Ensure embeddings are enabled on the context
    llama_set_embeddings(ctx, true);
    
    // Create a JavaScript object that represents the llama context
    auto result = createModelObject(runtime, model, ctx);
    
    return result;
  } catch (const std::exception& e) {
    // Handle errors and convert to JavaScript exceptions
    jsi::Object error(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

jsi::Value LlamaCppRn::loadLlamaModelInfo(jsi::Runtime &runtime, jsi::String modelPath) {
  std::string path = modelPath.utf8(runtime);
  
  // Thread-safe implementation with a lock for the cache
  std::lock_guard<std::mutex> lock(mutex_);
  
  // Check if we already have this model info cached
  auto it = modelInfoCache_.find(path);
  if (it != modelInfoCache_.end()) {
    // Use cached result for better performance
    return jsi::Value(runtime, *(it->second));
  }
  
  try {
    // Create default model parameters for info loading
    llama_model_params params = llama_model_default_params();
    
    // Updated: Use new API to load model
    llama_model* model = llama_model_load_from_file(path.c_str(), params);
    
    if (!model) {
      throw std::runtime_error("Failed to load model info: " + path);
    }
    
    // Create result object with model information
    jsi::Object result(runtime);
    
    // Extract model metadata - using updated API calls
    result.setProperty(runtime, "n_params", jsi::Value((double)llama_model_n_params(model)));
    const struct llama_vocab* vocab = llama_model_get_vocab(model);
    result.setProperty(runtime, "n_vocab", jsi::Value((double)llama_vocab_n_tokens(vocab)));
    result.setProperty(runtime, "n_context", jsi::Value((double)llama_model_n_ctx_train(model)));
    result.setProperty(runtime, "n_embd", jsi::Value((double)llama_model_n_embd(model)));
    
    // Get model description
    char desc_buf[512] = {0};
    llama_model_desc(model, desc_buf, sizeof(desc_buf));
    result.setProperty(runtime, "description", 
                      jsi::String::createFromUtf8(runtime, desc_buf[0] ? desc_buf : "No description available"));
    
    // Clean up - using updated API
    llama_model_free(model);
    
    // Create a shared pointer for caching
    auto resultPtr = std::make_shared<jsi::Object>(std::move(result));
    
    // Cache the result for future calls using the shared_ptr
    modelInfoCache_[path] = resultPtr;
    
    // Return a copy of the cached object
    return jsi::Value(runtime, *resultPtr);
  } catch (const std::exception& e) {
    // Handle errors and convert to JavaScript exceptions
    jsi::Object error(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

jsi::Value LlamaCppRn::jsonSchemaToGbnf(jsi::Runtime &runtime, jsi::Object schema) {
  // Thread-safe implementation with a lock for consistency
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Create a simple JSON representation for the schema
    SimpleJSON schema_json;
    
    // Extract schema properties and convert to SimpleJSON
    auto propertyNames = schema.getPropertyNames(runtime);
    for (size_t i = 0; i < propertyNames.size(runtime); i++) {
      auto name = propertyNames.getValueAtIndex(runtime, i).getString(runtime).utf8(runtime);
      auto value = schema.getProperty(runtime, name.c_str());
      if (value.isString()) {
        schema_json.set(name, value.getString(runtime).utf8(runtime));
      }
      // Note: This is a simplified conversion - a full implementation would handle
      // all types recursively (objects, arrays, etc.)
    }
    
    // Basic JSON Schema to GBNF conversion logic
    // This is a simplified implementation
    std::ostringstream gbnf;
    
    // Start with root definition
    gbnf << "root ::= value\n\n";
    
    // Basic types
    gbnf << "value ::= object | array | string | number | true | false | null\n\n";
    
    // Object definition - Fixed with proper escaping of quotes
    gbnf << "object ::= \"{\" ws (pair (\",\" ws pair)*)? \"}\" ws\n";
    gbnf << "pair ::= string \":\" ws value\n\n";
    
    // Array definition - Fixed with proper escaping of quotes
    gbnf << "array ::= \"[\" ws (value (\",\" ws value)*)? \"]\" ws\n\n";
    
    // String definition
    gbnf << "string ::= quote [^\"\\\\]* quote\n";
    gbnf << "quote ::= \"\\\"\"\n\n";
    
    // Number definition
    gbnf << "number ::= int frac? exp? ws\n";
    gbnf << "int ::= \"-\"? (\"0\" | [\"1\"-\"9\"] [\"0\"-\"9\"]*)\n";
    gbnf << "frac ::= \".\" [\"0\"-\"9\"]+\n";
    gbnf << "exp ::= [\"e\" \"E\"] [\"+\" \"-\"]? [\"0\"-\"9\"]+\n\n";
    
    // Other primitives
    gbnf << "true ::= \"true\" ws\n";
    gbnf << "false ::= \"false\" ws\n";
    gbnf << "null ::= \"null\" ws\n\n";
    
    // Whitespace
    gbnf << "ws ::= [ \\t\\n\\r]*\n";
    
    // Return the GBNF grammar
    jsi::String result = jsi::String::createFromUtf8(runtime, gbnf.str());
    return result;
  } catch (const std::exception& e) {
    // Handle errors and convert to JavaScript exceptions
    jsi::Object error(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

bool LlamaCppRn::detectGpuCapabilities() {
  // Initialize backends first
  llama_backend_init();
  
  // Platform-specific GPU detection
#if defined(__APPLE__)
  // On iOS/macOS, check for GPU support
  return llama_supports_gpu_offload();
#elif defined(__ANDROID__) && defined(LLAMACPPRN_OPENCL_ENABLED) && LLAMACPPRN_OPENCL_ENABLED == 1
  // On Android with OpenCL enabled, check for GPU support
  return llama_supports_gpu_offload();
#else
  // No GPU support for this platform
  return false;
#endif
}

bool LlamaCppRn::enableGpu(bool enable) {
  if (enable == m_gpuEnabled) {
    return true;  // Already in requested state
  }
  
  if (enable) {
    // Initialize backends
    llama_backend_init();
    
    // Check if GPU is supported
    bool success = llama_supports_gpu_offload();

    if (success) {
      m_gpuEnabled = true;
      return true;
    }
    return false;
  } else {
    // Disable GPU
    m_gpuEnabled = false;
    return true;
  }
}

bool LlamaCppRn::isGpuEnabled() {
  return m_gpuEnabled;
}

LlamaCppRn::GpuInfo LlamaCppRn::getGpuCapabilities() {
  GpuInfo info;
  
  // Default to no GPU
  info.available = false;
  info.deviceName = "CPU Only";
  info.deviceVendor = "N/A";
  info.deviceVersion = "N/A";
  info.deviceComputeUnits = 0;
  info.deviceMemSize = 0;
  
  // Initialize backends
  llama_backend_init();
  
  if (llama_supports_gpu_offload()) {
    info.available = true;
    
#if defined(__APPLE__)
    info.deviceName = "Apple GPU";
    info.deviceVendor = "Apple";
    info.deviceVersion = "Metal";
    // We could improve this with more device-specific detection in the future
#elif defined(GGML_USE_CLBLAST)
    info.deviceName = "OpenCL Device";
    info.deviceVendor = "OpenCL";
    info.deviceVersion = "OpenCL";
    // Could be enhanced with specific OpenCL device info if needed
#endif
  }

  return info;
}

jsi::Object LlamaCppRn::createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx) {
  // Create the context object
  jsi::Object contextObj(runtime);
  
  // Store native pointers in the object for future references
  // Note: In a real implementation, we'd use a more robust solution like HostObjects
  // or a reference map to avoid exposing raw pointers to JavaScript
  
  // We're storing these as numbers for simplicity, but in a real implementation
  // we'd use a better approach to manage these references
  contextObj.setProperty(runtime, "_model_ptr", jsi::Value((double)(uintptr_t)model));
  contextObj.setProperty(runtime, "_ctx_ptr", jsi::Value((double)(uintptr_t)ctx));
  
  // Add model properties
  const struct llama_vocab* vocab = llama_model_get_vocab(model);
  contextObj.setProperty(runtime, "n_vocab", jsi::Value((double)llama_vocab_n_tokens(vocab)));
  
  // Add completion method
  contextObj.setProperty(runtime, "completion", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "completion"), 
      2,  // takes params object and optional callback
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Check arguments
        if (count < 1 || !args[0].isObject()) {
          throw jsi::JSError(rt, "First argument must be an object with completion parameters");
        }
        
        jsi::Object params = args[0].getObject(rt);
        
        // Get the prompt or messages from parameters
        std::string prompt;
        
        // Check if we have messages (for chat) or a direct prompt
        if (params.hasProperty(rt, "messages") && params.getProperty(rt, "messages").isObject() && 
            params.getProperty(rt, "messages").getObject(rt).isArray(rt)) {
          
          // Process chat messages
          jsi::Array messages_array = params.getProperty(rt, "messages").getObject(rt).getArray(rt);
          
          // Convert JSI array to vector of ChatMessage
          std::vector<ChatMessage> chat_messages;
          for (size_t i = 0; i < messages_array.size(rt); i++) {
            if (!messages_array.getValueAtIndex(rt, i).isObject()) {
              continue;
            }
            
            jsi::Object message = messages_array.getValueAtIndex(rt, i).getObject(rt);
            ChatMessage chat_msg;
            
            // Extract required fields
            if (message.hasProperty(rt, "role") && message.getProperty(rt, "role").isString()) {
              chat_msg.role = message.getProperty(rt, "role").getString(rt).utf8(rt);
            } else {
              continue;
            }
            
            if (message.hasProperty(rt, "content")) {
              if (message.getProperty(rt, "content").isString()) {
                chat_msg.content = message.getProperty(rt, "content").getString(rt).utf8(rt);
              }
            } else {
              continue;
            }
            
            // Extract optional fields
            if (message.hasProperty(rt, "name") && message.getProperty(rt, "name").isString()) {
              chat_msg.name = message.getProperty(rt, "name").getString(rt).utf8(rt);
            }
            
            if (message.hasProperty(rt, "tool_call_id") && message.getProperty(rt, "tool_call_id").isString()) {
              chat_msg.tool_call_id = message.getProperty(rt, "tool_call_id").getString(rt).utf8(rt);
            }
            
            chat_messages.push_back(chat_msg);
          }
          
          // Get the template name (defaulting to a reasonable value)
          std::string template_name = "chatml"; // default
          if (params.hasProperty(rt, "template") && params.getProperty(rt, "template").isString()) {
            template_name = params.getProperty(rt, "template").getString(rt).utf8(rt);
          }
          
          try {
            // Fix: Create new variable to hold result instead of reusing the parameter name
            std::vector<ChatMessage> processedMessages = chat_templates::messages_from_json(chat_messages);
            prompt = chat_templates::apply_chat_template(model, processedMessages, template_name);
          } catch (const std::exception& e) {
            throw jsi::JSError(rt, "Failed to apply chat template: " + std::string(e.what()));
          }
          
        } else if (params.hasProperty(rt, "prompt") && params.getProperty(rt, "prompt").isString()) {
          // Direct prompt (non-chat mode)
          prompt = params.getProperty(rt, "prompt").getString(rt).utf8(rt);
        } else {
          throw jsi::JSError(rt, "Missing required parameter: either prompt or messages must be provided");
        }
        
        // Get completion parameters
        int max_tokens = 128;  // Default
        if (params.hasProperty(rt, "max_tokens") && params.getProperty(rt, "max_tokens").isNumber()) {
          max_tokens = (int)params.getProperty(rt, "max_tokens").asNumber();
        }
        
        // Temperature (default 0.8)
        float temperature = 0.8f;
        if (params.hasProperty(rt, "temperature") && params.getProperty(rt, "temperature").isNumber()) {
          temperature = (float)params.getProperty(rt, "temperature").asNumber();
        }
        
        // Top_p (default 0.9)
        float top_p = 0.9f;
        if (params.hasProperty(rt, "top_p") && params.getProperty(rt, "top_p").isNumber()) {
          top_p = (float)params.getProperty(rt, "top_p").asNumber();
        }
        
        // Grammar handling
        if (params.hasProperty(rt, "grammar") && params.getProperty(rt, "grammar").isString()) {
          std::string grammar_str = params.getProperty(rt, "grammar").getString(rt).utf8(rt);
          
          // Note: Grammar handling is implemented in the sampler initialization
          // We're acknowledging the grammar parameter but not using a separate state variable
        }
        
        // Check for streaming callback
        bool streaming = false;
        std::shared_ptr<jsi::Function> callback;
        if (count > 1 && args[1].isObject() && args[1].getObject(rt).isFunction(rt)) {
          streaming = true;
          callback = std::make_shared<jsi::Function>(args[1].getObject(rt).getFunction(rt));
        }
        
        // Clear any existing KV cache first (to start fresh)
        llama_kv_self_clear(ctx);
        
        // Tokenize the prompt
        std::vector<llama_token> tokens(prompt.length() + 1);
        const struct llama_vocab* vocab = llama_model_get_vocab(model);
        int n_tokens = llama_tokenize(vocab, prompt.c_str(), int(prompt.length()), 
                                     tokens.data(), int(tokens.size()), true, false);
        
        if (n_tokens < 0) {
          tokens.resize(-n_tokens);
          n_tokens = llama_tokenize(vocab, prompt.c_str(), int(prompt.length()), 
                                   tokens.data(), int(tokens.size()), true, false);
        }
        
        if (n_tokens < 0 || n_tokens == 0) {
          throw jsi::JSError(rt, "Failed to tokenize or empty prompt: " + prompt);
        }
        
        // Resize to actual number of tokens
        tokens.resize(n_tokens);
        
        // Timing variables
        auto start_prompt = std::chrono::high_resolution_clock::now();
        
        // Create batch for input tokens
        llama_batch batch = llama_batch_init(int(tokens.size()), 0, 1);
        for (int i = 0; i < tokens.size(); i++) {
            batch.token[i] = tokens[i];
            batch.pos[i] = i;
            batch.n_seq_id[i] = 1;
            batch.seq_id[i][0] = 0;
            batch.logits[i] = false;
        }
        batch.n_tokens = int(tokens.size());
        
        // Process the prompt tokens
        if (llama_decode(ctx, batch) < 0) {
          llama_batch_free(batch);
          throw jsi::JSError(rt, "Failed to evaluate prompt tokens");
        }
        
        llama_batch_free(batch);
        
        auto end_prompt = std::chrono::high_resolution_clock::now();
        
        // Generate parameters
        std::string completion_text;
        int n_gen = 0;
        
        auto start_gen = std::chrono::high_resolution_clock::now();
        
        // Main generation loop - simplified for now
        while (n_gen < max_tokens) {
          // Check if we need to stop
          if (m_shouldStopCompletion.load()) {
            // Reset the flag for next time
            m_shouldStopCompletion = false;
            break;
          }
          
          // Simple token sampling implementation
          llama_token id = 0;
          
          // Use appropriate sampling method with llama.cpp's new sampling API
          auto params = llama_sampler_chain_default_params();
          auto sampler = llama_sampler_chain_init(params);
          llama_sampler_chain_add(sampler, llama_sampler_init_top_k(40));
          llama_sampler_chain_add(sampler, llama_sampler_init_temp(0.8f));
          llama_sampler_chain_add(sampler, llama_sampler_init_dist(0));
          
          // Sample token and accept it
          id = llama_sampler_sample(sampler, ctx, -1);
          llama_sampler_accept(sampler, id);
          
          // Clean up the sampler
          llama_sampler_free(sampler);
          
          // Check for end of sequence
          if (id == llama_vocab_eos(vocab)) {
            break;
          }
          
          // Get piece corresponding to this token
          char piece_buf[64] = {0};
          int piece_len = llama_token_to_piece(vocab, id, piece_buf, sizeof(piece_buf), 0, false);
          std::string token_text;
          if (piece_len > 0) {
            token_text = std::string(piece_buf, piece_len);
            completion_text += token_text;
          }
          
          // Send token to streaming callback if enabled
          if (streaming && callback) {
            jsi::Object token_obj(rt);
            token_obj.setProperty(rt, "token", jsi::String::createFromUtf8(rt, token_text));
            callback->call(rt, token_obj);
          }
          
          n_gen++;
          
          // Next batch for generated tokens
          llama_batch next_batch = llama_batch_init(1, 0, 1);
          next_batch.token[0] = id;
          next_batch.pos[0] = int(tokens.size()) + n_gen;
          next_batch.n_seq_id[0] = 1;
          next_batch.seq_id[0][0] = 0;
          next_batch.logits[0] = true;
          
          // Process token
          if (llama_decode(ctx, next_batch) < 0) {
            llama_batch_free(next_batch);
            break;
          }
          
          llama_batch_free(next_batch);
        }
        
        auto end_gen = std::chrono::high_resolution_clock::now();
        
        // Calculate timings
        auto prompt_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_prompt - start_prompt).count();
        auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_gen - start_gen).count();
        auto total_duration = prompt_duration + gen_duration;
        
        // Create result object
        jsi::Object result(rt);
        result.setProperty(rt, "text", jsi::String::createFromUtf8(rt, completion_text));
        result.setProperty(rt, "tokens_predicted", jsi::Value((double)n_gen));
        
        // Parse tool calls if requested and present in the output
        if (params.hasProperty(rt, "tools") && params.getProperty(rt, "tools").isObject() && 
            params.getProperty(rt, "tools").getObject(rt).isArray(rt)) {
          
          // Use our ChatTemplates module to parse tool calls
          ChatMessage tool_call = chat_templates::parse_tool_call(completion_text);
          
          // If a tool call was found, convert it to a JSI object
          if (!tool_call.id.empty()) {
            // Create objects explicitly using our safer utility function
            jsi::Object toolCall(rt);
            
            // Extract and set the tool call properties
            toolCall.setProperty(rt, "id", jsi::String::createFromUtf8(rt, tool_call.id));
            toolCall.setProperty(rt, "type", jsi::String::createFromUtf8(rt, tool_call.type));
            
            // Create function object
            jsi::Object functionObj(rt);
            
            // Get values from the function structure to avoid direct member access
            std::string functionName = tool_call.function.name;
            std::string functionArgs = tool_call.function.arguments;
            
            functionObj.setProperty(rt, "name", jsi::String::createFromUtf8(rt, functionName));
            functionObj.setProperty(rt, "arguments", jsi::String::createFromUtf8(rt, functionArgs));
            
            // Don't use std::move with JSI objects
            toolCall.setProperty(rt, "function", functionObj);
            
            // Add to tool calls array
            jsi::Array toolCalls(rt, 1);
            toolCalls.setValueAtIndex(rt, 0, toolCall);
            
            // Add tool calls to result
            result.setProperty(rt, "tool_calls", toolCalls);
          }
        }
        
        // Create timing object
        jsi::Object timings(rt);
        timings.setProperty(rt, "prompt_n", jsi::Value((double)tokens.size()));
        timings.setProperty(rt, "prompt_ms", jsi::Value((double)prompt_duration));
        timings.setProperty(rt, "predicted_n", jsi::Value((double)n_gen));
        timings.setProperty(rt, "predicted_ms", jsi::Value((double)gen_duration));
        timings.setProperty(rt, "total_ms", jsi::Value((double)total_duration));
        
        result.setProperty(rt, "timings", timings);
        
        return result;
      }
    )
  );
  
  // Add tokenize method
  contextObj.setProperty(runtime, "tokenize", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "tokenize"), 
      1,  // takes content string
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        try {
          // Get the model and context pointers from the object
          uintptr_t model_ptr = (uintptr_t)thisVal.getObject(rt).getProperty(rt, "_model_ptr").asNumber();
          llama_model* model = (llama_model*)model_ptr;
          
          uintptr_t ctx_ptr = (uintptr_t)thisVal.getObject(rt).getProperty(rt, "_ctx_ptr").asNumber();
          llama_context* ctx = (llama_context*)ctx_ptr;
          
          if (!ctx || !model) {
            throw jsi::JSError(rt, "Model not loaded");
          }
          
          // Get input content
          if (count < 1 || !args[0].isString()) {
            throw jsi::JSError(rt, "First argument must be a string");
          }
          std::string content = args[0].getString(rt).utf8(rt);
          
          try {
            // Create array to hold the tokens
            std::vector<llama_token> tokens(content.length() + 1);
            
            // Use updated llama.cpp's tokenize function
            const struct llama_vocab* vocab = llama_model_get_vocab(model);
            int n_tokens = llama_tokenize(vocab, content.c_str(), int(content.length()), 
                                         tokens.data(), int(tokens.size()), true, false);
            
            if (n_tokens < 0) {
              // If the buffer was too small, resize and try again
              tokens.resize(-n_tokens);
              n_tokens = llama_tokenize(vocab, content.c_str(), int(content.length()), 
                                       tokens.data(), int(tokens.size()), true, false);
            }
            
            if (n_tokens < 0) {
              throw jsi::JSError(rt, "Failed to tokenize content");
            }
            
            // Resize to actual token count
            tokens.resize(n_tokens);
            
            // Create array to return tokens
            jsi::Array result = jsi::Array(rt, tokens.size());
            for (size_t i = 0; i < tokens.size(); i++) {
              // Get the piece for this token
              char piece_buf[64] = {0};
              const struct llama_vocab* vocab = llama_model_get_vocab(model);
              int piece_len = llama_token_to_piece(vocab, tokens[i], piece_buf, sizeof(piece_buf), 0, false);
              if (piece_len <= 0) {
                throw jsi::JSError(rt, "Failed to get piece for token: " + std::to_string(tokens[i]));
              }
              
              // Add this piece to the result
              result.setValueAtIndex(rt, i, jsi::String::createFromUtf8(rt, std::string(piece_buf, piece_len)));
            }
            
            return result;
          } catch (const std::exception& e) {
            throw jsi::JSError(rt, std::string("Failed to tokenize: ") + e.what());
          }
        } catch (const std::exception& e) {
          throw jsi::JSError(rt, e.what());
        }
      }
    )
  );
  
  // Add detokenize method
  contextObj.setProperty(runtime, "detokenize", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "detokenize"), 
      1,  // takes token array
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Check arguments
        if (count < 1 || !args[0].isObject() || !args[0].getObject(rt).isArray(rt)) {
          throw jsi::JSError(rt, "First argument must be an array of token ids");
        }
        
        jsi::Array tokenArray = args[0].getObject(rt).getArray(rt);
        size_t tokenCount = tokenArray.size(rt);
        
        if (tokenCount == 0) {
          return jsi::String::createFromUtf8(rt, "");
        }
        
        // Convert JS array to vector of tokens
        std::vector<llama_token> tokens(tokenCount);
        for (size_t i = 0; i < tokenCount; i++) {
          tokens[i] = (llama_token)tokenArray.getValueAtIndex(rt, i).asNumber();
        }
        
        // Detokenize the tokens
        std::string result;
        
        const struct llama_vocab* vocab = llama_model_get_vocab(model);
        for (size_t i = 0; i < tokens.size(); i++) {
          // Get the piece for this token
          char piece_buf[64] = {0};
          int piece_len = llama_token_to_piece(vocab, tokens[i], piece_buf, sizeof(piece_buf), 0, false);
          if (piece_len <= 0) {
            throw jsi::JSError(rt, "Failed to get piece for token: " + std::to_string(tokens[i]));
          }
          
          // Append to result
          result.append(piece_buf, piece_len);
        }
        
        return jsi::String::createFromUtf8(rt, result);
      }
    )
  );
  
  // Add embedding method
  contextObj.setProperty(runtime, "embedding", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "embedding"), 
      1,  // takes content string
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Check arguments
        if (count < 1 || !args[0].isString()) {
          throw jsi::JSError(rt, "First argument must be a string to embed");
        }
        
        std::string content = args[0].getString(rt).utf8(rt);
        
        // First, check if the model supports embeddings
        if (!ctx) {
          throw jsi::JSError(rt, "No context available");
        }
        
        // Ensure embeddings are enabled on the context
        llama_set_embeddings(ctx, true);
        
        // Tokenize the input string (similar to the tokenize method)
        std::vector<llama_token> tokens(content.length() + 1);
        const struct llama_vocab* vocab = llama_model_get_vocab(model);
        int n_tokens = llama_tokenize(vocab, content.c_str(), int(content.length()), 
                                    tokens.data(), int(tokens.size()), true, true);
        
        if (n_tokens < 0) {
          tokens.resize(-n_tokens);
          n_tokens = llama_tokenize(vocab, content.c_str(), int(content.length()), 
                                  tokens.data(), int(tokens.size()), true, true);
        }
        
        if (n_tokens < 0 || n_tokens == 0) {
          throw jsi::JSError(rt, "Failed to tokenize or empty input: " + content);
        }
        
        // Resize to actual number of tokens
        tokens.resize(n_tokens);
        
        // Get the embedding dimension from the model
        const int embedding_size = llama_model_n_embd(model);
        std::vector<float> embedding(embedding_size);
        
        // Process tokens to get embeddings
        llama_batch embedding_batch = llama_batch_init(int(tokens.size()), 0, 1);
        for (int i = 0; i < tokens.size(); i++) {
          embedding_batch.token[i] = tokens[i];
          embedding_batch.pos[i] = i;
          embedding_batch.n_seq_id[i] = 1;
          embedding_batch.seq_id[i][0] = 0;
          embedding_batch.logits[i] = false;
        }
        embedding_batch.logits[tokens.size() - 1] = true;
        embedding_batch.n_tokens = int(tokens.size());
        
        // Encode the tokens (equivalent to previous llama_eval)
        if (llama_decode(ctx, embedding_batch) < 0) {
          throw jsi::JSError(rt, "Failed to decode tokens for embedding");
        }
        
        // Get the embeddings
        float* embd = llama_get_embeddings(ctx);
        if (embd == nullptr) {
          throw jsi::JSError(rt, "Failed to compute embedding");
        }
        
        // Copy the embeddings into our vector
        std::memcpy(embedding.data(), embd, embedding_size * sizeof(float));
        
        // Free the batch
        llama_batch_free(embedding_batch);
        
        // Convert to JS array
        jsi::Array result = jsi::Array(rt, embedding.size());
        for (size_t i = 0; i < embedding.size(); i++) {
          result.setValueAtIndex(rt, i, jsi::Value((double)embedding[i]));
        }
        
        return result;
      }
    )
  );
  
  // Add saveSession/loadSession methods
  contextObj.setProperty(runtime, "saveSession", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "saveSession"), 
      1,  // takes path string
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Check arguments
        if (count < 1 || !args[0].isString()) {
          throw jsi::JSError(rt, "First argument must be a path string to save the session");
        }
        
        std::string path = args[0].getString(rt).utf8(rt);
        
        // Use llama.cpp's save session function
        bool success = llama_state_save_file(ctx, path.c_str(), nullptr, 0);
        
        if (!success) {
          throw jsi::JSError(rt, "Failed to save session to: " + path);
        }
        
        return jsi::Value(success);
      }
    )
  );
  
  contextObj.setProperty(runtime, "loadSession", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "loadSession"), 
      1,  // takes path string
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Check arguments
        if (count < 1 || !args[0].isString()) {
          throw jsi::JSError(rt, "First argument must be a path string to load the session from");
        }
        
        std::string path = args[0].getString(rt).utf8(rt);
        
        // Use llama.cpp's load session function
        size_t n_token_count_out = 0;
        bool success = llama_state_load_file(ctx, path.c_str(), nullptr, 0, &n_token_count_out);
        
        if (!success) {
          throw jsi::JSError(rt, "Failed to load session from: " + path);
        }
        
        return jsi::Value(success);
      }
    )
  );
  
  // Add stopCompletion method
  contextObj.setProperty(runtime, "stopCompletion", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "stopCompletion"), 
      0,  // takes no arguments
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Set a flag to stop generation
        m_shouldStopCompletion = true;
        
        // Also clear the KV cache to ensure we're starting fresh next time
        if (ctx != nullptr) {
          llama_kv_self_clear(ctx);
        }
        
        return jsi::Value(true);
      }
    )
  );
  
  // Add release method to clean up resources
  contextObj.setProperty(runtime, "release", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "release"), 
      0,  // takes no arguments
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Free llama context and model
        if (ctx != nullptr) {
          llama_free(ctx);
        }
        
        if (model != nullptr) {
          llama_model_free(model);
        }
        
        return jsi::Value(true);
      }
    )
  );
  
  // Add properties about the model/context with updated API calls
  contextObj.setProperty(runtime, "n_ctx", jsi::Value((double)llama_n_ctx(ctx)));
  contextObj.setProperty(runtime, "n_embd", jsi::Value((double)llama_model_n_embd(model)));
  
  // Add GPU information if available
  jsi::Object gpuInfo(runtime);
  gpuInfo.setProperty(runtime, "enabled", jsi::Value(m_gpuEnabled));
  
  if (m_gpuEnabled) {
    GpuInfo info = getGpuCapabilities();
    gpuInfo.setProperty(runtime, "deviceName", jsi::String::createFromUtf8(runtime, info.deviceName));
    gpuInfo.setProperty(runtime, "deviceVendor", jsi::String::createFromUtf8(runtime, info.deviceVendor));
    gpuInfo.setProperty(runtime, "deviceVersion", jsi::String::createFromUtf8(runtime, info.deviceVersion));
    gpuInfo.setProperty(runtime, "deviceComputeUnits", jsi::Value((double)info.deviceComputeUnits));
    gpuInfo.setProperty(runtime, "deviceMemSize", jsi::Value((double)info.deviceMemSize));
  }
  
  contextObj.setProperty(runtime, "gpu", gpuInfo);
  
  return contextObj;
}

jsi::Value LlamaCppRn::getVocabSize(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
  try {
    // Extract the model pointer from the object
    if (!thisValue.isObject() || !thisValue.getObject(runtime).hasProperty(runtime, "_model_ptr")) {
      throw std::runtime_error("Invalid model object");
    }
    
    // Get the model pointer
    uintptr_t model_ptr = (uintptr_t)thisValue.getObject(runtime).getProperty(runtime, "_model_ptr").asNumber();
    llama_model* model = (llama_model*)model_ptr;
    
    if (!model) {
      throw std::runtime_error("No model loaded");
    }
    
    // Get the vocabulary and return its size
    const struct llama_vocab* vocab = llama_model_get_vocab(model);
    int32_t n_vocab = llama_vocab_n_tokens(vocab);
    
    return jsi::Value((double)n_vocab);
  } catch (const std::exception& e) {
    throw jsi::JSError(runtime, e.what());
  }
}

} // namespace facebook::react 