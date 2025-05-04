#pragma once

#include "common.h"
#include "llama.h"
#include "rn-utils.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

// Forward declarations
struct llama_model;
struct llama_context;
struct llama_vocab;

namespace facebook::react {

// RN-Llama context adapter structure
struct rn_llama_context {
    // Model parameters
    common_params params;
    
    // Core llama.cpp components
    llama_model* model = nullptr;
    llama_context* ctx = nullptr;
    const llama_vocab* vocab = nullptr;
    
    // Extensions
    std::vector<common_adapter_lora_info> lora_adapters;
    common_chat_templates_ptr chat_templates;
    
    // State
    bool model_loaded = false;
    std::mutex mutex;
};

// Core completion functions
CompletionResult run_completion(
    rn_llama_context* rn_ctx,
    const CompletionOptions& options,
    std::function<bool(const std::string&, bool)> callback);

CompletionResult run_chat_completion(
    rn_llama_context* rn_ctx,
    const CompletionOptions& options,
    std::function<bool(const std::string&, bool)> callback);

// Helper to find partial stop words
size_t find_partial_stop_string(const std::string& stop_word, const std::string& text);

} // namespace facebook::react 