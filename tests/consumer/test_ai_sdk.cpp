#include <doctest/doctest.h>

#include <pi/ai/cancellation.hpp>
#include <pi/ai/event_stream.hpp>
#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/ai/openai_compatible.hpp>
#include <pi/ai/provider.hpp>

#include <string>

namespace {

class ConsumerProvider final : public pi::ai::Provider {
public:
    pi::ai::AssistantMessageEventStream stream(
        const pi::ai::Model& model,
        const pi::ai::Context&,
        const pi::ai::StreamOptions&) override {
        pi::ai::AssistantMessage message;
        message.api = model.api;
        message.provider = model.provider;
        message.model = model.id;
        message.stopReason = pi::ai::StopReason::Stop;

        auto output = pi::ai::createAssistantMessageEventStream();
        output.push(pi::ai::EvDone{pi::ai::StopReason::Stop, message});
        return output;
    }
};

} // namespace

TEST_CASE("ai sdk is independently consumable") {
    pi::ai::UserMessage user;
    user.content = std::string("hello");

    pi::ai::Message message = user;
    nlohmann::json j = message;
    CHECK(j.at("role") == "user");

    pi::ai::CancellationToken token;
    CHECK_FALSE(token.requested());

    auto stream = pi::ai::createAssistantMessageEventStream();
    CHECK_FALSE(stream.done());

    pi::ai::Model model;
    model.id = "consumer-model";
    model.name = "Consumer Model";
    model.api = "openai-completions";
    model.provider = "consumer";
    model.baseUrl = "https://example.invalid/v1";

    pi::ai::Context context;
    context.messages.push_back(message);

    pi::ai::StreamOptions options;
    options.cancellation = std::make_shared<pi::ai::CancellationToken>();

    ConsumerProvider provider;
    auto providerStream = provider.stream(model, context, options);
    const auto terminal = providerStream.next();
    REQUIRE(terminal.has_value());
    CHECK(std::holds_alternative<pi::ai::EvDone>(*terminal));
    CHECK_EQ(providerStream.result().model, "consumer-model");

    pi::ai::OpenAICompatibleProvider openaiProvider;
    pi::ai::Provider* publicProviderApi = &openaiProvider;
    CHECK(publicProviderApi != nullptr);
}
