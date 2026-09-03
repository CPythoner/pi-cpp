#include <pi/ai/message.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace {

template <class... Ts>
struct Overload : Ts... {
    using Ts::operator()...;
};
template <class... Ts>
Overload(Ts...) -> Overload<Ts...>;

} // namespace

namespace pi::ai {

void to_json(nlohmann::json& j, const ContentBlock& b) {
    std::visit(Overload{
        [&](const TextContent& v) { j = v; j["type"] = "text"; },
        [&](const ThinkingContent& v) { j = v; j["type"] = "thinking"; },
        [&](const ImageContent& v) { j = v; j["type"] = "image"; },
        [&](const ToolCall& v) { j = v; j["type"] = "toolCall"; },
    }, b);
}

void from_json(const nlohmann::json& j, ContentBlock& b) {
    const auto type = j.at("type").get<std::string>();
    if (type == "text") b = j.get<TextContent>();
    else if (type == "thinking") b = j.get<ThinkingContent>();
    else if (type == "image") b = j.get<ImageContent>();
    else if (type == "toolCall") b = j.get<ToolCall>();
    else throw std::runtime_error("unknown content block type: " + type);
}

void to_json(nlohmann::json& j, const Message& m) {
    std::visit(Overload{
        [&](const UserMessage& v) { j = v; j["role"] = "user"; },
        [&](const AssistantMessage& v) { j = v; j["role"] = "assistant"; },
        [&](const ToolResultMessage& v) { j = v; j["role"] = "toolResult"; },
    }, m);
}

void from_json(const nlohmann::json& j, Message& m) {
    const auto role = j.at("role").get<std::string>();
    if (role == "user") m = j.get<UserMessage>();
    else if (role == "assistant") m = j.get<AssistantMessage>();
    else if (role == "toolResult") m = j.get<ToolResultMessage>();
    else throw std::runtime_error("unknown ai message role: " + role);
}

} // namespace pi::ai
