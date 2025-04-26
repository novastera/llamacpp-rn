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
#include <nlohmann/json.hpp>

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
    
    if (params.hasProperty(runtime, "embedding") && params.getProperty(runtime, "embedding").asBool()) {
      modelParams.embedding = true;
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
    
    // Load the model
    llama_model* model = llama_load_model_from_file(modelPath.c_str(), modelParams);
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
      // This is just a placeholder for now
      // contextParams.grammar = llama_grammar_init(grammar.c_str());
    }
    
    // Create the context
    llama_context* ctx = llama_new_context_with_model(model, contextParams);
    if (!ctx) {
      // Clean up the model if context creation fails
      llama_free_model(model);
      throw std::runtime_error("Failed to create context");
    }
    
    // Create a JavaScript object that represents the llama context
    auto result = createModelObject(runtime, model, ctx);
    
    return result;
  } catch (const std::exception& e) {
    // Handle errors and convert to JavaScript exceptions
    jsi::Object error = jsi::Object(runtime);
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
    return it->second;
  }
  
  try {
    // Create default model parameters for info loading
    llama_model_params params = llama_model_default_params();
    
    // Just load model info without loading the full model
    // This is a simplified approach - in a real implementation
    // we would use a more efficient API if llama.cpp provides one
    llama_model* model = llama_load_model_from_file(path.c_str(), params);
    
    if (!model) {
      throw std::runtime_error("Failed to load model info: " + path);
    }
    
    // Create result object with model information
    jsi::Object result = jsi::Object(runtime);
    
    // Extract model metadata
    result.setProperty(runtime, "n_params", jsi::Value((double)llama_model_n_params(model)));
    result.setProperty(runtime, "n_vocab", jsi::Value((double)llama_n_vocab(model)));
    result.setProperty(runtime, "n_context", jsi::Value((double)llama_n_ctx_train(model)));
    result.setProperty(runtime, "n_embd", jsi::Value((double)llama_n_embd(model)));
    
    // Get model description
    const char* description = llama_model_desc(model);
    result.setProperty(runtime, "description", 
                      jsi::String::createFromUtf8(runtime, description ? description : "No description available"));
    
    // Clean up
    llama_free_model(model);
    
    // Cache the result for future calls
    modelInfoCache_[path] = jsi::Value(runtime, result).getObject(runtime);
    
    return result;
  } catch (const std::exception& e) {
    // Handle errors and convert to JavaScript exceptions
    jsi::Object error = jsi::Object(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

jsi::Value LlamaCppRn::jsonSchemaToGbnf(jsi::Runtime &runtime, jsi::Object schema) {
  // Thread-safe implementation with a lock for consistency
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Convert JSI object to nlohmann::json for easier manipulation
    nlohmann::json schema_json;
    
    // Helper function to convert JSI value to nlohmann::json
    std::function<nlohmann::json(const jsi::Value&)> convertJsiValueToJson = 
      [&runtime, &convertJsiValueToJson](const jsi::Value& value) -> nlohmann::json {
        if (value.isNull()) {
          return nullptr;
        } else if (value.isBool()) {
          return value.getBool();
        } else if (value.isNumber()) {
          return value.getNumber();
        } else if (value.isString()) {
          return value.getString(runtime).utf8(runtime);
        } else if (value.isObject()) {
          auto obj = value.getObject(runtime);
          
          // Handle arrays
          if (obj.isArray(runtime)) {
            auto array = obj.getArray(runtime);
            nlohmann::json result = nlohmann::json::array();
            for (size_t i = 0; i < array.size(runtime); i++) {
              result.push_back(convertJsiValueToJson(array.getValueAtIndex(runtime, i)));
            }
            return result;
          }
          
          // Regular object
          nlohmann::json result = nlohmann::json::object();
          auto propertyNames = obj.getPropertyNames(runtime);
          for (size_t i = 0; i < propertyNames.size(runtime); i++) {
            auto name = propertyNames.getValueAtIndex(runtime, i).getString(runtime).utf8(runtime);
            result[name] = convertJsiValueToJson(obj.getProperty(runtime, name.c_str()));
          }
          return result;
        } else {
          // undefined or other types
          return nullptr;
        }
      };
    
    // Convert the entire schema
    schema_json = convertJsiValueToJson(jsi::Value(runtime, schema));
    
    // Basic JSON Schema to GBNF conversion logic
    // This is a simplified implementation - more complex schemas would require more complex rules
    std::ostringstream gbnf;
    
    // Start with root definition
    gbnf << "root ::= value\n\n";
    
    // Basic types
    gbnf << "value ::= object | array | string | number | true | false | null\n\n";
    
    // Object definition
    gbnf << "object ::= \"{\" ws (pair (\"," ws pair)*)? \"}\" ws\n";
    gbnf << "pair ::= string \":\" ws value\n\n";
    
    // Array definition
    gbnf << "array ::= \"[\" ws (value (\"," ws value)*)? \"]\" ws\n\n";
    
    // String definition with escapes
    gbnf << "string ::= \"\\\"\" (char)* \"\\\"\" ws\n";
    gbnf << "char ::= [^\"\\\\] | \"\\\\\" ([\"\\\\bfnrt] | \"u\" [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F] [0-9a-fA-F])\n\n";
    
    // Number definition
    gbnf << "number ::= int frac? exp? ws\n";
    gbnf << "int ::= \"-\"? ([0-9] | [1-9] [0-9]+)\n";
    gbnf << "frac ::= \".\" [0-9]+\n";
    gbnf << "exp ::= [eE] [\+\-]? [0-9]+\n\n";
    
    // Constants
    gbnf << "true ::= \"true\" ws\n";
    gbnf << "false ::= \"false\" ws\n";
    gbnf << "null ::= \"null\" ws\n\n";
    
    // Whitespace
    gbnf << "ws ::= ([ \\t\\n\\r]*)\n";
    
    // If we have a properties dictionary in the schema, we can enhance with specific fields
    if (schema_json.contains("properties") && schema_json["properties"].is_object()) {
      gbnf << "\n# Schema-specific object rules\n";
      gbnf << "# For schema with properties: " << schema_json["properties"].size() << "\n";
      
      // Add object-specific rule with required properties
      std::vector<std::string> required;
      if (schema_json.contains("required") && schema_json["required"].is_array()) {
        for (const auto& req : schema_json["required"]) {
          if (req.is_string()) {
            required.push_back(req.get<std::string>());
          }
        }
      }
      
      // Add object-specific rules (more complex implementation would be needed for full schema support)
    }
    
    return jsi::String::createFromUtf8(runtime, gbnf.str());
  } catch (const std::exception& e) {
    // Handle errors and convert to JavaScript exceptions
    jsi::Object error = jsi::Object(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

bool LlamaCppRn::detectGpuCapabilities() {
  // Platform-specific GPU detection
#if defined(__APPLE__)
  // On iOS/macOS, check for Metal support
  return llama_backend_metal_init();
#elif defined(__ANDROID__) && defined(LLAMACPPRN_OPENCL_ENABLED) && LLAMACPPRN_OPENCL_ENABLED == 1
  // On Android with OpenCL enabled, check for OpenCL support
  // This is a simplified check - in reality we'd do more thorough detection
  return true; // Will be refined based on actual detection in getGpuCapabilities
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
    // Let llama.cpp handle initialization based on platform
    bool success = false;
    
#if defined(__APPLE__)
    success = llama_backend_metal_init();
#elif defined(GGML_USE_CLBLAST)
    success = llama_backend_has_cl(); // This should already be initialized by llama.cpp
#endif

    if (success) {
      m_gpuEnabled = true;
      return true;
    }
    return false;
  } else {
    // Disable GPU
#if defined(__APPLE__)
    llama_backend_metal_free();
#endif
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
  
#if defined(__APPLE__)
  // Use llama.cpp's Metal detection
  if (llama_backend_has_metal()) {
    info.available = true;
    info.deviceName = "Apple GPU";
    info.deviceVendor = "Apple";
    info.deviceVersion = "Metal";
    // We could improve this with more device-specific detection in the future
  }
#elif defined(GGML_USE_CLBLAST)
  // Use llama.cpp's OpenCL detection
  if (llama_backend_has_cl()) {
    info.available = true;
    info.deviceName = "OpenCL Device";
    info.deviceVendor = "OpenCL";
    info.deviceVersion = "OpenCL";
    // Could be enhanced with specific OpenCL device info if needed
  }
#endif

  return info;
}

jsi::Object LlamaCppRn::createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx) {
  // Create the context object
  jsi::Object contextObj = jsi::Object(runtime);
  
  // Store native pointers in the object for future references
  // Note: In a real implementation, we'd use a more robust solution like HostObjects
  // or a reference map to avoid exposing raw pointers to JavaScript
  
  // We're storing these as numbers for simplicity, but in a real implementation
  // we'd use a better approach to manage these references
  contextObj.setProperty(runtime, "_model_ptr", jsi::Value((double)(uintptr_t)model));
  contextObj.setProperty(runtime, "_ctx_ptr", jsi::Value((double)(uintptr_t)ctx));
  
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
          
          // Convert JSI array to nlohmann::json
          nlohmann::json messages_json = nlohmann::json::array();
          for (size_t i = 0; i < messages_array.size(rt); i++) {
            if (!messages_array.getValueAtIndex(rt, i).isObject()) {
              continue;
            }
            
            jsi::Object message = messages_array.getValueAtIndex(rt, i).getObject(rt);
            nlohmann::json msg_json = nlohmann::json::object();
            
            // Extract required fields
            if (message.hasProperty(rt, "role") && message.getProperty(rt, "role").isString()) {
              msg_json["role"] = message.getProperty(rt, "role").getString(rt).utf8(rt);
            } else {
              continue;
            }
            
            if (message.hasProperty(rt, "content")) {
              if (message.getProperty(rt, "content").isString()) {
                msg_json["content"] = message.getProperty(rt, "content").getString(rt).utf8(rt);
              } else if (message.getProperty(rt, "content").isNull()) {
                msg_json["content"] = nullptr;
              } else {
                continue;
              }
            } else {
              continue;
            }
            
            // Extract optional fields
            if (message.hasProperty(rt, "name") && message.getProperty(rt, "name").isString()) {
              msg_json["name"] = message.getProperty(rt, "name").getString(rt).utf8(rt);
            }
            
            if (message.hasProperty(rt, "tool_call_id") && message.getProperty(rt, "tool_call_id").isString()) {
              msg_json["tool_call_id"] = message.getProperty(rt, "tool_call_id").getString(rt).utf8(rt);
            }
            
            messages_json.push_back(msg_json);
          }
          
          // Get the template name (defaulting to a reasonable value)
          std::string template_name = "chatml"; // default
          if (params.hasProperty(rt, "template") && params.getProperty(rt, "template").isString()) {
            template_name = params.getProperty(rt, "template").getString(rt).utf8(rt);
          }
          
          try {
            // Use our ChatTemplates module to handle the messages and apply the template
            auto chat_messages = chat_templates::messages_from_json(messages_json);
            prompt = chat_templates::apply_chat_template(model, chat_messages, template_name);
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
        
        // Check for grammar constraint
        llama_grammar* grammar = nullptr;
        if (params.hasProperty(rt, "grammar") && params.getProperty(rt, "grammar").isString()) {
          std::string grammar_str = params.getProperty(rt, "grammar").getString(rt).utf8(rt);
          
          if (!grammar_str.empty()) {
            // Create a grammar
            grammar = llama_grammar_init(grammar_str.c_str(), 1, nullptr);
            
            if (!grammar) {
              throw jsi::JSError(rt, "Failed to parse grammar string");
            }
          }
        }
        
        // Check for streaming callback
        bool streaming = false;
        std::shared_ptr<jsi::Function> callback;
        if (count > 1 && args[1].isObject() && args[1].getObject(rt).isFunction(rt)) {
          streaming = true;
          callback = std::make_shared<jsi::Function>(args[1].getObject(rt).getFunction(rt));
        }
        
        // Clear any existing KV cache first (to start fresh)
        llama_kv_cache_tokens_rm(ctx, -1, -1);
        
        // Tokenize the prompt
        std::vector<llama_token> tokens(prompt.length() + 1);
        int n_tokens = llama_tokenize(model, prompt.c_str(), prompt.length(), 
                                     tokens.data(), tokens.size(), true, true);
        
        if (n_tokens < 0) {
          tokens.resize(-n_tokens);
          n_tokens = llama_tokenize(model, prompt.c_str(), prompt.length(), 
                                   tokens.data(), tokens.size(), true, true);
        }
        
        if (n_tokens < 0 || n_tokens == 0) {
          throw jsi::JSError(rt, "Failed to tokenize or empty prompt: " + prompt);
        }
        
        // Resize to actual number of tokens
        tokens.resize(n_tokens);
        
        // Timing variables
        auto start_prompt = std::chrono::high_resolution_clock::now();
        
        // Process the prompt tokens
        if (llama_eval(ctx, tokens.data(), tokens.size(), 0, nullptr) < 0) {
          throw jsi::JSError(rt, "Failed to evaluate prompt tokens");
        }
        
        auto end_prompt = std::chrono::high_resolution_clock::now();
        
        // Sampling parameters
        llama_sampling_params sampling_params = {};
        sampling_params.temp = temperature;
        sampling_params.top_p = top_p;
        sampling_params.n_prev = 64;  // Consider last 64 tokens for repetition penalty
        sampling_params.repeat_penalty = 1.1f;
        sampling_params.grammar = grammar;  // Add the grammar if available
        
        // Create a sampling context
        llama_sampling_context * sampling_ctx = llama_sampling_init(sampling_params);
        
        // Generate completion
        int n_predict = max_tokens;
        int n_remain = n_predict;
        
        std::string completion_text;
        int n_gen = 0;
        
        auto start_gen = std::chrono::high_resolution_clock::now();
        
        // Main generation loop
        while (n_remain > 0) {
          // Check if we need to stop
          if (m_shouldStopCompletion.load()) {
            // Reset the flag for next time
            m_shouldStopCompletion = false;
            break;
          }
          
          // Get logits for the next token
          llama_token id = llama_sampling_sample(sampling_ctx, ctx, nullptr);
            
          // Check for end of sequence
          if (id == llama_token_eos(model)) {
            break;
          }
          
          // Convert token to text
          const char* piece = llama_token_to_piece(model, id, nullptr);
          if (piece == nullptr) {
            continue;
          }
          
          // Append to result
          std::string token_text(piece);
          completion_text += token_text;
          n_gen++;
          
          // If streaming, call the callback with this token
          if (streaming && callback) {
            jsi::Object token_obj = jsi::Object(rt);
            token_obj.setProperty(rt, "token", jsi::String::createFromUtf8(rt, token_text));
            callback->call(rt, token_obj);
          }
          
          // Add token to evaluate next
          std::vector<llama_token> next_tokens = { id };
          
          // Evaluate
          if (llama_eval(ctx, next_tokens.data(), next_tokens.size(), tokens.size() + n_gen - 1, nullptr) < 0) {
            break;
          }
          
          // Decrement remaining
          n_remain--;
        }
        
        auto end_gen = std::chrono::high_resolution_clock::now();
        
        // Clean up sampling context
        llama_sampling_free(sampling_ctx);
        
        // Clean up grammar if we created one
        if (grammar != nullptr) {
          llama_grammar_free(grammar);
        }
        
        // Calculate timings
        auto prompt_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_prompt - start_prompt).count();
        auto gen_duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_gen - start_gen).count();
        auto total_duration = prompt_duration + gen_duration;
        
        // Create result object
        jsi::Object result = jsi::Object(rt);
        result.setProperty(rt, "text", jsi::String::createFromUtf8(rt, completion_text));
        result.setProperty(rt, "tokens_predicted", jsi::Value((double)n_gen));
        
        // Parse tool calls if requested and present in the output
        if (params.hasProperty(rt, "tools") && params.getProperty(rt, "tools").isObject() && 
            params.getProperty(rt, "tools").getObject(rt).isArray(rt)) {
          
          // Use our ChatTemplates module to parse tool calls
          nlohmann::json tool_call_json = chat_templates::parse_tool_call(completion_text);
          
          // If a tool call was found, convert it to a JSI object
          if (!tool_call_json.is_null()) {
            jsi::Array toolCalls = jsi::Array(rt, 1);
            jsi::Object toolCall = jsi::Object(rt);
            
            // Extract and set the tool call properties
            toolCall.setProperty(rt, "id", jsi::String::createFromUtf8(rt, tool_call_json["id"].get<std::string>()));
            toolCall.setProperty(rt, "type", jsi::String::createFromUtf8(rt, tool_call_json["type"].get<std::string>()));
            
            // Create function object
            jsi::Object functionObj = jsi::Object(rt);
            functionObj.setProperty(rt, "name", jsi::String::createFromUtf8(rt, 
                                   tool_call_json["function"]["name"].get<std::string>()));
            functionObj.setProperty(rt, "arguments", jsi::String::createFromUtf8(rt, 
                                   tool_call_json["function"]["arguments"].get<std::string>()));
            
            toolCall.setProperty(rt, "function", functionObj);
            
            // Add to tool calls array
            toolCalls.setValueAtIndex(rt, 0, toolCall);
            
            // Add tool calls to result
            result.setProperty(rt, "tool_calls", toolCalls);
          }
        }
        
        // Create timing object
        jsi::Object timings = jsi::Object(rt);
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
        // Check arguments
        if (count < 1 || !args[0].isString()) {
          throw jsi::JSError(rt, "First argument must be a string to tokenize");
        }
        
        std::string content = args[0].getString(rt).utf8(rt);
        
        // Allocate a buffer for the tokens
        // Start with a reasonable size (each token is ~4 chars on average)
        std::vector<llama_token> tokens(content.length() + 1);
        
        // Use llama.cpp's tokenize function
        int n_tokens = llama_tokenize(model, content.c_str(), content.length(), 
                                      tokens.data(), tokens.size(), true, true);
        
        if (n_tokens < 0) {
          // If the buffer was too small, resize and try again
          tokens.resize(-n_tokens);
          n_tokens = llama_tokenize(model, content.c_str(), content.length(), 
                                    tokens.data(), tokens.size(), true, true);
        }
        
        if (n_tokens < 0) {
          throw jsi::JSError(rt, "Failed to tokenize string: " + content);
        }
        
        // Resize to actual number of tokens
        tokens.resize(n_tokens);
        
        // Convert to JS array
        jsi::Array result = jsi::Array(rt, tokens.size());
        for (size_t i = 0; i < tokens.size(); i++) {
          result.setValueAtIndex(rt, i, jsi::Value((double)tokens[i]));
        }
        
        return result;
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
        
        for (size_t i = 0; i < tokens.size(); i++) {
          // Get the piece for this token
          const char* piece = llama_token_to_piece(model, tokens[i], nullptr);
          if (piece == nullptr) {
            throw jsi::JSError(rt, "Failed to get piece for token: " + std::to_string(tokens[i]));
          }
          
          // Append to result
          result.append(piece);
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
        if (!llama_model_has_embeddings(model)) {
          throw jsi::JSError(rt, "Model does not support embeddings");
        }
        
        // Tokenize the input string (similar to the tokenize method)
        std::vector<llama_token> tokens(content.length() + 1);
        int n_tokens = llama_tokenize(model, content.c_str(), content.length(), 
                                     tokens.data(), tokens.size(), true, true);
        
        if (n_tokens < 0) {
          tokens.resize(-n_tokens);
          n_tokens = llama_tokenize(model, content.c_str(), content.length(), 
                                   tokens.data(), tokens.size(), true, true);
        }
        
        if (n_tokens < 0 || n_tokens == 0) {
          throw jsi::JSError(rt, "Failed to tokenize or empty input: " + content);
        }
        
        // Resize to actual number of tokens
        tokens.resize(n_tokens);
        
        // Compute the embedding for these tokens
        // Get the embedding dimension from the model
        const int embedding_size = llama_n_embd(model);
        std::vector<float> embedding(embedding_size);
        
        // Get the embedding
        if (!llama_get_embeddings(ctx, embedding.data())) {
          throw jsi::JSError(rt, "Failed to compute embedding");
        }
        
        // Evaluate tokens to get the embedding
        if (llama_eval(ctx, tokens.data(), tokens.size(), 0, nullptr) < 0) {
          throw jsi::JSError(rt, "Failed to evaluate tokens for embedding");
        }
        
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
        
        // Use llama.cpp's KV cache save function
        bool success = llama_kv_cache_tokens_save(ctx, path.c_str());
        
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
        
        // Use llama.cpp's KV cache load function
        bool success = llama_kv_cache_tokens_load(ctx, path.c_str());
        
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
          llama_kv_cache_clear(ctx);
          llama_reset_timings(ctx);
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
          llama_free_model(model);
        }
        
        return jsi::Value(true);
      }
    )
  );
  
  // Add properties about the model/context
  contextObj.setProperty(runtime, "n_ctx", jsi::Value((double)llama_n_ctx(ctx)));
  contextObj.setProperty(runtime, "n_vocab", jsi::Value((double)llama_n_vocab(model)));
  contextObj.setProperty(runtime, "n_embd", jsi::Value((double)llama_n_embd(model)));
  
  // Add GPU information if available
  jsi::Object gpuInfo = jsi::Object(runtime);
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

} // namespace facebook::react 