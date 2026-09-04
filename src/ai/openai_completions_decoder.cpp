#include "ai/openai_completions_decoder.hpp"

#include "ai/streaming_json.hpp"

#include <algorithm>
#include <exception>
#include <string>
#include <utility>

namespace pi::ai::detail {

namespace {

std::int64_t integerOrZero(const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_number_integer()) return 0;
    return it->get<std::int64_t>();
}

std::optional<std::string> stringValue(const nlohmann::json& object, const char* key) {
    const auto it = object.find(key);
    if (it == object.end() || !it->is_string()) return std::nullopt;
    return it->get<std::string>();
}

bool isEncryptedReasoningDetail(const nlohmann::json& detail) {
    if (!detail.is_object()) return false;
    const auto type = stringValue(detail, "type");
    const auto id = stringValue(detail, "id");
    const auto data = stringValue(detail, "data");
    return type && *type == "reasoning.encrypted" &&
           id && !id->empty() && data && !data->empty();
}

} // namespace

OpenAIChatCompletionsDecoder::OpenAIChatCompletionsDecoder(
    Model model,
    std::int64_t timestamp)
    : model_(std::move(model)),
      parser_([this](SseEvent event) { onSseEvent(std::move(event)); }) {
    output_.api = model_.api;
    output_.provider = model_.provider;
    output_.model = model_.id;
    output_.stopReason = StopReason::Stop;
    output_.timestamp = timestamp;
}

void OpenAIChatCompletionsDecoder::start() {
    if (started_ || terminal_) return;
    started_ = true;
    stream_.push(EvStart{output_});
}

void OpenAIChatCompletionsDecoder::feed(std::string_view transportChunk) {
    if (terminal_) return;
    start();
    parser_.feed(transportChunk);
}

void OpenAIChatCompletionsDecoder::finish() {
    if (terminal_) return;
    parser_.finish();
    if (!terminal_) complete();
}

void OpenAIChatCompletionsDecoder::fail(std::string message, StopReason reason) {
    terminateError(std::move(message), reason);
}

void OpenAIChatCompletionsDecoder::onSseEvent(SseEvent event) {
    if (terminal_) return;

    if (event.data == "[DONE]") {
        complete();
        return;
    }

    if (event.data.empty()) return;

    try {
        const auto chunk = nlohmann::json::parse(event.data);
        processChunk(chunk);
    } catch (const std::exception& error) {
        terminateError(std::string("Failed to parse OpenAI SSE JSON: ") + error.what(), StopReason::Error);
    }
}

void OpenAIChatCompletionsDecoder::processChunk(const nlohmann::json& chunk) {
    if (!chunk.is_object() || terminal_) return;

    if (!output_.responseId) {
        if (const auto id = stringValue(chunk, "id"); id && !id->empty()) {
            output_.responseId = *id;
        }
    }

    if (!output_.responseModel) {
        if (const auto responseModel = stringValue(chunk, "model");
            responseModel && !responseModel->empty() && *responseModel != model_.id) {
            output_.responseModel = *responseModel;
        }
    }

    if (const auto usage = chunk.find("usage"); usage != chunk.end() && usage->is_object()) {
        parseUsage(*usage);
    }

    const auto choices = chunk.find("choices");
    if (choices == chunk.end() || !choices->is_array() || choices->empty()) return;

    const auto& choice = (*choices)[0];
    if (!choice.is_object()) return;

    if (chunk.find("usage") == chunk.end()) {
        if (const auto usage = choice.find("usage"); usage != choice.end() && usage->is_object()) {
            parseUsage(*usage);
        }
    }

    if (const auto finishReason = stringValue(choice, "finish_reason");
        finishReason && !finishReason->empty()) {
        applyFinishReason(*finishReason);
        hasFinishReason_ = true;
    }

    if (const auto delta = choice.find("delta"); delta != choice.end() && delta->is_object()) {
        processDelta(*delta);
    }
}

void OpenAIChatCompletionsDecoder::processDelta(const nlohmann::json& delta) {
    if (const auto content = stringValue(delta, "content"); content && !content->empty()) {
        const bool blockStarted = textIndex_.has_value();
        const auto index = ensureTextBlock();
        auto& block = std::get<TextContent>(output_.content[index]);
        block.text += *content;
        if (!blockStarted) {
            stream_.push(EvTextStart{index, output_});
        }
        stream_.push(EvTextDelta{index, *content, output_});
    }

    static constexpr const char* reasoningFields[] = {
        "reasoning_content",
        "reasoning",
        "reasoning_text",
    };

    for (const auto* field : reasoningFields) {
        const auto reasoning = stringValue(delta, field);
        if (!reasoning || reasoning->empty()) continue;

        std::string signature = field;
        if (model_.provider == "opencode-go" && signature == "reasoning") {
            signature = "reasoning_content";
        }

        const bool blockStarted = thinkingIndex_.has_value();
        const auto index = ensureThinkingBlock(std::move(signature));
        auto& block = std::get<ThinkingContent>(output_.content[index]);
        block.thinking += *reasoning;
        if (!blockStarted) {
            stream_.push(EvThinkingStart{index, output_});
        }
        stream_.push(EvThinkingDelta{index, *reasoning, output_});
        break;
    }

    if (const auto toolCalls = delta.find("tool_calls");
        toolCalls != delta.end() && toolCalls->is_array()) {
        processToolCalls(*toolCalls);
    }

    if (const auto reasoningDetails = delta.find("reasoning_details");
        reasoningDetails != delta.end() && reasoningDetails->is_array()) {
        processReasoningDetails(*reasoningDetails);
    }
}

void OpenAIChatCompletionsDecoder::processToolCalls(const nlohmann::json& toolCalls) {
    for (const auto& toolCallDelta : toolCalls) {
        if (!toolCallDelta.is_object()) continue;

        const auto stateCountBefore = toolStates_.size();
        auto& state = ensureToolState(toolCallDelta);
        const bool blockStarted = toolStates_.size() == stateCountBefore;
        auto& block = std::get<ToolCall>(output_.content[state.contentIndex]);

        std::string argumentDelta;
        if (const auto function = toolCallDelta.find("function");
            function != toolCallDelta.end() && function->is_object()) {
            if (const auto name = stringValue(*function, "name"); name && !name->empty() && block.name.empty()) {
                block.name = *name;
            }
            if (const auto arguments = stringValue(*function, "arguments"); arguments) {
                argumentDelta = *arguments;
                state.partialArgs += *arguments;
                block.arguments = parseStreamingArguments(state.partialArgs);
            }
        }

        if (const auto id = stringValue(toolCallDelta, "id"); id && !id->empty()) {
            if (block.id.empty()) block.id = *id;
            toolById_[*id] = state.contentIndex;
        }

        if (!block.id.empty()) {
            if (const auto pending = pendingReasoningDetailsByToolCallId_.find(block.id);
                pending != pendingReasoningDetailsByToolCallId_.end()) {
                block.thoughtSignature = pending->second;
                pendingReasoningDetailsByToolCallId_.erase(pending);
            }
        }

        if (!blockStarted) {
            stream_.push(EvToolCallStart{state.contentIndex, output_});
        }
        stream_.push(EvToolCallDelta{state.contentIndex, argumentDelta, output_});
    }
}

void OpenAIChatCompletionsDecoder::processReasoningDetails(
    const nlohmann::json& reasoningDetails) {
    for (const auto& detail : reasoningDetails) {
        if (!isEncryptedReasoningDetail(detail)) continue;

        const auto id = detail.at("id").get<std::string>();
        const auto serialized = detail.dump();
        if (const auto tool = toolById_.find(id); tool != toolById_.end()) {
            auto& block = std::get<ToolCall>(output_.content[tool->second]);
            block.thoughtSignature = serialized;
        } else {
            pendingReasoningDetailsByToolCallId_[id] = serialized;
        }
    }
}

void OpenAIChatCompletionsDecoder::parseUsage(const nlohmann::json& usage) {
    const auto promptTokens = integerOrZero(usage, "prompt_tokens");
    const auto outputTokens = integerOrZero(usage, "completion_tokens");

    std::int64_t cacheReadTokens = integerOrZero(usage, "prompt_cache_hit_tokens");
    std::int64_t cacheWriteTokens = 0;

    if (const auto details = usage.find("prompt_tokens_details");
        details != usage.end() && details->is_object()) {
        if (details->contains("cached_tokens") && (*details)["cached_tokens"].is_number_integer()) {
            cacheReadTokens = (*details)["cached_tokens"].get<std::int64_t>();
        }
        cacheWriteTokens = integerOrZero(*details, "cache_write_tokens");
    }

    output_.usage.input = std::max<std::int64_t>(
        0,
        promptTokens - cacheReadTokens - cacheWriteTokens);
    output_.usage.output = outputTokens;
    output_.usage.cacheRead = cacheReadTokens;
    output_.usage.cacheWrite = cacheWriteTokens;
    output_.usage.totalTokens =
        output_.usage.input + output_.usage.output + output_.usage.cacheRead + output_.usage.cacheWrite;

    constexpr double perMillion = 1'000'000.0;
    output_.usage.cost.input = output_.usage.input * model_.cost.input / perMillion;
    output_.usage.cost.output = output_.usage.output * model_.cost.output / perMillion;
    output_.usage.cost.cacheRead = output_.usage.cacheRead * model_.cost.cacheRead / perMillion;
    output_.usage.cost.cacheWrite = output_.usage.cacheWrite * model_.cost.cacheWrite / perMillion;
    output_.usage.cost.total =
        output_.usage.cost.input +
        output_.usage.cost.output +
        output_.usage.cost.cacheRead +
        output_.usage.cost.cacheWrite;
}

void OpenAIChatCompletionsDecoder::applyFinishReason(std::string_view reason) {
    if (reason == "stop") {
        output_.stopReason = StopReason::Stop;
        output_.errorMessage.reset();
    } else if (reason == "length") {
        output_.stopReason = StopReason::Length;
        output_.errorMessage.reset();
    } else if (reason == "function_call" || reason == "tool_calls") {
        output_.stopReason = StopReason::ToolUse;
        output_.errorMessage.reset();
    } else {
        output_.stopReason = StopReason::Error;
        output_.errorMessage = std::string("Provider finish_reason: ") + std::string(reason);
    }
}

std::size_t OpenAIChatCompletionsDecoder::ensureTextBlock() {
    if (textIndex_) return *textIndex_;

    output_.content.emplace_back(TextContent{});
    textIndex_ = output_.content.size() - 1;
    return *textIndex_;
}

std::size_t OpenAIChatCompletionsDecoder::ensureThinkingBlock(std::string signature) {
    if (thinkingIndex_) return *thinkingIndex_;

    ThinkingContent block;
    block.thinkingSignature = std::move(signature);
    output_.content.emplace_back(std::move(block));
    thinkingIndex_ = output_.content.size() - 1;
    return *thinkingIndex_;
}

OpenAIChatCompletionsDecoder::ToolState& OpenAIChatCompletionsDecoder::ensureToolState(
    const nlohmann::json& toolCall) {
    std::optional<std::int64_t> streamIndex;
    if (const auto it = toolCall.find("index"); it != toolCall.end() && it->is_number_integer()) {
        streamIndex = it->get<std::int64_t>();
    }
    const auto id = stringValue(toolCall, "id");

    std::optional<std::size_t> contentIndex;
    if (streamIndex) {
        if (const auto it = toolByStreamIndex_.find(*streamIndex); it != toolByStreamIndex_.end()) {
            contentIndex = it->second;
        }
    }
    if (!contentIndex && id && !id->empty()) {
        if (const auto it = toolById_.find(*id); it != toolById_.end()) {
            contentIndex = it->second;
        }
    }

    if (!contentIndex) {
        ToolCall block;
        if (id) block.id = *id;
        if (const auto function = toolCall.find("function");
            function != toolCall.end() && function->is_object()) {
            if (const auto name = stringValue(*function, "name")) block.name = *name;
        }
        block.arguments = nlohmann::json::object();

        output_.content.emplace_back(std::move(block));
        contentIndex = output_.content.size() - 1;

        ToolState state;
        state.contentIndex = *contentIndex;
        state.streamIndex = streamIndex;
        toolStates_.emplace(*contentIndex, std::move(state));

        if (streamIndex) toolByStreamIndex_[*streamIndex] = *contentIndex;
        if (id && !id->empty()) toolById_[*id] = *contentIndex;
    }

    auto& state = toolStates_.at(*contentIndex);
    if (!state.streamIndex && streamIndex) {
        state.streamIndex = streamIndex;
        toolByStreamIndex_[*streamIndex] = *contentIndex;
    }
    if (id && !id->empty()) {
        toolById_[*id] = *contentIndex;
    }
    return state;
}

void OpenAIChatCompletionsDecoder::finalizeBlocks() {
    if (blocksFinalized_) return;
    blocksFinalized_ = true;

    for (std::size_t index = 0; index < output_.content.size(); ++index) {
        auto& block = output_.content[index];
        if (auto* text = std::get_if<TextContent>(&block)) {
            stream_.push(EvTextEnd{index, text->text, output_});
        } else if (auto* thinking = std::get_if<ThinkingContent>(&block)) {
            stream_.push(EvThinkingEnd{index, thinking->thinking, output_});
        } else if (auto* toolCall = std::get_if<ToolCall>(&block)) {
            if (const auto it = toolStates_.find(index); it != toolStates_.end()) {
                toolCall->arguments = parseStreamingArguments(it->second.partialArgs);
            }
            stream_.push(EvToolCallEnd{index, *toolCall, output_});
        }
    }
}

void OpenAIChatCompletionsDecoder::complete() {
    if (terminal_) return;

    finalizeBlocks();

    if (output_.stopReason == StopReason::Aborted) {
        terminateError("Request was aborted", StopReason::Aborted);
        return;
    }
    if (output_.stopReason == StopReason::Error) {
        terminateError(
            output_.errorMessage.value_or("Provider returned an error stop reason"),
            StopReason::Error);
        return;
    }
    if (!hasFinishReason_) {
        terminateError("Stream ended without finish_reason", StopReason::Error);
        return;
    }

    terminal_ = true;
    stream_.push(EvDone{output_.stopReason, output_});
}

void OpenAIChatCompletionsDecoder::terminateError(std::string message, StopReason reason) {
    if (terminal_) return;

    output_.stopReason = reason;
    output_.errorMessage = std::move(message);
    terminal_ = true;
    stream_.push(EvError{reason, output_});
}

nlohmann::json OpenAIChatCompletionsDecoder::parseStreamingArguments(std::string_view partial) {
    return parseStreamingJson(partial);
}

} // namespace pi::ai::detail
