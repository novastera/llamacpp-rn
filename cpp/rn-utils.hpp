#pragma once

#include "common.h"
#include "log.h"
#include "llama.h"
#include "sampling.h"

// Change JSON_ASSERT from assert() to GGML_ASSERT:
#define JSON_ASSERT GGML_ASSERT
#include "json.hpp"
#include "chat.h"

#include <random>
#include <sstream>
#include <string>
#include <vector>
#include <memory>

using json = nlohmann::ordered_json;

#define DEFAULT_OAICOMPAT_MODEL "gpt-3.5-turbo"

const static std::string build_info("b" + std::to_string(LLAMA_BUILD_NUMBER) + "-" + LLAMA_COMMIT);

// Error types simplified for a library context
enum rn_error_type {
    RN_ERROR_INVALID_PARAM,  // Invalid parameters provided
    RN_ERROR_MODEL_LOAD,     // Failed to load model
    RN_ERROR_CONTEXT,        // Context-related errors
    RN_ERROR_INFERENCE,      // Inference-related errors
    RN_ERROR_GENERAL         // General errors
};

// Forward declaration
struct common_sampler;

// These functions are defined in common/sampling.cpp, only declare them here
common_sampler* common_sampler_init(const llama_model* model, const common_params_sampling& params);
void common_sampler_free(common_sampler* sampler);

// CompletionOptions struct to represent parameters for completion requests
struct CompletionOptions {
    std::string prompt;  // for simple completions
    json messages;       // for chat completions
    bool stream = false;
    int n_predict = -1;
    float temperature = 0.8f;
    float top_p = 0.9f;
    float top_k = 40.0f;
    float min_p = 0.05f;
    int n_keep = 0;
    int n_probs = 0;  // for log probabilities
    bool post_sampling_probs = false;
    std::vector<std::string> stop;
    std::string grammar;
    bool grammar_lazy = false;
    bool ignore_eos = false;
    std::string chat_template;
    bool use_jinja = false;
    int seed = -1;
    json tools;         // tools for function calling

    // Convert to JSON for the completion API
    json to_json() const {
        json j = {
            {"prompt", prompt},
            {"stream", stream},
            {"temperature", temperature},
            {"top_p", top_p},
            {"top_k", top_k},
            {"min_p", min_p},
            {"n_predict", n_predict},
            {"n_keep", n_keep},
            {"n_probs", n_probs},
            {"post_sampling_probs", post_sampling_probs},
            {"stop", stop},
            {"ignore_eos", ignore_eos},
            {"seed", seed}
        };

        if (!grammar.empty()) {
            j["grammar"] = grammar;
            j["grammar_lazy"] = grammar_lazy;
        }
        return j;
    }

    // Convert to JSON for the chat completion API
    json to_chat_json() const {
        json j = {
            {"messages", messages},
            {"stream", stream},
            {"temperature", temperature},
            {"top_p", top_p},
            {"top_k", top_k},
            {"min_p", min_p},
            {"max_tokens", n_predict},
            {"n_probs", n_probs},
            {"post_sampling_probs", post_sampling_probs},
            {"stop", stop},
            {"ignore_eos", ignore_eos},
            {"seed", seed}
        };

        if (!grammar.empty()) {
            j["grammar"] = grammar;
            j["grammar_lazy"] = grammar_lazy;
        }
        
        if (!chat_template.empty()) {
            j["chat_template"] = chat_template;
        }
        
        return j;
    }
};

// CompletionResult struct to hold completion response data
struct CompletionResult {
    std::string content;
    bool success = true;
    std::string error_msg;
    rn_error_type error_type = RN_ERROR_GENERAL;
    int n_prompt_tokens = 0;
    int n_predicted_tokens = 0;
    std::vector<llama_token> tokens;
};

// Utility functions

template <typename T>
static T json_value(const json & body, const std::string & key, const T & default_value) {
    // Fallback null to default value
    if (body.contains(key) && !body.at(key).is_null()) {
        try {
            return body.at(key);
        } catch (NLOHMANN_JSON_NAMESPACE::detail::type_error const &) {
            return default_value;
        }
    } else {
        return default_value;
    }
}

static std::string gen_chatcmplid() {
    static const std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::random_device rd;
    std::mt19937 generator(rd());
    std::string result(32, ' ');
    for (int i = 0; i < 32; ++i) {
        result[i] = str[generator() % str.size()];
    }
    return "chatcmpl-" + result;
}

static std::string gen_tool_call_id() {
    static const std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");
    std::random_device rd;
    std::mt19937 generator(rd());
    std::string result(32, ' ');
    for (int i = 0; i < 32; ++i) {
        result[i] = str[generator() % str.size()];
    }
    return result;
}

// Validate that a string is valid UTF-8
static size_t validate_utf8(const std::string& text) {
    size_t len = text.size();
    if (len == 0) return 0;

    // Check the last few bytes to see if a multi-byte character is cut off
    for (size_t i = 1; i <= 4 && i <= len; ++i) {
        unsigned char c = text[len - i];
        // Check for start of a multi-byte sequence from the end
        if ((c & 0xE0) == 0xC0) {
            // 2-byte character start: 110xxxxx
            // Needs at least 2 bytes
            if (i < 2) return len - i;
        } else if ((c & 0xF0) == 0xE0) {
            // 3-byte character start: 1110xxxx
            // Needs at least 3 bytes
            if (i < 3) return len - i;
        } else if ((c & 0xF8) == 0xF0) {
            // 4-byte character start: 11110xxx
            // Needs at least 4 bytes
            if (i < 4) return len - i;
        }
    }

    // If no cut-off multi-byte character is found, return full length
    return len;
}

static bool is_valid_utf8(const std::string & str) {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(str.data());
    const unsigned char* end = bytes + str.length();

    while (bytes < end) {
        if (*bytes <= 0x7F) {
            // 1-byte sequence (0xxxxxxx)
            bytes++;
        } else if ((*bytes & 0xE0) == 0xC0) {
            // 2-byte sequence (110xxxxx 10xxxxxx)
            if (end - bytes < 2 || (bytes[1] & 0xC0) != 0x80)
                return false;
            bytes += 2;
        } else if ((*bytes & 0xF0) == 0xE0) {
            // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
            if (end - bytes < 3 || (bytes[1] & 0xC0) != 0x80 || (bytes[2] & 0xC0) != 0x80)
                return false;
            bytes += 3;
        } else if ((*bytes & 0xF8) == 0xF0) {
            // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            if (end - bytes < 4 || (bytes[1] & 0xC0) != 0x80 ||
                (bytes[2] & 0xC0) != 0x80 || (bytes[3] & 0xC0) != 0x80)
                return false;
            bytes += 4;
        } else {
            // Invalid UTF-8 lead byte
            return false;
        }
    }

    return true;
}

static bool ends_with(const std::string & str, const std::string & suffix) {
    return str.size() >= suffix.size() && 0 == str.compare(str.size() - suffix.size(), suffix.size(), suffix);
}

static size_t find_partial_stop_string(const std::string &stop, const std::string &text) {
    if (!text.empty() && !stop.empty()) {
        const char text_last_char = text.back();
        for (int64_t char_index = stop.size() - 1; char_index >= 0; char_index--) {
            if (stop[char_index] == text_last_char) {
                const std::string current_partial = stop.substr(0, char_index + 1);
                if (ends_with(text, current_partial)) {
                    return text.size() - char_index - 1;
                }
            }
        }
    }

    return std::string::npos;
}

static json format_logit_bias(const std::vector<llama_logit_bias> & logit_bias) {
    json data = json::array();
    for (const auto & lb : logit_bias) {
        data.push_back(json{
            {"bias", lb.bias},
            {"token", lb.token},
        });
    }
    return data;
}

static bool json_is_array_of_numbers(const json & data) {
    if (data.is_array()) {
        for (const auto & e : data) {
            if (!e.is_number_integer()) {
                return false;
            }
        }
        return true;
    }
    return false;
}

// is array having BOTH numbers & strings?
static bool json_is_array_of_mixed_numbers_strings(const json & data) {
    bool seen_string = false;
    bool seen_number = false;
    if (data.is_array()) {
        for (const auto & e : data) {
            seen_string |= e.is_string();
            seen_number |= e.is_number_integer();
            if (seen_number && seen_string) {
                return true;
            }
        }
    }
    return false;
}

/**
 * this handles 2 cases:
 * - only string, example: "string"
 * - mixed string and tokens, example: [12, 34, "string", 56, 78]
 */
static llama_tokens tokenize_mixed(const llama_vocab * vocab, const json & json_prompt, bool add_special, bool parse_special) {
    // If `add_bos` is true, we only add BOS, when json_prompt is a string,
    // or the first element of the json_prompt array is a string.
    llama_tokens prompt_tokens;

    if (json_prompt.is_array()) {
        bool first = true;
        for (const auto & p : json_prompt) {
            if (p.is_string()) {
                auto s = p.template get<std::string>();

                llama_tokens p;
                if (first) {
                    p = common_tokenize(vocab, s, add_special, parse_special);
                    first = false;
                } else {
                    p = common_tokenize(vocab, s, false, parse_special);
                }

                prompt_tokens.insert(prompt_tokens.end(), p.begin(), p.end());
            } else {
                if (first) {
                    first = false;
                }

                prompt_tokens.push_back(p.template get<llama_token>());
            }
        }
    } else {
        auto s = json_prompt.template get<std::string>();
        prompt_tokens = common_tokenize(vocab, s, add_special, parse_special);
    }

    return prompt_tokens;
}

/**
 * break the input "prompt" object into multiple prompt if needed, then tokenize them
 * this supports these cases:
 * - "prompt": "string"
 * - "prompt": [12, 34, 56]
 * - "prompt": [12, 34, "string", 56, 78]
 * and multiple prompts (multi-tasks):
 * - "prompt": ["string1", "string2"]
 * - "prompt": ["string1", [12, 34, 56]]
 * - "prompt": [[12, 34, 56], [78, 90, 12]]
 * - "prompt": [[12, 34, "string", 56, 78], [12, 34, 56]]
 */
static std::vector<llama_tokens> tokenize_input_prompts(const llama_vocab * vocab, const json & json_prompt, bool add_special, bool parse_special) {
    std::vector<llama_tokens> result;
    if (json_prompt.is_string() || json_is_array_of_mixed_numbers_strings(json_prompt)) {
        // string or mixed
        result.push_back(tokenize_mixed(vocab, json_prompt, add_special, parse_special));
    } else if (json_is_array_of_numbers(json_prompt)) {
        // array of tokens
        result.push_back(json_prompt.get<llama_tokens>());
    } else if (json_prompt.is_array()) {
        // array of prompts
        result.reserve(json_prompt.size());
        for (const auto & p : json_prompt) {
            if (p.is_string() || json_is_array_of_mixed_numbers_strings(p)) {
                result.push_back(tokenize_mixed(vocab, p, add_special, parse_special));
            } else if (json_is_array_of_numbers(p)) {
                // array of tokens
                result.push_back(p.get<llama_tokens>());
            } else {
                throw std::runtime_error("element of \"prompt\" must be a string, an list of tokens, or a list of mixed strings & tokens");
            }
        }
    } else {
        throw std::runtime_error("\"prompt\" must be a string, an list of tokens, a list of mixed strings & tokens, or a list of prompts");
    }
    if (result.empty()) {
        throw std::runtime_error("\"prompt\" must not be empty");
    }
    return result;
}

static json format_error_response(const std::string & message, rn_error_type type) {
    std::string type_str;
    switch (type) {
        case RN_ERROR_INVALID_PARAM:
            type_str = "invalid_parameter";
            break;
        case RN_ERROR_MODEL_LOAD:
            type_str = "model_load_error";
            break;
        case RN_ERROR_CONTEXT:
            type_str = "context_error";
            break;
        case RN_ERROR_INFERENCE:
            type_str = "inference_error";
            break;
        default:
            type_str = "general_error";
            break;
    }
    return json {
        {"error", type_str},
        {"message", message}
    };
}

static json oaicompat_completion_params_parse(const json & body) {
    json llama_params;

    if (!body.contains("prompt")) {
        throw std::runtime_error("\"prompt\" is required");
    }

    // Handle "stop" field
    if (body.contains("stop") && body.at("stop").is_string()) {
        llama_params["stop"] = json::array({body.at("stop").get<std::string>()});
    } else {
        llama_params["stop"] = json_value(body, "stop", json::array());
    }

    // Handle "n" field
    int n_choices = json_value(body, "n", 1);
    if (n_choices != 1) {
        throw std::runtime_error("Only one completion choice is allowed");
    }

    // Handle "echo" field
    if (json_value(body, "echo", false)) {
        throw std::runtime_error("Only no echo is supported");
    }

    // Params supported by OAI but unsupported by llama.cpp
    static const std::vector<std::string> unsupported_params { "best_of", "suffix" };
    for (const auto & param : unsupported_params) {
        if (body.contains(param)) {
            throw std::runtime_error("Unsupported param: " + param);
        }
    }

    // Copy remaining properties to llama_params
    for (const auto & item : body.items()) {
        // Exception: if "n_predict" is present, we overwrite the value specified earlier by "max_tokens"
        if (!llama_params.contains(item.key()) || item.key() == "n_predict") {
            llama_params[item.key()] = item.value();
        }
    }

    return llama_params;
}

static json oaicompat_completion_params_parse(
    const json & body, /* openai api json semantics */
    bool use_jinja,
    common_reasoning_format reasoning_format,
    const struct common_chat_templates * tmpls)
{
    json llama_params;

    auto tools = json_value(body, "tools", json());
    auto stream = json_value(body, "stream", false);

    if (tools.is_array() && !tools.empty()) {
        if (stream) {
            throw std::runtime_error("Cannot use tools with stream");
        }
        if (!use_jinja) {
            throw std::runtime_error("tools param requires --jinja flag");
        }
    }
    if (!use_jinja) {
        if (body.contains("tool_choice") && !body.at("tool_choice").is_null()) {
            throw std::runtime_error("Unsupported param: tool_choice");
        }
    }

    // Handle "stop" field
    if (body.contains("stop") && body.at("stop").is_string()) {
        llama_params["stop"] = json::array({body.at("stop").get<std::string>()});
    } else {
        llama_params["stop"] = json_value(body, "stop", json::array());
    }

    auto json_schema = json_value(body, "json_schema", json());
    auto grammar = json_value(body, "grammar", std::string());
    if (!json_schema.is_null() && !grammar.empty()) {
        throw std::runtime_error("Cannot use both json_schema and grammar");
    }

    // Handle "response_format" field
    if (body.contains("response_format")) {
        json response_format      = json_value(body, "response_format", json::object());
        std::string response_type = json_value(response_format, "type", std::string());
        if (response_type == "json_object") {
            json_schema = json_value(response_format, "schema", json::object());
        } else if (response_type == "json_schema") {
            auto schema_wrapper = json_value(response_format, "json_schema", json::object());
            json_schema = json_value(schema_wrapper, "schema", json::object());
        } else if (!response_type.empty() && response_type != "text") {
            throw std::runtime_error("response_format type must be one of \"text\" or \"json_object\", but got: " + response_type);
        }
    }

    common_chat_templates_inputs inputs;
    inputs.messages              = common_chat_msgs_parse_oaicompat(body.at("messages"));
    inputs.tools                 = common_chat_tools_parse_oaicompat(tools);
    inputs.tool_choice           = common_chat_tool_choice_parse_oaicompat(json_value(body, "tool_choice", std::string("auto")));
    inputs.json_schema           = json_schema.is_null() ? "" : json_schema.dump();
    inputs.grammar               = grammar;
    inputs.add_generation_prompt = json_value(body, "add_generation_prompt", true);
    inputs.use_jinja             = use_jinja;
    inputs.parallel_tool_calls   = json_value(body, "parallel_tool_calls", false);
    inputs.extract_reasoning     = reasoning_format != COMMON_REASONING_FORMAT_NONE;
    inputs.add_generation_prompt = json_value(body, "add_generation_prompt", true);
    if (!inputs.tools.empty() && inputs.tool_choice != COMMON_CHAT_TOOL_CHOICE_NONE && body.contains("grammar")) {
        throw std::runtime_error("Cannot use custom grammar constraints with tools.");
    }

    // if the assistant message appears at the end of list, we do not add end-of-turn token
    // for ex. this can be useful to modify the reasoning process in reasoning models
    bool prefill_assistant_message = !inputs.messages.empty() && inputs.messages.back().role == "assistant";
    common_chat_msg last_message;
    if (prefill_assistant_message) {
        last_message = inputs.messages.back();
        inputs.messages.pop_back();

        /* sanity check, max one assistant message at the end of the list */
        if (!inputs.messages.empty() && inputs.messages.back().role == "assistant"){
            throw std::runtime_error("Cannot have 2 or more assistant messages at the end of the list.");
        }

        inputs.extract_reasoning = false;
        inputs.add_generation_prompt = true;
    }

    // Apply chat template to the list of messages
    auto chat_params = common_chat_templates_apply(tmpls, inputs);

    /* Append assistant prefilled message */
    if (prefill_assistant_message) {
         chat_params.prompt += last_message.content;
    }

    llama_params["chat_format"]      = static_cast<int>(chat_params.format);
    llama_params["prompt"]           = chat_params.prompt;
    if (!chat_params.grammar.empty()) {
        llama_params["grammar"] = chat_params.grammar;
    }
    llama_params["grammar_lazy"]     = chat_params.grammar_lazy;
    auto grammar_triggers = json::array();
    for (const auto & trigger : chat_params.grammar_triggers) {
        grammar_triggers.push_back(json{
            {"type", (int)trigger.type},
            {"value", trigger.value},
            {"token", (trigger.type == COMMON_GRAMMAR_TRIGGER_TYPE_TOKEN) ? (int)trigger.token : -1}
        });
    }
    llama_params["grammar_triggers"] = grammar_triggers;
    llama_params["preserved_tokens"] = chat_params.preserved_tokens;
    for (const auto & stop : chat_params.additional_stops) {
        llama_params["stop"].push_back(stop);
    }

    // Handle "n" field
    int n_choices = json_value(body, "n", 1);
    if (n_choices != 1) {
        throw std::runtime_error("Only one completion choice is allowed");
    }

    // Handle "logprobs" field
    if (json_value(body, "logprobs", false)) {
        llama_params["n_probs"] = json_value(body, "top_logprobs", 20);
    } else if (body.contains("top_logprobs") && !body.at("top_logprobs").is_null()) {
        throw std::runtime_error("top_logprobs requires logprobs to be set to true");
    }

    // Copy remaining properties to llama_params
    for (const auto & item : body.items()) {
        // Exception: if "n_predict" is present, we overwrite the value specified earlier by "max_tokens"
        if (!llama_params.contains(item.key()) || item.key() == "n_predict") {
            llama_params[item.key()] = item.value();
        }
    }

    return llama_params;
}

// Function to safely convert JSON to string
static std::string safe_json_to_str(const json & data) {
    return data.dump(-1, ' ', false, json::error_handler_t::replace);
}
