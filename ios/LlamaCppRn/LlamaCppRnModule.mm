#import "LlamaCppRnModule.h"

#import <React/RCTBridge+Private.h>
#import <React/RCTUtils.h>
#import <ReactCommon/RCTTurboModule.h>
#import <jsi/jsi.h>

#include "../../tm/NativeLlamaCppRn.h"

@implementation LlamaCppRnModule

// Only use ONE of these registration macros - the NO_LOAD version is preferred for TurboModules
RCT_EXPORT_MODULE_NO_LOAD(LlamaCppRn, LlamaCppRnModule)

- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params
{
  return std::make_shared<facebook::react::LlamaCppRn>(params.jsInvoker);
}

// Static method to create the Turbo Module
+ (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:(const std::string &)name
                                                      jsInvoker:(std::shared_ptr<facebook::react::CallInvoker>)jsInvoker
{
  return std::make_shared<facebook::react::LlamaCppRn>(jsInvoker);
}

@end