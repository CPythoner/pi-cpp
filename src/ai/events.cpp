#include <pi/ai/events.hpp>

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

namespace pi::ai {

void to_json(nlohmann::json& j, const AssistantMessageEvent& e) {
    std::visit(Overload{
        [&](const EvStart& v) { j = v; j["type"] = "start"; },
        [&](const EvTextStart& v) { j = v; j["type"] = "text_start"; },
        [&](const EvTextDelta& v) { j = v; j["type"] = "text_delta"; },
        [&](const EvTextEnd& v) { j = v; j["type"] = "text_end"; },
        [&](const EvThinkingStart& v) { j = v; j["type"] = "thinking_start"; },
        [&](const EvThinkingDelta& v) { j = v; j["type"] = "thinking_delta"; },
        [&](const EvThinkingEnd& v) { j = v; j["type"] = "thinking_end"; },
        [&](const EvToolCallStart& v) { j = v; j["type"] = "toolcall_start"; },
        [&](const EvToolCallDelta& v) { j = v; j["type"] = "toolcall_delta"; },
        [&](const EvToolCallEnd& v) { j = v; j["type"] = "toolcall_end"; },
        [&](const EvDone& v) { j = v; j["type"] = "done"; },
        [&](const EvError& v) { j = v; j["type"] = "error"; },
    }, e);
}

void from_json(const nlohmann::json& j, AssistantMessageEvent& e) {
    const auto type = j.at("type").get<std::string>();
    if (type == "start") e = j.get<EvStart>();
    else if (type == "text_start") e = j.get<EvTextStart>();
    else if (type == "text_delta") e = j.get<EvTextDelta>();
    else if (type == "text_end") e = j.get<EvTextEnd>();
    else if (type == "thinking_start") e = j.get<EvThinkingStart>();
    else if (type == "thinking_delta") e = j.get<EvThinkingDelta>();
    else if (type == "thinking_end") e = j.get<EvThinkingEnd>();
    else if (type == "toolcall_start") e = j.get<EvToolCallStart>();
    else if (type == "toolcall_delta") e = j.get<EvToolCallDelta>();
    else if (type == "toolcall_end") e = j.get<EvToolCallEnd>();
    else if (type == "done") e = j.get<EvDone>();
    else if (type == "error") e = j.get<EvError>();
    else throw std::runtime_error("unknown assistant message event type: " + type);
}

} // namespace pi::ai
