#include <doctest/doctest.h>

#include <pi/ai/event_stream.hpp>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace ai = pi::ai;

namespace {

ai::AssistantMessage makeAssistant(
    std::string text,
    ai::StopReason reason = ai::StopReason::Stop) {
    ai::AssistantMessage message;
    message.content.push_back(ai::TextContent{std::move(text), std::nullopt});
    message.api = "openai-completions";
    message.provider = "test";
    message.model = "test-model";
    message.stopReason = reason;
    return message;
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

TEST_CASE("AssistantMessageEventStream preserves FIFO and delivers terminal done event") {
    ai::AssistantMessageEventStream stream;
    const auto partial = makeAssistant("");
    const auto finalMessage = makeAssistant("hello");

    stream.push(ai::EvStart{partial});
    stream.push(ai::EvTextStart{0, partial});
    stream.push(ai::EvTextDelta{0, "hel", partial});
    stream.push(ai::EvTextDelta{0, "lo", partial});
    stream.push(ai::EvTextEnd{0, "hello", finalMessage});
    stream.push(ai::EvDone{ai::StopReason::Stop, finalMessage});

    CHECK(stream.done());

    CHECK_EQ(eventType(*stream.next()), "start");
    CHECK_EQ(eventType(*stream.next()), "text_start");
    CHECK_EQ(eventType(*stream.next()), "text_delta");
    CHECK_EQ(eventType(*stream.next()), "text_delta");
    CHECK_EQ(eventType(*stream.next()), "text_end");
    CHECK_EQ(eventType(*stream.next()), "done");
    CHECK_FALSE(stream.next().has_value());

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    CHECK_EQ(std::get<ai::TextContent>(result.content[0]).text, "hello");
    CHECK_EQ(result.stopReason, ai::StopReason::Stop);
}

TEST_CASE("push after terminal event is ignored") {
    ai::AssistantMessageEventStream stream;
    const auto finalMessage = makeAssistant("done");

    stream.push(ai::EvDone{ai::StopReason::Stop, finalMessage});
    stream.push(ai::EvTextDelta{0, "ignored", finalMessage});

    const auto terminal = stream.next();
    REQUIRE(terminal.has_value());
    CHECK_EQ(eventType(*terminal), "done");
    CHECK_FALSE(stream.next().has_value());
}

TEST_CASE("error event is terminal, delivered, and becomes final result") {
    ai::AssistantMessageEventStream stream;
    auto errorMessage = makeAssistant("", ai::StopReason::Error);
    errorMessage.errorMessage = "provider failed";

    stream.push(ai::EvError{ai::StopReason::Error, errorMessage});

    const auto terminal = stream.next();
    REQUIRE(terminal.has_value());
    CHECK_EQ(eventType(*terminal), "error");
    CHECK_FALSE(stream.next().has_value());

    const auto result = stream.result();
    CHECK_EQ(result.stopReason, ai::StopReason::Error);
    REQUIRE(result.errorMessage.has_value());
    CHECK_EQ(*result.errorMessage, "provider failed");
}

TEST_CASE("result is available without consuming the terminal event") {
    ai::AssistantMessageEventStream stream;
    const auto finalMessage = makeAssistant("final");

    stream.push(ai::EvDone{ai::StopReason::Stop, finalMessage});

    const auto result = stream.result();
    REQUIRE_EQ(result.content.size(), 1);
    CHECK_EQ(std::get<ai::TextContent>(result.content[0]).text, "final");

    const auto terminal = stream.next();
    REQUIRE(terminal.has_value());
    CHECK_EQ(eventType(*terminal), "done");
}

TEST_CASE("copied EventStream handles share the same state") {
    ai::AssistantMessageEventStream producer;
    auto consumer = producer;

    producer.push(ai::EvDone{ai::StopReason::Stop, makeAssistant("shared")});

    CHECK(consumer.done());
    const auto terminal = consumer.next();
    REQUIRE(terminal.has_value());
    CHECK_EQ(eventType(*terminal), "done");

    const auto result = consumer.result();
    CHECK_EQ(std::get<ai::TextContent>(result.content[0]).text, "shared");
}

TEST_CASE("blocking next wakes when producer pushes") {
    ai::AssistantMessageEventStream stream;
    auto producer = stream;

    std::atomic<bool> waiting{false};
    std::atomic<bool> received{false};

    std::thread consumer([&] {
        waiting.store(true);
        const auto event = stream.next();
        received.store(event.has_value() && eventType(*event) == "done");
    });

    while (!waiting.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    CHECK_FALSE(received.load());

    producer.push(ai::EvDone{ai::StopReason::Stop, makeAssistant("wake")});
    consumer.join();

    CHECK(received.load());
}

TEST_CASE("generic EventStream end wakes a blocked consumer") {
    ai::EventStream<int, int> stream(
        [](const int&) { return false; },
        [](const int& value) { return value; });
    auto producer = stream;

    std::atomic<bool> waiting{false};
    std::atomic<bool> ended{false};

    std::thread consumer([&] {
        waiting.store(true);
        ended.store(!stream.next().has_value());
    });

    while (!waiting.load()) {
        std::this_thread::yield();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    producer.end();
    consumer.join();

    CHECK(ended.load());
    CHECK(stream.done());
}

TEST_CASE("generic EventStream end(result) publishes final result") {
    ai::EventStream<int, int> stream(
        [](const int&) { return false; },
        [](const int& value) { return value; });

    stream.end(42);

    CHECK(stream.done());
    CHECK_EQ(stream.result(), 42);
    CHECK_FALSE(stream.next().has_value());
}
