#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/ai/openai_compatible.hpp>
#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace ai = pi::ai;

namespace {

template <class... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

nlohmann::json normalizeThoughtSignature(const std::string& value) {
    auto parsed = nlohmann::json::parse(value, nullptr, false);
    if (!parsed.is_discarded()) return parsed;
    return value;
}

nlohmann::json normalizeContent(const ai::ContentBlock& block) {
    return std::visit(
        Overload{
            [](const ai::TextContent& text) {
                nlohmann::json result{{"type", "text"}, {"text", text.text}};
                if (text.textSignature) result["textSignature"] = *text.textSignature;
                return result;
            },
            [](const ai::ThinkingContent& thinking) {
                nlohmann::json result{
                    {"type", "thinking"},
                    {"thinking", thinking.thinking},
                };
                if (thinking.thinkingSignature) {
                    result["thinkingSignature"] = *thinking.thinkingSignature;
                }
                if (thinking.redacted) result["redacted"] = true;
                return result;
            },
            [](const ai::ImageContent& image) {
                return nlohmann::json{
                    {"type", "image"},
                    {"data", image.data},
                    {"mimeType", image.mimeType},
                };
            },
            [](const ai::ToolCall& call) {
                nlohmann::json result{
                    {"type", "toolCall"},
                    {"id", call.id},
                    {"name", call.name},
                    {"arguments", call.arguments},
                };
                if (call.thoughtSignature) {
                    result["thoughtSignature"] = normalizeThoughtSignature(*call.thoughtSignature);
                }
                return result;
            },
        },
        block);
}

nlohmann::json normalizeUsage(const ai::Usage& usage) {
    return {
        {"input", usage.input},
        {"output", usage.output},
        {"cacheRead", usage.cacheRead},
        {"cacheWrite", usage.cacheWrite},
        {"totalTokens", usage.totalTokens},
    };
}

nlohmann::json normalizeMessage(const ai::AssistantMessage& message) {
    nlohmann::json content = nlohmann::json::array();
    for (const auto& block : message.content) {
        content.push_back(normalizeContent(block));
    }

    nlohmann::json reason = message.stopReason;
    nlohmann::json result{
        {"api", message.api},
        {"provider", message.provider},
        {"model", message.model},
        {"content", std::move(content)},
        {"usage", normalizeUsage(message.usage)},
        {"stopReason", std::move(reason)},
    };
    if (message.responseId) result["responseId"] = *message.responseId;
    if (message.responseModel) result["responseModel"] = *message.responseModel;
    if (message.errorMessage) result["errorMessage"] = *message.errorMessage;
    return result;
}

nlohmann::json eventBase(const char* type) {
    return {{"kind", "event"}, {"type", type}};
}

nlohmann::json normalizeEvent(const ai::AssistantMessageEvent& event) {
    return std::visit(
        Overload{
            [](const ai::EvStart& value) {
                auto result = eventBase("start");
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvTextStart& value) {
                auto result = eventBase("text_start");
                result["contentIndex"] = value.contentIndex;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvTextDelta& value) {
                auto result = eventBase("text_delta");
                result["contentIndex"] = value.contentIndex;
                result["delta"] = value.delta;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvTextEnd& value) {
                auto result = eventBase("text_end");
                result["contentIndex"] = value.contentIndex;
                result["content"] = value.content;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvThinkingStart& value) {
                auto result = eventBase("thinking_start");
                result["contentIndex"] = value.contentIndex;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvThinkingDelta& value) {
                auto result = eventBase("thinking_delta");
                result["contentIndex"] = value.contentIndex;
                result["delta"] = value.delta;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvThinkingEnd& value) {
                auto result = eventBase("thinking_end");
                result["contentIndex"] = value.contentIndex;
                result["content"] = value.content;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvToolCallStart& value) {
                auto result = eventBase("toolcall_start");
                result["contentIndex"] = value.contentIndex;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvToolCallDelta& value) {
                auto result = eventBase("toolcall_delta");
                result["contentIndex"] = value.contentIndex;
                result["delta"] = value.delta;
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvToolCallEnd& value) {
                auto result = eventBase("toolcall_end");
                result["contentIndex"] = value.contentIndex;
                result["toolCall"] = normalizeContent(ai::ContentBlock{value.toolCall});
                result["partial"] = normalizeMessage(value.partial);
                return result;
            },
            [](const ai::EvDone& value) {
                auto result = eventBase("done");
                result["reason"] = nlohmann::json(value.reason);
                result["message"] = normalizeMessage(value.message);
                return result;
            },
            [](const ai::EvError& value) {
                auto result = eventBase("error");
                result["reason"] = nlohmann::json(value.reason);
                result["error"] = normalizeMessage(value.error);
                return result;
            },
        },
        event);
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: pi_reference_trace <base-url>\n";
        return 2;
    }

    ai::Model model;
    model.id = "gpt-ref";
    model.name = "GPT Reference";
    model.api = "openai-completions";
    model.provider = "openai";
    model.baseUrl = argv[1];
    model.contextWindow = 128000;
    model.maxTokens = 4096;

    ai::Context context;
    context.systemPrompt = "You are concise.";
    ai::UserMessage user;
    user.content = std::string("hello");
    user.timestamp = 1;
    context.messages.emplace_back(std::move(user));
    context.tools.push_back(ai::ToolDefinition{
        "read",
        "Read a file",
        nlohmann::json{
            {"type", "object"},
            {"properties", {{"path", {{"type", "string"}}}}},
            {"required", nlohmann::json::array({"path"})},
        },
    });

    ai::StreamOptions options;
    options.apiKey = "test-key";
    options.maxRetries = 0;

    ai::OpenAICompatibleProvider provider;
    auto stream = provider.stream(model, context, options);
    while (auto event = stream.next()) {
        std::cout << normalizeEvent(*event).dump() << '\n';
    }
    return 0;
}
