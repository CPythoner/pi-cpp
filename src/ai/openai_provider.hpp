#pragma once

#include "ai/http.hpp"

#include <pi/ai/provider.hpp>

#include <memory>

namespace pi::ai::detail {

class OpenAIProviderCore {
public:
    explicit OpenAIProviderCore(std::shared_ptr<HttpTransport> transport);

    AssistantMessageEventStream stream(
        const Model& model,
        const Context& context,
        const StreamOptions& options) const;

private:
    std::shared_ptr<HttpTransport> transport_;
};

} // namespace pi::ai::detail
