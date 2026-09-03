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
    // Returns false when the wait was cancelled and the request should abort.
    std::function<bool(
        std::chrono::milliseconds,
        const std::shared_ptr<CancellationToken>&)> sleep;
};

struct RetryDecision {
    bool retry = false;
    std::chrono::milliseconds delay{0};
    std::optional<std::string> rejectionMessage;
};

RetryHooks defaultRetryHooks();

bool isRetryableHttpStatus(long status) noexcept;

std::optional<std::chrono::milliseconds> parseRetryAfter(
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
    std::chrono::milliseconds maxRetryDelay,
    RetryHooks::Clock::time_point now,
    double randomUnit);

} // namespace pi::ai::detail
