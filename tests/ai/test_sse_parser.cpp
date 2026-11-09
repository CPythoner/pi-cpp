#include <doctest/doctest.h>

#include "ai/sse_parser.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace detail = pi::ai::detail;

TEST_CASE("SSE parser emits a complete data event") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data: {\"ok\":true}\n\n");

    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events[0].event, "message");
    CHECK_EQ(events[0].data, "{\"ok\":true}");
}

TEST_CASE("SSE parser is independent of arbitrary transport chunk boundaries") {
    const std::string input =
        "data: {\"choices\":[{\"delta\":{\"content\":\"hello\"}}]}\r\n"
        "\r\n"
        "data: [DONE]\r\n"
        "\r\n";

    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    for (const char ch : input) {
        parser.feed(std::string(1, ch));
    }

    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(events[0].data, "{\"choices\":[{\"delta\":{\"content\":\"hello\"}}]}");
    CHECK_EQ(events[1].data, "[DONE]");
}

TEST_CASE("SSE parser handles multiple events in one transport chunk") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data: one\n\ndata: two\n\ndata: three\n\n");

    REQUIRE_EQ(events.size(), 3);
    CHECK_EQ(events[0].data, "one");
    CHECK_EQ(events[1].data, "two");
    CHECK_EQ(events[2].data, "three");
}

TEST_CASE("SSE parser joins multiple data fields with newlines") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data: first\ndata: second\ndata: third\n\n");

    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events[0].data, "first\nsecond\nthird");
}

TEST_CASE("SSE parser treats a CRLF split across chunks as one line ending") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data: split\r");
    CHECK(events.empty());
    parser.feed("\n\r");
    CHECK(events.empty());
    parser.feed("\n");

    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events[0].data, "split");
}

TEST_CASE("SSE parser supports event id retry and comments") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed(
        ": heartbeat\n"
        "id: req-1\n"
        "event: completion\n"
        "retry: 1500\n"
        "data: payload\n"
        "\n");

    REQUIRE_EQ(events.size(), 1);
    CHECK_EQ(events[0].event, "completion");
    CHECK_EQ(events[0].id, "req-1");
    REQUIRE(events[0].retryMs.has_value());
    CHECK_EQ(*events[0].retryMs, 1500);
}

TEST_CASE("SSE id persists across subsequent events") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("id: stable\ndata: one\n\ndata: two\n\n");

    REQUIRE_EQ(events.size(), 2);
    CHECK_EQ(events[0].id, "stable");
    CHECK_EQ(events[1].id, "stable");
}

TEST_CASE("an empty data field still dispatches an SSE event") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data:\n\n");

    REQUIRE_EQ(events.size(), 1);
    CHECK(events[0].data.empty());
}

TEST_CASE("comments and fields without data do not dispatch") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed(": heartbeat\nevent: ping\nid: x\n\n");

    CHECK(events.empty());
}

TEST_CASE("finish discards an unterminated pending SSE event") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data: incomplete");
    parser.finish();

    CHECK(events.empty());
}

TEST_CASE("finish accepts a trailing CR as a complete line terminator but not an event separator") {
    std::vector<detail::SseEvent> events;
    detail::SseParser parser([&](detail::SseEvent event) {
        events.push_back(std::move(event));
    });

    parser.feed("data: incomplete\r");
    parser.finish();

    CHECK(events.empty());
}
