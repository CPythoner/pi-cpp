#include "agent/message.hpp"

#include "util/overload.hpp"

#include <stdexcept>
#include <string>

namespace pi {

// ---- ContentBlock：wire 判别字段 "type" ----
void to_json(nlohmann::json& j, const ContentBlock& b) {
    std::visit(overload{
        [&](const TextContent& v)     { j = v; j["type"] = "text"; },
        [&](const ThinkingContent& v) { j = v; j["type"] = "thinking"; },
        [&](const ImageContent& v)    { j = v; j["type"] = "image"; },
        [&](const ToolCall& v)        { j = v; j["type"] = "toolCall"; },
    }, b);
}

void from_json(const nlohmann::json& j, ContentBlock& b) {
    const auto type = j.at("type").get<std::string>();
    if      (type == "text")     b = j.get<TextContent>();
    else if (type == "thinking") b = j.get<ThinkingContent>();
    else if (type == "image")    b = j.get<ImageContent>();
    else if (type == "toolCall") b = j.get<ToolCall>();
    else throw std::runtime_error("unknown content block type: " + type);
}

// ---- AgentMessage：wire 判别字段 "role" ----
void to_json(nlohmann::json& j, const AgentMessage& m) {
    std::visit(overload{
        [&](const UserMessage& v)             { j = v; j["role"] = "user"; },
        [&](const AssistantMessage& v)        { j = v; j["role"] = "assistant"; },
        [&](const ToolResultMessage& v)       { j = v; j["role"] = "toolResult"; },
        [&](const BashExecutionMessage& v)    { j = v; j["role"] = "bashExecution"; },
        [&](const CustomMessage& v)           { j = v; j["role"] = "custom"; },
        [&](const BranchSummaryMessage& v)    { j = v; j["role"] = "branchSummary"; },
        [&](const CompactionSummaryMessage& v){ j = v; j["role"] = "compactionSummary"; },
    }, m);
}

void from_json(const nlohmann::json& j, AgentMessage& m) {
    const auto role = j.at("role").get<std::string>();
    if      (role == "user")              m = j.get<UserMessage>();
    else if (role == "assistant")         m = j.get<AssistantMessage>();
    else if (role == "toolResult")        m = j.get<ToolResultMessage>();
    else if (role == "bashExecution")     m = j.get<BashExecutionMessage>();
    else if (role == "custom")            m = j.get<CustomMessage>();
    else if (role == "branchSummary")     m = j.get<BranchSummaryMessage>();
    else if (role == "compactionSummary") m = j.get<CompactionSummaryMessage>();
    else throw std::runtime_error("unknown role: " + role);
}

} // namespace pi
