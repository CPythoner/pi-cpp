#include <doctest/doctest.h>

#include "ai/openai_compatible.hpp"

#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

#include <string>
#include <utility>

namespace ai = pi::ai;
namespace detail = pi::ai::detail;

namespace {

ai::Model makeModel() {
    ai::Model model;
    model.id = "tool-model";
    model.name = "Tool Model";
    model.api = "openai-completions";
    model.provider = "openai";
    model.baseUrl = "https://example.invalid/v1";
    return model;
}

void feedJson(detail::OpenAiChatCompletionsDecoder& decoder, const nlohmann::json& chunk) {
    decoder.feed(std::string("data: ") + chunk.dump() + "\n\n");
}

void feedDone(detail::OpenAiChatCompletionsDecoder& decoder) {
    decoder.feed("data: [DONE]\n\n");
}

} // namespace

TEST_CASE("toolcall delta exposes partially parsed arguments") {
    detail::OpenAiChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"index", 0},
                    {"id", "call-1"},
                    {"function", {{"name", "read"}, {"arguments", "{\"path\":\"READ"}}},
                }})
            }}},
            {"finish_reason", nullptr},
        }})},
    });

    REQUIRE(stream.next().has_value()); // start
    REQUIRE(stream.next().has_value()); // toolcall_start
    const auto deltaEvent = stream.next();
    REQUIRE(deltaEvent.has_value());
    REQUIRE(std::holds_alternative<ai::EvToolCallDelta>(*deltaEvent));
    const auto& partial = std::get<ai::EvToolCallDelta>(*deltaEvent).partial;
    REQUIRE_EQ(partial.content.size(), 1);
    const auto& partialCall = std::get<ai::ToolCall>(partial.content[0]);
    REQUIRE(partialCall.arguments.is_object());
    CHECK_EQ(partialCall.arguments.at("path"), "READ");

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"index", 0},
                    {"function", {{"arguments", "ME.md\"}"}}},
                }})
            }}},
            {"finish_reason", "tool_calls"},
        }})},
    });
    feedDone(decoder);

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    CHECK_EQ(std::get<ai::ToolCall>(result.content[0]).arguments.at("path"), "README.md");
}

TEST_CASE("tool call can receive id and name after its stream index was created") {
    detail::OpenAiChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"index", 0},
                    {"function", {{"arguments", "{\"value\":"}}},
                }})
            }}},
            {"finish_reason", nullptr},
        }})},
    });
    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"index", 0},
                    {"id", "call-late"},
                    {"function", {{"name", "write"}, {"arguments", "1}"}}},
                }})
            }}},
            {"finish_reason", "tool_calls"},
        }})},
    });
    feedDone(decoder);

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    const auto& call = std::get<ai::ToolCall>(result.content[0]);
    CHECK_EQ(call.id, "call-late");
    CHECK_EQ(call.name, "write");
    CHECK_EQ(call.arguments.at("value"), 1);
}

TEST_CASE("tool calls without stream index merge by id") {
    detail::OpenAiChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"id", "call-by-id"},
                    {"function", {{"name", "read"}, {"arguments", "{\"path\":\""}}},
                }})
            }}},
            {"finish_reason", nullptr},
        }})},
    });
    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"id", "call-by-id"},
                    {"function", {{"arguments", "README.md\"}"}}},
                }})
            }}},
            {"finish_reason", "tool_calls"},
        }})},
    });
    feedDone(decoder);

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    const auto& call = std::get<ai::ToolCall>(result.content[0]);
    CHECK_EQ(call.id, "call-by-id");
    CHECK_EQ(call.arguments.at("path"), "README.md");
}

TEST_CASE("encrypted reasoning detail attaches to an existing tool call") {
    detail::OpenAiChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    const nlohmann::json detailJson{
        {"type", "reasoning.encrypted"},
        {"id", "call-1"},
        {"data", "opaque-data"},
    };

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {
                {"tool_calls", nlohmann::json::array({{
                    {"index", 0},
                    {"id", "call-1"},
                    {"function", {{"name", "read"}, {"arguments", "{}"}}},
                }})},
                {"reasoning_details", nlohmann::json::array({detailJson})},
            }},
            {"finish_reason", "tool_calls"},
        }})},
    });
    feedDone(decoder);

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    const auto& call = std::get<ai::ToolCall>(result.content[0]);
    REQUIRE(call.thoughtSignature.has_value());
    CHECK_EQ(*call.thoughtSignature, detailJson.dump());
}

TEST_CASE("encrypted reasoning detail waits for a later tool call id") {
    detail::OpenAiChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    const nlohmann::json detailJson{
        {"type", "reasoning.encrypted"},
        {"id", "call-late"},
        {"data", "opaque-late"},
    };

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{"reasoning_details", nlohmann::json::array({detailJson})}}},
            {"finish_reason", nullptr},
        }})},
    });
    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"index", 0},
                    {"id", "call-late"},
                    {"function", {{"name", "read"}, {"arguments", "{}"}}},
                }})
            }}},
            {"finish_reason", "tool_calls"},
        }})},
    });
    feedDone(decoder);

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    const auto& call = std::get<ai::ToolCall>(result.content[0]);
    REQUIRE(call.thoughtSignature.has_value());
    CHECK_EQ(*call.thoughtSignature, detailJson.dump());
}
