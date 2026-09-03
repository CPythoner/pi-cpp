#include <pi/agent/message.hpp>

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

void to_json(nlohmann::json& j, const AgentMessage& m) {
    std::visit(Overload{
        [&](const ai::UserMessage& v) { j = v; j["role"] = "user"; },
        [&](const ai::AssistantMessage& v) { j = v; j["role"] = "assistant"; },
        [&](const ai::ToolResultMessage& v) { j = v; j["role"] = "toolResult"; },
        [&](const BashExecutionMessage& v) { j = v; j["role"] = "bashExecution"; },
        [&](const CustomMessage& v) { j = v; j["role"] = "custom"; },
        [&](const BranchSummaryMessage& v) { j = v; j["role"] = "branchSummary"; },
        [&](const CompactionSummaryMessage& v) { j = v; j["role"] = "compactionSummary"; },
    }, m);
}

void from_json(const nlohmann::json& j, AgentMessage& m) {
    const auto role = j.at("role").get<std::string>();
    if (role == "user") m = j.get<ai::UserMessage>();
    else if (role == "assistant") m = j.get<ai::AssistantMessage>();
    else if (role == "toolResult") m = j.get<ai::ToolResultMessage>();
    else if (role == "bashExecution") m = j.get<BashExecutionMessage>();
    else if (role == "custom") m = j.get<CustomMessage>();
    else if (role == "branchSummary") m = j.get<BranchSummaryMessage>();
    else if (role == "compactionSummary") m = j.get<CompactionSummaryMessage>();
    else throw std::runtime_error("unknown agent message role: " + role);
}

} // namespace pi::agent
