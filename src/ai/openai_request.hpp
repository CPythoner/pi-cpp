#pragma once

#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

namespace pi::ai::detail {

// Build the stable OpenAI Chat Completions request subset used by v0.0.2.
// Provider-specific compatibility knobs remain private and can be layered on
// top without changing the public pi::ai SDK surface.
nlohmann::json buildOpenAiChatCompletionsRequest(
    const Model& model,
    const Context& context,
    const StreamOptions& options);

} // namespace pi::ai::detail
