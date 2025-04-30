#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/TurboModule.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <thread>
#include <vector>

// Forward declarations for C++ only
struct llama_model;
struct llama_context;
struct llama_vocabulary;

using namespace facebook;

namespace facebook::react {

// LlamaCppRn native module
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

  // Model methods 
  jsi::Value getVocabSize(jsi::Runtime& runtime, const jsi::Value& thisValue, const jsi::Value* args, size_t count);

private:
  // Helper methods
  jsi::Object createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx);
  bool detectGpuCapabilities();
  bool enableGpu(bool enable);
  bool isGpuEnabled();
  std::string normalizeFilePath(const std::string& path);

private:
  // Module state
  bool m_gpuEnabled = false;
  std::atomic<bool> m_shouldStopCompletion{false};
  std::mutex mutex_;
  std::unordered_map<std::string, std::shared_ptr<jsi::Object>> modelInfoCache_;
};

} // namespace facebook::react 