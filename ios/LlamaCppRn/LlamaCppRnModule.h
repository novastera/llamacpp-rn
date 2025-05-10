#import <React/RCTBridgeModule.h>
#import <ReactCommon/RCTTurboModule.h>

// Forward declarations for C++ types
namespace facebook {
namespace react {
class CallInvoker;
class TurboModule;
}
}

@interface LlamaCppRnModule : NSObject <RCTBridgeModule, RCTTurboModule>

// New architecture specific code
+ (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const std::string &)name
    jsInvoker:(std::shared_ptr<facebook::react::CallInvoker>)jsInvoker;

@end

// We're using RCT_EXPORT_MODULE_NO_LOAD in the .mm file, so we don't need this additional registration
// LLAMACPPRN_REGISTER_TURBO_MODULE()