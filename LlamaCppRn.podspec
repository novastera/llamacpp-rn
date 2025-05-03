require "json"

package = JSON.parse(File.read(File.join(__dir__, "package.json")))

Pod::Spec.new do |s|
  s.name         = "LlamaCppRn"
  s.version      = package["version"]
  s.summary      = package["description"]
  s.homepage     = package["homepage"]
  s.license      = package["license"]
  s.authors      = package["author"]

  s.platforms    = { :ios => "13.0" }
  s.source       = { :git => "https://github.com/novastera/llamacpp-rn.git", :tag => "#{s.version}" }

  # Only include the necessary implementation files
  s.source_files = "ios/**/*.{h,m,mm}", 
                   "cpp/LlamaCppRnModule.h", 
                   "cpp/LlamaCppRnModule.cpp", 
                   "cpp/LlamaCppModel.h", 
                   "cpp/LlamaCppModel.cpp", 
                   "cpp/SystemUtils.h",
                   "cpp/SystemUtils.cpp",
                   "cpp/llama.cpp/common/common.h",
                   "cpp/llama.cpp/common/build-info.cpp",
                   "cpp/llama.cpp/common/json-schema-to-grammar.cpp",
                   "cpp/llama.cpp/common/json-schema-to-grammar.h",
                   "cpp/llama.cpp/common/common.cpp",
                   "cpp/llama.cpp/common/chat.cpp",
                   "cpp/llama.cpp/common/chat.h",
                   "cpp/llama.cpp/common/log.cpp",
                   "cpp/llama.cpp/common/log.h",
                   "cpp/llama.cpp/common/sampling.cpp",
                   "cpp/llama.cpp/common/sampling.h",
                   "cpp/llama.cpp/common/ngram-cache.cpp",
                   "cpp/llama.cpp/common/ngram-cache.h",
                   "cpp/llama.cpp/common/minja/chat-template.hpp",
                   "cpp/llama.cpp/common/minja/minja.hpp",
                   "cpp/llama.cpp/llama.cpp",
                   "cpp/llama.cpp/common/json.hpp",
                   "cpp/llama.cpp/common/speculative.cpp",
                   "cpp/llama.cpp/common/speculative.h"
                   
  # Include llama.cpp headers for compilation
  s.preserve_paths = "ios/include/**/*.h", "ios/framework/**/*"
  
  # Use the prebuilt framework
  s.vendored_frameworks = "ios/framework/build-apple/llama.xcframework"

  # Compiler settings
  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/ios/include\" \"$(PODS_TARGET_SRCROOT)/cpp\" \"$(PODS_ROOT)/boost\" \"$(PODS_ROOT)/Headers/Public/React-bridging\" \"$(PODS_ROOT)/Headers/Public/React\"",
    "OTHER_CPLUSPLUSFLAGS" => "-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -DLLAMA_METAL -DRCT_NEW_ARCH_ENABLED=1",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++17",
    "GCC_OPTIMIZATION_LEVEL" => "3", # Maximum optimization
    "SWIFT_OPTIMIZATION_LEVEL" => "-O",
    "ENABLE_BITCODE" => "NO",
    "DEFINES_MODULE" => "YES",
    "OTHER_LDFLAGS" => "$(inherited)"
  }

  # Add user_target_xcconfig to propagate linker flags
  s.user_target_xcconfig = {
    "OTHER_LDFLAGS" => "$(inherited)"
  }

  # React Native dependencies (new architecture)
  s.dependency "React-Core"
  s.dependency "React-Codegen"
  s.dependency "React-RCTAppDelegate"
  s.dependency "RCT-Folly"
  s.dependency "RCTRequired"
  s.dependency "RCTTypeSafety"
  s.dependency "ReactCommon/turbomodule/core"

  # Install dependencies for Turbo Modules
  install_modules_dependencies(s)
end