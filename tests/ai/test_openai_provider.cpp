#include <doctest/doctest.h>

#include "ai/http.hpp"
#include "ai/openai_provider.hpp"

#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace ai = pi::ai;
namespace detail = pi::ai::detail;

namespace {

class FakeHttpTransport final : public detail::HttpTransport {
public:
    detail::HttpResponse response;
    std::vector<std::string> chunks;

    detail::HttpResponse postStream(
        const detail::HttpRequest& request,
        detail::HttpChunkHandler onChunk) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastRequest_ = request;
        }
        calls_.fetch_add(1);

        auto result = response;
        for (const auto& chunk : chunks) {
            if (onChunk && !onChunk(chunk)) {
                if (result.errorKind == detail::HttpErrorKind::None) {
                    result.errorKind = detail::HttpErrorKind::ConsumerAborted;
                }
                break;
            }
        }
        return result;
    }

    int calls() const noexcept { return calls_.load(); }

    detail::HttpRequest lastRequest() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastRequest_;
    }

private:
    mutable std::mutex mutex_;
    std::atomic<int> calls_{0};
    detail::HttpRequest lastRequest_;
};

ai::Model makeModel() {
    ai::Model model;
    model.id = "gpt-test";
    model.name = "GPT Test";
    model.api = "openai-completions";
    model.provider = "openai";
    model.baseUrl = "https://example.invalid/v1/";
    return model;
}

ai::Context makeContext() {
    ai::Context context;
    context.systemPrompt = "Be concise.";
    ai::UserMessage user;
    user.content = std::string("hello");
    context.messages.emplace_back(std::move(user));
    return context;
}

ai::StreamOptions authenticatedOptions() {
    ai::StreamOptions options;
    options.apiKey = "secret";
    return options;
}

std::vector<ai::AssistantMessageEvent> drain(ai::AssistantMessageEventStream stream) {
    std::vector<ai::AssistantMessageEvent> events;
    while (auto event = stream.next()) {
        events.push_back(std::move(*event));
    }
    return events;
}

std::string eventType(const ai::AssistantMessageEvent& event) {
    if (std::holds_alternative<ai::EvStart>(event)) return "start";
    if (std::holds_alternative<ai::EvTextStart>(event)) return "text_start";
    if (std::holds_alternative<ai::EvTextDelta>(event)) return "text_delta";
    if (std::holds_alternative<ai::EvTextEnd>(event)) return "text_end";
    if (std::holds_alternative<ai::EvDone>(event)) return "done";
    if (std::holds_alternative<ai::EvError>(event)) return "error";
    return "other";
}

} // namespace

TEST_CASE("OpenAI provider core streams a successful fake HTTP response") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 200;
    transport->chunks = {
        "data: {\"id\":\"resp-1\",\"choices\":[{\"delta\":{\"content\":\"hello\"},\"finish_reason\":\"stop\"}]}\n\n",
        "data: [DONE]\n\n",
    };

    detail::OpenAIProviderCore provider(transport);
    auto stream = provider.stream(makeModel(), makeContext(), authenticatedOptions());

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    CHECK_EQ(std::get<ai::TextContent>(result.content[0]).text, "hello");
    CHECK_EQ(result.stopReason, ai::StopReason::Stop);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 5);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "text_start");
    CHECK_EQ(eventType(events[2]), "text_delta");
    CHECK_EQ(eventType(events[3]), "text_end");
    CHECK_EQ(eventType(events[4]), "done");

    CHECK_EQ(transport->calls(), 1);
    const auto request = transport->lastRequest();
    CHECK_EQ(request.url, "https://example.invalid/v1/chat/completions");
    CHECK_EQ(request.headers.at("Authorization"), "Bearer secret");
    CHECK_EQ(request.headers.at("Accept"), "text/event-stream");
    CHECK_EQ(request.headers.at("Content-Type"), "application/json");

    const auto body = nlohmann::json::parse(request.body);
    CHECK_EQ(body.at("model"), "gpt-test");
    CHECK_EQ(body.at("stream"), true);
    CHECK_EQ(body.at("messages")[0].at("role"), "system");
    CHECK_EQ(body.at("messages")[1].at("role"), "user");
}

TEST_CASE("OpenAI provider core reports missing authentication before HTTP") {
    auto transport = std::make_shared<FakeHttpTransport>();
    detail::OpenAIProviderCore provider(transport);

    auto stream = provider.stream(makeModel(), makeContext(), ai::StreamOptions{});
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "No API key for provider: openai");
    CHECK_EQ(transport->calls(), 0);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(eventType(events[0]), "error");
}

TEST_CASE("OpenAI provider core accepts caller authorization header without api key") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 200;
    transport->chunks = {
        "data: {\"choices\":[{\"delta\":{},\"finish_reason\":\"stop\"}]}\n\n",
        "data: [DONE]\n\n",
    };

    ai::StreamOptions options;
    options.headers["authorization"] = "Bearer caller-token";

    detail::OpenAIProviderCore provider(transport);
    auto stream = provider.stream(makeModel(), makeContext(), options);
    CHECK_EQ(stream.result().stopReason, ai::StopReason::Stop);

    const auto request = transport->lastRequest();
    CHECK_EQ(request.headers.at("authorization"), "Bearer caller-token");
}

TEST_CASE("OpenAI provider core maps HTTP auth failure without start event") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 401;
    transport->response.errorBody = R"({"error":{"message":"bad key"}})";

    detail::OpenAIProviderCore provider(transport);
    auto stream = provider.stream(makeModel(), makeContext(), authenticatedOptions());
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK(result.errorMessage->find("HTTP 401") != std::string::npos);
    CHECK(result.errorMessage->find("bad key") != std::string::npos);

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(eventType(events[0]), "error");
}

TEST_CASE("OpenAI provider core maps transport cancellation to aborted") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.errorKind = detail::HttpErrorKind::Cancelled;
    transport->response.errorMessage = "cancelled";

    detail::OpenAIProviderCore provider(transport);
    auto stream = provider.stream(makeModel(), makeContext(), authenticatedOptions());
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Aborted);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "Request was aborted");

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(eventType(events[0]), "error");
    CHECK_EQ(std::get<ai::EvError>(events[0]).reason, ai::StopReason::Aborted);
}

TEST_CASE("successful HTTP response with empty stream emits start then protocol error") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 200;

    detail::OpenAIProviderCore provider(transport);
    auto stream = provider.stream(makeModel(), makeContext(), authenticatedOptions());
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "Stream ended without finish_reason");

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "error");
}
