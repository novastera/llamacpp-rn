#include "LlamaCppRnModule.h"
#include <jsi/jsi.h>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <thread>

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
    // In a real implementation, this would use llama.cpp's json schema to GBNF conversion
    // For now, we'll implement a simplified version that handles basic JSON schema structures
    
    // Default JSON GBNF grammar
    std::string gbnf = R"(
root   ::= object
value  ::= object | array | string | number | ("true" | "false" | "null") ws

object ::=
  "{" ws (
            string ":" ws value
    ("," ws string ":" ws value)*
  )? "}" ws

array  ::=
  "[" ws (
            value
    ("," ws value)*
  )? "]" ws

string ::=
  "\"" (
    [^"\\\x7F\x00-\x1F] |
    "\\" (["\\bfnrt] | "u" [0-9a-fA-F]{4}) # escapes
  )* "\"" ws

number ::= ("-"? ([0-9] | [1-9] [0-9]{0,15})) ("." [0-9]+)? ([eE] [-+]? [0-9] [1-9]{0,15})? ws

# Optional space: by convention, applied in this grammar after literal chars when allowed
ws ::= | " " | "\n" [ \t]{0,20}
)";

    // In the actual implementation, this would analyze the schema and generate
    // a custom GBNF grammar that enforces the schema constraints.
    // We would look at:
    // - Required properties
    // - Property types
    // - Enums
    // - Nested objects
    // - Arrays with specific item types

    return jsi::String::createFromUtf8(runtime, gbnf);
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
    // Already in requested state
    return true;
  }
  
  if (enable) {
    // Try to enable GPU
    if (!detectGpuCapabilities()) {
      return false;
    }
    
    // Successfully enabled GPU
    m_gpuEnabled = true;
    return true;
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
  // Check for Metal support on iOS/macOS
  if (llama_backend_metal_init()) {
    // Get Metal device info - these are placeholder values
    // In a real implementation, we'd query the Metal device properties
    info.available = true;
    info.deviceName = "Apple GPU";
    info.deviceVendor = "Apple";
    info.deviceVersion = "Metal";
    info.deviceComputeUnits = 8; // Placeholder - would detect actual value
    info.deviceMemSize = 4 * 1024 * 1024 * 1024ULL; // Placeholder - 4GB
  }
#elif defined(__ANDROID__) && defined(LLAMACPPRN_OPENCL_ENABLED) && LLAMACPPRN_OPENCL_ENABLED == 1
  // On Android with OpenCL, check OpenCL devices
  // This is a simplified implementation - actual code would use OpenCL APIs
  // to query available devices and their capabilities
  
  // Placeholder detection - in real implementation we'd use proper OpenCL APIs
  info.available = true; 
  info.deviceName = "Mobile GPU";
  info.deviceVendor = "Unknown";
  info.deviceVersion = "OpenCL 2.0";
  info.deviceComputeUnits = 4; // Placeholder
  info.deviceMemSize = 2 * 1024 * 1024 * 1024ULL; // Placeholder - 2GB

  // TODO: Actual OpenCL detection code
  // 1. Initialize OpenCL
  // 2. Query platforms and devices
  // 3. Select best device and get its properties
  // 4. Populate the info structure with real values
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
        
        // Function to handle completion - to be implemented
        // This is a placeholder that would be replaced with actual implementation
        
        // Return a result object with text and timings
        jsi::Object result = jsi::Object(rt);
        result.setProperty(rt, "text", jsi::String::createFromUtf8(rt, "Sample completion text"));
        
        // Create timing object
        jsi::Object timings = jsi::Object(rt);
        timings.setProperty(rt, "prompt_n", jsi::Value(10.0));
        timings.setProperty(rt, "prompt_ms", jsi::Value(100.0));
        timings.setProperty(rt, "predicted_n", jsi::Value(50.0));
        timings.setProperty(rt, "predicted_ms", jsi::Value(500.0));
        timings.setProperty(rt, "total_ms", jsi::Value(600.0));
        
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
        
        // Placeholder implementation - would use llama_tokenize in real code
        jsi::Array tokens = jsi::Array(rt, 3);
        tokens.setValueAtIndex(rt, 0, jsi::Value(1.0));
        tokens.setValueAtIndex(rt, 1, jsi::Value(2.0));
        tokens.setValueAtIndex(rt, 2, jsi::Value(3.0));
        
        return tokens;
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
        
        // Placeholder implementation - would use llama_token_to_piece in real code
        return jsi::String::createFromUtf8(rt, "Detokenized text");
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
        
        // Placeholder implementation - would generate real embeddings in actual code
        jsi::Array embedding = jsi::Array(rt, 5);
        for (int i = 0; i < 5; i++) {
          embedding.setValueAtIndex(rt, i, jsi::Value(0.1 * i));
        }
        
        return embedding;
      }
    )
  );
  
  // Add saveSession/loadSession methods
  contextObj.setProperty(runtime, "saveSession", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "saveSession"), 
      1,  // takes path string
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Placeholder
        return jsi::Value(true);
      }
    )
  );
  
  contextObj.setProperty(runtime, "loadSession", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "loadSession"), 
      1,  // takes path string
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Placeholder
        return jsi::Value(true);
      }
    )
  );
  
  // Add stopCompletion method
  contextObj.setProperty(runtime, "stopCompletion", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "stopCompletion"), 
      0,  // takes no arguments
      [this, model, ctx](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Placeholder implementation
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