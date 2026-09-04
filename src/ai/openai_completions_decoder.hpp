#pragma once

#include "ai/sse_parser.hpp"

#include <pi/ai/provider.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace pi::ai::detail {

class OpenAIChatCompletionsDecoder {
public:
    OpenAIChatCompletionsDecoder(
        Model model,
        std::int64_t timestamp = 0);

    OpenAIChatCompletionsDecoder(const OpenAIChatCompletionsDecoder&) = delete;
    OpenAIChatCompletionsDecoder& operator=(const OpenAIChatCompletionsDecoder&) = delete;

    void start();
    void feed(std::string_view transportChunk);
    void finish();
    void fail(std::string message, StopReason reason = StopReason::Error);

    AssistantMessageEventStream stream() const { return stream_; }
    bool started() const noexcept { return started_; }
    bool terminal() const noexcept { return terminal_; }

private:
    struct ToolState {
        std::size_t contentIndex = 0;
        std::string partialArgs;
        std::optional<std::int64_t> streamIndex;
    };

    void onSseEvent(SseEvent event);
    void processChunk(const nlohmann::json& chunk);
    void processDelta(const nlohmann::json& delta);
    void processToolCalls(const nlohmann::json& toolCalls);
    void processReasoningDetails(const nlohmann::json& reasoningDetails);
    void parseUsage(const nlohmann::json& usage);
    void applyFinishReason(std::string_view reason);

    std::size_t ensureTextBlock();
    std::size_t ensureThinkingBlock(std::string signature);
    ToolState& ensureToolState(const nlohmann::json& toolCall);

    void finalizeBlocks();
    void complete();
    void terminateError(std::string message, StopReason reason);

    static nlohmann::json parseStreamingArguments(std::string_view partial);

    Model model_;
    AssistantMessage output_;
    AssistantMessageEventStream stream_;
    SseParser parser_;

    bool started_ = false;
    bool terminal_ = false;
    bool hasFinishReason_ = false;
    bool blocksFinalized_ = false;
    std::optional<std::size_t> textIndex_;
    std::optional<std::size_t> thinkingIndex_;
    std::map<std::int64_t, std::size_t> toolByStreamIndex_;
    std::map<std::string, std::size_t> toolById_;
    std::map<std::size_t, ToolState> toolStates_;
    std::map<std::string, std::string> pendingReasoningDetailsByToolCallId_;
};

} // namespace pi::ai::detail
