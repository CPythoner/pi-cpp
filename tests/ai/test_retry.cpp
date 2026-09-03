#include <doctest/doctest.h>

#include "ai/retry.hpp"

#include <chrono>
#include <map>
#include <string>

namespace detail = pi::ai::detail;
using namespace std::chrono_literals;

TEST_CASE("retry policy classifies only rate limit and server HTTP errors") {
    CHECK(detail::isRetryableHttpStatus(429));
    CHECK(detail::isRetryableHttpStatus(500));
    CHECK(detail::isRetryableHttpStatus(503));
    CHECK(detail::isRetryableHttpStatus(599));

    CHECK_FALSE(detail::isRetryableHttpStatus(400));
    CHECK_FALSE(detail::isRetryableHttpStatus(401));
    CHECK_FALSE(detail::isRetryableHttpStatus(403));
    CHECK_FALSE(detail::isRetryableHttpStatus(499));
    CHECK_FALSE(detail::isRetryableHttpStatus(600));
}

TEST_CASE("Retry-After accepts delta seconds case insensitively") {
    const std::map<std::string, std::string> headers{{"rEtRy-AfTeR", " 2 "}};
    const auto delay = detail::parseRetryAfter(headers, detail::RetryHooks::Clock::time_point{});
    REQUIRE(delay.has_value());
    CHECK_EQ(*delay, 2000ms);
}

TEST_CASE("Retry-After accepts RFC 7231 HTTP date independent of local timezone") {
    const std::map<std::string, std::string> headers{
        {"Retry-After", "Wed, 21 Oct 2015 07:28:00 GMT"},
    };
    const auto now = detail::RetryHooks::Clock::time_point{std::chrono::seconds{1'445'412'450}};
    const auto delay = detail::parseRetryAfter(headers, now);
    REQUIRE(delay.has_value());
    CHECK_EQ(*delay, 30s);
}

TEST_CASE("exponential retry backoff is capped and jitter is bounded") {
    CHECK_EQ(detail::exponentialBackoffDelay(0, 0.0), 500ms);
    CHECK_EQ(detail::exponentialBackoffDelay(0, 1.0), 375ms);
    CHECK_EQ(detail::exponentialBackoffDelay(1, 0.0), 1000ms);
    CHECK_EQ(detail::exponentialBackoffDelay(4, 0.0), 8000ms);
    CHECK_EQ(detail::exponentialBackoffDelay(20, 0.0), 8000ms);
}

TEST_CASE("retry decision respects attempts Retry-After and maximum delay") {
    detail::HttpResponse response;
    response.status = 429;
    response.headers["Retry-After"] = "2";

    const auto retry = detail::makeRetryDecision(
        response,
        false,
        0,
        2,
        60s,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK(retry.retry);
    CHECK_EQ(retry.delay, 2s);
    CHECK_FALSE(retry.rejectionMessage.has_value());

    const auto exhausted = detail::makeRetryDecision(
        response,
        false,
        2,
        2,
        60s,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(exhausted.retry);

    const auto tooLong = detail::makeRetryDecision(
        response,
        false,
        0,
        2,
        1s,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(tooLong.retry);
    REQUIRE(tooLong.rejectionMessage.has_value());
    CHECK(tooLong.rejectionMessage->find("exceeds maximum") != std::string::npos);

    const auto uncapped = detail::makeRetryDecision(
        response,
        false,
        0,
        2,
        0ms,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK(uncapped.retry);
    CHECK_EQ(uncapped.delay, 2s);
}

TEST_CASE("transport failures retry only before streaming starts") {
    detail::HttpResponse response;
    response.errorKind = detail::HttpErrorKind::Transport;

    const auto beforeStart = detail::makeRetryDecision(
        response,
        false,
        0,
        1,
        60s,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK(beforeStart.retry);
    CHECK_EQ(beforeStart.delay, 500ms);

    const auto afterStart = detail::makeRetryDecision(
        response,
        true,
        0,
        1,
        60s,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(afterStart.retry);

    response.errorKind = detail::HttpErrorKind::Cancelled;
    const auto cancelled = detail::makeRetryDecision(
        response,
        false,
        0,
        1,
        60s,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(cancelled.retry);
}
