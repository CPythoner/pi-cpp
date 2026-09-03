#include <pi/agent/events.hpp>

#include <stdexcept>
#include <string>

namespace {

template <class... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

} // namespace

namespace pi::agent {

void to_json(nlohmann::json& j, const AgentEvent& e) {
    std::visit(Overload{
        [&](const EvAgentStart&) { j = nlohmann::json::object(); j["type"] = "agent_start"; },
        [&](const EvAgentEnd& v) { j = v; j["type"] = "agent_end"; },
        [&](const EvTurnStart&) { j = nlohmann::json::object(); j["type"] = "turn_start"; },
        [&](const EvTurnEnd& v) { j = v; j["type"] = "turn_end"; },
        [&](const EvMessageStart& v) { j = v; j["type"] = "message_start"; },
        [&](const EvMessageUpdate& v) { j = v; j["type"] = "message_update"; },
        [&](const EvMessageEnd& v) { j = v; j["type"] = "message_end"; },
        [&](const EvToolExecutionStart& v) { j = v; j["type"] = "tool_execution_start"; },
        [&](const EvToolExecutionUpdate& v) { j = v; j["type"] = "tool_execution_update"; },
        [&](const EvToolExecutionEnd& v) { j = v; j["type"] = "tool_execution_end"; },
    }, e);
}

void from_json(const nlohmann::json& j, AgentEvent& e) {
    const auto type = j.at("type").get<std::string>();
    if (type == "agent_start") e = j.get<EvAgentStart>();
    else if (type == "agent_end") e = j.get<EvAgentEnd>();
    else if (type == "turn_start") e = j.get<EvTurnStart>();
    else if (type == "turn_end") e = j.get<EvTurnEnd>();
    else if (type == "message_start") e = j.get<EvMessageStart>();
    else if (type == "message_update") e = j.get<EvMessageUpdate>();
    else if (type == "message_end") e = j.get<EvMessageEnd>();
    else if (type == "tool_execution_start") e = j.get<EvToolExecutionStart>();
    else if (type == "tool_execution_update") e = j.get<EvToolExecutionUpdate>();
    else if (type == "tool_execution_end") e = j.get<EvToolExecutionEnd>();
    else throw std::runtime_error("unknown agent event type: " + type);
}

} // namespace pi::agent
