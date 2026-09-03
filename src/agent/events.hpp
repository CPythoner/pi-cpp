#pragma once
#include "agent/message.hpp"

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace pi {

// ---- L1 provider 事件：一个 assistant 消息如何流式长出来 ----
// 契约（pi types.ts:439-459）：start 先于一切 partial；终止必须是 done 或 error 之一。
// 每个 delta 事件都携带全量 partial（C++ 里拷贝成本可接受）。
struct EvStart          { AssistantMessage partial; };
struct EvTextStart      { std::size_t contentIndex = 0; AssistantMessage partial; };
struct EvTextDelta      { std::size_t contentIndex = 0; std::string delta; AssistantMessage partial; };
struct EvTextEnd        { std::size_t contentIndex = 0; std::string content; AssistantMessage partial; };
struct EvThinkingStart  { std::size_t contentIndex = 0; AssistantMessage partial; };
struct EvThinkingDelta  { std::size_t contentIndex = 0; std::string delta; AssistantMessage partial; };
struct EvThinkingEnd    { std::size_t contentIndex = 0; std::string content; AssistantMessage partial; };
struct EvToolCallStart  { std::size_t contentIndex = 0; AssistantMessage partial; };
struct EvToolCallDelta  { std::size_t contentIndex = 0; std::string delta; AssistantMessage partial; };
struct EvToolCallEnd    { std::size_t contentIndex = 0; ToolCall toolCall; AssistantMessage partial; };
struct EvDone           { StopReason reason; AssistantMessage message; };  // stop|length|toolUse
struct EvError          { StopReason reason; AssistantMessage error; };    // error|aborted

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
    EvStart, EvTextStart, EvTextDelta, EvTextEnd,
    EvThinkingStart, EvThinkingDelta, EvThinkingEnd,
    EvToolCallStart, EvToolCallDelta, EvToolCallEnd, EvDone, EvError>;

// JSON（events.cpp 实现；wire 判别字段 "type"，未知 type 抛 std::runtime_error）
void to_json(nlohmann::json& j, const AssistantMessageEvent& e);
void from_json(const nlohmann::json& j, AssistantMessageEvent& e);

// ---- L2 agent 事件：agent 生命周期 ----
struct EvAgentStart {};
struct EvAgentEnd  { std::vector<AgentMessage> messages; };
struct EvTurnStart {};
struct EvTurnEnd   { AgentMessage message; std::vector<AgentMessage> toolResults; };
struct EvMessageStart { AgentMessage message; };
struct EvMessageUpdate { AgentMessage message; AssistantMessageEvent assistantMessageEvent; };
struct EvMessageEnd   { AgentMessage message; };
struct EvToolExecutionStart  { std::string toolCallId, toolName; nlohmann::json args; };
struct EvToolExecutionUpdate { std::string toolCallId, toolName; nlohmann::json args; nlohmann::json partialResult; };
struct EvToolExecutionEnd    { std::string toolCallId, toolName; nlohmann::json result; bool isError = false; };

inline void to_json(nlohmann::json& j, const EvAgentStart&) { j = nlohmann::json::object(); }
inline void from_json(const nlohmann::json&, EvAgentStart&) {}
inline void to_json(nlohmann::json& j, const EvTurnStart&) { j = nlohmann::json::object(); }
inline void from_json(const nlohmann::json&, EvTurnStart&) {}

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvAgentEnd, messages)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvTurnEnd, message, toolResults)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvMessageStart, message)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvMessageUpdate, message, assistantMessageEvent)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvMessageEnd, message)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvToolExecutionStart, toolCallId, toolName, args)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvToolExecutionUpdate, toolCallId, toolName, args, partialResult)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(EvToolExecutionEnd, toolCallId, toolName, result, isError)

using AgentEvent = std::variant<
    EvAgentStart, EvAgentEnd, EvTurnStart, EvTurnEnd,
    EvMessageStart, EvMessageUpdate, EvMessageEnd,
    EvToolExecutionStart, EvToolExecutionUpdate, EvToolExecutionEnd>;

void to_json(nlohmann::json& j, const AgentEvent& e);
void from_json(const nlohmann::json& j, AgentEvent& e);

} // namespace pi
