#pragma once

#include <pi/ai/message.hpp>

#include <cstddef>
#include <string>
#include <variant>

namespace pi::ai {

struct EvStart { AssistantMessage partial; };
struct EvTextStart { std::size_t contentIndex = 0; AssistantMessage partial; };
struct EvTextDelta { std::size_t contentIndex = 0; std::string delta; AssistantMessage partial; };
struct EvTextEnd { std::size_t contentIndex = 0; std::string content; AssistantMessage partial; };
struct EvThinkingStart { std::size_t contentIndex = 0; AssistantMessage partial; };
struct EvThinkingDelta { std::size_t contentIndex = 0; std::string delta; AssistantMessage partial; };
struct EvThinkingEnd { std::size_t contentIndex = 0; std::string content; AssistantMessage partial; };
struct EvToolCallStart { std::size_t contentIndex = 0; AssistantMessage partial; };
struct EvToolCallDelta { std::size_t contentIndex = 0; std::string delta; AssistantMessage partial; };
struct EvToolCallEnd { std::size_t contentIndex = 0; ToolCall toolCall; AssistantMessage partial; };
struct EvDone { StopReason reason; AssistantMessage message; };
struct EvError { StopReason reason; AssistantMessage error; };

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvStart, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvTextStart, contentIndex, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvTextDelta, contentIndex, delta, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvTextEnd, contentIndex, content, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvThinkingStart, contentIndex, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvThinkingDelta, contentIndex, delta, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvThinkingEnd, contentIndex, content, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvToolCallStart, contentIndex, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvToolCallDelta, contentIndex, delta, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvToolCallEnd, contentIndex, toolCall, partial)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvDone, reason, message)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvError, reason, error)

using AssistantMessageEvent = std::variant<
    EvStart,
    EvTextStart,
    EvTextDelta,
    EvTextEnd,
    EvThinkingStart,
    EvThinkingDelta,
    EvThinkingEnd,
    EvToolCallStart,
    EvToolCallDelta,
    EvToolCallEnd,
    EvDone,
    EvError>;

void to_json(nlohmann::json& j, const AssistantMessageEvent& e);
void from_json(const nlohmann::json& j, AssistantMessageEvent& e);

} // namespace pi::ai

namespace pi {
using ai::AssistantMessageEvent;
using ai::EvDone;
using ai::EvError;
using ai::EvStart;
using ai::EvTextDelta;
using ai::EvTextEnd;
using ai::EvTextStart;
using ai::EvThinkingDelta;
using ai::EvThinkingEnd;
using ai::EvThinkingStart;
using ai::EvToolCallDelta;
using ai::EvToolCallEnd;
using ai::EvToolCallStart;
} // namespace pi
