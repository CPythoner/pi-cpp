#include <doctest/doctest.h>

#include "ai/http.hpp"
#include "ai/openai_provider.hpp"
#include "ai/retry.hpp"

#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/ai/provider.hpp>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
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
    std::vector<detail::HttpResponse> responses;
    std::vector<std::vector<std::string>> chunkBatches;

    detail::HttpResponse postStream(
        const detail::HttpRequest& request,
        detail::HttpChunkHandler onChunk) override {
        const auto callIndex = static_cast<std::size_t>(calls_.fetch_add(1));
        {
            std::lock_guard<std::mutex> lock(mutex_);
            lastRequest_ = request;
        }

        auto result = response;
        if (!responses.empty()) {
            result = responses[std::min(callIndex, responses.size() - 1)];
        }

        const std::vector<std::string>* selectedChunks = &chunks;
        if (!chunkBatches.empty()) {
            selectedChunks = &chunkBatches[std::min(callIndex, chunkBatches.size() - 1)];
        }

        for (const auto& chunk : *selectedChunks) {
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

struct RetryProbe {
    std::mutex mutex;
    std::vector<std::chrono::milliseconds> delays;
    double randomUnit = 0.0;
    bool allowSleep = true;
};

detail::RetryHooks retryHooks(const std::shared_ptr<RetryProbe>& probe) {
    detail::RetryHooks hooks;
    hooks.now = [] {
        return detail::RetryHooks::Clock::time_point{std::chrono::seconds{1'700'000'000}};
    };
    hooks.randomUnit = [probe] { return probe->randomUnit; };
    hooks.sleep = [probe](
                      std::chrono::milliseconds delay,
                      const std::shared_ptr<ai::CancellationToken>&) {
        std::lock_guard<std::mutex> lock(probe->mutex);
        probe->delays.push_back(delay);
        return probe->allowSleep;
    };
    return hooks;
}

std::vector<std::chrono::milliseconds> retryDelays(const std::shared_ptr<RetryProbe>& probe) {
    std::lock_guard<std::mutex> lock(probe->mutex);
    return probe->delays;
}

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

const std::vector<std::string> successfulChunks{
    "data: {\"id\":\"resp-1\",\"choices\":[{\"delta\":{\"content\":\"hello\"},\"finish_reason\":\"stop\"}]}\n\n",
    "data: [DONE]\n\n",
};

} // namespace

TEST_CASE("OpenAI provider core streams a successful fake HTTP response") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 200;
    transport->chunks = successfulChunks;

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

TEST_CASE("OpenAI provider core maps HTTP auth failure without retry or start event") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 401;
    transport->response.errorBody = R"({"error":{"message":"bad key"}})";

    auto options = authenticatedOptions();
    options.maxRetries = 3;

    detail::OpenAIProviderCore provider(transport);
    auto stream = provider.stream(makeModel(), makeContext(), options);
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK(result.errorMessage->find("HTTP 401") != std::string::npos);
    CHECK(result.errorMessage->find("bad key") != std::string::npos);
    CHECK_EQ(transport->calls(), 1);

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

TEST_CASE("OpenAI provider retries 429 before stream start and honors Retry-After") {
    auto transport = std::make_shared<FakeHttpTransport>();

    detail::HttpResponse rateLimited;
    rateLimited.status = 429;
    rateLimited.headers["Retry-After"] = "2";
    detail::HttpResponse success;
    success.status = 200;
    transport->responses = {rateLimited, success};
    transport->chunkBatches = {{}, successfulChunks};

    auto options = authenticatedOptions();
    options.maxRetries = 1;
    auto probe = std::make_shared<RetryProbe>();
    detail::OpenAIProviderCore provider(transport, retryHooks(probe));

    auto stream = provider.stream(makeModel(), makeContext(), options);
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Stop);
    CHECK_EQ(transport->calls(), 2);
    const auto delays = retryDelays(probe);
    REQUIRE_EQ(delays.size(), 1);
    CHECK_EQ(delays[0], std::chrono::milliseconds{2000});

    const auto events = drain(stream);
    REQUIRE_EQ(events.size(), 5);
    CHECK_EQ(eventType(events.front()), "start");
    CHECK_EQ(eventType(events.back()), "done");
}

TEST_CASE("OpenAI provider retries transport failure with deterministic backoff") {
    auto transport = std::make_shared<FakeHttpTransport>();

    detail::HttpResponse transportError;
    transportError.errorKind = detail::HttpErrorKind::Transport;
    transportError.errorMessage = "connection reset";
    detail::HttpResponse success;
    success.status = 200;
    transport->responses = {transportError, success};
    transport->chunkBatches = {{}, successfulChunks};

    auto options = authenticatedOptions();
    options.maxRetries = 1;
    auto probe = std::make_shared<RetryProbe>();
    probe->randomUnit = 0.0;
    detail::OpenAIProviderCore provider(transport, retryHooks(probe));

    auto stream = provider.stream(makeModel(), makeContext(), options);
    CHECK_EQ(stream.result().stopReason, ai::StopReason::Stop);
    CHECK_EQ(transport->calls(), 2);

    const auto delays = retryDelays(probe);
    REQUIRE_EQ(delays.size(), 1);
    CHECK_EQ(delays[0], std::chrono::milliseconds{500});
}

TEST_CASE("OpenAI provider never retries after any SSE stream data was emitted") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.errorKind = detail::HttpErrorKind::Transport;
    transport->response.errorMessage = "connection reset";
    transport->chunks = {
        "data: {\"choices\":[{\"delta\":{\"content\":\"partial\"},\"finish_reason\":null}]}\n\n",
    };

    auto options = authenticatedOptions();
    options.maxRetries = 3;
    auto probe = std::make_shared<RetryProbe>();
    detail::OpenAIProviderCore provider(transport, retryHooks(probe));

    auto stream = provider.stream(makeModel(), makeContext(), options);
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK(result.errorMessage->find("connection reset") != std::string::npos);
    CHECK_EQ(transport->calls(), 1);
    CHECK(retryDelays(probe).empty());

    const auto events = drain(stream);
    REQUIRE(events.size() >= 4);
    CHECK_EQ(eventType(events[0]), "start");
    CHECK_EQ(eventType(events[1]), "text_start");
    CHECK_EQ(eventType(events[2]), "text_delta");
    CHECK_EQ(eventType(events.back()), "error");
}

TEST_CASE("OpenAI provider ignores maxRetryDelay like pi v0.80 openai path") {
    auto transport = std::make_shared<FakeHttpTransport>();

    detail::HttpResponse rateLimited;
    rateLimited.status = 429;
    rateLimited.headers["Retry-After"] = "120";
    detail::HttpResponse success;
    success.status = 200;
    transport->responses = {rateLimited, success};
    transport->chunkBatches = {{}, successfulChunks};

    auto options = authenticatedOptions();
    options.maxRetries = 1;
    options.maxRetryDelay = std::chrono::milliseconds{1};
    auto probe = std::make_shared<RetryProbe>();
    detail::OpenAIProviderCore provider(transport, retryHooks(probe));

    auto stream = provider.stream(makeModel(), makeContext(), options);
    CHECK_EQ(stream.result().stopReason, ai::StopReason::Stop);
    CHECK_EQ(transport->calls(), 2);

    const auto delays = retryDelays(probe);
    REQUIRE_EQ(delays.size(), 1);
    CHECK_EQ(delays[0], std::chrono::milliseconds{120000});
}

TEST_CASE("x-should-retry false suppresses retry for server error") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 503;
    transport->response.headers["x-should-retry"] = "false";

    auto options = authenticatedOptions();
    options.maxRetries = 3;
    auto probe = std::make_shared<RetryProbe>();
    detail::OpenAIProviderCore provider(transport, retryHooks(probe));

    auto stream = provider.stream(makeModel(), makeContext(), options);
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    CHECK_EQ(transport->calls(), 1);
    CHECK(retryDelays(probe).empty());
}

TEST_CASE("OpenAI provider cancellation during retry backoff aborts before next attempt") {
    auto transport = std::make_shared<FakeHttpTransport>();
    transport->response.status = 503;

    auto options = authenticatedOptions();
    options.maxRetries = 2;
    auto probe = std::make_shared<RetryProbe>();
    probe->allowSleep = false;
    detail::OpenAIProviderCore provider(transport, retryHooks(probe));

    auto stream = provider.stream(makeModel(), makeContext(), options);
    const auto result = stream.result();

    CHECK_EQ(result.stopReason, ai::StopReason::Aborted);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "Request was aborted");
    CHECK_EQ(transport->calls(), 1);
    REQUIRE_EQ(retryDelays(probe).size(), 1);
}
