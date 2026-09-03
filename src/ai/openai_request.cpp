#include "ai/openai_request.hpp"

#include <string>
#include <utility>
#include <vector>

namespace pi::ai::detail {

namespace {

template <class... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

std::string textOnly(const std::vector<ContentBlock>& content) {
    std::string text;
    for (const auto& block : content) {
        if (const auto* value = std::get_if<TextContent>(&block)) {
            text += value->text;
        }
    }
    return text;
}

bool hasNonWhitespace(std::string_view value) {
    return value.find_first_not_of(" \t\n\r") != std::string_view::npos;
}

std::vector<const ThinkingContent*> nonEmptyThinkingBlocks(
    const std::vector<ContentBlock>& content) {
    std::vector<const ThinkingContent*> result;
    for (const auto& block : content) {
        if (const auto* thinking = std::get_if<ThinkingContent>(&block);
            thinking && hasNonWhitespace(thinking->thinking)) {
            result.push_back(thinking);
        }
    }
    return result;
}

std::string joinedThinking(const std::vector<const ThinkingContent*>& blocks) {
    std::string result;
    for (std::size_t index = 0; index < blocks.size(); ++index) {
        if (index != 0) result += '\n';
        result += blocks[index]->thinking;
    }
    return result;
}

bool jsonTruthy(const nlohmann::json& value) {
    if (value.is_null()) return false;
    if (value.is_boolean()) return value.get<bool>();
    if (value.is_number()) return value != 0;
    if (value.is_string()) return !value.get_ref<const std::string&>().empty();
    return true;
}

nlohmann::json userContent(const UserContent& content) {
    if (const auto* text = std::get_if<std::string>(&content)) {
        return *text;
    }

    nlohmann::json parts = nlohmann::json::array();
    for (const auto& block : std::get<std::vector<ContentBlock>>(content)) {
        if (const auto* text = std::get_if<TextContent>(&block)) {
            parts.push_back({{"type", "text"}, {"text", text->text}});
        } else if (const auto* image = std::get_if<ImageContent>(&block)) {
            parts.push_back({
                {"type", "image_url"},
                {"image_url", {
                    {"url", "data:" + image->mimeType + ";base64," + image->data},
                }},
            });
        }
    }
    return parts;
}

nlohmann::json assistantMessage(const AssistantMessage& message) {
    nlohmann::json result;
    result["role"] = "assistant";

    const auto text = textOnly(message.content);
    const auto thinkingBlocks = nonEmptyThinkingBlocks(message.content);
    if (!thinkingBlocks.empty() && thinkingBlocks.front()->thinkingSignature &&
        !thinkingBlocks.front()->thinkingSignature->empty()) {
        auto signature = *thinkingBlocks.front()->thinkingSignature;
        if (message.provider == "opencode-go" && signature == "reasoning") {
            signature = "reasoning_content";
        }
        result[signature] = joinedThinking(thinkingBlocks);
    }

    nlohmann::json toolCalls = nlohmann::json::array();
    nlohmann::json reasoningDetails = nlohmann::json::array();
    for (const auto& block : message.content) {
        if (const auto* call = std::get_if<ToolCall>(&block)) {
            toolCalls.push_back({
                {"id", call->id},
                {"type", "function"},
                {"function", {
                    {"name", call->name},
                    {"arguments", call->arguments.dump()},
                }},
            });

            if (call->thoughtSignature && !call->thoughtSignature->empty()) {
                const auto detail = nlohmann::json::parse(
                    call->thoughtSignature->begin(),
                    call->thoughtSignature->end(),
                    nullptr,
                    false);
                if (!detail.is_discarded() && jsonTruthy(detail)) {
                    reasoningDetails.push_back(detail);
                }
            }
        }
    }

    if (text.empty() && !toolCalls.empty()) {
        result["content"] = nullptr;
    } else {
        result["content"] = text;
    }
    if (!toolCalls.empty()) {
        result["tool_calls"] = std::move(toolCalls);
    }
    if (!reasoningDetails.empty()) {
        result["reasoning_details"] = std::move(reasoningDetails);
    }
    return result;
}

nlohmann::json toOpenAiMessage(const Message& message) {
    return std::visit(
        Overload{
            [](const UserMessage& user) {
                return nlohmann::json{
                    {"role", "user"},
                    {"content", userContent(user.content)},
                };
            },
            [](const AssistantMessage& assistant) {
                return assistantMessage(assistant);
            },
            [](const ToolResultMessage& toolResult) {
                nlohmann::json result{
                    {"role", "tool"},
                    {"tool_call_id", toolResult.toolCallId},
                    {"content", textOnly(toolResult.content)},
                };
                if (!toolResult.toolName.empty()) {
                    result["name"] = toolResult.toolName;
                }
                return result;
            },
        },
        message);
}

} // namespace

nlohmann::json buildOpenAiChatCompletionsRequest(
    const Model& model,
    const Context& context,
    const StreamOptions& options) {
    nlohmann::json request;
    request["model"] = model.id;
    request["stream"] = true;
    request["stream_options"] = {{"include_usage", true}};

    nlohmann::json messages = nlohmann::json::array();
    if (context.systemPrompt && !context.systemPrompt->empty()) {
        messages.push_back({
            {"role", "system"},
            {"content", *context.systemPrompt},
        });
    }
    for (const auto& message : context.messages) {
        messages.push_back(toOpenAiMessage(message));
    }
    request["messages"] = std::move(messages);

    if (!context.tools.empty()) {
        nlohmann::json tools = nlohmann::json::array();
        for (const auto& tool : context.tools) {
            tools.push_back({
                {"type", "function"},
                {"function", {
                    {"name", tool.name},
                    {"description", tool.description},
                    {"parameters", tool.parameters},
                }},
            });
        }
        request["tools"] = std::move(tools);
    }

    if (options.temperature) {
        request["temperature"] = *options.temperature;
    }
    if (options.maxTokens) {
        request["max_tokens"] = *options.maxTokens;
    }

    return request;
}

} // namespace pi::ai::detail
