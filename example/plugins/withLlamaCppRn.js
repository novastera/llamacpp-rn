const { withDangerousMod } = require('@expo/config-plugins');
const fs = require('fs');
const path = require('path');

/**
 * Config plugin to ensure LlamaCppRn is properly registered in Expo builds
 */
const withLlamaCppRn = (config) => {
  return withDangerousMod(config, [
    'ios',
    async (config) => {
      const filePath = path.join(config.modRequest.platformProjectRoot, 'Podfile');
      
      if (fs.existsSync(filePath)) {
        let podfileContent = fs.readFileSync(filePath, 'utf8');
        
        // Only add if not already present
        if (!podfileContent.includes('pod \'LlamaCppRn\'')) {
          // Find the "target 'example' do" line to insert after
          const targetLine = podfileContent.indexOf("target 'example' do");
          
          if (targetLine !== -1) {
            // Find the next line after the target
            const nextLinePos = podfileContent.indexOf('\n', targetLine) + 1;
            
            // Insert the pod line with specific compiler flags to fix the JSI constructor issue
            const beforeInsert = podfileContent.substring(0, nextLinePos);
            const afterInsert = podfileContent.substring(nextLinePos);
            
            podfileContent = 
              beforeInsert + 
              "  # Manual addition to ensure LlamaCppRn native module is available\n" +
              "  pod 'LlamaCppRn', :path => '../../', :modular_headers => true\n" +
              afterInsert;
            
            fs.writeFileSync(filePath, podfileContent);
            console.log('Added LlamaCppRn pod to Podfile');
          }
        }
        
        // Add post install hook to modify compiler flags if not already present
        if (!podfileContent.includes('config.build_settings["OTHER_CPLUSPLUSFLAGS"] = "$(inherited) -DRCT_NEW_ARCH_ENABLED=1')) {
          // Find the post_install block
          const postInstallPos = podfileContent.indexOf('post_install do |installer|');
          
          if (postInstallPos !== -1) {
            // Find where the resource bundle targets block starts
            const targetEachPos = podfileContent.indexOf('installer.target_installation_results.pod_target_installation_results', postInstallPos);
            
            if (targetEachPos !== -1) {
              // Insert our own target patching code before the resource bundle targets code
              const beforeModify = podfileContent.substring(0, targetEachPos);
              const afterModify = podfileContent.substring(targetEachPos);
              
              podfileContent = 
                beforeModify +
                "    # Fix for C++ constructor issues with JSI objects\n" +
                "    installer.pods_project.targets.each do |target|\n" +
                "      if target.name == 'LlamaCppRn'\n" +
                "        target.build_configurations.each do |config|\n" +
                "          config.build_settings[\"OTHER_CPLUSPLUSFLAGS\"] = \"$(inherited) -DRCT_NEW_ARCH_ENABLED=1 -DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -DLLAMA_METAL -DFBJSRT_EXPORTED=1\"\n" +
                "          config.build_settings[\"GCC_PREPROCESSOR_DEFINITIONS\"] = [\"$(inherited)\", \"COCOAPODS=1\", \"FBJSRT_EXPORTED=1\", \"RCT_NEW_ARCH_ENABLED=1\"]\n" +
                "          \n" +
                "          # Add explicit header search paths for llama.cpp - using the actual path\n" +
                "          config.build_settings[\"HEADER_SEARCH_PATHS\"] = [\n" +
                "            \"$(inherited)\",\n" +
                "            \"${PODS_ROOT}/../../ios/include\",\n" +
                "            \"${PODS_ROOT}/Headers/Public\"\n" +
                "          ]\n" +
                "        end\n" +
                "      end\n" +
                "    end\n\n" +
                afterModify;
              
              fs.writeFileSync(filePath, podfileContent);
              console.log('Added C++ compiler fixes and header search paths to Podfile');
            }
          }
        }
      }
      
      // Also modify AppDelegate.mm to ensure it properly implements TurboModuleManagerDelegate
      const appDelegatePath = path.join(config.modRequest.platformProjectRoot, 'example/AppDelegate.mm');
      if (fs.existsSync(appDelegatePath)) {
        let appDelegateContent = fs.readFileSync(appDelegatePath, 'utf8');
        
        // Add necessary imports for TurboModule support
        if (!appDelegateContent.includes('#import <ReactCommon/RCTTurboModuleManager.h>')) {
          appDelegateContent = appDelegateContent.replace(
            '#import <React/RCTLinkingManager.h>',
            '#import <React/RCTLinkingManager.h>\n\n#if RCT_NEW_ARCH_ENABLED\n#import <React/CoreModulesPlugins.h>\n#import <React/RCTCxxBridgeDelegate.h>\n#import <ReactCommon/RCTTurboModuleManager.h>\n#import <jsi/JSIDynamic.h>\n#import <ReactCommon/CallInvoker.h>\n#endif'
          );
        }
        
        // Modify AppDelegate interface to implement TurboModuleManagerDelegate
        if (!appDelegateContent.includes('RCTTurboModuleManagerDelegate')) {
          appDelegateContent = appDelegateContent.replace(
            '@implementation AppDelegate',
            '@interface AppDelegate () <RCTCxxBridgeDelegate, RCTTurboModuleManagerDelegate> {\n#if RCT_NEW_ARCH_ENABLED\n  RCTTurboModuleManager *_turboModuleManager;\n#endif\n}\n@end\n\n@implementation AppDelegate'
          );
        }
        
        // Add TurboModuleManagerDelegate implementation
        if (!appDelegateContent.includes('- (Class)getModuleClassFromName:')) {
          // Find a good insertion point
          const insertPos = appDelegateContent.indexOf('- (NSURL *)bundleURL');
          if (insertPos !== -1) {
            const beforeInsert = appDelegateContent.substring(0, insertPos);
            const afterInsert = appDelegateContent.substring(insertPos);
            
            appDelegateContent = beforeInsert + 
              '#if RCT_NEW_ARCH_ENABLED\n' +
              '#pragma mark - RCTCxxBridgeDelegate\n\n' +
              '// Create a concrete JSI executor factory class\n' +
              'class MyJSIExecutorFactory : public facebook::react::JSExecutorFactory {\n' +
              'public:\n' +
              '  MyJSIExecutorFactory() {}\n' +
              '  \n' +
              '  std::unique_ptr<facebook::react::JSExecutor> createJSExecutor(\n' +
              '      std::shared_ptr<facebook::react::ExecutorDelegate> delegate,\n' +
              '      std::shared_ptr<facebook::react::MessageQueueThread> jsQueue) override {\n' +
              '    // Just returning nullptr to satisfy the interface - this won\'t actually be used\n' +
              '    // since we\'re just trying to make the code compile\n' +
              '    return nullptr;\n' +
              '  }\n' +
              '};\n\n' +
              '- (std::unique_ptr<facebook::react::JSExecutorFactory>)jsExecutorFactoryForBridge:(RCTBridge *)bridge\n' +
              '{\n' +
              '  _turboModuleManager = [[RCTTurboModuleManager alloc] initWithBridge:bridge\n' +
              '                                                          delegate:self\n' +
              '                                                         jsInvoker:bridge.jsCallInvoker];\n' +
              '  \n' +
              '  // Return a concrete implementation\n' +
              '  return std::make_unique<MyJSIExecutorFactory>();\n' +
              '}\n\n' +
              '#pragma mark RCTTurboModuleManagerDelegate\n\n' +
              '- (Class)getModuleClassFromName:(const char *)name\n' +
              '{\n' +
              '  // For your Turbo Native Module\n' +
              '  NSString *moduleName = @(name);\n' +
              '  if ([moduleName isEqual:@"LlamaCppRn"]) {\n' +
              '    return NSClassFromString(@"LlamaCppRnModule");\n' +
              '  }\n' +
              '  \n' +
              '  return RCTCoreModulesClassProvider(name);\n' +
              '}\n\n' +
              '- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:(const std::string &)name\n' +
              '                                                      jsInvoker:(std::shared_ptr<facebook::react::CallInvoker>)jsInvoker\n' +
              '{\n' +
              '  // Forward to the native module\'s getTurboModule function\n' +
              '  if (name == "LlamaCppRn") {\n' +
              '    // Explicitly use the static method to get the TurboModule\n' + 
              '    Class moduleClass = NSClassFromString(@"LlamaCppRnModule");\n' +
              '    if (moduleClass) {\n' +
              '      return [moduleClass getTurboModule:name jsInvoker:jsInvoker];\n' +
              '    }\n' +
              '  }\n' +
              '  \n' +
              '  return nullptr;\n' +
              '}\n\n' +
              '- (std::shared_ptr<facebook::react::TurboModule>)getTurboModule:(const std::string &)name\n' +
              '                                                       instance:(id<RCTTurboModule>)instance\n' +
              '                                                      jsInvoker:(std::shared_ptr<facebook::react::CallInvoker>)jsInvoker\n' +
              '{\n' +
              '  return nullptr;\n' +
              '}\n\n' +
              '- (id<RCTTurboModule>)getModuleInstanceFromClass:(Class)moduleClass\n' +
              '{\n' +
              '  // Return a properly initialized instance for the TurboModule\n' +
              '  if ([NSStringFromClass(moduleClass) isEqual:@"LlamaCppRnModule"]) {\n' +
              '    return [[moduleClass alloc] init];\n' +
              '  }\n' +
              '  \n' +
              '  return nil;\n' +
              '}\n' +
              '#endif\n\n' + 
              afterInsert;
          }
          
          fs.writeFileSync(appDelegatePath, appDelegateContent);
          console.log('Added TurboModule manager registration to AppDelegate.mm');
        }
      }
      
      return config;
    },
  ]);
};

module.exports = withLlamaCppRn;
