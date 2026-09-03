#include <doctest/doctest.h>

#include <pi/ai/cancellation.hpp>
#include <pi/ai/events.hpp>
#include <pi/ai/message.hpp>
#include <pi/agent/events.hpp>
#include <pi/agent/message.hpp>
#include <pi/coding-agent/fwd.hpp>

#include <variant>

TEST_CASE("public sdk headers are consumable through canonical namespaces") {
    pi::ai::UserMessage user;
    user.content = std::string("hello");

    pi::agent::AgentMessage message = user;
    CHECK(std::holds_alternative<pi::ai::UserMessage>(message));

    pi::ai::CancellationToken token;
    CHECK_FALSE(token.requested());

    pi::coding_agent::CodingAgent* codingAgent = nullptr;
    CHECK(codingAgent == nullptr);
}
