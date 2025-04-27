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

// Include the llama.cpp headers directly
#include "llama.h"

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <sys/sysctl.h>
#endif

namespace facebook::react {

// Host function definitions
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

static jsi::Value __hostFunction_LlamaCppRnSpecGetGPUInfo(
    jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value *args, size_t count) {
  return static_cast<LlamaCppRn *>(&turboModule)->getGPUInfo(rt);
}

static jsi::Value __hostFunction_LlamaCppRnSpecGetAbsolutePath(
    jsi::Runtime &rt, TurboModule &turboModule, const jsi::Value *args, size_t count) {
  return static_cast<LlamaCppRn *>(&turboModule)->getAbsolutePath(rt, args[0].getString(rt));
}

LlamaCppRn::LlamaCppRn(std::shared_ptr<CallInvoker> jsInvoker)
    : TurboModule(kModuleName, std::move(jsInvoker)) {
  // Initialize and register methods
  methodMap_["initLlama"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecInitLlama};
  methodMap_["loadLlamaModelInfo"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecLoadLlamaModelInfo};
  methodMap_["jsonSchemaToGbnf"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecJsonSchemaToGbnf};
  methodMap_["getGPUInfo"] = MethodMetadata{0, __hostFunction_LlamaCppRnSpecGetGPUInfo};
  methodMap_["getAbsolutePath"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecGetAbsolutePath};
}

std::shared_ptr<TurboModule> LlamaCppRn::create(std::shared_ptr<CallInvoker> jsInvoker) {
  return std::make_shared<LlamaCppRn>(std::move(jsInvoker));
}

std::string LlamaCppRn::normalizeFilePath(const std::string& path) {
    // Remove file:// prefix if present
    if (path.substr(0, 7) == "file://") {
        return path.substr(7);
    }
    return path;
}

jsi::Value LlamaCppRn::loadLlamaModelInfo(jsi::Runtime &runtime, jsi::String modelPath) {
  // Thread-safe implementation with a lock for the cache
  std::lock_guard<std::mutex> lock(mutex_);
  std::string path = normalizeFilePath(modelPath.utf8(runtime));
  
  // Check if we already have this model info cached
  auto it = modelInfoCache_.find(path);
  if (it != modelInfoCache_.end()) {
    return jsi::Value(runtime, *(it->second));
  }
  
  try {
    // Initialize llama backend
    llama_backend_init();
    
    // Create model params
    llama_model_params params = llama_model_default_params();
    
    // Always use CPU for model info loading
    params.n_gpu_layers = 0;
    
    // Load the model
    llama_model* model = llama_model_load_from_file(path.c_str(), params);
    
    if (!model) {
      throw std::runtime_error("Failed to load model from file: " + path);
    }
    
    // Create result object
    jsi::Object result(runtime);
    
    // Get model parameters
    result.setProperty(runtime, "n_params", jsi::Value((double)llama_model_n_params(model)));
    
    // Get vocabulary
    const llama_vocab* vocab = llama_model_get_vocab(model);
    result.setProperty(runtime, "n_vocab", jsi::Value((double)llama_vocab_n_tokens(vocab)));
    
    // Get context size
    result.setProperty(runtime, "n_context", jsi::Value((double)llama_model_n_ctx_train(model)));
    
    // Get embedding size
    result.setProperty(runtime, "n_embd", jsi::Value((double)llama_model_n_embd(model)));
    
    // Get model description
    char buf[512];
    llama_model_desc(model, buf, sizeof(buf));
    result.setProperty(runtime, "description", 
                      jsi::String::createFromUtf8(runtime, buf[0] ? buf : "Unknown model"));
    
    // Check if Metal/GPU is supported
    bool gpuSupported = llama_supports_gpu_offload();
    result.setProperty(runtime, "gpuSupported", jsi::Value(gpuSupported));
    
    // Add architecture info
    result.setProperty(runtime, "architecture", 
                      jsi::String::createFromUtf8(runtime, "Unknown"));
    
    // Free the model
    llama_model_free(model);
    
    // Cache the result
    auto resultPtr = std::make_shared<jsi::Object>(std::move(result));
    modelInfoCache_[path] = resultPtr;
    
    // Return a copy of the cached object
    return jsi::Value(runtime, *resultPtr);
  } catch (const std::exception& e) {
    jsi::Object error(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

jsi::Value LlamaCppRn::initLlama(jsi::Runtime &runtime, jsi::Object params) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Check if model param exists
    if (!params.hasProperty(runtime, "model")) {
      throw std::runtime_error("Missing required parameter: model");
    }
    
    std::string modelPath = normalizeFilePath(params.getProperty(runtime, "model").asString(runtime).utf8(runtime));
    
    // Initialize llama backend
    llama_backend_init();
    
    // Create model params
    llama_model_params modelParams = llama_model_default_params();
    
    // Get optional parameters
    if (params.hasProperty(runtime, "n_gpu_layers")) {
      modelParams.n_gpu_layers = (int)params.getProperty(runtime, "n_gpu_layers").asNumber();
    }
    
    if (params.hasProperty(runtime, "use_mlock") && params.getProperty(runtime, "use_mlock").asBool()) {
      modelParams.use_mlock = true;
    }
    
    // Check if GPU is supported
    bool gpuSupported = llama_supports_gpu_offload();
    if (gpuSupported && modelParams.n_gpu_layers > 0) {
      enableGpu(true);
    } else {
      if (modelParams.n_gpu_layers > 0) {
        modelParams.n_gpu_layers = 0;
      }
      enableGpu(false);
    }
    
    // Load the model
    llama_model* model = llama_model_load_from_file(modelPath.c_str(), modelParams);
    if (!model) {
      throw std::runtime_error("Failed to load model: " + modelPath);
    }
    
    // Create context params
    llama_context_params contextParams = llama_context_default_params();
    
    // Get optional context parameters
    if (params.hasProperty(runtime, "n_ctx")) {
      contextParams.n_ctx = (int)params.getProperty(runtime, "n_ctx").asNumber();
    }
    
    if (params.hasProperty(runtime, "n_batch")) {
      contextParams.n_batch = (int)params.getProperty(runtime, "n_batch").asNumber();
    }
    
    // Get thread count
    if (params.hasProperty(runtime, "n_threads")) {
      contextParams.n_threads = (int)params.getProperty(runtime, "n_threads").asNumber();
    } else {
      // Default to hardware concurrency - 1 (leave one core for UI thread)
      int cpuCores = std::thread::hardware_concurrency();
      contextParams.n_threads = std::max(1, cpuCores - 1);
    }
    
    // Create context
    llama_context* ctx = llama_init_from_model(model, contextParams);
    if (!ctx) {
      llama_model_free(model);
      throw std::runtime_error("Failed to create context");
    }
    
    // Create and return the model object
    auto result = createModelObject(runtime, model, ctx);
    
    return result;
  } catch (const std::exception& e) {
    jsi::Object error(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

jsi::Value LlamaCppRn::jsonSchemaToGbnf(jsi::Runtime &runtime, jsi::Object schema) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Simple implementation for JSON schema to GBNF conversion
    std::ostringstream gbnf;
    
    // Basic JSON grammar
    gbnf << "root ::= value\n\n";
    gbnf << "value ::= object | array | string | number | true | false | null\n\n";
    gbnf << "object ::= \"{\" ws (pair (\",\" ws pair)*)? \"}\" ws\n";
    gbnf << "pair ::= string \":\" ws value\n\n";
    gbnf << "array ::= \"[\" ws (value (\",\" ws value)*)? \"]\" ws\n\n";
    gbnf << "string ::= quote ([^\\\\\"]* | escaped)* quote\n";
    gbnf << "escaped ::= \"\\\\\\\\\"\n";
    gbnf << "quote ::= \"\\\"\"\n\n";
    gbnf << "number ::= int frac? exp? ws\n";
    gbnf << "int ::= \"-\"? (\"0\" | [1-9] [0-9]*)\n";
    gbnf << "frac ::= \".\" [0-9]+\n";
    gbnf << "exp ::= [eE] [+\\-]? [0-9]+\n\n";
    gbnf << "true ::= \"true\" ws\n";
    gbnf << "false ::= \"false\" ws\n";
    gbnf << "null ::= \"null\" ws\n\n";
    gbnf << "ws ::= [ \\t\\n\\r]*";
    
    return jsi::String::createFromUtf8(runtime, gbnf.str());
  } catch (const std::exception& e) {
    jsi::Object error(runtime);
    error.setProperty(runtime, "message", jsi::String::createFromUtf8(runtime, e.what()));
    throw jsi::JSError(runtime, error.getProperty(runtime, "message").asString(runtime));
  }
}

bool LlamaCppRn::detectGpuCapabilities() {
  llama_backend_init();
  return llama_supports_gpu_offload();
}

bool LlamaCppRn::enableGpu(bool enable) {
  if (enable == m_gpuEnabled) {
    return true;  // Already in desired state
  }
  
  if (enable) {
    // Check if GPU is available
    if (llama_supports_gpu_offload()) {
      m_gpuEnabled = true;
      return true;
    }
    return false;
  } else {
    m_gpuEnabled = false;
    return true;
  }
}

bool LlamaCppRn::isGpuEnabled() {
  return m_gpuEnabled;
}

LlamaCppRn::GpuInfo LlamaCppRn::getGpuCapabilities() {
  GpuInfo info;
  
  // Default values
  info.available = false;
  info.deviceName = "CPU Only";
  info.deviceVendor = "N/A";
  info.deviceVersion = "N/A";
  info.deviceComputeUnits = 0;
  info.deviceMemSize = 0;
  
  // Check if GPU is supported
  llama_backend_init();
  
  if (llama_supports_gpu_offload()) {
    info.available = true;
    
    // Platform-specific GPU details
#if defined(__APPLE__)
    info.deviceName = "Apple GPU";
    info.deviceVendor = "Apple";
    info.deviceVersion = "Metal";
    
    // Get basic hardware info
    size_t size;
    uint64_t memsize = 0;
    
    size = sizeof(memsize);
    if (sysctlbyname("hw.memsize", &memsize, &size, NULL, 0) == 0) {
      info.deviceMemSize = memsize / 3;  // Estimated GPU memory
    }
    
    // Estimate compute units
#if TARGET_OS_IPHONE
    info.deviceComputeUnits = 8;  // iOS estimate
#else
    info.deviceComputeUnits = 16; // macOS estimate 
#endif

#elif defined(GGML_USE_CLBLAST)
    info.deviceName = "OpenCL GPU";
    info.deviceVendor = "OpenCL";
    info.deviceVersion = "OpenCL";
#endif
  }
  
  return info;
}

jsi::Value LlamaCppRn::getGPUInfo(jsi::Runtime &runtime) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  jsi::Object result(runtime);
  
  // Initialize
  llama_backend_init();
  
  // Check if GPU is supported
  bool gpuSupported = llama_supports_gpu_offload();
  result.setProperty(runtime, "isSupported", jsi::Value(gpuSupported));
  
  // Get detailed info
  GpuInfo info = getGpuCapabilities();
  
  result.setProperty(runtime, "available", jsi::Value(info.available));
  result.setProperty(runtime, "deviceName", jsi::String::createFromUtf8(runtime, info.deviceName));
  result.setProperty(runtime, "deviceVendor", jsi::String::createFromUtf8(runtime, info.deviceVendor));
  result.setProperty(runtime, "deviceVersion", jsi::String::createFromUtf8(runtime, info.deviceVersion));
  result.setProperty(runtime, "deviceComputeUnits", jsi::Value((double)info.deviceComputeUnits));
  result.setProperty(runtime, "deviceMemorySize", jsi::Value((double)info.deviceMemSize));
  
  // Add implementation info
#if defined(__APPLE__)
  result.setProperty(runtime, "implementation", jsi::String::createFromUtf8(runtime, "Metal"));
  result.setProperty(runtime, "metalEnabled", jsi::Value(m_gpuEnabled));
#elif defined(GGML_USE_CLBLAST)
  result.setProperty(runtime, "implementation", jsi::String::createFromUtf8(runtime, "OpenCL"));
#else
  result.setProperty(runtime, "implementation", jsi::String::createFromUtf8(runtime, "CPU"));
#endif
  
  return result;
}

jsi::Value LlamaCppRn::getAbsolutePath(jsi::Runtime &runtime, jsi::String relativePath) {
  std::string path = relativePath.utf8(runtime);
  std::string normalizedPath = normalizeFilePath(path);
  
  jsi::Object result(runtime);
  result.setProperty(runtime, "relativePath", jsi::String::createFromUtf8(runtime, path));
  
  // Check if file exists
  FILE* file = fopen(normalizedPath.c_str(), "rb");
  bool exists = (file != nullptr);
  
  if (exists) {
    // Get file size
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fclose(file);
    
    jsi::Object attributes(runtime);
    attributes.setProperty(runtime, "size", jsi::Value((double)fileSize));
    result.setProperty(runtime, "attributes", attributes);
  }
  
  result.setProperty(runtime, "path", jsi::String::createFromUtf8(runtime, normalizedPath));
  result.setProperty(runtime, "exists", jsi::Value(exists));
  
  return result;
}

jsi::Object LlamaCppRn::createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx) {
  // Create object to represent the model
  jsi::Object result(runtime);
  
  // Store pointers as numbers
  result.setProperty(runtime, "_model", jsi::Value((double)(uintptr_t)model));
  result.setProperty(runtime, "_ctx", jsi::Value((double)(uintptr_t)ctx));
  
  // Add model properties
  const llama_vocab* vocab = llama_model_get_vocab(model);
  
  result.setProperty(runtime, "n_vocab", jsi::Value((double)llama_vocab_n_tokens(vocab)));
  result.setProperty(runtime, "n_ctx", jsi::Value((double)llama_n_ctx(ctx)));
  result.setProperty(runtime, "n_embd", jsi::Value((double)llama_model_n_embd(model)));
  
  // Add methods (stub implementations - expand as needed)
  result.setProperty(runtime, "tokenize", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "tokenize"), 
      1,  // takes text to tokenize
      [this](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Simple stub implementation
        return jsi::Object(rt);
      })
  );
  
  result.setProperty(runtime, "completion", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "completion"), 
      1,  // takes completion options
      [this](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Simple stub implementation
        return jsi::Object(rt);
      })
  );
  
  result.setProperty(runtime, "embedding", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "embedding"), 
      1,  // takes text to embed
      [this](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Simple stub implementation
        return jsi::Object(rt);
      })
  );
  
  result.setProperty(runtime, "release", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "release"), 
      0,  // takes no arguments
      [this](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        // Simple stub implementation
        return jsi::Value(true);
      })
  );
  
  return result;
}

jsi::Value LlamaCppRn::getVocabSize(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count) {
  try {
    if (!thisValue.isObject()) {
      throw std::runtime_error("Invalid model object");
    }
    
    // Get model pointer
    auto obj = thisValue.getObject(runtime);
    if (!obj.hasProperty(runtime, "_model")) {
      throw std::runtime_error("Invalid model object - no _model property");
    }
    
    uintptr_t model_ptr = (uintptr_t)obj.getProperty(runtime, "_model").asNumber();
    llama_model* model = (llama_model*)model_ptr;
    
    if (!model) {
      throw std::runtime_error("No model loaded");
    }
    
    // Get vocab size
    const llama_vocab* vocab = llama_model_get_vocab(model);
    int32_t n_vocab = llama_vocab_n_tokens(vocab);
    
    return jsi::Value((double)n_vocab);
  } catch (const std::exception& e) {
    throw jsi::JSError(runtime, e.what());
  }
}

} // namespace facebook::react 