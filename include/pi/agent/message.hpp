#pragma once

#include <pi/ai/message.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace pi::agent {

struct BashExecutionMessage {
    std::string command;
    std::string output;
    std::int64_t exitCode = 0;
    bool cancelled = false;
    bool truncated = false;
    std::optional<std::string> fullOutputPath;
    bool excludeFromContext = false;
    std::int64_t timestamp = 0;
};

inline void to_json(nlohmann::json& j, const BashExecutionMessage& m) {
    j["command"] = m.command;
    j["output"] = m.output;
    j["exitCode"] = m.exitCode;
    j["cancelled"] = m.cancelled;
    j["truncated"] = m.truncated;
    if (m.fullOutputPath) j["fullOutputPath"] = *m.fullOutputPath;
    j["excludeFromContext"] = m.excludeFromContext;
    j["timestamp"] = m.timestamp;
}

inline void from_json(const nlohmann::json& j, BashExecutionMessage& m) {
    m.command = j.value("command", std::string{});
    m.output = j.value("output", std::string{});
    m.exitCode = j.value("exitCode", std::int64_t{0});
    m.cancelled = j.value("cancelled", false);
    m.truncated = j.value("truncated", false);
    if (j.contains("fullOutputPath")) m.fullOutputPath = j.at("fullOutputPath").get<std::string>();
    else m.fullOutputPath = std::nullopt;
    m.excludeFromContext = j.value("excludeFromContext", false);
    m.timestamp = j.value("timestamp", std::int64_t{0});
}

struct CustomMessage {
    std::string customType;
    ai::UserContent content;
    nlohmann::json details;
    bool display = true;
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    CustomMessage, customType, content, details, display, timestamp)

struct BranchSummaryMessage {
    std::string summary;
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BranchSummaryMessage, summary, timestamp)

struct CompactionSummaryMessage {
    std::string summary;
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CompactionSummaryMessage, summary, timestamp)

using AgentMessage = std::variant<
    ai::UserMessage,
    ai::AssistantMessage,
    ai::ToolResultMessage,
    BashExecutionMessage,
    CustomMessage,
    BranchSummaryMessage,
    CompactionSummaryMessage>;

void to_json(nlohmann::json& j, const AgentMessage& m);
void from_json(const nlohmann::json& j, AgentMessage& m);

} // namespace pi::agent

namespace pi {
using agent::AgentMessage;
using agent::BashExecutionMessage;
using agent::BranchSummaryMessage;
using agent::CompactionSummaryMessage;
using agent::CustomMessage;
} // namespace pi
