#include "ai/openai_provider.hpp"

#include "ai/openai_completions_decoder.hpp"
#include "ai/openai_request.hpp"

#include <pi/ai/openai_compatible.hpp>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <exception>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>

namespace pi::ai::detail {

namespace {

struct HeaderEntry {
    std::string name;
    std::string value;
};

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

void setHeader(
    std::map<std::string, HeaderEntry>& headers,
    std::string name,
    std::optional<std::string> value) {
    const auto key = lowerAscii(name);
    if (!value) {
        headers.erase(key);
        return;
    }
    headers[key] = HeaderEntry{std::move(name), std::move(*value)};
}

std::map<std::string, std::string> buildHeaders(
    const Model& model,
    const StreamOptions& options) {
    std::map<std::string, HeaderEntry> merged;
    setHeader(merged, "Content-Type", std::string("application/json"));
    setHeader(merged, "Accept", std::string("text/event-stream"));

    if (options.apiKey && !options.apiKey->empty()) {
        setHeader(merged, "Authorization", std::string("Bearer ") + *options.apiKey);
    }

    for (const auto& [name, value] : model.headers) {
        setHeader(merged, name, value);
    }
    for (const auto& [name, value] : options.headers) {
        setHeader(merged, name, value);
    }

    std::map<std::string, std::string> result;
    for (auto& [key, entry] : merged) {
        (void)key;
        result.emplace(std::move(entry.name), std::move(entry.value));
    }
    return result;
}

bool hasAuthenticationHeader(const std::map<std::string, std::string>& headers) {
    for (const auto& [name, value] : headers) {
        const auto key = lowerAscii(name);
        if ((key == "authorization" || key == "cf-aig-authorization") && !value.empty()) {
            return true;
        }
    }
    return false;
}

std::string endpointUrl(std::string baseUrl) {
    while (!baseUrl.empty() && baseUrl.back() == '/') {
        baseUrl.pop_back();
    }
    constexpr std::string_view suffix = "/chat/completions";
    if (baseUrl.size() >= suffix.size() &&
        baseUrl.compare(baseUrl.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return baseUrl;
    }
    return baseUrl + std::string(suffix);
}

std::int64_t nowMilliseconds() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

std::string httpFailureMessage(const HttpResponse& response) {
    std::string message = "HTTP " + std::to_string(response.status);
    if (!response.errorBody.empty()) {
        constexpr std::size_t maxBody = 4096;
        message += ": ";
        message += response.errorBody.substr(0, maxBody);
    }
    return message;
}

void failFromResponse(
    const std::shared_ptr<OpenAIChatCompletionsDecoder>& decoder,
    const HttpResponse& response) {
    const bool successfulResponse = response.status >= 200 && response.status < 300;
    if (successfulResponse) {
        // pi emits start immediately after the OpenAI SDK has a successful HTTP
        // Response, before iterating its body. Preserve that ordering even when
        // the body stream fails before yielding the first SSE chunk.
        decoder->start();
    }

    if (response.errorKind == HttpErrorKind::Cancelled) {
        decoder->fail("Request was aborted", StopReason::Aborted);
        return;
    }
    if (response.errorKind == HttpErrorKind::Transport) {
        decoder->fail(
            response.errorMessage.empty()
                ? "HTTP transport failed"
                : "HTTP transport failed: " + response.errorMessage);
        return;
    }
    if (response.errorKind == HttpErrorKind::ConsumerAborted) {
        decoder->fail("HTTP stream consumer aborted");
        return;
    }
    if (!successfulResponse) {
        decoder->fail(httpFailureMessage(response));
        return;
    }

    // Successful HTTP response with no body chunks: finish produces the
    // protocol error for a missing finish_reason after the already-emitted start.
    decoder->finish();
}

} // namespace

OpenAIProviderCore::OpenAIProviderCore(
    std::shared_ptr<HttpTransport> transport,
    RetryHooks retryHooks)
    : transport_(std::move(transport)),
      retryHooks_(std::move(retryHooks)) {}

AssistantMessageEventStream OpenAIProviderCore::stream(
    const Model& model,
    const Context& context,
    const StreamOptions& options) const {
    auto decoder = std::make_shared<OpenAIChatCompletionsDecoder>(model, nowMilliseconds());
    auto output = decoder->stream();

    if (!transport_) {
        decoder->fail("HTTP transport is not configured");
        return output;
    }

    const auto transport = transport_;
    const auto retryHooks = retryHooks_;
    const auto modelCopy = model;
    const auto contextCopy = context;
    const auto optionsCopy = options;

    try {
        std::thread([
            decoder,
            transport,
            retryHooks,
            modelCopy,
            contextCopy,
            optionsCopy]() mutable {
            try {
                auto headers = buildHeaders(modelCopy, optionsCopy);
                if (!hasAuthenticationHeader(headers)) {
                    decoder->fail("No API key for provider: " + modelCopy.provider);
                    return;
                }
                if (modelCopy.baseUrl.empty()) {
                    decoder->fail("OpenAI-compatible model baseUrl is empty");
                    return;
                }

                HttpRequest request;
                request.url = endpointUrl(modelCopy.baseUrl);
                request.headers = std::move(headers);
                request.body = buildOpenAiChatCompletionsRequest(
                                   modelCopy,
                                   contextCopy,
                                   optionsCopy)
                                   .dump();
                request.timeout = optionsCopy.timeout;
                request.cancellation = optionsCopy.cancellation;
                if (request.timeout && *request.timeout < request.connectTimeout) {
                    request.connectTimeout = *request.timeout;
                }

                // pi v0.80.0 passes maxRetries into openai-node 6.26.0 for this
                // path, but does not apply StreamOptions::maxRetryDelay here.
                const std::size_t maxRetries = optionsCopy.maxRetries.value_or(0);
                std::size_t retriesPerformed = 0;

                for (;;) {
                    const auto response = transport->postStream(
                        request,
                        [decoder](std::string_view chunk) {
                            decoder->feed(chunk);
                            return !decoder->terminal();
                        });

                    if (decoder->terminal()) return;

                    const auto now = retryHooks.now
                        ? retryHooks.now()
                        : RetryHooks::Clock::now();
                    const auto randomUnit = retryHooks.randomUnit
                        ? retryHooks.randomUnit()
                        : 0.0;
                    const auto retry = makeRetryDecision(
                        response,
                        decoder->started(),
                        retriesPerformed,
                        maxRetries,
                        now,
                        randomUnit);

                    if (retry.retry) {
                        const bool waited = retryHooks.sleep
                            ? retryHooks.sleep(retry.delay, optionsCopy.cancellation)
                            : !optionsCopy.cancellation ||
                                  !optionsCopy.cancellation->wait_for(retry.delay);
                        if (!waited) {
                            decoder->fail("Request was aborted", StopReason::Aborted);
                            return;
                        }
                        ++retriesPerformed;
                        continue;
                    }

                    failFromResponse(decoder, response);
                    return;
                }
            } catch (const std::exception& error) {
                decoder->fail(error.what());
            } catch (...) {
                decoder->fail("Unknown OpenAI-compatible provider failure");
            }
        }).detach();
    } catch (const std::exception& error) {
        decoder->fail(error.what());
    }

    return output;
}

} // namespace pi::ai::detail

namespace pi::ai {

struct OpenAICompatibleProvider::Impl {
    detail::OpenAIProviderCore core{detail::makeCprHttpTransport()};
};

OpenAICompatibleProvider::OpenAICompatibleProvider()
    : impl_(std::make_shared<Impl>()) {}

OpenAICompatibleProvider::~OpenAICompatibleProvider() = default;

AssistantMessageEventStream OpenAICompatibleProvider::stream(
    const Model& model,
    const Context& context,
    const StreamOptions& options) {
    return impl_->core.stream(model, context, options);
}

} // namespace pi::ai
