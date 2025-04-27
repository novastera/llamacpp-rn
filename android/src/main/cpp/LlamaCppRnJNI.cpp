#include <jni.h>
#include <string>
#include <memory>
#include <fbjni/fbjni.h>
#include <jsi/jsi.h>
#include <ReactCommon/CallInvokerHolder.h>
#include <ReactCommon/TurboModuleUtils.h>
#include <react/bridging/ABI.h>
#include <android/log.h>
#include <dlfcn.h>

#include <stdexcept>
#include <unistd.h>
#include "llama.h"

#include "../../../../cpp/LlamaCppRnModule.h"

#define LLAMACPPRN_TAG "LlamaCppRn"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LLAMACPPRN_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LLAMACPPRN_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LLAMACPPRN_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LLAMACPPRN_TAG, __VA_ARGS__)

using namespace facebook;
using namespace facebook::jni;
using namespace facebook::react;

namespace llama_cpp_rn {

// Class to handle device capabilities detection
class DeviceCapabilities {
public:
    static bool detectAndInitializeGPU() {
        try {
            LOGI("Checking for GPU acceleration capabilities...");
            
            #if defined(LLAMACPPRN_OPENCL_ENABLED) && LLAMACPPRN_OPENCL_ENABLED == 1
                // Check if OpenCL is available and supported
                LOGI("OpenCL support is enabled in build");
                
                // Try to load the OpenCL library dynamically
                bool hasOpenCL = tryLoadOpenCLLibrary();
                
                if (hasOpenCL) {
                    LOGI("OpenCL is available on this device");
                    return true;
                } else {
                    LOGW("OpenCL is not available on this device, using CPU only");
                    return false;
                }
            #else
                LOGI("OpenCL support is not enabled in build, using CPU only");
                return false;
            #endif
        } catch (const std::exception& e) {
            LOGE("Error detecting GPU capabilities: %s", e.what());
            return false;
        }
    }
    
    static int getOptimalThreadCount() {
        // Get the number of CPU cores available for optimal thread count
        int cpuCores = sysconf(_SC_NPROCESSORS_ONLN);
        
        // Advanced thread allocation strategy:
        // - For single-core devices: use 1 thread
        // - For 2-4 core devices: leave 1 core free for UI and system
        // - For devices with >4 cores: leave 2 cores free for UI, system, and background processes
        if (cpuCores <= 1) {
            return 1;
        } else if (cpuCores <= 4) {
            return cpuCores - 1;
        } else {
            return cpuCores - 2;
        }
    }
    
    static void logDeviceInfo() {
        // Log useful device information
        LOGI("Device information:");
        LOGI("- CPU cores: %d", sysconf(_SC_NPROCESSORS_ONLN));
        LOGI("- Optimal thread count: %d", getOptimalThreadCount());
        
        // Log memory information
        long pages = sysconf(_SC_PHYS_PAGES);
        long page_size = sysconf(_SC_PAGE_SIZE);
        double total_memory_gb = (pages * page_size) / (1024.0 * 1024.0 * 1024.0);
        
        LOGI("- Total device memory: %.2f GB", total_memory_gb);
        
        // Check for GPU capabilities
        checkGPUCapabilities();
    }
    
private:
    static bool tryLoadOpenCLLibrary() {
        // Common paths for OpenCL libraries on Android devices
        const char* potentialPaths[] = {
            "/system/vendor/lib64/libOpenCL.so",
            "/system/lib64/libOpenCL.so",
            "/vendor/lib64/libOpenCL.so",
            "/vendor/lib64/egl/libGLES_mali.so",    // Mali GPUs
            "/vendor/lib64/libPVROCL.so",           // PowerVR
            "/vendor/lib64/libq3dtools_adreno.so"   // Qualcomm Adreno
        };
        
        void* handle = nullptr;
        
        // Try to load the OpenCL library from various potential locations
        for (const char* path : potentialPaths) {
            handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
            if (handle) {
                LOGI("Successfully loaded OpenCL from: %s", path);
                dlclose(handle);  // We just want to check if it's loadable
                return true;
            }
        }
        
        LOGW("Could not find a loadable OpenCL library on this device");
        return false;
    }
    
    static void checkGPUCapabilities() {
        LOGI("GPU capabilities:");
        
        #if defined(LLAMACPPRN_OPENCL_ENABLED) && LLAMACPPRN_OPENCL_ENABLED == 1
            bool hasOpenCL = tryLoadOpenCLLibrary();
            LOGI("- OpenCL support: %s", hasOpenCL ? "Available" : "Not available");
            
            if (hasOpenCL) {
                // In a real implementation, we could query more detailed GPU information
                // This would require initializing OpenCL and querying device properties
                LOGI("- GPU acceleration will be available for compatible models");
            }
        #else
            LOGI("- OpenCL support: Not compiled in this build");
        #endif
        
        // Check for other acceleration options
        LOGI("- BLAS acceleration: Available for ARM64");
        LOGI("- NEON/SIMD acceleration: Available for ARM64");
    }
};

} // namespace llama_cpp_rn

// Turbo Module provider function for the C++ implementation
extern "C" JNIEXPORT jni::local_ref<JTurboModule> createTurboModule(
    jni::alias_ref<JRuntimeExecutor::javaobject> runtimeExecutor,
    jni::alias_ref<CallInvokerHolder::javaobject> jsCallInvokerHolder) {
    
    try {
        // Log llama.cpp version
        LOGI("Initializing LlamaCppRn with llama.cpp version: %s", LLAMA_VERSION);
        
        // Detect and log device capabilities
        llama_cpp_rn::DeviceCapabilities::logDeviceInfo();
        bool gpuAvailable = llama_cpp_rn::DeviceCapabilities::detectAndInitializeGPU();
        LOGI("GPU acceleration available: %s", gpuAvailable ? "yes" : "no");
        
        // Create the Turbo Module
        auto jsCallInvoker = jsCallInvokerHolder->cthis()->getCallInvoker();
        auto turboModule = LlamaCppRn::create(jsCallInvoker);
        
        return jni::make_local(abi40_0_0::bridging::createCxxTurboModuleJavaObject(
            runtimeExecutor, turboModule, LlamaCppRn::kModuleName));
    } catch (const std::exception& e) {
        LOGE("Failed to create LlamaCppRn Turbo Module: %s", e.what());
        throw;
    }
}

// Load the fbjni module
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *) {
    return facebook::jni::initialize(vm, [] {
        LOGI("LlamaCppRn JNI module loaded");
    });
} 