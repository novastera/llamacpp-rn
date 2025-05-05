#include "rn-llama.hpp"
#include "common.h"
#include "log.h"
#include "chat.h"
#include "llama.h"
#include "sampling.h"

#include <string>
#include <vector>
#include <thread>
#include <functional>
#include <mutex>
#include <condition_variable>

namespace facebook::react {

// Forward declaration for internal use only
struct completion_state;

// Helper function to check for stopping criteria
static bool check_stop_conditions(
    completion_state& state,
    const std::vector<std::string>& stop_words,
    const std::string& token_text,
    bool ignore_eos);

// Helper function to find partial stop words
size_t find_partial_stop_string(const std::string& stop_word, const std::string& text) {
    // If the text ends with a partial match of the stop word, return the position
    // where that partial match begins
    if (stop_word.size() > text.size()) {
        for (size_t i = 1; i < text.size() + 1; ++i) {
            if (stop_word.substr(0, i) == text.substr(text.size() - i, i)) {
                return text.size() - i;
            }
        }
    }
    return std::string::npos;
}

// Struct to track prediction completion state
struct completion_state {
    bool stream = false;
    bool has_next_token = true;
    bool has_new_line = false;
    bool truncated = false;
    
    int n_past = 0;
    int n_ctx = 0;
    int n_predict = 0;
    int n_decoded = 0;
    int n_remaining = 0;
    
    size_t n_sent_text = 0;
    size_t last_nl_pos = 0;
    
    std::string generated_text;
    std::string stopping_word;
    bool stop_found = false;
    
    std::vector<llama_token> prompt_tokens;
    std::vector<llama_token> generated_tokens;
    
    common_sampler* sampler = nullptr;
    std::vector<std::string> antiprompt; // Storing stop words here
    
    ~completion_state() {
        if (sampler) {
            common_sampler_free(sampler);
            sampler = nullptr;
        }
    }
};

// Helper function to check for stopping criteria
static bool check_stop_conditions(
    completion_state& state,
    const std::vector<std::string>& stop_words,
    const std::string& token_text,
    bool ignore_eos) {
    
    if (state.n_remaining <= 0) {
        state.has_next_token = false;
        return true;
    }
    
    // Check for stopping strings
    size_t stop_pos = std::string::npos;
    
    for (const std::string & word : stop_words) {
        size_t pos = state.generated_text.find(word, state.n_sent_text > 0 ? state.n_sent_text - 1 : 0);
        if (pos != std::string::npos && (stop_pos == std::string::npos || pos < stop_pos)) {
            state.stopping_word = word;
            state.has_next_token = false;
            stop_pos = pos;
        }
    }
    
    if (stop_pos != std::string::npos) {
        state.generated_text.erase(
            state.generated_text.begin() + stop_pos,
            state.generated_text.end()
        );
        state.stop_found = true;
        return true;
    }
    
    // Check for partial stop strings at the end
    for (const std::string & word : stop_words) {
        if (size_t partial_pos = find_partial_stop_string(word, state.generated_text); 
            partial_pos != std::string::npos) {
            return false; // Don't send token yet, wait for full stop word
        }
    }
    
    // Check if context is full
    if (state.n_past >= state.n_ctx) {
        state.truncated = true;
        state.has_next_token = false;
        return true;
    }
    
    // Check for newline condition
    if (token_text.find('\n') != std::string::npos) {
        state.has_new_line = true;
    }
    
    return false;
}

CompletionResult run_completion(
    rn_llama_context* rn_ctx,
    const CompletionOptions& options,
    std::function<bool(const std::string&, bool)> callback) {
    
    CompletionResult result;
    completion_state state;
    
    if (!rn_ctx || !rn_ctx->model || !rn_ctx->ctx) {
        result.success = false;
        result.error_msg = "Model not initialized";
        result.error_type = RN_ERROR_MODEL_LOAD;
        return result;
    }
    
    try {
        // Convert CompletionOptions to JSON for processing
        json data = options.to_json();
        // Prepare the sampling parameters
        auto& params = rn_ctx->params;
        
        // Set the prompt
        if (data.contains("prompt")) {
            // Tokenize the prompt
            auto tokenized_prompts = tokenize_input_prompts(rn_ctx->vocab, data["prompt"], true, true);
            if (tokenized_prompts.empty() || tokenized_prompts[0].empty()) {
                result.success = false;
                result.error_msg = "Empty prompt";
                result.error_type = RN_ERROR_INVALID_PARAM;
                return result;
            }
            state.prompt_tokens = std::move(tokenized_prompts[0]);
        } else {
            result.success = false;
            result.error_msg = "No prompt provided";
            result.error_type = RN_ERROR_INVALID_PARAM;
            return result;
        }
        
        // Configure state
        state.n_ctx = llama_n_ctx(rn_ctx->ctx);
        state.n_predict = options.n_predict > 0 ? options.n_predict : params.n_predict;
        state.n_remaining = state.n_predict;
        state.stream = options.stream;
        
        // Initialize the sampler
        state.sampler = common_sampler_init(rn_ctx->model, params.sampling);
        if (!state.sampler) {
            result.success = false;
            result.error_msg = "Failed to initialize sampler";
            result.error_type = RN_ERROR_INFERENCE;
            return result;
        }
        
        // Process stop words
        if (data.contains("stop")) {
            if (data["stop"].is_string()) {
                state.antiprompt.push_back(data["stop"].get<std::string>());
            } else if (data["stop"].is_array()) {
                for (const auto& stop : data["stop"]) {
                    if (stop.is_string()) {
                        state.antiprompt.push_back(stop.get<std::string>());
                    }
                }
            }
        }
        
        /*RN_INF("Processing prompt with %d tokens, n_predict = %d", 
            (int)state.prompt_tokens.size(), state.n_predict);*/
        
        // Process the prompt
        for (int i = 0; i < (int)state.prompt_tokens.size(); ++i) {
            llama_token token = state.prompt_tokens[i];
            
            llama_batch batch = { 
                /* n_tokens    */ 1,
                /* token       */ &token,
                /* embd        */ nullptr,
                /* pos         */ &i,
                /* n_seq_id    */ nullptr,
                /* seq_id      */ nullptr,
                /* logits      */ nullptr
            };
            
            if (llama_decode(rn_ctx->ctx, batch) != 0) {
                result.success = false;
                result.error_msg = "Failed to process prompt";
                result.error_type = RN_ERROR_INFERENCE;
                return result;
            }
            
            common_sampler_accept(state.sampler, token, true);
            state.n_past++;
        }
        
        result.n_prompt_tokens = state.prompt_tokens.size();
        
        // Start generating tokens
        const int64_t t_start_generation = ggml_time_us();
        
        while (state.has_next_token && state.n_remaining > 0) {
            // Sample the next token
            llama_token token_id = common_sampler_sample(state.sampler, rn_ctx->ctx, -1);
            
            // Extract the token text
            std::string token_text = common_token_to_piece(rn_ctx->ctx, token_id);
            
            // Add to generated text
            state.generated_text += token_text;
            state.generated_tokens.push_back(token_id);
            
            // Update state
            state.n_decoded++;
            state.n_remaining--;
            
            // Accept the new token
            common_sampler_accept(state.sampler, token_id, true);
            
            // Prepare for next token
            llama_batch batch = { 
                /* n_tokens    */ 1,
                /* token       */ &token_id,
                /* embd        */ nullptr,
                /* pos         */ &state.n_past,
                /* n_seq_id    */ nullptr,
                /* seq_id      */ nullptr,
                /* logits      */ nullptr
            };
            
            if (llama_decode(rn_ctx->ctx, batch) != 0) {
                result.success = false;
                result.error_msg = "Failed to decode generated token";
                result.error_type = RN_ERROR_INFERENCE;
                return result;
            }
            
            state.n_past++;
            
            // Check stopping conditions
            bool should_stop = check_stop_conditions(state, state.antiprompt, token_text, options.ignore_eos);
            
            // Handle stream mode
            if (state.stream && callback && !should_stop) {
                std::string text_to_send = state.generated_text.substr(state.n_sent_text);
                state.n_sent_text = state.generated_text.size();
                
                // Send the token
                if (!callback(text_to_send, false)) {
                    // Callback returned false, stop generation
                    state.has_next_token = false;
                    break;
                }
            }
            
            // Check if should stop (after streaming so we don't miss the last token)
            if (should_stop) {
                break;
            }
            
            // Check for EOS token if not ignoring
            if (!options.ignore_eos && token_id == llama_vocab_eos(rn_ctx->vocab)) {
                state.has_next_token = false;
                break;
            }
        }
        
        const int64_t t_end_generation = ggml_time_us();
        const double generation_time_ms = (t_end_generation - t_start_generation) / 1000.0;
        
        /*RN_INF("Completion finished: %d tokens generated in %.2f ms (%.2f tokens/s)",
            state.n_decoded, generation_time_ms, 
            state.n_decoded > 0 ? (state.n_decoded * 1000.0) / generation_time_ms : 0);*/
        
        // Set the result
        result.content = state.generated_text;
        result.tokens = state.generated_tokens;
        result.n_prompt_tokens = state.prompt_tokens.size();
        result.n_predicted_tokens = state.n_decoded;
        
        // Final callback with is_done=true
        if (callback) {
            callback(state.stream ? "" : state.generated_text, true);
        }
        
        return result;
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_msg = e.what();
        result.error_type = RN_ERROR_GENERAL;
        return result;
    }
}

CompletionResult run_chat_completion(
    rn_llama_context* rn_ctx,
    const CompletionOptions& options,
    std::function<bool(const std::string&, bool)> callback) {
    
    CompletionResult result;
    
    if (!rn_ctx || !rn_ctx->model || !rn_ctx->ctx) {
        result.success = false;
        result.error_msg = "Model not initialized";
        result.error_type = RN_ERROR_MODEL_LOAD;
        return result;
    }
    //should be in the model init TOMOVE
    try {
        // Convert chat options to JSON
        json data = options.to_chat_json();
        
        // Parse into llama_params
        json llama_params = oaicompat_completion_params_parse(
            data, 
            rn_ctx->params.use_jinja, 
            rn_ctx->params.reasoning_format,
            rn_ctx->chat_templates.get()
        );
        
        // Debug information for parameters
        // Now run standard completion with the processed parameters
        CompletionOptions processed_options = options;
        processed_options.prompt = llama_params["prompt"].get<std::string>();
        
        // Copy any stop words from the processed parameters
        if (llama_params.contains("stop") && llama_params["stop"].is_array()) {
            processed_options.stop.clear();
            for (const auto& stop : llama_params["stop"]) {
                processed_options.stop.push_back(stop.get<std::string>());
            }
        }
        
        return run_completion(rn_ctx, processed_options, callback);
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_msg = e.what();
        result.error_type = RN_ERROR_INVALID_PARAM;
        return result;
    }
}

} // namespace facebook::react

