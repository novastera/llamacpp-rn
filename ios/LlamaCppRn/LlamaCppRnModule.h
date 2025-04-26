#import <React/RCTBridgeModule.h>
#import <React/RCTTurboModule.h>

@interface LlamaCppRnModule : NSObject <RCTBridgeModule, RCTTurboModule>

// New architecture specific code
+ (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:
    (const facebook::react::ObjCTurboModule::InitParams &)params;

@end 