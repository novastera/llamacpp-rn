#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/TurboModule.h>
#include <memory>
#include <string>
#include <mutex>

// Forward declarations for C++ only
struct llama_model;
struct llama_context;

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

private:
  // Helper methods
  jsi::Object createModelObject(jsi::Runtime& runtime, llama_model* model, llama_context* ctx);
  std::string normalizeFilePath(const std::string& path);

private:
  // Module state
  std::mutex mutex_;
};

} // namespace facebook::react 