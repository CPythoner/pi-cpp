#include <doctest/doctest.h>

#include "ai/openai_completions_decoder.hpp"

#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace ai = pi::ai;
namespace detail = pi::ai::detail;

namespace {

ai::Model makeModel() {
    ai::Model model;
    model.id = "requested-model";
    model.name = "Requested Model";
    model.api = "openai-completions";
    model.provider = "openai";
    model.baseUrl = "https://example.invalid/v1";
    model.cost.input = 1.0;
    model.cost.output = 2.0;
    model.cost.cacheRead = 0.5;
    model.cost.cacheWrite = 0.25;
    return model;
}

void feedJson(detail::OpenAIChatCompletionsDecoder& decoder, const nlohmann::json& chunk) {
    decoder.feed(std::string("data: ") + chunk.dump() + "\n\n");
}

void feedDone(detail::OpenAIChatCompletionsDecoder& decoder) {
    decoder.feed("data: [DONE]\n\n");
}

std::vector<ai::AssistantMessageEvent> drain(ai::AssistantMessageEventStream stream) {
    std::vector<ai::AssistantMessageEvent> events;
    while (auto event = stream.next()) {
        events.push_back(std::move(*event));
    }
    return events;
}

std::string eventType(const ai::AssistantMessageEvent& event) {
    if (std::holds_alternative<ai::EvStart>(event)) return "start";
    if (std::holds_alternative<ai::EvTextStart>(event)) return "text_start";
    if (std::holds_alternative<ai::EvTextDelta>(event)) return "text_delta";
    if (std::holds_alternative<ai::EvTextEnd>(event)) return "text_end";
    if (std::holds_alternative<ai::EvThinkingStart>(event)) return "thinking_start";
    if (std::holds_alternative<ai::EvThinkingDelta>(event)) return "thinking_delta";
    if (std::holds_alternative<ai::EvThinkingEnd>(event)) return "thinking_end";
    if (std::holds_alternative<ai::EvToolCallStart>(event)) return "toolcall_start";
    if (std::holds_alternative<ai::EvToolCallDelta>(event)) return "toolcall_delta";
    if (std::holds_alternative<ai::EvToolCallEnd>(event)) return "toolcall_end";
    if (std::holds_alternative<ai::EvDone>(event)) return "done";
    if (std::holds_alternative<ai::EvError>(event)) return "error";
    return "unknown";
}

} // namespace

TEST_CASE("OpenAI decoder emits cumulative text events and final metadata") {
    detail::OpenAIChatCompletionsDecoder decoder(makeModel(), 1234);
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"id", "resp-1"},
        {"model", "actual-model"},
        {"choices", nlohmann::json::array({{
            {"delta", {{"content", "Hel"}}},
            {"finish_reason", nullptr},
        }})},
    });
    feedJson(decoder, {
        {"id", "resp-1"},
        {"model", "actual-model"},
        {"choices", nlohmann::json::array({{
            {"delta", {{"content", "lo"}}},
            {"finish_reason", "stop"},
        }})},
    });
    feedDone(decoder);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 6);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "text_start");
    CHECK_EQ(eventType(events[2]), "text_delta");
    CHECK_EQ(eventType(events[3]), "text_delta");
    CHECK_EQ(eventType(events[4]), "text_end");
    CHECK_EQ(eventType(events[5]), "done");

    CHECK_EQ(std::get<ai::EvTextDelta>(events[2]).delta, "Hel");
    CHECK_EQ(std::get<ai::EvTextDelta>(events[3]).delta, "lo");
    CHECK_EQ(
        std::get<ai::TextContent>(std::get<ai::EvTextDelta>(events[2]).partial.content[0]).text,
        "Hel");
    CHECK_EQ(
        std::get<ai::TextContent>(std::get<ai::EvTextDelta>(events[3]).partial.content[0]).text,
        "Hello");

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    CHECK_EQ(std::get<ai::TextContent>(result.content[0]).text, "Hello");
    CHECK_EQ(result.stopReason, ai::StopReason::Stop);
    CHECK_EQ(result.timestamp, 1234);
    REQUIRE(result.responseId.has_value());
    CHECK_EQ(*result.responseId, "resp-1");
    REQUIRE(result.responseModel.has_value());
    CHECK_EQ(*result.responseModel, "actual-model");
}

TEST_CASE("OpenAI decoder aggregates reasoning into one thinking block") {
    detail::OpenAIChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{"reasoning_content", "think "}}},
            {"finish_reason", nullptr},
        }})},
    });
    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{"reasoning_content", "more"}}},
            {"finish_reason", "stop"},
        }})},
    });
    feedDone(decoder);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 6);
    CHECK_EQ(eventType(events[1]), "thinking_start");
    CHECK_EQ(eventType(events[2]), "thinking_delta");
    CHECK_EQ(eventType(events[3]), "thinking_delta");
    CHECK_EQ(eventType(events[4]), "thinking_end");
    CHECK_EQ(eventType(events[5]), "done");

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    const auto& thinking = std::get<ai::ThinkingContent>(result.content[0]);
    CHECK_EQ(thinking.thinking, "think more");
    REQUIRE(thinking.thinkingSignature.has_value());
    CHECK_EQ(*thinking.thinkingSignature, "reasoning_content");
}

TEST_CASE("OpenAI decoder merges tool call fragments by stream index") {
    detail::OpenAIChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", {{
                "tool_calls",
                nlohmann::json::array({{
                    {"index", 0},
                    {"id", "call-1"},
                    {"function", {{"name", "read"}, {"arguments", "{\"pa"}}},
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
                    {"function", {{"arguments", "th\":\"README.md\"}"}}},
                }})
            }}},
            {"finish_reason", "tool_calls"},
        }})},
    });
    feedDone(decoder);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 6);
    CHECK_EQ(eventType(events[1]), "toolcall_start");
    CHECK_EQ(eventType(events[2]), "toolcall_delta");
    CHECK_EQ(eventType(events[3]), "toolcall_delta");
    CHECK_EQ(eventType(events[4]), "toolcall_end");
    CHECK_EQ(eventType(events[5]), "done");

    const auto result = stream.result();
    CHECK_EQ(result.stopReason, ai::StopReason::ToolUse);
    REQUIRE_EQ(result.content.size(), 1);
    const auto& call = std::get<ai::ToolCall>(result.content[0]);
    CHECK_EQ(call.id, "call-1");
    CHECK_EQ(call.name, "read");
    REQUIRE(call.arguments.is_object());
    CHECK_EQ(call.arguments.at("path"), "README.md");
}

TEST_CASE("OpenAI decoder maps usage cache tokens and model cost") {
    auto model = makeModel();
    detail::OpenAIChatCompletionsDecoder decoder(model);
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"usage", {
            {"prompt_tokens", 100},
            {"completion_tokens", 30},
            {"prompt_tokens_details", {
                {"cached_tokens", 20},
                {"cache_write_tokens", 5},
            }},
        }},
        {"choices", nlohmann::json::array()},
    });
    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", nlohmann::json::object()},
            {"finish_reason", "stop"},
        }})},
    });
    feedDone(decoder);

    const auto result = stream.result();
    CHECK_EQ(result.usage.input, 75);
    CHECK_EQ(result.usage.output, 30);
    CHECK_EQ(result.usage.cacheRead, 20);
    CHECK_EQ(result.usage.cacheWrite, 5);
    CHECK_EQ(result.usage.totalTokens, 130);
    CHECK(result.usage.cost.input == doctest::Approx(0.000075));
    CHECK(result.usage.cost.output == doctest::Approx(0.000060));
    CHECK(result.usage.cost.cacheRead == doctest::Approx(0.000010));
    CHECK(result.usage.cost.cacheWrite == doctest::Approx(0.00000125));
    CHECK(result.usage.cost.total == doctest::Approx(0.00014625));

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "done");
}

TEST_CASE("OpenAI decoder turns malformed SSE JSON into terminal error") {
    detail::OpenAIChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    decoder.feed("data: {not-json}\n\n");

    CHECK(decoder.terminal());
    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "error");

    const auto result = stream.result();
    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK(result.errorMessage->find("Failed to parse OpenAI SSE JSON") != std::string::npos);
}

TEST_CASE("OpenAI decoder rejects DONE without finish_reason") {
    detail::OpenAIChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedDone(decoder);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "error");

    const auto result = stream.result();
    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "Stream ended without finish_reason");
}

TEST_CASE("OpenAI decoder maps provider error finish reasons to EvError") {
    detail::OpenAIChatCompletionsDecoder decoder(makeModel());
    auto stream = decoder.stream();

    feedJson(decoder, {
        {"choices", nlohmann::json::array({{
            {"delta", nlohmann::json::object()},
            {"finish_reason", "content_filter"},
        }})},
    });
    feedDone(decoder);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(eventType(events[1]), "error");

    const auto result = stream.result();
    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "Provider finish_reason: content_filter");
}
