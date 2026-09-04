#pragma once

#include "ai/http.hpp"

#include <pi/ai/cancellation.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace pi::ai::detail {

struct RetryHooks {
    using Clock = std::chrono::system_clock;

    std::function<Clock::time_point()> now;
    std::function<double()> randomUnit;
    // Returns false when cancellation should terminate retry before another HTTP
    // attempt. The default hook mirrors openai-node 6.26.0: the delay itself is
    // not interrupted; cancellation is observed after the delay and before the
    // next request. Injection keeps deterministic tests free of real sleeps.
    std::function<bool(
        std::chrono::milliseconds,
        const std::shared_ptr<CancellationToken>&)> sleep;
};

struct RetryDecision {
    bool retry = false;
    std::chrono::milliseconds delay{0};
};

RetryHooks defaultRetryHooks();

// Mirrors the status classes retried by openai-node 6.26.0, which is the SDK
// pinned by pi v0.80.0 for the OpenAI Completions path.
bool isRetryableHttpStatus(long status) noexcept;

bool shouldRetryHttpResponse(const HttpResponse& response);

// Resolve openai-node's server-requested retry delay precedence:
// retry-after-ms first, then Retry-After (delta seconds or HTTP date).
std::optional<std::chrono::milliseconds> parseRetryDelay(
    const std::map<std::string, std::string>& headers,
    RetryHooks::Clock::time_point now);

std::chrono::milliseconds exponentialBackoffDelay(
    std::size_t retryIndex,
    double randomUnit);

RetryDecision makeRetryDecision(
    const HttpResponse& response,
    bool streamStarted,
    std::size_t retriesAlreadyPerformed,
    std::size_t maxRetries,
    RetryHooks::Clock::time_point now,
    double randomUnit);

} // namespace pi::ai::detail
