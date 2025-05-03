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
    // Get model path - required (preserve custom path handling)
    if (!options.hasProperty(runtime, "model")) {
      throw std::runtime_error("model path is required");
    }
    
    // Load dynamic backends if needed
    ggml_backend_load_all();
    
    std::string model_path = SystemUtils::normalizeFilePath(options.getProperty(runtime, "model").asString(runtime).utf8(runtime));
    
    // Create common_params structure for initialization
    common_params params = {};
    
    // Set model path
    params.model.path = model_path;
    
    // Set context size and batch parameters
    SystemUtils::setIfExists(runtime, options, "n_ctx", params.n_ctx);
    SystemUtils::setIfExists(runtime, options, "n_batch", params.n_batch);
    SystemUtils::setIfExists(runtime, options, "n_ubatch", params.n_ubatch);
    SystemUtils::setIfExists(runtime, options, "n_keep", params.n_keep);
    
    // Memory and resource options
    SystemUtils::setIfExists(runtime, options, "use_mmap", params.use_mmap);
    SystemUtils::setIfExists(runtime, options, "use_mlock", params.use_mlock);
    
    // Extract threading parameters (preserve custom thread logic)
    int n_threads = 0; // 0 = auto
    if (options.hasProperty(runtime, "n_threads")) {
      n_threads = options.getProperty(runtime, "n_threads").asNumber();
    } else {
      n_threads = SystemUtils::getOptimalThreadCount();
    }
    params.cpuparams.n_threads = n_threads;
    
    // Set n_gpu_layers (preserve custom GPU logic)
    int n_gpu_layers = 0;
    bool gpuSupported = llama_supports_gpu_offload();
    if (options.hasProperty(runtime, "n_gpu_layers") && gpuSupported) {
      n_gpu_layers = options.getProperty(runtime, "n_gpu_layers").asNumber();
    }
    params.n_gpu_layers = n_gpu_layers;
    
    // Additional model parameters
    SystemUtils::setIfExists(runtime, options, "vocab_only", params.logits_all);
    SystemUtils::setIfExists(runtime, options, "embedding", params.embedding);
    SystemUtils::setIfExists(runtime, options, "rope_freq_base", params.rope_freq_base);
    SystemUtils::setIfExists(runtime, options, "rope_freq_scale", params.rope_freq_scale);
    
    // Sampling parameters
    SystemUtils::setIfExists(runtime, options, "seed", params.sampling.seed);
    
    // Other system parameters
    SystemUtils::setIfExists(runtime, options, "verbose", params.verbosity);
    
    // RoPE settings if provided
    if (options.hasProperty(runtime, "yarn_ext_factor")) {
      params.yarn_ext_factor = options.getProperty(runtime, "yarn_ext_factor").asNumber();
    }
    if (options.hasProperty(runtime, "yarn_attn_factor")) {
      params.yarn_attn_factor = options.getProperty(runtime, "yarn_attn_factor").asNumber();
    }
    if (options.hasProperty(runtime, "yarn_beta_fast")) {
      params.yarn_beta_fast = options.getProperty(runtime, "yarn_beta_fast").asNumber();
    }
    if (options.hasProperty(runtime, "yarn_beta_slow")) {
      params.yarn_beta_slow = options.getProperty(runtime, "yarn_beta_slow").asNumber();
    }
    
    // Support for chat template override
    std::string chat_template;
    if (SystemUtils::setIfExists(runtime, options, "chat_template", chat_template)) {
      params.chat_template = chat_template;
    }
    
    // Support for LoRA adapters
    if (options.hasProperty(runtime, "lora_adapters") && options.getProperty(runtime, "lora_adapters").isObject()) {
      jsi::Object lora_obj = options.getProperty(runtime, "lora_adapters").asObject(runtime);
      if (lora_obj.isArray(runtime)) {
        jsi::Array lora_array = lora_obj.asArray(runtime);
        size_t n_lora = lora_array.size(runtime);
        
        for (size_t i = 0; i < n_lora; i++) {
          if (lora_array.getValueAtIndex(runtime, i).isObject()) {
            jsi::Object adapter = lora_array.getValueAtIndex(runtime, i).asObject(runtime);
            if (adapter.hasProperty(runtime, "path") && adapter.getProperty(runtime, "path").isString()) {
              common_adapter_lora_info lora;
              lora.path = adapter.getProperty(runtime, "path").asString(runtime).utf8(runtime);
              
              // Get scale if provided
              lora.scale = 1.0f; // Default scale
              if (adapter.hasProperty(runtime, "scale") && adapter.getProperty(runtime, "scale").isNumber()) {
                lora.scale = adapter.getProperty(runtime, "scale").asNumber();
              }
              
              params.lora_adapters.push_back(lora);
            }
          }
        }
      }
    }
    
    // Initialize using common_init_from_params   
    common_init_result result = common_init_from_params(params);
    
    // Check if initialization was successful
    if (!result.model || !result.context) {
      throw std::runtime_error("Failed to initialize model and context");
    }
    
    // Get raw pointers and release ownership from shared_ptr
    llama_model* model = result.model.release();
    llama_context* ctx = result.context.release();
    
    
    // Handle any LoRA adapters that were loaded
    // In this implementation, we don't need to do anything special since the adapters
    // have already been applied by common_init_from_params
    
   //fprintf(stderr, "Model loaded successfully with vocab size: %d\n", 
   //         llama_vocab_n_tokens(llama_model_get_vocab(model)));
    
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