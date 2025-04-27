// Stub header for RCTTurboModule.h
// This file ensures the build succeeds when using prebuilt libraries

#pragma once

#include <memory>

namespace facebook {
namespace react {

class CallInvoker;

// Forward declaration for ObjCTurboModule
class ObjCTurboModule {
public:
  struct InitParams {
    std::shared_ptr<CallInvoker> jsInvoker;
  };
};

// Forward declaration for TurboModule
class TurboModule {
public:
  virtual ~TurboModule() = default;
};

}}
