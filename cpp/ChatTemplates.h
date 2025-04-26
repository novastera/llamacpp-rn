#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include "llama.h"

/**
 * ChatTemplates - Handles chat template formatting for different LLM models
 * This is a thin wrapper around llama.cpp's chat template functionality, which
 * uses minja (a lightweight Jinja2 implementation) under the hood.
 */
namespace chat_templates {

/**
 * Message structure matching the format expected by llama.cpp
 */
struct ChatMessage {
    std::string role;
    std::string content;
    std::string name;
    std::string tool_call_id;
};

/**
 * Format a list of messages using a chat template
 * 
 * @param model The llama model to use for template application
 * @param messages A vector of ChatMessage objects
 * @param template_name The name of the template to use (e.g., "chatml", "llama-2", etc.)
 * @return The formatted prompt string
 */
std::string apply_chat_template(
    llama_model* model,
    const std::vector<ChatMessage>& messages,
    const std::string& template_name = "chatml");

/**
 * Convert JSON messages array to ChatMessage vector
 * 
 * @param messages_json JSON array of message objects
 * @return Vector of ChatMessage objects
 */
std::vector<ChatMessage> messages_from_json(const nlohmann::json& messages_json);

/**
 * Parse function/tool call from completion text
 * 
 * @param completion_text The text to parse for function calls
 * @return JSON object with tool call information, or null if no tool call found
 */
nlohmann::json parse_tool_call(const std::string& completion_text);

} // namespace chat_templates 