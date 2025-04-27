#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/TurboModule.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>

// Forward declarations for C++ only
struct llama_model;
struct llama_context;
struct llama_vocabulary;

using namespace facebook;

namespace facebook::react {

class LlamaCppRn : public TurboModule {
public:
  static constexpr auto kModuleName = "LlamaCppRn";

  LlamaCppRn(std::shared_ptr<CallInvoker> jsInvoker);
  virtual ~LlamaCppRn() = default;

  // Required for TurboModule system
  static std::shared_ptr<TurboModule> create(
      std::shared_ptr<CallInvoker> jsInvoker);

  // JavaScript accessible methods
  jsi::Value initLlama(jsi::Runtime &runtime, jsi::Object params);
  jsi::Value loadLlamaModelInfo(jsi::Runtime &runtime, jsi::String modelPath);
  jsi::Value jsonSchemaToGbnf(jsi::Runtime &runtime, jsi::Object schema);
  jsi::Value getGPUInfo(jsi::Runtime &runtime);
  jsi::Value getAbsolutePath(jsi::Runtime &runtime, jsi::String relativePath);

private:
  struct SimpleJSON {
    std::unordered_map<std::string, std::string> stringValues;
    void set(const std::string& key, const std::string& value) {
      stringValues[key] = value;
    }
  };

  struct GpuInfo {
    bool available;
    std::string deviceName;
    std::string deviceVendor;
    std::string deviceVersion;
    size_t deviceComputeUnits;
    size_t deviceMemSize;
  };

  // Helper methods
  jsi::Object createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx);
  GpuInfo getGpuCapabilities();
  bool detectGpuCapabilities();
  bool enableGpu(bool enable);
  bool isGpuEnabled();
  jsi::Value getVocabSize(jsi::Runtime& runtime, const jsi::Value& thisValue, 
                            const jsi::Value* args, size_t count);

private:
  // Module state
  bool m_gpuEnabled = false;
  std::atomic<bool> m_shouldStopCompletion{false};
  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<jsi::Object>> modelInfoCache_;
};

} // namespace facebook::react 