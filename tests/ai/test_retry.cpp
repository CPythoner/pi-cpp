#include <doctest/doctest.h>

#include "ai/retry.hpp"

#include <chrono>
#include <map>
#include <string>

namespace detail = pi::ai::detail;
using namespace std::chrono_literals;

TEST_CASE("retry policy matches openai node retryable HTTP statuses") {
    CHECK(detail::isRetryableHttpStatus(408));
    CHECK(detail::isRetryableHttpStatus(409));
    CHECK(detail::isRetryableHttpStatus(429));
    CHECK(detail::isRetryableHttpStatus(500));
    CHECK(detail::isRetryableHttpStatus(599));
    CHECK(detail::isRetryableHttpStatus(600));

    CHECK_FALSE(detail::isRetryableHttpStatus(400));
    CHECK_FALSE(detail::isRetryableHttpStatus(401));
    CHECK_FALSE(detail::isRetryableHttpStatus(403));
    CHECK_FALSE(detail::isRetryableHttpStatus(499));
}

TEST_CASE("x-should-retry explicitly overrides status classification") {
    detail::HttpResponse response;
    response.status = 400;
    response.headers["X-Should-Retry"] = "true";
    CHECK(detail::shouldRetryHttpResponse(response));

    response.status = 503;
    response.headers["X-Should-Retry"] = "false";
    CHECK_FALSE(detail::shouldRetryHttpResponse(response));
}

TEST_CASE("retry-after-ms takes precedence and accepts fractional values") {
    const std::map<std::string, std::string> headers{
        {"Retry-After-Ms", "125.5"},
        {"Retry-After", "2"},
    };
    const auto delay = detail::parseRetryDelay(
        headers,
        detail::RetryHooks::Clock::time_point{});
    REQUIRE(delay.has_value());
    CHECK_EQ(*delay, 125ms);
}

TEST_CASE("zero retry-after-ms falls through to Retry-After like openai node") {
    const std::map<std::string, std::string> headers{
        {"retry-after-ms", "0"},
        {"retry-after", "2.5"},
    };
    const auto delay = detail::parseRetryDelay(
        headers,
        detail::RetryHooks::Clock::time_point{});
    REQUIRE(delay.has_value());
    CHECK_EQ(*delay, 2500ms);
}

TEST_CASE("Retry-After numeric parsing follows parseFloat prefix behavior") {
    const std::map<std::string, std::string> headers{{"rEtRy-AfTeR", " 2 seconds "}};
    const auto delay = detail::parseRetryDelay(
        headers,
        detail::RetryHooks::Clock::time_point{});
    REQUIRE(delay.has_value());
    CHECK_EQ(*delay, 2000ms);
}

TEST_CASE("Retry-After accepts RFC 7231 HTTP date independent of local timezone") {
    const std::map<std::string, std::string> headers{
        {"Retry-After", "Wed, 21 Oct 2015 07:28:00 GMT"},
    };
    const auto now = detail::RetryHooks::Clock::time_point{std::chrono::seconds{1'445'412'450}};
    const auto delay = detail::parseRetryDelay(headers, now);
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

TEST_CASE("retry decision respects attempts and server requested delay") {
    detail::HttpResponse response;
    response.status = 429;
    response.headers["Retry-After"] = "2";

    const auto retry = detail::makeRetryDecision(
        response,
        false,
        0,
        2,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK(retry.retry);
    CHECK_EQ(retry.delay, 2s);

    const auto exhausted = detail::makeRetryDecision(
        response,
        false,
        2,
        2,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(exhausted.retry);
}

TEST_CASE("transport failures retry only before streaming starts") {
    detail::HttpResponse response;
    response.errorKind = detail::HttpErrorKind::Transport;

    const auto beforeStart = detail::makeRetryDecision(
        response,
        false,
        0,
        1,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK(beforeStart.retry);
    CHECK_EQ(beforeStart.delay, 500ms);

    const auto afterStart = detail::makeRetryDecision(
        response,
        true,
        0,
        1,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(afterStart.retry);

    response.errorKind = detail::HttpErrorKind::Cancelled;
    const auto cancelled = detail::makeRetryDecision(
        response,
        false,
        0,
        1,
        detail::RetryHooks::Clock::time_point{},
        0.0);
    CHECK_FALSE(cancelled.retry);
}
