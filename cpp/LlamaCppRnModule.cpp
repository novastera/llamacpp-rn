#include "LlamaCppRnModule.h"
#include <jsi/jsi.h>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <thread>
#include "SystemUtils.h"
#include "LlamaCppModel.h"

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

LlamaCppRn::LlamaCppRn(std::shared_ptr<CallInvoker> jsInvoker)
    : TurboModule(kModuleName, std::move(jsInvoker)) {
  // Initialize and register methods
  methodMap_["initLlama"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecInitLlama};
  methodMap_["loadLlamaModelInfo"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecLoadLlamaModelInfo};
  methodMap_["jsonSchemaToGbnf"] = MethodMetadata{1, __hostFunction_LlamaCppRnSpecJsonSchemaToGbnf};
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
  std::string path = normalizeFilePath(modelPath.utf8(runtime));
  
  try {
    // Initialize llama backend
    llama_backend_init();
    
    // Create model params
    llama_model_params params = llama_model_default_params();
    params.n_gpu_layers = 0; // Use CPU for model info loading
    
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
    
    return result;
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
    
    // Load the model
    std::cout << "Loading model from path: " << modelPath << std::endl;
    llama_model* model = llama_model_load_from_file(modelPath.c_str(), modelParams);
    if (!model) {
      std::string errorMsg = "Failed to load model: " + modelPath;
      std::cout << "Error: " << errorMsg << std::endl;
      throw std::runtime_error(errorMsg);
    }
    
    // Check if GPU is supported
    bool gpuSupported = llama_supports_gpu_offload();
    if (params.hasProperty(runtime, "n_gpu_layers")) {
      // User specified GPU layers
      modelParams.n_gpu_layers = (int)params.getProperty(runtime, "n_gpu_layers").asNumber();
      if (modelParams.n_gpu_layers > 0 && !gpuSupported) {
        // GPU not supported - silently fall back to CPU
        std::cout << "Warning: GPU layers requested but GPU is not supported. Using CPU only." << std::endl;
        modelParams.n_gpu_layers = 0;
      }
    } else {
      // User didn't specify - use optimal GPU layers if supported
      modelParams.n_gpu_layers = gpuSupported ? SystemUtils::getOptimalGpuLayers(model) : 0;
    }
    
    // Create context params
    llama_context_params contextParams = llama_context_default_params();
    
    // Get optional context parameters
    if (params.hasProperty(runtime, "n_ctx")) {
      contextParams.n_ctx = (int)params.getProperty(runtime, "n_ctx").asNumber();
    }
    
    // Set batch size with a conservative default for mobile
    if (params.hasProperty(runtime, "n_batch")) {
      contextParams.n_batch = (int)params.getProperty(runtime, "n_batch").asNumber();
    } else {
      // Use a conservative batch size for mobile devices (default in llama.cpp is 512)
      // For small models like 1B, 128 is usually sufficient and safer
      contextParams.n_batch = 128;
      std::cout << "Using conservative default batch size for mobile: " << contextParams.n_batch << std::endl;
    }
    
    // Get thread count
    if (params.hasProperty(runtime, "n_threads")) {
      contextParams.n_threads = (int)params.getProperty(runtime, "n_threads").asNumber();
      std::cout << "Using user-specified thread count: " << contextParams.n_threads << std::endl;
    } else {
      contextParams.n_threads = SystemUtils::getOptimalThreadCount();
      std::cout << "Using optimal thread count: " << contextParams.n_threads << std::endl;
    }
    
    // Create context
    std::cout << "Creating llama context with n_ctx=" << contextParams.n_ctx << ", n_batch=" << contextParams.n_batch << ", n_threads=" << contextParams.n_threads << std::endl;
    llama_context* ctx = llama_init_from_model(model, contextParams);
    if (!ctx) {
      std::string errorMsg = "Failed to create context";
      std::cout << "Error: " << errorMsg << std::endl;
      llama_model_free(model);
      throw std::runtime_error(errorMsg);
    }
    
    std::cout << "Context created successfully" << std::endl;
    
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

jsi::Object LlamaCppRn::createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx) {
  // Create the LlamaCppModel instance to handle model operations
  auto* llamaModel = new LlamaCppModel(model, ctx);
  
  // Create object to represent the model
  jsi::Object result(runtime);
  
  // Store a pointer to the model as a number
  result.setProperty(runtime, "_model", jsi::Value((double)(uintptr_t)llamaModel));
  
  // Add basic model properties 
  result.setProperty(runtime, "n_vocab", jsi::Value((double)llamaModel->getVocabSize()));
  result.setProperty(runtime, "n_ctx", jsi::Value((double)llamaModel->getContextSize()));
  result.setProperty(runtime, "n_embd", jsi::Value((double)llamaModel->getEmbeddingSize()));
  
  // Add tokenize method
  result.setProperty(runtime, "tokenize", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "tokenize"), 
      1,  // takes text to tokenize
      [llamaModel](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        return llamaModel->tokenizeJsi(rt, args, count);
      })
  );
  
  // Add completion method
  result.setProperty(runtime, "completion", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "completion"), 
      1,  // takes completion options
      [llamaModel](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        return llamaModel->completionJsi(rt, args, count);
      })
  );
  
  // Add embedding method
  result.setProperty(runtime, "embedding", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "embedding"), 
      1,  // takes text to embed
      [llamaModel](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        return llamaModel->embeddingJsi(rt, args, count);
      })
  );
  
  // Add testProcessTokens method (for debugging)
  result.setProperty(runtime, "testProcessTokens", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "testProcessTokens"), 
      1,  // takes text to test
      [llamaModel](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        return llamaModel->testProcessTokensJsi(rt, args, count);
      })
  );
  
  // Add release method
  result.setProperty(runtime, "release", 
    jsi::Function::createFromHostFunction(runtime, 
      jsi::PropNameID::forAscii(runtime, "release"), 
      0,  // takes no arguments
      [llamaModel](jsi::Runtime& rt, const jsi::Value& thisVal, const jsi::Value* args, size_t count) -> jsi::Value {
        jsi::Value result = llamaModel->releaseJsi(rt, args, count);
        delete llamaModel; // Free the model when release is called
        return result;
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
    LlamaCppModel* llamaModel = (LlamaCppModel*)model_ptr;
    
    if (!llamaModel) {
      throw std::runtime_error("No model loaded");
    }
    
    // Get vocab size using the model
    return jsi::Value((double)llamaModel->getVocabSize());
  } catch (const std::exception& e) {
    throw jsi::JSError(runtime, e.what());
  }
}

} // namespace facebook::react 