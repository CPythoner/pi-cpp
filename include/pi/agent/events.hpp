#pragma once

#include <pi/agent/message.hpp>
#include <pi/ai/events.hpp>

#include <string>
#include <variant>
#include <vector>

namespace pi::agent {

struct EvAgentStart {};
struct EvAgentEnd { std::vector<AgentMessage> messages; };
struct EvTurnStart {};
struct EvTurnEnd { AgentMessage message; std::vector<AgentMessage> toolResults; };
struct EvMessageStart { AgentMessage message; };
struct EvMessageUpdate { AgentMessage message; ai::AssistantMessageEvent assistantMessageEvent; };
struct EvMessageEnd { AgentMessage message; };
struct EvToolExecutionStart { std::string toolCallId, toolName; nlohmann::json args; };
struct EvToolExecutionUpdate { std::string toolCallId, toolName; nlohmann::json args; nlohmann::json partialResult; };
struct EvToolExecutionEnd { std::string toolCallId, toolName; nlohmann::json result; bool isError = false; };

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
    EvAgentStart,
    EvAgentEnd,
    EvTurnStart,
    EvTurnEnd,
    EvMessageStart,
    EvMessageUpdate,
    EvMessageEnd,
    EvToolExecutionStart,
    EvToolExecutionUpdate,
    EvToolExecutionEnd>;

void to_json(nlohmann::json& j, const AgentEvent& e);
void from_json(const nlohmann::json& j, AgentEvent& e);

} // namespace pi::agent

namespace pi {
using agent::AgentEvent;
using agent::EvAgentEnd;
using agent::EvAgentStart;
using agent::EvMessageEnd;
using agent::EvMessageStart;
using agent::EvMessageUpdate;
using agent::EvToolExecutionEnd;
using agent::EvToolExecutionStart;
using agent::EvToolExecutionUpdate;
using agent::EvTurnEnd;
using agent::EvTurnStart;
} // namespace pi
