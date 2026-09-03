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

} // namespace pi
