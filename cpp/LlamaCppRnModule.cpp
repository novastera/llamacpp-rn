#include "LlamaCppRnModule.h"
#include <jsi/jsi.h>
#include <functional>
#include <future>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <thread>
#include "SystemUtils.h"
#include "LlamaCppModel.h"

// Include the llama.cpp common headers
#include "json-schema-to-grammar.h"
#include "chat.h"

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

/**
 * Converts a JSON Schema to GBNF (Grammar BNF) for constrained generation
 * 
 * @param args - An object with a "schema" property that contains the JSON Schema as a string
 * Example:
 * {
 *   schema: JSON.stringify({
 *     type: "object",
 *     properties: {
 *       name: { type: "string" },
 *       age: { type: "number" }
 *     },
 *     required: ["name"]
 *   })
 * }
 * 
 * @returns A string containing the GBNF grammar
 */
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
    
    // Check if GPU is supported
    bool gpuSupported = llama_supports_gpu_offload();
    result.setProperty(runtime, "gpuSupported", jsi::Value(gpuSupported));
    
    // Calculate optimal GPU layers if GPU is supported
    int optimalGpuLayers = 0;
    if (gpuSupported) {
      optimalGpuLayers = SystemUtils::getOptimalGpuLayers(model);
    }
    result.setProperty(runtime, "optimalGpuLayers", jsi::Value(optimalGpuLayers));
    
    // Extract quantization type from model description
    std::string desc(buf);
    std::string quantType = "Unknown";
    size_t qPos = desc.find(" Q");
    if (qPos != std::string::npos && qPos + 5 <= desc.length()) {
      // Extract quantization string (like Q4_K, Q5_K, Q8_0)
      quantType = desc.substr(qPos + 1, 4);
      // Remove any trailing non-alphanumeric characters
      quantType.erase(std::find_if(quantType.rbegin(), quantType.rend(), [](char c) {
        return std::isalnum(c);
      }).base(), quantType.end());
    }
    result.setProperty(runtime, "quant_type", jsi::String::createFromUtf8(runtime, quantType));
    
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

jsi::Value LlamaCppRn::initLlama(jsi::Runtime &runtime, jsi::Object options) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Get model path - required
    if (!options.hasProperty(runtime, "model")) {
      throw std::runtime_error("model path is required");
    }
    
    // Load dynamic backends if needed
    ggml_backend_load_all();
    
    std::string model_path = options.getProperty(runtime, "model").asString(runtime).utf8(runtime);
    
    // Setup parameters from options (or use defaults if not supplied)
    struct llama_model_params model_params = llama_model_default_params();
    
    // Set n_gpu_layers
    int n_gpu_layers = 0;
    bool gpuSupported = llama_supports_gpu_offload();
    if (options.hasProperty(runtime, "n_gpu_layers") && gpuSupported) {
      n_gpu_layers = options.getProperty(runtime, "n_gpu_layers").asNumber();
    }
    model_params.n_gpu_layers = n_gpu_layers;
    
    // Set use_mlock
    bool use_mlock = false;
    if (options.hasProperty(runtime, "use_mlock")) {
      use_mlock = options.getProperty(runtime, "use_mlock").asBool();
    }
    model_params.use_mlock = use_mlock;
    
    // Create inference parameters
    llama_context_params ctx_params = llama_context_default_params();
    
    // Set n_ctx - default to a reasonable context length
    int n_ctx = 2048;
    if (options.hasProperty(runtime, "n_ctx")) {
      n_ctx = options.getProperty(runtime, "n_ctx").asNumber();
    }
    ctx_params.n_ctx = n_ctx;
    
    // Set n_batch - how many tokens to process at once
    int n_batch = 512;
    if (options.hasProperty(runtime, "n_batch")) {
      n_batch = options.getProperty(runtime, "n_batch").asNumber();
    }
    ctx_params.n_batch = n_batch;
    
    // Set number of threads
    int n_threads = 0; // 0 = auto
    if (options.hasProperty(runtime, "n_threads")) {
      n_threads = options.getProperty(runtime, "n_threads").asNumber();
    }
    ctx_params.n_threads = n_threads;
    
    // Initialize seed for reproducibility or randomness
    int seed = -1; // -1 = random
    if (options.hasProperty(runtime, "seed")) {
      seed = options.getProperty(runtime, "seed").asNumber();
    }
    // The seed should not be assigned to ctx_params directly
    // It will be passed to the sampling parameters in the LlamaCppModel
    
    // Load the model
    fprintf(stderr, "Loading model: %s\n", model_path.c_str());
    fprintf(stderr, "Parameters: n_gpu_layers=%d, use_mlock=%d, n_ctx=%d, n_batch=%d, n_threads=%d, seed=%d\n",
            n_gpu_layers, use_mlock, n_ctx, n_batch, n_threads, seed);
    
    // Use the non-deprecated function
    llama_model* model = llama_model_load_from_file(model_path.c_str(), model_params);
    if (!model) {
      fprintf(stderr, "Failed to load model: %s\n", model_path.c_str());
      throw std::runtime_error("failed to load model");
    }
    
    // Initialize the context using the non-deprecated function
    fprintf(stderr, "Initializing context (n_ctx=%d, n_batch=%d, n_threads=%d)\n", 
            ctx_params.n_ctx, ctx_params.n_batch, ctx_params.n_threads);
    llama_context* ctx = llama_init_from_model(model, ctx_params);
    if (!ctx) {
      // Use the non-deprecated function
      fprintf(stderr, "Failed to initialize context\n");
      llama_model_free(model);
      throw std::runtime_error("failed to initialize context");
    }
    
    fprintf(stderr, "Model loaded successfully with vocab size: %d\n", llama_vocab_n_tokens(llama_model_get_vocab(model)));
    
    // Create the model object and return it
    return createModelObject(runtime, model, ctx);
  } catch (const std::exception& e) {
    fprintf(stderr, "initLlama error: %s\n", e.what());
    throw jsi::JSError(runtime, e.what());
  }
}

/**
 * Converts a JSON Schema to GBNF (Grammar BNF) for constrained generation
 * 
 * @param runtime - The JavaScript runtime
 * @param schema - An object with a "schema" property that contains the JSON Schema as a string
 * Example:
 * {
 *   schema: JSON.stringify({
 *     type: "object",
 *     properties: {
 *       name: { type: "string" },
 *       age: { type: "number" }
 *     },
 *     required: ["name"]
 *   })
 * }
 * 
 * @returns A string containing the GBNF grammar
 */
jsi::Value LlamaCppRn::jsonSchemaToGbnf(jsi::Runtime &runtime, jsi::Object schema) {
  std::lock_guard<std::mutex> lock(mutex_);
  
  try {
    // Check if schema property exists
    if (!schema.hasProperty(runtime, "schema")) {
      throw std::runtime_error("Missing required parameter: schema");
    }

    // Get the schema property and verify it's a string
    jsi::Value schemaValue = schema.getProperty(runtime, "schema");
    if (!schemaValue.isString()) {
      throw std::runtime_error("Schema must be a string");
    }

    // Parse the JSON schema
    std::string jsonSchemaString = schemaValue.asString(runtime).utf8(runtime);
    
    // Handle empty schema
    if (jsonSchemaString.empty()) {
      throw std::runtime_error("Schema cannot be empty");
    }
    
    // Verify the JSON is valid
    try {
      auto parsed_json = nlohmann::json::parse(jsonSchemaString);
    } catch (const nlohmann::json::exception& e) {
      throw std::runtime_error(std::string("Invalid JSON in schema: ") + e.what());
    }
    
    // Convert the JSON schema to a GBNF grammar
    std::string gbnfGrammar;
    try {
      gbnfGrammar = json_schema_to_grammar(jsonSchemaString);
    } catch (const std::exception& e) {
      throw std::runtime_error(std::string("Failed to convert schema to grammar: ") + e.what());
    }
    
    // Return the GBNF grammar
    return jsi::String::createFromUtf8(runtime, gbnfGrammar);
  } catch (const std::exception& e) {
    // Log the error and rethrow
    fprintf(stderr, "jsonSchemaToGbnf error: %s\n", e.what());
    throw jsi::JSError(runtime, e.what());
  }
}

jsi::Object LlamaCppRn::createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx) {
  // Create a shared_ptr to a new LlamaCppModel instance
  auto llamaModel = std::make_shared<LlamaCppModel>(model, ctx);
  
  // Create a host object from the LlamaCppModel instance
  return jsi::Object::createFromHostObject(runtime, std::move(llamaModel));
}

} // namespace facebook::react 