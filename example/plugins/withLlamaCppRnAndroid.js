const { withDangerousMod } = require('@expo/config-plugins');
const fs = require('fs');
const path = require('path');

/**
 * Config plugin to ensure LlamaCppRn is properly registered in Android Expo builds
 */
const withLlamaCppRnAndroid = (config) => {
  return withDangerousMod(config, [
    'android',
    async (config) => {
      const mainApplicationPath = path.join(
        config.modRequest.platformProjectRoot,
        'app/src/main/java/com/anonymous/example/MainApplication.kt'
      );
      
      if (fs.existsSync(mainApplicationPath)) {
        let mainApplicationContent = fs.readFileSync(mainApplicationPath, 'utf8');
        
        // Add import if needed
        if (!mainApplicationContent.includes('import com.novastera.llamacpprn.LlamaCppRnPackage')) {
          mainApplicationContent = mainApplicationContent.replace(
            'import expo.modules.ReactNativeHostWrapper',
            'import expo.modules.ReactNativeHostWrapper\n\n// Import the LlamaCppRn package\nimport com.novastera.llamacpprn.LlamaCppRnPackage'
          );
        }
        
        // Add package registration if needed
        if (!mainApplicationContent.includes('packages.add(LlamaCppRnPackage())')) {
          mainApplicationContent = mainApplicationContent.replace(
            'val packages = PackageList(this).packages',
            'val packages = PackageList(this).packages\n            // Manually add the LlamaCppRnPackage\n            packages.add(LlamaCppRnPackage())'
          );
        }
        
        fs.writeFileSync(mainApplicationPath, mainApplicationContent);
        console.log('Added LlamaCppRnPackage registration to MainApplication.kt');
      }
      
      // Find or create the cpp directory if it doesn't exist
      const cppDir = path.join(config.modRequest.platformProjectRoot, 'app/src/main/cpp');
      if (!fs.existsSync(cppDir)) {
        fs.mkdirSync(cppDir, { recursive: true });
      }
      
      // Create or update OnLoad.cpp
      const onLoadPath = path.join(cppDir, 'OnLoad.cpp');
      const onLoadContent = `
#include <ReactCommon/CallInvokerHolder.h>
#include <fbjni/fbjni.h>
#include <jsi/jsi.h>
#include <memory>
#include <string>

// Import the LlamaCppRn module header
#include "../../../../../../node_modules/@novastera-oss/llamacpp-rn/cpp/LlamaCppRnModule.h"

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
  return facebook::jni::initialize(vm, [] {});
}

extern "C"
JNIEXPORT jsi::jsi::Runtime *JNICALL
Java_com_facebook_react_turbomodule_core_TurboModuleManager_codegenNativeModules(
    JNIEnv *env, jclass clazz, jsi::jsi::Runtime *rt, 
    facebook::react::CallInvokerHolder *jsCallInvokerHolder, jstring moduleName) {
    
    auto jsCallInvoker = jsCallInvokerHolder->cthis()->getCallInvoker();
    const char *moduleNameStr = env->GetStringUTFChars(moduleName, nullptr);
    std::string name = moduleNameStr;
    env->ReleaseStringUTFChars(moduleName, moduleNameStr);
    
    // Create and install the LlamaCppRn module if requested
    if (name == "LlamaCppRn") {
      auto module = facebook::react::LlamaCppRn::create(jsCallInvoker);
      rt->global().setProperty(*rt, jsi::PropNameID::forAscii(*rt, name), 
                              jsi::Object::createFromHostObject(*rt, module));
    }
    
    return rt;
}
`;
      
      if (!fs.existsSync(onLoadPath) || 
          !fs.readFileSync(onLoadPath, 'utf8').includes('LlamaCppRn::create')) {
        fs.writeFileSync(onLoadPath, onLoadContent);
        console.log('Created or updated OnLoad.cpp with LlamaCppRn support');
      }
      
      // Make sure C++ part of the module is properly configured
      const cmakeListsPath = path.join(
        config.modRequest.platformProjectRoot,
        'app/src/main/cpp/CMakeLists.txt'
      );
      
      const cmakeContent = `
cmake_minimum_required(VERSION 3.13)

# Define the library target
project(appmodules)

# Define path to React Native Android source
set(REACT_NATIVE_DIR "\${CMAKE_CURRENT_SOURCE_DIR}/../../../../../../node_modules/react-native")
set(APP_ROOT_DIR "\${CMAKE_CURRENT_SOURCE_DIR}/../../../../../..")

# Include modules we need
include_directories(
    "\${REACT_NATIVE_DIR}/ReactAndroid/src/main/jni"
    "\${REACT_NATIVE_DIR}/ReactAndroid/src/main/java/com/facebook/react/turbomodule/core/jni"
    "\${REACT_NATIVE_DIR}/ReactCommon"
    "\${REACT_NATIVE_DIR}/ReactCommon/callinvoker"
    "\${REACT_NATIVE_DIR}/ReactCommon/jsi"
    "\${APP_ROOT_DIR}/node_modules/@novastera-oss/llamacpp-rn/cpp"
)

# Create our module library
add_library(appmodules SHARED OnLoad.cpp)

# Link required libraries
target_link_libraries(appmodules
    fbjni
    jsi
    react_nativemodule_core
)

# Get all the default compiler and linker flags
include("\${REACT_NATIVE_DIR}/ReactAndroid/cmake-utils/ReactNative-application.cmake")

# Add the LlamaCppRn module
add_subdirectory("\${APP_ROOT_DIR}/node_modules/@novastera-oss/llamacpp-rn/android/src/main/cpp" llamacpp-rn-build)
target_link_libraries(appmodules llamacpprn)
`;
      
      if (!fs.existsSync(cmakeListsPath) || 
          !fs.readFileSync(cmakeListsPath, 'utf8').includes('llamacpprn')) {
        fs.writeFileSync(cmakeListsPath, cmakeContent);
        console.log('Created or updated CMakeLists.txt with LlamaCppRn support');
      }
      
      return config;
    },
  ]);
};

module.exports = withLlamaCppRnAndroid;