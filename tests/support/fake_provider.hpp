#pragma once

#include <pi/ai/provider.hpp>

#include <cstddef>
#include <utility>
#include <vector>

namespace pi::test_support {

class FakeProvider final : public ai::Provider {
public:
    explicit FakeProvider(std::vector<ai::AssistantMessageEvent> events)
        : events_(std::move(events)) {}

    ai::AssistantMessageEventStream stream(
        const ai::Model& model,
        const ai::Context& context,
        const ai::StreamOptions& options) override {
        ++callCount_;
        lastModel_ = model;
        lastContext_ = context;
        lastOptions_ = options;

        auto output = ai::createAssistantMessageEventStream();
        for (const auto& event : events_) {
            output.push(event);
        }
        return output;
    }

    std::size_t callCount() const noexcept { return callCount_; }
    const ai::Model& lastModel() const noexcept { return lastModel_; }
    const ai::Context& lastContext() const noexcept { return lastContext_; }
    const ai::StreamOptions& lastOptions() const noexcept { return lastOptions_; }

private:
    std::vector<ai::AssistantMessageEvent> events_;
    std::size_t callCount_ = 0;
    ai::Model lastModel_;
    ai::Context lastContext_;
    ai::StreamOptions lastOptions_;
};

} // namespace pi::test_support
