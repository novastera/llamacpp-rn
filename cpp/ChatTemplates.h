#pragma once

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include "llama.h"

/**
 * @brief Function structure for tool calls
 */
struct Function {
    std::string name;
    std::string arguments;
    
    // Add default constructor and copy constructor to ensure proper handling
    Function() : name(), arguments() {}
    
    Function(const Function& other) : name(other.name), arguments(other.arguments) {}
    
    // Add assignment operator for explicit control
    Function& operator=(const Function& other) {
        if (this != &other) {
            name = other.name;
            arguments = other.arguments;
        }
        return *this;
    }
};

/**
 * @brief Simple structure representing a chat message
 */
struct ChatMessage {
    std::string role;
    std::string content;
    std::string name;
    std::string tool_call_id;
    
    // Tool call fields
    std::string id;
    std::string type;
    Function function;
    
    // Add default constructor for proper initialization
    ChatMessage() : role(), content(), name(), tool_call_id(), id(), type(), function() {}
    
    // Add explicit copy constructor
    ChatMessage(const ChatMessage& other) 
        : role(other.role), 
          content(other.content), 
          name(other.name), 
          tool_call_id(other.tool_call_id),
          id(other.id),
          type(other.type),
          function(other.function) {}
    
    // Add assignment operator
    ChatMessage& operator=(const ChatMessage& other) {
        if (this != &other) {
            role = other.role;
            content = other.content;
            name = other.name;
            tool_call_id = other.tool_call_id;
            id = other.id;
            type = other.type;
            function = other.function;
        }
        return *this;
    }
};

/**
 * A simple JSON class to replace the need for nlohmann/json
 */
class SimpleJSON {
private:
    std::map<std::string, std::string> values;
    
public:
    SimpleJSON() {}
    
    void set(const std::string& key, const std::string& value) {
        values[key] = value;
    }
    
    bool contains(const std::string& key) const {
        return values.find(key) != values.end();
    }
    
    std::string dump() const {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (const auto& kv : values) {
            if (!first) oss << ",";
            oss << "\"" << kv.first << "\":\"" << kv.second << "\"";
            first = false;
        }
        oss << "}";
        return oss.str();
    }
};

/**
 * ChatTemplates - Handles chat template formatting for different LLM models
 * This is a thin wrapper around llama.cpp's chat template functionality, which
 * uses minja (a lightweight Jinja2 implementation) under the hood.
 */
namespace chat_templates {

/**
 * @brief Apply a chat template to a list of messages
 * 
 * @param model The llama model
 * @param messages Vector of chat messages
 * @param template_name Name of the template to use (e.g., "llama-2", "chatgpt")
 * @return std::string Formatted prompt ready for inference
 */
std::string apply_chat_template(
    llama_model* model, 
    const std::vector<ChatMessage>& messages,
    const std::string& template_name);

/**
 * @brief Parse a JSON array of messages into a vector of ChatMessage objects
 * 
 * @param messages_json The JSON array of messages
 * @return std::vector<ChatMessage> Vector of chat message objects
 */
std::vector<ChatMessage> messages_from_json(const std::string& messages_json);

/**
 * @brief Convert a vector of ChatMessages into a formatted JSON string
 * 
 * @param messages The vector of chat messages to convert
 * @return std::string Formatted JSON string
 */
std::vector<ChatMessage> messages_from_json(const std::vector<ChatMessage>& messages);

/**
 * @brief Parse a completion text to extract a tool call if present
 * 
 * @param completion_text The completion text to parse
 * @return ChatMessage A ChatMessage containing the tool call information
 */
ChatMessage parse_tool_call(const std::string& completion_text);

} // namespace chat_templates 