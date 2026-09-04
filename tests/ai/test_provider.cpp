#include <doctest/doctest.h>

#include <pi/ai/provider.hpp>

#include "../support/fake_provider.hpp"

#include <memory>
#include <string>
#include <vector>

namespace ai = pi::ai;

namespace {

ai::AssistantMessage makeAssistant(
    const ai::Model& model,
    std::string text,
    ai::StopReason reason = ai::StopReason::Stop) {
    ai::AssistantMessage message;
    message.content.push_back(ai::TextContent{std::move(text), std::nullopt});
    message.api = model.api;
    message.provider = model.provider;
    message.model = model.id;
    message.stopReason = reason;
    return message;
}

ai::Model makeModel() {
    ai::Model model;
    model.id = "test-model";
    model.name = "Test Model";
    model.api = "openai-completions";
    model.provider = "test-provider";
    model.baseUrl = "https://example.invalid/v1";
    model.reasoning = true;
    model.input = {ai::ModelInput::Text, ai::ModelInput::Image};
    model.contextWindow = 128000;
    model.maxTokens = 8192;
    model.headers["x-model-header"] = "model-value";
    return model;
}

} // namespace

TEST_CASE("Provider exposes only pi::ai model/context/options and returns L1 stream") {
    const auto model = makeModel();
    const auto finalMessage = makeAssistant(model, "hello");

    pi::test_support::FakeProvider fake({
        ai::AssistantMessageEvent(ai::EvStart{makeAssistant(model, "")}),
        ai::AssistantMessageEvent(ai::EvDone{ai::StopReason::Stop, finalMessage}),
    });

    ai::Context context;
    ai::UserMessage user;
    user.content = std::string("hi");
    context.messages.emplace_back(user);
    context.systemPrompt = "You are a test provider.";
    context.tools.push_back(ai::ToolDefinition{
        "echo",
        "Echo text",
        nlohmann::json{{"type", "object"}},
    });

    ai::StreamOptions options;
    options.temperature = 0.2;
    options.maxTokens = 100;
    options.apiKey = "test-key";
    options.cancellation = std::make_shared<ai::CancellationToken>();
    options.headers["authorization"] = std::string("Bearer test");
    options.headers["x-suppress-me"] = std::nullopt;
    options.maxRetries = 2;

    ai::Provider& provider = fake;
    auto stream = provider.stream(model, context, options);

    CHECK_EQ(fake.callCount(), 1);
    CHECK_EQ(fake.lastModel().id, "test-model");
    CHECK_EQ(fake.lastContext().messages.size(), 1);
    CHECK_EQ(fake.lastContext().tools.size(), 1);
    REQUIRE(fake.lastOptions().cancellation != nullptr);
    CHECK_FALSE(fake.lastOptions().cancellation->requested());
    REQUIRE(fake.lastOptions().headers.at("authorization").has_value());
    CHECK_EQ(*fake.lastOptions().headers.at("authorization"), "Bearer test");
    CHECK_FALSE(fake.lastOptions().headers.at("x-suppress-me").has_value());

    const auto first = stream.next();
    REQUIRE(first.has_value());
    CHECK(std::holds_alternative<ai::EvStart>(*first));

    const auto terminal = stream.next();
    REQUIRE(terminal.has_value());
    CHECK(std::holds_alternative<ai::EvDone>(*terminal));
    CHECK_FALSE(stream.next().has_value());

    const auto result = stream.result();
    CHECK_EQ(result.api, "openai-completions");
    CHECK_EQ(result.provider, "test-provider");
    CHECK_EQ(result.model, "test-model");
    REQUIRE_EQ(result.content.size(), 1);
    CHECK_EQ(std::get<ai::TextContent>(result.content[0]).text, "hello");
}

TEST_CASE("Provider failures are representable as terminal EvError instead of exceptions") {
    const auto model = makeModel();
    auto errorMessage = makeAssistant(model, "", ai::StopReason::Error);
    errorMessage.errorMessage = "synthetic provider failure";

    pi::test_support::FakeProvider fake({
        ai::AssistantMessageEvent(ai::EvError{ai::StopReason::Error, errorMessage}),
    });

    ai::Context context;
    ai::StreamOptions options;

    auto stream = fake.stream(model, context, options);

    const auto event = stream.next();
    REQUIRE(event.has_value());
    REQUIRE(std::holds_alternative<ai::EvError>(*event));

    const auto result = stream.result();
    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "synthetic provider failure");
}
