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

  # Core React Native module files - keep iOS-specific files separate
  s.source_files = "ios/**/*.{h,m,mm}",  # iOS-specific Obj-C++ files
                   # Core C++ module implementation (keep as .cpp)
                   "tm/build-info.cpp",
                   "tm/LlamaCppRnModule.{h,cpp}",
                   "tm/LlamaCppModel.{h,cpp}",
                   "tm/SystemUtils.{h,cpp}",
                   "tm/rn-*.{hpp,cpp}",
                   # llama.cpp common utilities
                   "tm/llama.cpp/common/common.{h,cpp}",
                   "tm/llama.cpp/common/log.{h,cpp}",
                   "tm/llama.cpp/common/sampling.{h,cpp}",
                   "tm/llama.cpp/common/chat.{h,cpp}",
                   "tm/llama.cpp/common/ngram-cache.{h,cpp}",
                   "tm/llama.cpp/common/json-schema-to-grammar.{h,cpp}",
                   "tm/llama.cpp/common/speculative.{h,cpp}",
                   "tm/llama.cpp/common/*.hpp",
                   "tm/llama.cpp/common/minja/*.hpp"
  
  # Include all necessary headers for compilation
  s.preserve_paths = "ios/include/**/*.h",
                     "ios/libs/**/*", 
                     "tm/**/*"
  
  # Use the prebuilt framework
  s.vendored_frameworks = "ios/libs/llama.xcframework"

  # Compiler settings
  s.pod_target_xcconfig = {
    "HEADER_SEARCH_PATHS" => "\"$(PODS_TARGET_SRCROOT)/ios/include\" \"$(PODS_TARGET_SRCROOT)/tm\" \"$(PODS_TARGET_SRCROOT)/tm/llama.cpp\" \"$(PODS_TARGET_SRCROOT)/tm/llama.cpp/include\" \"$(PODS_TARGET_SRCROOT)/tm/llama.cpp/ggml/include\" \"$(PODS_TARGET_SRCROOT)/tm/llama.cpp/common\" \"$(PODS_ROOT)/boost\" \"$(PODS_ROOT)/Headers/Public/React-bridging\" \"$(PODS_ROOT)/Headers/Public/React\"",
    "OTHER_CPLUSPLUSFLAGS" => "-DFOLLY_NO_CONFIG -DFOLLY_MOBILE=1 -DFOLLY_USE_LIBCPP=1 -DLLAMA_METAL -DRCT_NEW_ARCH_ENABLED=1 -DFBJSRT_EXPORTED=1",
    "CLANG_CXX_LANGUAGE_STANDARD" => "c++17",
    "GCC_OPTIMIZATION_LEVEL" => "3", # Maximum optimization
    "SWIFT_OPTIMIZATION_LEVEL" => "-O",
    "ENABLE_BITCODE" => "NO",
    "DEFINES_MODULE" => "YES",
    "OTHER_LDFLAGS" => "$(inherited)",
    # These preprocessor macros ensure TurboModule registration works correctly
    "GCC_PREPROCESSOR_DEFINITIONS" => ["$(inherited)", "RCT_NEW_ARCH_ENABLED=1", 
                                       "__STDC_FORMAT_MACROS=1", # For format macros in C++
                                       "LLAMA_SHARED=1"]         # For llama shared symbols
  }

  # Add user_target_xcconfig to propagate linker flags
  s.user_target_xcconfig = {
    "OTHER_LDFLAGS" => "$(inherited)"
  }

  # Install dependencies for Turbo Modules
  install_modules_dependencies(s)
end