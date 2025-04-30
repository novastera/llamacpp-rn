#include "SystemUtils.h"
#include "llama.h"

namespace facebook::react {

int SystemUtils::getOptimalThreadCount() {
    int cpuCores = std::thread::hardware_concurrency();
    
    if (cpuCores <= 1) {
        return 1;
    } else if (cpuCores < 4) {
        return cpuCores - 1;
    } else {
        return cpuCores - 2;
    }
}

std::string SystemUtils::normalizeFilePath(const std::string& path) {
    // Remove file:// prefix if present
    if (path.substr(0, 7) == "file://") {
        return path.substr(7);
    }
    return path;
}

int SystemUtils::getOptimalGpuLayers() {
#if defined(__APPLE__)
    // For Apple devices, we can be more aggressive with GPU layers
    // since Metal is well optimized
    return 20;  // Default for Apple devices
#else
    // For other platforms, be more conservative
    return 10;  // Default for other platforms
#endif
}

} // namespace facebook::react 