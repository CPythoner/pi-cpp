#include "agent/events.hpp"

#include "util/overload.hpp"

#include <stdexcept>
#include <string>

namespace pi {

// ---- L1：wire 判别字段 "type" ----
void to_json(nlohmann::json& j, const AssistantMessageEvent& e) {
    std::visit(overload{
        [&](const EvStart& e)         { j = e; j["type"] = "start"; },
        [&](const EvTextStart& e)     { j = e; j["type"] = "text_start"; },
        [&](const EvTextDelta& e)     { j = e; j["type"] = "text_delta"; },
        [&](const EvTextEnd& e)       { j = e; j["type"] = "text_end"; },
        [&](const EvThinkingStart& e) { j = e; j["type"] = "thinking_start"; },
        [&](const EvThinkingDelta& e) { j = e; j["type"] = "thinking_delta"; },
        [&](const EvThinkingEnd& e)   { j = e; j["type"] = "thinking_end"; },
        [&](const EvToolCallStart& e) { j = e; j["type"] = "toolcall_start"; },
        [&](const EvToolCallDelta& e) { j = e; j["type"] = "toolcall_delta"; },
        [&](const EvToolCallEnd& e)   { j = e; j["type"] = "toolcall_end"; },
        [&](const EvDone& e)          { j = e; j["type"] = "done"; },
        [&](const EvError& e)         { j = e; j["type"] = "error"; },
    }, e);
}

void from_json(const nlohmann::json& j, AssistantMessageEvent& e) {
    const auto type = j.at("type").get<std::string>();
    if      (type == "start")           e = j.get<EvStart>();
    else if (type == "text_start")      e = j.get<EvTextStart>();
    else if (type == "text_delta")      e = j.get<EvTextDelta>();
    else if (type == "text_end")        e = j.get<EvTextEnd>();
    else if (type == "thinking_start")  e = j.get<EvThinkingStart>();
    else if (type == "thinking_delta")  e = j.get<EvThinkingDelta>();
    else if (type == "thinking_end")    e = j.get<EvThinkingEnd>();
    else if (type == "toolcall_start")  e = j.get<EvToolCallStart>();
    else if (type == "toolcall_delta")  e = j.get<EvToolCallDelta>();
    else if (type == "toolcall_end")    e = j.get<EvToolCallEnd>();
    else if (type == "done")            e = j.get<EvDone>();
    else if (type == "error")           e = j.get<EvError>();
    else throw std::runtime_error("unknown assistant message event type: " + type);
}

// ---- L2：wire 判别字段 "type" ----
void to_json(nlohmann::json& j, const AgentEvent& e) {
    std::visit(overload{
        [&](const EvAgentStart&)          { j = nlohmann::json::object(); j["type"] = "agent_start"; },
        [&](const EvAgentEnd& e)          { j = e; j["type"] = "agent_end"; },
        [&](const EvTurnStart&)           { j = nlohmann::json::object(); j["type"] = "turn_start"; },
        [&](const EvTurnEnd& e)           { j = e; j["type"] = "turn_end"; },
        [&](const EvMessageStart& e)      { j = e; j["type"] = "message_start"; },
        [&](const EvMessageUpdate& e)     { j = e; j["type"] = "message_update"; },
        [&](const EvMessageEnd& e)        { j = e; j["type"] = "message_end"; },
        [&](const EvToolExecutionStart& e)  { j = e; j["type"] = "tool_execution_start"; },
        [&](const EvToolExecutionUpdate& e) { j = e; j["type"] = "tool_execution_update"; },
        [&](const EvToolExecutionEnd& e)    { j = e; j["type"] = "tool_execution_end"; },
    }, e);
}

void from_json(const nlohmann::json& j, AgentEvent& e) {
    const auto type = j.at("type").get<std::string>();
    if      (type == "agent_start")           e = j.get<EvAgentStart>();
    else if (type == "agent_end")             e = j.get<EvAgentEnd>();
    else if (type == "turn_start")            e = j.get<EvTurnStart>();
    else if (type == "turn_end")              e = j.get<EvTurnEnd>();
    else if (type == "message_start")         e = j.get<EvMessageStart>();
    else if (type == "message_update")        e = j.get<EvMessageUpdate>();
    else if (type == "message_end")           e = j.get<EvMessageEnd>();
    else if (type == "tool_execution_start")  e = j.get<EvToolExecutionStart>();
    else if (type == "tool_execution_update") e = j.get<EvToolExecutionUpdate>();
    else if (type == "tool_execution_end")    e = j.get<EvToolExecutionEnd>();
    else throw std::runtime_error("unknown agent event type: " + type);
}

} // namespace pi
