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

} // namespace pi
