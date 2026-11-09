#pragma once

#include <pi/ai/provider.hpp>

#include <memory>

namespace pi::ai {

class OpenAICompatibleProvider final : public Provider {
public:
    OpenAICompatibleProvider();
    ~OpenAICompatibleProvider() override;

    AssistantMessageEventStream stream(
        const Model& model,
        const Context& context,
        const StreamOptions& options) override;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

} // namespace pi::ai
