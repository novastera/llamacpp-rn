#pragma once

#include <ReactCommon/TurboModule.h>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <jsi/jsi.h>
#include "llama.h"
#include <atomic>

namespace facebook::react {

/**
 * C++ implementation of the LlamaCppRn Turbo Module interface.
 * Optimized for performance with the new architecture.
 */
class LlamaCppRn : public TurboModule {
public:
  static constexpr auto kModuleName = "LlamaCppRn";

  // Constructor calls parent constructor with name
  LlamaCppRn(std::shared_ptr<CallInvoker> jsInvoker);

  // Factory method to create the Turbo Module
  static std::shared_ptr<TurboModule> create(
      std::shared_ptr<CallInvoker> jsInvoker);

  // Initialize a Llama context with the given model parameters
  jsi::Value initLlama(
      jsi::Runtime &runtime,
      jsi::Object params);

  // Load model info without creating a full context
  jsi::Value loadLlamaModelInfo(
      jsi::Runtime &runtime,
      jsi::String modelPath);

  // Convert JSON schema to GBNF grammar
  jsi::Value jsonSchemaToGbnf(
      jsi::Runtime &runtime,
      jsi::Object schema);

  void install(jsi::Runtime& jsiRuntime);

private:
  // Mutex for thread-safe operations
  std::mutex mutex_;
  
  // Cache for loaded model info to improve performance on repeated calls
  std::unordered_map<std::string, jsi::Object> modelInfoCache_;
  
  // Flag to indicate if GPU is enabled
  bool m_gpuEnabled = false;
  
  // Flag to indicate if an ongoing completion should be stopped
  std::atomic<bool> m_shouldStopCompletion{false};

  Value setThreadCount(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value loadModel(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value getSystemInfo(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value tokenize(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value getTokens(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value evaluate(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value embeddings(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value isModelLoaded(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value freeModel(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value getContextLength(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value getVocabSize(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value encode(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value decode(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value getGpuInfo(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value enableGpuAcceleration(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  Value isGpuAvailable(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);
  
  // Helper methods
  std::vector<int32_t> jsiArrayToVector(jsi::Runtime& runtime, const jsi::Object& array);
  jsi::Object vectorToJsiArray(jsi::Runtime& runtime, const std::vector<int32_t>& vector);
  jsi::Object vectorToJsiFloat32Array(jsi::Runtime& runtime, const std::vector<float>& vector);
  llama_model_params createModelParams(jsi::Runtime& runtime, const jsi::Value* args, size_t count);
  llama_context_params createContextParams(jsi::Runtime& runtime, const jsi::Value* args, size_t count);
  jsi::Object createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx);
  
  // OpenCL support
  bool detectGpuCapabilities();
  bool enableGpu(bool enable);
  bool isGpuEnabled();
  struct GpuInfo {
    bool available;
    std::string deviceName;
    std::string deviceVendor;
    std::string deviceVersion;
    int deviceComputeUnits;
    uint64_t deviceMemSize;
  };
  GpuInfo getGpuCapabilities();
  
  int m_threadCount;
  GpuInfo m_gpuInfo;
};

} // namespace facebook::react 