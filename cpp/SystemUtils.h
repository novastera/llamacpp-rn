#pragma once

#include <string>
#include <thread>
#include "llama.h"

namespace facebook::react {

class SystemUtils {
public:
    /**
     * Calculates the optimal number of threads for llama.cpp based on available CPU cores.
     * The logic follows these rules:
     * - If only 1 core, use 1 thread
     * - If less than 4 cores, use (cores - 1) threads
     * - If 4+ cores, use (cores - 2) threads
     */
    static int getOptimalThreadCount();

    /**
     * Normalizes a file path by removing file:// prefix if present.
     * This is useful for handling paths that might come from different sources.
     */
    static std::string normalizeFilePath(const std::string& path);

    /**
     * Calculates the optimal number of GPU layers for model inference.
     * Should only be called if llama_supports_gpu_offload() returns true.
     * The calculation takes into account:
     * - Available GPU memory
     * - Model size and parameters
     * - Platform-specific optimizations
     * - Current quantization method
     * 
     * @param model Pointer to an already loaded llama model
     * @return Optimal number of GPU layers (0 if GPU not supported)
     */
    static int getOptimalGpuLayers(struct llama_model* model);

};

} // namespace facebook::react 