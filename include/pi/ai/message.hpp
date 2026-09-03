#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace pi::ai {

enum class StopReason { Stop, Length, ToolUse, Error, Aborted };
NLOHMANN_JSON_SERIALIZE_ENUM(StopReason, {
    {StopReason::Stop, "stop"},
    {StopReason::Length, "length"},
    {StopReason::ToolUse, "toolUse"},
    {StopReason::Error, "error"},
    {StopReason::Aborted, "aborted"},
})

struct Cost {
    double input = 0;
    double output = 0;
    double cacheRead = 0;
    double cacheWrite = 0;
    double total = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Cost, input, output, cacheRead, cacheWrite, total)

struct Usage {
    std::int64_t input = 0;
    std::int64_t output = 0;
    std::int64_t cacheRead = 0;
    std::int64_t cacheWrite = 0;
    std::optional<std::int64_t> cacheWrite1h;
    std::int64_t totalTokens = 0;
    Cost cost;
};

inline void to_json(nlohmann::json& j, const Usage& u) {
    j["input"] = u.input;
    j["output"] = u.output;
    j["cacheRead"] = u.cacheRead;
    j["cacheWrite"] = u.cacheWrite;
    if (u.cacheWrite1h) j["cacheWrite1h"] = *u.cacheWrite1h;
    j["totalTokens"] = u.totalTokens;
    j["cost"] = u.cost;
}

inline void from_json(const nlohmann::json& j, Usage& u) {
    const Usage d{};
    u.input = j.value("input", d.input);
    u.output = j.value("output", d.output);
    u.cacheRead = j.value("cacheRead", d.cacheRead);
    u.cacheWrite = j.value("cacheWrite", d.cacheWrite);
    if (j.contains("cacheWrite1h")) u.cacheWrite1h = j.at("cacheWrite1h").get<std::int64_t>();
    else u.cacheWrite1h = std::nullopt;
    u.totalTokens = j.value("totalTokens", d.totalTokens);
    u.cost = j.contains("cost") ? j.at("cost").get<Cost>() : d.cost;
}

struct DiagnosticError {
    std::optional<std::string> name;
    std::optional<std::string> stack;
    std::optional<std::string> code;
    std::string message;
};

inline void to_json(nlohmann::json& j, const DiagnosticError& e) {
    if (e.name) j["name"] = *e.name;
    if (e.stack) j["stack"] = *e.stack;
    if (e.code) j["code"] = *e.code;
    j["message"] = e.message;
}

inline void from_json(const nlohmann::json& j, DiagnosticError& e) {
    if (j.contains("name")) e.name = j.at("name").get<std::string>();
    else e.name = std::nullopt;
    if (j.contains("stack")) e.stack = j.at("stack").get<std::string>();
    else e.stack = std::nullopt;
    if (j.contains("code")) e.code = j.at("code").get<std::string>();
    else e.code = std::nullopt;
    e.message = j.value("message", std::string{});
}

struct Diagnostic {
    std::string type;
    std::int64_t timestamp = 0;
    std::optional<DiagnosticError> error;
    nlohmann::json details;
};

inline void to_json(nlohmann::json& j, const Diagnostic& d) {
    j["type"] = d.type;
    j["timestamp"] = d.timestamp;
    if (d.error) j["error"] = *d.error;
    j["details"] = d.details;
}

inline void from_json(const nlohmann::json& j, Diagnostic& d) {
    d.type = j.value("type", std::string{});
    d.timestamp = j.value("timestamp", std::int64_t{0});
    if (j.contains("error")) d.error = j.at("error").get<DiagnosticError>();
    else d.error = std::nullopt;
    d.details = j.contains("details") ? j.at("details") : nlohmann::json{};
}

struct TextContent {
    std::string text;
    std::optional<std::string> textSignature;
};

inline void to_json(nlohmann::json& j, const TextContent& t) {
    j["text"] = t.text;
    if (t.textSignature) j["textSignature"] = *t.textSignature;
}

inline void from_json(const nlohmann::json& j, TextContent& t) {
    t.text = j.value("text", std::string{});
    if (j.contains("textSignature")) t.textSignature = j.at("textSignature").get<std::string>();
    else t.textSignature = std::nullopt;
}

struct ThinkingContent {
    std::string thinking;
    std::optional<std::string> thinkingSignature;
    bool redacted = false;
};

inline void to_json(nlohmann::json& j, const ThinkingContent& t) {
    j["thinking"] = t.thinking;
    if (t.thinkingSignature) j["thinkingSignature"] = *t.thinkingSignature;
    j["redacted"] = t.redacted;
}

inline void from_json(const nlohmann::json& j, ThinkingContent& t) {
    t.thinking = j.value("thinking", std::string{});
    if (j.contains("thinkingSignature")) t.thinkingSignature = j.at("thinkingSignature").get<std::string>();
    else t.thinkingSignature = std::nullopt;
    t.redacted = j.value("redacted", false);
}

struct ImageContent {
    std::string data;
    std::string mimeType;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ImageContent, data, mimeType)

struct ToolCall {
    std::string id;
    std::string name;
    nlohmann::json arguments;
    std::optional<std::string> thoughtSignature;
};

inline void to_json(nlohmann::json& j, const ToolCall& t) {
    j["id"] = t.id;
    j["name"] = t.name;
    j["arguments"] = t.arguments;
    if (t.thoughtSignature) j["thoughtSignature"] = *t.thoughtSignature;
}

inline void from_json(const nlohmann::json& j, ToolCall& t) {
    t.id = j.value("id", std::string{});
    t.name = j.value("name", std::string{});
    t.arguments = j.contains("arguments") ? j.at("arguments") : nlohmann::json{};
    if (j.contains("thoughtSignature")) t.thoughtSignature = j.at("thoughtSignature").get<std::string>();
    else t.thoughtSignature = std::nullopt;
}

using ContentBlock = std::variant<TextContent, ThinkingContent, ImageContent, ToolCall>;

void to_json(nlohmann::json& j, const ContentBlock& b);
void from_json(const nlohmann::json& j, ContentBlock& b);

using UserContent = std::variant<std::string, std::vector<ContentBlock>>;

inline void to_json(nlohmann::json& j, const UserContent& c) {
    std::visit([&](const auto& v) { j = v; }, c);
}

inline void from_json(const nlohmann::json& j, UserContent& c) {
    c = j.is_string() ? UserContent(j.get<std::string>())
                      : UserContent(j.get<std::vector<ContentBlock>>());
}

struct UserMessage {
    UserContent content;
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(UserMessage, content, timestamp)

struct AssistantMessage {
    std::vector<ContentBlock> content;
    std::string api;
    std::string provider;
    std::string model;
    std::optional<std::string> responseModel;
    std::optional<std::string> responseId;
    std::optional<std::vector<Diagnostic>> diagnostics;
    Usage usage;
    StopReason stopReason = StopReason::Stop;
    std::optional<std::string> errorMessage;
    std::int64_t timestamp = 0;
};

inline void to_json(nlohmann::json& j, const AssistantMessage& m) {
    j["content"] = m.content;
    j["api"] = m.api;
    j["provider"] = m.provider;
    j["model"] = m.model;
    if (m.responseModel) j["responseModel"] = *m.responseModel;
    if (m.responseId) j["responseId"] = *m.responseId;
    if (m.diagnostics) j["diagnostics"] = *m.diagnostics;
    j["usage"] = m.usage;
    j["stopReason"] = m.stopReason;
    if (m.errorMessage) j["errorMessage"] = *m.errorMessage;
    j["timestamp"] = m.timestamp;
}

inline void from_json(const nlohmann::json& j, AssistantMessage& m) {
    if (j.contains("content")) m.content = j.at("content").get<std::vector<ContentBlock>>();
    else m.content = {};
    m.api = j.value("api", std::string{});
    m.provider = j.value("provider", std::string{});
    m.model = j.value("model", std::string{});
    if (j.contains("responseModel")) m.responseModel = j.at("responseModel").get<std::string>();
    else m.responseModel = std::nullopt;
    if (j.contains("responseId")) m.responseId = j.at("responseId").get<std::string>();
    else m.responseId = std::nullopt;
    if (j.contains("diagnostics")) m.diagnostics = j.at("diagnostics").get<std::vector<Diagnostic>>();
    else m.diagnostics = std::nullopt;
    m.usage = j.contains("usage") ? j.at("usage").get<Usage>() : Usage{};
    if (j.contains("stopReason")) m.stopReason = j.at("stopReason").get<StopReason>();
    else m.stopReason = StopReason::Stop;
    if (j.contains("errorMessage")) m.errorMessage = j.at("errorMessage").get<std::string>();
    else m.errorMessage = std::nullopt;
    m.timestamp = j.value("timestamp", std::int64_t{0});
}

struct ToolResultMessage {
    std::string toolCallId;
    std::string toolName;
    std::vector<ContentBlock> content;
    nlohmann::json details;
    bool isError = false;
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(
    ToolResultMessage, toolCallId, toolName, content, details, isError, timestamp)

using Message = std::variant<UserMessage, AssistantMessage, ToolResultMessage>;

void to_json(nlohmann::json& j, const Message& m);
void from_json(const nlohmann::json& j, Message& m);

} // namespace pi::ai

// v0.0.x source-compat aliases. Canonical SDK names live in pi::ai.
namespace pi {
using ai::AssistantMessage;
using ai::ContentBlock;
using ai::Cost;
using ai::Diagnostic;
using ai::DiagnosticError;
using ai::ImageContent;
using ai::Message;
using ai::StopReason;
using ai::TextContent;
using ai::ThinkingContent;
using ai::ToolCall;
using ai::ToolResultMessage;
using ai::Usage;
using ai::UserContent;
using ai::UserMessage;
} // namespace pi
