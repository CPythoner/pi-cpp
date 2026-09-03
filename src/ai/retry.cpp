#include "ai/retry.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>

namespace pi::ai::detail {

namespace {

std::string lowerAscii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        if (ch >= 'A' && ch <= 'Z') return static_cast<char>(ch - 'A' + 'a');
        return static_cast<char>(ch);
    });
    return value;
}

std::string_view trimAscii(std::string_view value) {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

std::optional<std::string_view> headerValue(
    const std::map<std::string, std::string>& headers,
    std::string_view wanted) {
    const auto expected = lowerAscii(std::string(wanted));
    for (const auto& [name, value] : headers) {
        if (lowerAscii(name) == expected) return value;
    }
    return std::nullopt;
}

std::optional<unsigned> monthNumber(std::string_view month) {
    static constexpr std::string_view months[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
    };
    for (unsigned index = 0; index < 12; ++index) {
        if (month == months[index]) return index + 1;
    }
    return std::nullopt;
}

std::int64_t daysFromCivil(int year, unsigned month, unsigned day) noexcept {
    year -= month <= 2 ? 1 : 0;
    const int era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yearOfEra = static_cast<unsigned>(year - era * 400);
    const unsigned dayOfYear =
        (153 * (month + (month > 2 ? static_cast<unsigned>(-3) : 9)) + 2) / 5 + day - 1;
    const unsigned dayOfEra =
        yearOfEra * 365 + yearOfEra / 4 - yearOfEra / 100 + dayOfYear;
    return static_cast<std::int64_t>(era) * 146097 +
           static_cast<std::int64_t>(dayOfEra) - 719468;
}

std::optional<RetryHooks::Clock::time_point> parseHttpDate(std::string_view value) {
    std::istringstream stream{std::string(value)};
    std::string weekday;
    int day = 0;
    std::string monthText;
    int year = 0;
    std::string timeText;
    std::string zone;
    if (!(stream >> weekday >> day >> monthText >> year >> timeText >> zone)) {
        return std::nullopt;
    }
    if (weekday.empty() || weekday.back() != ',' || zone != "GMT") return std::nullopt;
    const auto month = monthNumber(monthText);
    if (!month || day < 1 || day > 31 || year < 1970) return std::nullopt;
    if (timeText.size() != 8 || timeText[2] != ':' || timeText[5] != ':') return std::nullopt;

    auto parseTwoDigits = [](char first, char second) -> std::optional<int> {
        if (first < '0' || first > '9' || second < '0' || second > '9') return std::nullopt;
        return (first - '0') * 10 + (second - '0');
    };
    const auto hour = parseTwoDigits(timeText[0], timeText[1]);
    const auto minute = parseTwoDigits(timeText[3], timeText[4]);
    const auto second = parseTwoDigits(timeText[6], timeText[7]);
    if (!hour || !minute || !second || *hour > 23 || *minute > 59 || *second > 60) {
        return std::nullopt;
    }

    // Convert the RFC 7231 IMF-fixdate directly to Unix seconds so this remains
    // independent of the process locale and timezone on all three platforms.
    const auto days = daysFromCivil(year, *month, static_cast<unsigned>(day));
    const auto seconds =
        days * 86'400 +
        static_cast<std::int64_t>(*hour) * 3'600 +
        static_cast<std::int64_t>(*minute) * 60 +
        static_cast<std::int64_t>(*second);
    return RetryHooks::Clock::time_point{std::chrono::seconds{seconds}};
}

} // namespace

RetryHooks defaultRetryHooks() {
    RetryHooks hooks;
    hooks.now = [] { return RetryHooks::Clock::now(); };
    hooks.randomUnit = [] {
        thread_local std::mt19937 generator{std::random_device{}()};
        thread_local std::uniform_real_distribution<double> distribution(0.0, 1.0);
        return distribution(generator);
    };
    hooks.sleep = [](
                      std::chrono::milliseconds delay,
                      const std::shared_ptr<CancellationToken>& cancellation) {
        if (cancellation) {
            if (cancellation->requested()) return false;
            return !cancellation->wait_for(delay);
        }
        std::this_thread::sleep_for(delay);
        return true;
    };
    return hooks;
}

bool isRetryableHttpStatus(long status) noexcept {
    return status == 429 || (status >= 500 && status <= 599);
}

std::optional<std::chrono::milliseconds> parseRetryAfter(
    const std::map<std::string, std::string>& headers,
    RetryHooks::Clock::time_point now) {
    const auto raw = headerValue(headers, "retry-after");
    if (!raw) return std::nullopt;

    const auto value = trimAscii(*raw);
    if (value.empty()) return std::nullopt;

    std::uint64_t seconds = 0;
    const auto* begin = value.data();
    const auto* end = value.data() + value.size();
    const auto parsed = std::from_chars(begin, end, seconds);
    if (parsed.ec == std::errc{} && parsed.ptr == end) {
        constexpr auto maxMilliseconds =
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
        if (seconds > maxMilliseconds / 1000) {
            return std::chrono::milliseconds{std::numeric_limits<std::int64_t>::max()};
        }
        return std::chrono::milliseconds{static_cast<std::int64_t>(seconds * 1000)};
    }

    const auto date = parseHttpDate(value);
    if (!date) return std::nullopt;
    if (*date <= now) return std::chrono::milliseconds{0};
    return std::chrono::duration_cast<std::chrono::milliseconds>(*date - now);
}

std::chrono::milliseconds exponentialBackoffDelay(
    std::size_t retryIndex,
    double randomUnit) {
    constexpr std::int64_t initialMilliseconds = 500;
    constexpr std::int64_t maxMilliseconds = 8'000;

    std::int64_t delay = initialMilliseconds;
    for (std::size_t index = 0; index < retryIndex && delay < maxMilliseconds; ++index) {
        delay = std::min<std::int64_t>(delay * 2, maxMilliseconds);
    }

    const auto boundedRandom = std::clamp(randomUnit, 0.0, 1.0);
    const auto jitterFactor = 1.0 - 0.25 * boundedRandom;
    return std::chrono::milliseconds{
        static_cast<std::int64_t>(static_cast<double>(delay) * jitterFactor)};
}

RetryDecision makeRetryDecision(
    const HttpResponse& response,
    bool streamStarted,
    std::size_t retriesAlreadyPerformed,
    std::size_t maxRetries,
    std::chrono::milliseconds maxRetryDelay,
    RetryHooks::Clock::time_point now,
    double randomUnit) {
    RetryDecision decision;

    if (streamStarted || retriesAlreadyPerformed >= maxRetries) return decision;
    if (response.errorKind == HttpErrorKind::Cancelled ||
        response.errorKind == HttpErrorKind::ConsumerAborted) {
        return decision;
    }

    const bool retryableTransport = response.errorKind == HttpErrorKind::Transport;
    const bool retryableStatus =
        response.errorKind == HttpErrorKind::None && isRetryableHttpStatus(response.status);
    if (!retryableTransport && !retryableStatus) return decision;

    auto delay = retryableStatus ? parseRetryAfter(response.headers, now) : std::nullopt;
    if (!delay) {
        delay = exponentialBackoffDelay(retriesAlreadyPerformed, randomUnit);
    }

    if (maxRetryDelay.count() > 0 && *delay > maxRetryDelay) {
        decision.rejectionMessage =
            "Retry-After delay " + std::to_string(delay->count()) +
            "ms exceeds maximum " + std::to_string(maxRetryDelay.count()) + "ms";
        return decision;
    }

    decision.retry = true;
    decision.delay = *delay;
    return decision;
}

} // namespace pi::ai::detail
