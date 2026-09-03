#include <doctest/doctest.h>

#include <pi/ai/cancellation.hpp>
#include <pi/ai/event_stream.hpp>
#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>

#include <string>

TEST_CASE("ai sdk is independently consumable") {
    pi::ai::UserMessage user;
    user.content = std::string("hello");

    pi::ai::Message message = user;
    nlohmann::json j = message;
    CHECK(j.at("role") == "user");

    pi::ai::CancellationToken token;
    CHECK_FALSE(token.requested());

    auto stream = pi::ai::createAssistantMessageEventStream();
    CHECK_FALSE(stream.done());
}
