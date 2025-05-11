#include <fbjni/fbjni.h>
#include <jsi/jsi.h>
#include <ReactCommon/TurboModuleUtils.h>
#include <react/bridging/CallbackWrapper.h>

// Include the C++ module header - make sure path is correct
#include "../../../../../../cpp/LlamaCppRnModule.h"

using namespace facebook;

// Single provider function to avoid duplication
std::shared_ptr<react::TurboModule> turboModuleProvider(
    const std::string& name,
    const std::shared_ptr<react::CallInvoker>& jsInvoker) {
  // Make sure the name matches EXACTLY what's in your TypeScript spec
  if (name == "LlamaCppRn") {
    return react::LlamaCppRn::create(jsInvoker);
  }
  return nullptr;
}

// JNI_OnLoad is the entry point
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return facebook::jni::initialize(vm, [] {
    facebook::react::TurboModuleManagerDelegate::cxxModuleProvider = &turboModuleProvider;
  });
}

// This is needed for the TurboModule system
std::shared_ptr<facebook::react::TurboModule> 
facebook::react::TurboModuleProviderFunctionHolder::cxxModuleProvider(
    const std::string& name, 
    const std::shared_ptr<facebook::react::CallInvoker>& jsInvoker) {
  return turboModuleProvider(name, jsInvoker);
}

std::shared_ptr<react::TurboModule> jniModuleProvider(
    const std::string& name,
    const jni::global_ref<jobject>& javaPart,
    const std::shared_ptr<react::CallInvoker>& jsInvoker) {
  // First try to find a C++ module
  auto cxxModule = turboModuleProvider(name, jsInvoker);
  if (cxxModule != nullptr) {
    return cxxModule;
  }
  
  return nullptr;
}

JNIEXPORT jni::local_ref<jobject> createTurboModuleProvider(
    jni::alias_ref<jclass>,
    jni::alias_ref<jobject> jsContext) {
  return nullptr;
} 
