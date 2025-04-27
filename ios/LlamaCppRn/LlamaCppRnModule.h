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