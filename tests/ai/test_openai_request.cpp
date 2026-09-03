#include <doctest/doctest.h>

#include "ai/openai_request.hpp"

#include <pi/ai/message.hpp>
#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace ai = pi::ai;
namespace detail = pi::ai::detail;

namespace {

ai::Model makeModel() {
    ai::Model model;
    model.id = "gpt-test";
    model.api = "openai-completions";
    model.provider = "openai";
    model.baseUrl = "https://example.invalid/v1";
    return model;
}

} // namespace

TEST_CASE("OpenAI request builder maps core conversation and tool fields") {
    ai::Context context;
    context.systemPrompt = "You are concise.";

    ai::UserMessage user;
    user.content = std::string("read the file");
    context.messages.emplace_back(user);

    ai::AssistantMessage assistant;
    assistant.content.emplace_back(ai::TextContent{"I will read it.", std::nullopt});
    assistant.content.emplace_back(ai::ToolCall{
        "call-1",
        "read",
        nlohmann::json{{"path", "README.md"}},
        std::nullopt,
    });
    context.messages.emplace_back(assistant);

    ai::ToolResultMessage result;
    result.toolCallId = "call-1";
    result.toolName = "read";
    result.content.emplace_back(ai::TextContent{"contents", std::nullopt});
    context.messages.emplace_back(result);

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
    options.temperature = 0.25;
    options.maxTokens = 256;

    const auto request = detail::buildOpenAiChatCompletionsRequest(makeModel(), context, options);

    CHECK_EQ(request.at("model"), "gpt-test");
    CHECK_EQ(request.at("stream"), true);
    CHECK_EQ(request.at("stream_options").at("include_usage"), true);
    CHECK_EQ(request.at("temperature"), 0.25);
    CHECK_EQ(request.at("max_tokens"), 256);

    const auto& messages = request.at("messages");
    REQUIRE_EQ(messages.size(), 4);
    CHECK_EQ(messages[0].at("role"), "system");
    CHECK_EQ(messages[0].at("content"), "You are concise.");
    CHECK_EQ(messages[1].at("role"), "user");
    CHECK_EQ(messages[1].at("content"), "read the file");

    CHECK_EQ(messages[2].at("role"), "assistant");
    CHECK_EQ(messages[2].at("content"), "I will read it.");
    REQUIRE_EQ(messages[2].at("tool_calls").size(), 1);
    CHECK_EQ(messages[2]["tool_calls"][0].at("id"), "call-1");
    CHECK_EQ(messages[2]["tool_calls"][0].at("type"), "function");
    CHECK_EQ(messages[2]["tool_calls"][0]["function"].at("name"), "read");
    CHECK_EQ(messages[2]["tool_calls"][0]["function"].at("arguments"), "{\"path\":\"README.md\"}");

    CHECK_EQ(messages[3].at("role"), "tool");
    CHECK_EQ(messages[3].at("tool_call_id"), "call-1");
    CHECK_EQ(messages[3].at("name"), "read");
    CHECK_EQ(messages[3].at("content"), "contents");

    const auto& tools = request.at("tools");
    REQUIRE_EQ(tools.size(), 1);
    CHECK_EQ(tools[0].at("type"), "function");
    CHECK_EQ(tools[0]["function"].at("name"), "read");
    CHECK_EQ(tools[0]["function"].at("description"), "Read a file");
    CHECK_EQ(tools[0]["function"]["parameters"].at("type"), "object");
}

TEST_CASE("OpenAI request builder maps multimodal user content to data URI parts") {
    ai::Context context;

    ai::UserMessage user;
    std::vector<ai::ContentBlock> content;
    content.emplace_back(ai::TextContent{"describe this", std::nullopt});
    content.emplace_back(ai::ImageContent{"aGVsbG8=", "image/png"});
    user.content = std::move(content);
    context.messages.emplace_back(std::move(user));

    const auto request = detail::buildOpenAiChatCompletionsRequest(
        makeModel(),
        context,
        ai::StreamOptions{});

    const auto& parts = request.at("messages")[0].at("content");
    REQUIRE(parts.is_array());
    REQUIRE_EQ(parts.size(), 2);
    CHECK_EQ(parts[0].at("type"), "text");
    CHECK_EQ(parts[0].at("text"), "describe this");
    CHECK_EQ(parts[1].at("type"), "image_url");
    CHECK_EQ(parts[1]["image_url"].at("url"), "data:image/png;base64,aGVsbG8=");
}

TEST_CASE("OpenAI request builder omits optional fields when not requested") {
    ai::Context context;
    ai::UserMessage user;
    user.content = std::string("hello");
    context.messages.emplace_back(user);

    const auto request = detail::buildOpenAiChatCompletionsRequest(
        makeModel(),
        context,
        ai::StreamOptions{});

    CHECK_FALSE(request.contains("temperature"));
    CHECK_FALSE(request.contains("max_tokens"));
    CHECK_FALSE(request.contains("tools"));
    CHECK_EQ(request.at("messages").size(), 1);
}
