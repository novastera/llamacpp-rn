#include "ChatTemplates.h"
#include <algorithm>
#include <regex>
#include <chrono>
#include <sstream>

namespace chat_templates {

std::string apply_chat_template(
    llama_model* model, 
    const std::vector<ChatMessage>& messages,
    const std::string& template_name) {
    
    // Simple implementation for known templates
    if (template_name == "llama-2") {
        // Format messages according to Llama 2 style
        std::ostringstream prompt;
        
        // Add system prompt if present
        for (const auto& msg : messages) {
            if (msg.role == "system") {
                prompt << "<s>[INST] <<SYS>>\n" << msg.content << "\n<</SYS>>\n\n";
                break;
            }
        }
        
        // Add conversation messages
        bool started = false;
        bool is_first_user_message = true;
        for (const auto& msg : messages) {
            if (msg.role == "system") continue; // Already handled
            
            if (msg.role == "user") {
                if (!started) {
                    if (!is_first_user_message) prompt << "<s>";
                    prompt << "[INST] ";
                    started = true;
                } else {
                    prompt << "[INST] ";
                }
                prompt << msg.content << " [/INST]\n";
                is_first_user_message = false;
            } else if (msg.role == "assistant") {
                prompt << msg.content << " </s>\n";
                started = false;
            }
        }
        
        return prompt.str();
    } 
    else if (template_name == "mistral") {
        // Format messages according to Mistral style
        std::ostringstream prompt;
        
        // Add system prompt if present
        for (const auto& msg : messages) {
            if (msg.role == "system") {
                prompt << "<s>[INST] " << msg.content << " [/INST]\n";
                break;
            }
        }
        
        // Add conversation messages
        for (size_t i = 0; i < messages.size(); i++) {
            const auto& msg = messages[i];
            if (msg.role == "system") continue; // Already handled
            
            if (msg.role == "user") {
                prompt << "<s>[INST] " << msg.content << " [/INST]\n";
            } else if (msg.role == "assistant") {
                prompt << msg.content << "</s>\n";
            }
        }
        
        return prompt.str();
    }
    else if (template_name == "chat-ml") {
        // ChatML format
        std::ostringstream prompt;
        prompt << "<|im_start|>system\n";
        
        // Add system message if present
        bool has_system = false;
        for (const auto& msg : messages) {
            if (msg.role == "system") {
                prompt << msg.content << "\n<|im_end|>\n";
                has_system = true;
                break;
            }
        }
        
        if (!has_system) {
            prompt << "You are a helpful assistant.\n<|im_end|>\n";
        }
        
        // Add conversation messages
        for (const auto& msg : messages) {
            if (msg.role == "system") continue; // Already handled
            
            if (msg.role == "user") {
                prompt << "<|im_start|>user\n" << msg.content << "\n<|im_end|>\n";
            } else if (msg.role == "assistant") {
                prompt << "<|im_start|>assistant\n" << msg.content << "\n<|im_end|>\n";
            }
        }
        
        // Add final assistant prompt
        prompt << "<|im_start|>assistant\n";
        return prompt.str();
    }
    else {
        // Simple default format for any other model
        std::ostringstream prompt;
        for (const auto& msg : messages) {
            if (msg.role == "system") {
                prompt << "System: " << msg.content << "\n\n";
            } else if (msg.role == "user") {
                prompt << "User: " << msg.content << "\n\n";
            } else if (msg.role == "assistant") {
                prompt << "Assistant: " << msg.content << "\n\n";
            }
        }
        prompt << "Assistant: ";
        return prompt.str();
    }
}

// Simple JSON parser for message arrays
std::vector<ChatMessage> messages_from_json(const std::string& messages_json) {
    std::vector<ChatMessage> messages;
    
    // Very basic JSON array parser - for production use you'd want a more robust solution
    if (messages_json.empty() || messages_json[0] != '[') {
        return messages;
    }
    
    size_t pos = 1;
    while (pos < messages_json.length()) {
        // Find start of object
        pos = messages_json.find('{', pos);
        if (pos == std::string::npos) break;
        
        // Find end of object
        int brace_count = 1;
        size_t start = pos;
        pos++;
        
        while (pos < messages_json.length() && brace_count > 0) {
            if (messages_json[pos] == '{') brace_count++;
            else if (messages_json[pos] == '}') brace_count--;
            pos++;
        }
        
        if (brace_count != 0) break;
        
        // Extract object
        std::string obj = messages_json.substr(start, pos - start);
        
        // Parse message fields
        ChatMessage message;
        
        // Extract role
        std::regex role_regex("\"role\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch role_match;
        if (std::regex_search(obj, role_match, role_regex) && role_match.size() > 1) {
            message.role = role_match[1].str();
        } else {
            continue; // role is required
        }
        
        // Extract content
        std::regex content_regex("\"content\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch content_match;
        if (std::regex_search(obj, content_match, content_regex) && content_match.size() > 1) {
            message.content = content_match[1].str();
        }
        
        // Extract name (optional)
        std::regex name_regex("\"name\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch name_match;
        if (std::regex_search(obj, name_match, name_regex) && name_match.size() > 1) {
            message.name = name_match[1].str();
        }
        
        // Extract tool_call_id (optional)
        std::regex tool_call_id_regex("\"tool_call_id\"\\s*:\\s*\"([^\"]*)\"");
        std::smatch tool_call_id_match;
        if (std::regex_search(obj, tool_call_id_match, tool_call_id_regex) && tool_call_id_match.size() > 1) {
            message.tool_call_id = tool_call_id_match[1].str();
        }
        
        messages.push_back(message);
    }
    
    return messages;
}

// Vector input version of messages_from_json
std::vector<ChatMessage> messages_from_json(const std::vector<ChatMessage>& messages) {
    // This overload just returns the input since it's already in the right format
    return messages;
}

ChatMessage parse_tool_call(const std::string& completion_text) {
    ChatMessage tool_call;
    
    // Skip early if this doesn't look like a tool call
    if (completion_text.find("\"function\"") == std::string::npos || 
        completion_text.find("\"name\"") == std::string::npos) {
        return tool_call;
    }
    
    // More robust approach for extracting function name, using better regex pattern
    std::string function_name;
    std::regex name_pattern("\"name\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch name_match;
    if (std::regex_search(completion_text, name_match, name_pattern) && name_match.size() > 1) {
        function_name = name_match[1].str();
    } else {
        // No valid function name found
        return tool_call;
    }
    
    // Better extraction of function arguments using regex with balanced braces
    std::string arguments = "{}";
    size_t args_pos = completion_text.find("\"arguments\"");
    if (args_pos != std::string::npos) {
        // Find the opening brace after "arguments":
        size_t open_brace = completion_text.find('{', args_pos);
        if (open_brace != std::string::npos) {
            int brace_count = 1;
            size_t pos = open_brace + 1;
            
            // Keep track of whether we're inside a string to handle braces in strings
            bool in_string = false;
            bool escaped = false;
            
            while (pos < completion_text.length() && brace_count > 0) {
                char c = completion_text[pos];
                
                if (in_string) {
                    if (c == '\\' && !escaped) {
                        escaped = true;
                    } else if (c == '"' && !escaped) {
                        in_string = false;
                    } else {
                        escaped = false;
                    }
                } else {
                    if (c == '"') {
                        in_string = true;
                    } else if (c == '{') {
                        brace_count++;
                    } else if (c == '}') {
                        brace_count--;
                    }
                }
                
                pos++;
                if (brace_count == 0) {
                    arguments = completion_text.substr(open_brace, pos - open_brace);
                    break;
                }
            }
        }
    }
    
    // Generate a deterministic ID based on function name and timestamp
    std::string id = "call_" + function_name + "_" + 
                     std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    
    // Set fields directly to avoid constructor issues
    tool_call.id = id;
    tool_call.type = "function";
    tool_call.function.name = function_name;
    tool_call.function.arguments = arguments;
    
    return tool_call;
}

} // namespace chat_templates 