#pragma once
#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace pi {

// ---- 基础枚举与值对象 ----
enum class StopReason { Stop, Length, ToolUse, Error, Aborted };
NLOHMANN_JSON_SERIALIZE_ENUM(StopReason, {
    {StopReason::Stop, "stop"}, {StopReason::Length, "length"},
    {StopReason::ToolUse, "toolUse"}, {StopReason::Error, "error"},
    {StopReason::Aborted, "aborted"},
})

struct Cost {                       // $ 计价
    double input = 0, output = 0, cacheRead = 0, cacheWrite = 0, total = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(Cost, input, output, cacheRead, cacheWrite, total)

struct Usage {
    std::int64_t input = 0, output = 0, cacheRead = 0, cacheWrite = 0;
    std::optional<std::int64_t> cacheWrite1h;   // Anthropic 1h 保留
    std::int64_t totalTokens = 0;
    Cost cost;
};
// 手写（nlohmann 3.11.3 不支持 std::optional；nullopt 序列化为缺省字段，对齐 pi/tau wire）
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

struct DiagnosticError {            // 对应 pi AssistantMessageDiagnostic.error
    std::optional<std::string> name, stack, code;
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
    nlohmann::json details;         // 自由结构
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

// ---- 内容块（wire 判别字段 "type"）----
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
    std::string data;               // base64
    std::string mimeType;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ImageContent, data, mimeType)

struct ToolCall {
    std::string id, name;
    nlohmann::json arguments;       // 已解析对象（流式期间由 parseStreamingJson 增量填充）
    std::optional<std::string> thoughtSignature;   // Google 思路签名
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

// JSON（message.cpp 实现；variant 按 type 判别分发，未知 type 抛 std::runtime_error）
void to_json(nlohmann::json& j, const ContentBlock& b);
void from_json(const nlohmann::json& j, ContentBlock& b);

// ---- 消息（wire 判别字段 "role"）----
// user 与 custom 的 content 允许「字符串 或 块数组」两种形态
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
    std::string api, provider, model;
    std::optional<std::string> responseModel, responseId;
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
    std::string toolCallId, toolName;
    std::vector<ContentBlock> content;   // 实际仅 Text|Image
    nlohmann::json details;              // UI/日志用，不进 LLM
    bool isError = false;
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(ToolResultMessage,
    toolCallId, toolName, content, details, isError, timestamp)

struct BashExecutionMessage {          // 应用消息：用户 ! 命令记录，不进 LLM 上下文
    std::string command, output;
    std::int64_t exitCode = 0;
    bool cancelled = false, truncated = false;
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

struct CustomMessage {                 // 应用消息：扩展注入，可选择进 LLM
    std::string customType;
    UserContent content;
    nlohmann::json details;
    bool display = true;               // TUI 显隐
    std::int64_t timestamp = 0;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CustomMessage,
    customType, content, details, display, timestamp)

struct BranchSummaryMessage   { std::string summary; std::int64_t timestamp = 0; };
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(BranchSummaryMessage, summary, timestamp)

struct CompactionSummaryMessage { std::string summary; std::int64_t timestamp = 0; };
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CompactionSummaryMessage, summary, timestamp)

using AgentMessage = std::variant<
    UserMessage, AssistantMessage, ToolResultMessage, BashExecutionMessage,
    CustomMessage, BranchSummaryMessage, CompactionSummaryMessage>;

// JSON（message.cpp 实现；variant 按 role 判别分发，未知 role 抛 std::runtime_error）
void to_json(nlohmann::json& j, const AgentMessage& m);
void from_json(const nlohmann::json& j, AgentMessage& m);

} // namespace pi
