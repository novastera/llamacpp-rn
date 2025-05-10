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

  # Core React Native module files
  s.source_files = "ios/**/*.{h,m,mm}",
                   # Core module implementation
                   "cpp/build-info.cpp",
                   "cpp/LlamaCppRnModule.{h,cpp}",
                   "cpp/LlamaCppModel.{h,cpp}",
                   "cpp/SystemUtils.{h,cpp}",
                   "cpp/rn-*.{hpp,cpp}",
                   # llama.cpp common utilities
                   "cpp/llama.cpp/common/common.{h,cpp}",
                   "cpp/llama.cpp/common/log.{h,cpp}",
                   "cpp/llama.cpp/common/sampling.{h,cpp}",
                   "cpp/llama.cpp/common/chat.{h,cpp}",
                   "cpp/llama.cpp/common/ngram-cache.{h,cpp}",
                   "cpp/llama.cpp/common/json-schema-to-grammar.{h,cpp}",
                   "cpp/llama.cpp/common/speculative.{h,cpp}",
                   "cpp/llama.cpp/common/*.hpp",
                   "cpp/llama.cpp/common/minja/*.hpp"
  
  # Include all necessary headers for compilation
  s.preserve_paths = "ios/include/**/*.h",
                     "ios/libs/**/*", 
                     "cpp/llama.cpp/**/*.h", 
                     "cpp/llama.cpp/**/*.hpp"
  
  # Use the prebuilt framework
  s.vendored_frameworks = "ios/libs/llama.xcframework"

  # Compiler settings
  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/ios/include\" \"$(PODS_TARGET_SRCROOT)/ios/include/ggml\" \"$(PODS_TARGET_SRCROOT)/ios/include/common\" \"$(PODS_TARGET_SRCROOT)/cpp\" \"$(PODS_TARGET_SRCROOT)/cpp/llama.cpp\" \"$(PODS_TARGET_SRCROOT)/cpp/llama.cpp/include\" \"$(PODS_TARGET_SRCROOT)/cpp/llama.cpp/ggml/include\" \"$(PODS_TARGET_SRCROOT)/cpp/llama.cpp/common\" \"$(PODS_ROOT)/boost\" \"$(PODS_ROOT)/Headers/Public/React-bridging\" \"$(PODS_ROOT)/Headers/Public/React\"",
    "OTHER_CPLUSPLUSFLAGS" => "-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -DLLAMA_METAL -DRCT_NEW_ARCH_ENABLED=1",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++17",
    "GCC_OPTIMIZATION_LEVEL" => "3", # Maximum optimization
    "SWIFT_OPTIMIZATION_LEVEL" => "-O",
    "ENABLE_BITCODE" => "NO",
    "DEFINES_MODULE" => "YES",
    "OTHER_LDFLAGS" => "$(inherited)",
    # These preprocessor macros ensure TurboModule registration works correctly
    "GCC_PREPROCESSOR_DEFINITIONS" => ["$(inherited)", "RCT_NEW_ARCH_ENABLED=1"]
  }

  # Add user_target_xcconfig to propagate linker flags
  s.user_target_xcconfig = {
    "OTHER_LDFLAGS" => "$(inherited)"
  }

  # Install dependencies for Turbo Modules
  install_modules_dependencies(s)
end