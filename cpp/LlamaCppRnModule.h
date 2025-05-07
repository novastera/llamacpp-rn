#pragma once

#include <jsi/jsi.h>
#include <ReactCommon/TurboModule.h>
#include <memory>
#include <string>
#include <mutex>
#include "LlamaCppModel.h"

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
  jsi::Value initLlama(jsi::Runtime &runtime, jsi::Object options);
  jsi::Value loadLlamaModelInfo(jsi::Runtime &runtime, jsi::String modelPath);

private:
  // Helper methods
  jsi::Object createModelObject(jsi::Runtime& runtime, rn_llama_context* rn_ctx);
  std::string normalizeFilePath(const std::string& path);

private:
  // Module state
  std::mutex mutex_;
  std::unique_ptr<rn_llama_context> rn_ctx_;
};

} // namespace facebook::react 