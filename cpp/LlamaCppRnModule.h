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

// Forward declaration for the rn_llama_context type
struct rn_llama_context;

using namespace facebook;

namespace facebook::react {

/**
 * Main TurboModule class for React Native
 */
class LlamaCppRn : public TurboModule {
public:
  // Constants for module name identification
  static constexpr auto kModuleName = "LlamaCppRn";
  
  // Constructor required for implementing TurboModule
  LlamaCppRn(std::shared_ptr<CallInvoker> jsInvoker);
  
  // Factory method required for TurboModule
  static std::shared_ptr<TurboModule> create(std::shared_ptr<CallInvoker> jsInvoker);

  // JSI host functions
  jsi::Value initLlama(jsi::Runtime& runtime, jsi::Object options);
  jsi::Value loadLlamaModelInfo(jsi::Runtime& runtime, jsi::String modelPath);
  
private:
  // Helper method to create model objects
  jsi::Object createModelObject(jsi::Runtime& runtime, rn_llama_context* rn_ctx);

  // Context for the currently loaded model, if any
  std::unique_ptr<rn_llama_context> rn_ctx_;
  
  // Mutex for thread safety
  std::mutex mutex_;
};

} // namespace facebook::react