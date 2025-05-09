#if __has_include(<fbjni/fbjni.h>)
#include <fbjni/fbjni.h>
#elif __has_include("fbjni/fbjni.h")
#include "fbjni/fbjni.h"
#elif __has_include(<react/jni/fbjni.h>)
#include <react/jni/fbjni.h>
#elif __has_include("react/jni/fbjni.h")
#include "react/jni/fbjni.h"
#else
// If all else fails, try a direct path
#include "../../../../../../node_modules/react-native/ReactAndroid/src/main/jni/first-party/fbjni/headers/fbjni/fbjni.h"
#endif

#include <jsi/jsi.h>
#include <ReactCommon/TurboModuleUtils.h>
#include <react/bridging/CallbackWrapper.h>

// Include the C++ module header
#include "../../../../../../cpp/LlamaCppRnModule.h"

using namespace facebook;

// Function to provide C++ modules
std::shared_ptr<react::TurboModule> cxxModuleProvider(
    const std::string& name,
    const std::shared_ptr<react::CallInvoker>& jsInvoker) {
  if (name == react::LlamaCppRn::kModuleName) {
    return react::LlamaCppRn::create(jsInvoker);
  }
  return nullptr;
}

// The Android TurboModule system initialization
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return facebook::jni::initialize(vm, [] {
    // Register the C++ module provider with React Native
    facebook::react::TurboModuleManagerDelegate::cxxModuleProvider = &cxxModuleProvider;
  });
}

// This hooks the module into the React Native TurboModule system
std::shared_ptr<facebook::react::TurboModule> 
facebook::react::TurboModuleProviderFunctionHolder::cxxModuleProvider(
    const std::string& name, 
    const std::shared_ptr<facebook::react::CallInvoker>& jsInvoker) {
  if (name == react::LlamaCppRn::kModuleName) {
    return react::LlamaCppRn::create(jsInvoker);
  }
  return nullptr;
}

std::shared_ptr<react::TurboModule> jniModuleProvider(
    const std::string& name,
    const jni::global_ref<jobject>& javaPart,
    const std::shared_ptr<react::CallInvoker>& jsInvoker) {
  // First try to find a C++ module
  auto cxxModule = cxxModuleProvider(name, jsInvoker);
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