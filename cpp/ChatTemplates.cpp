#include "ChatTemplates.h"
#include <algorithm>
#include <regex>

namespace chat_templates {

std::string apply_chat_template(
    llama_model* model, 
    const std::vector<ChatMessage>& messages,
    const std::string& template_name) {
    
    // Convert our ChatMessage vector to llama_chat_message array
    std::vector<llama_chat_message> llama_messages;
    llama_messages.reserve(messages.size());
    
    // Keep track of string data to prevent premature deallocation
    std::vector<std::string> role_strings;
    std::vector<std::string> content_strings;
    std::vector<std::string> name_strings;
    
    for (const auto& msg : messages) {
        role_strings.push_back(msg.role);
        content_strings.push_back(msg.content);
        
        llama_chat_message chat_message;
        chat_message.role = role_strings.back().c_str();
        chat_message.content = content_strings.back().c_str();
        
        // Add name if present
        if (!msg.name.empty()) {
            name_strings.push_back(msg.name);
            chat_message.name = name_strings.back().c_str();
        } else {
            chat_message.name = nullptr;
        }
        
        llama_messages.push_back(chat_message);
    }
    
    // Use llama.cpp's built-in chat template feature
    char* templated_prompt = llama_chat_apply_template(model, 
                                                      llama_messages.data(), 
                                                      llama_messages.size(), 
                                                      template_name.c_str(), 
                                                      nullptr);
    
    if (templated_prompt == nullptr) {
        throw std::runtime_error("Failed to apply chat template: " + template_name);
    }
    
    // Copy the prompt and free the original
    std::string result = templated_prompt;
    llama_chat_template_free(templated_prompt);
    
    return result;
}

std::vector<ChatMessage> messages_from_json(const nlohmann::json& messages_json) {
    if (!messages_json.is_array()) {
        throw std::runtime_error("Messages must be an array");
    }
    
    std::vector<ChatMessage> messages;
    messages.reserve(messages_json.size());
    
    for (const auto& msg_json : messages_json) {
        if (!msg_json.is_object()) {
            continue;
        }
        
        if (!msg_json.contains("role") || !msg_json["role"].is_string() ||
            !msg_json.contains("content") || 
            (!msg_json["content"].is_string() && !msg_json["content"].is_null())) {
            continue;
        }
        
        ChatMessage message;
        message.role = msg_json["role"].get<std::string>();
        
        // Content can be null for tool calls
        if (msg_json["content"].is_string()) {
            message.content = msg_json["content"].get<std::string>();
        } else {
            message.content = "";
        }
        
        // Optional fields
        if (msg_json.contains("name") && msg_json["name"].is_string()) {
            message.name = msg_json["name"].get<std::string>();
        }
        
        if (msg_json.contains("tool_call_id") && msg_json["tool_call_id"].is_string()) {
            message.tool_call_id = msg_json["tool_call_id"].get<std::string>();
        }
        
        messages.push_back(message);
    }
    
    return messages;
}

nlohmann::json parse_tool_call(const std::string& completion_text) {
    // Check if the completion looks like it contains a tool call
    if (completion_text.find("\"function\"") == std::string::npos || 
        completion_text.find("\"name\"") == std::string::npos ||
        completion_text.find("\"arguments\"") == std::string::npos) {
        return nlohmann::json(nullptr);
    }
    
    // Extract function name - simple approach
    std::string function_name = "unknown_function";
    std::regex name_regex("\"name\"\\s*:\\s*\"([^\"]*)\"");
    std::smatch name_match;
    if (std::regex_search(completion_text, name_match, name_regex) && name_match.size() > 1) {
        function_name = name_match[1].str();
    }
    
    // Extract arguments - simple approach
    std::string function_args = "{}";
    size_t args_pos = completion_text.find("\"arguments\"");
    if (args_pos != std::string::npos) {
        args_pos = completion_text.find(":", args_pos + 12);
        if (args_pos != std::string::npos) {
            size_t json_start = completion_text.find("{", args_pos);
            if (json_start != std::string::npos) {
                int brace_count = 1;
                size_t pos = json_start + 1;
                while (pos < completion_text.length() && brace_count > 0) {
                    if (completion_text[pos] == '{') brace_count++;
                    else if (completion_text[pos] == '}') brace_count--;
                    pos++;
                }
                if (brace_count == 0) {
                    function_args = completion_text.substr(json_start, pos - json_start);
                }
            }
        }
    }
    
    // Generate a random ID for the tool call
    std::string id = "call_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Create tool call JSON object
    nlohmann::json tool_call = {
        {"id", id},
        {"type", "function"},
        {"function", {
            {"name", function_name},
            {"arguments", function_args}
        }}
    };
    
    return tool_call;
}

} // namespace chat_templates 