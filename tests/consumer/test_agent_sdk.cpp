#include <doctest/doctest.h>

#include <pi/agent/events.hpp>
#include <pi/agent/message.hpp>

#include <string>
#include <variant>

TEST_CASE("agent sdk is independently consumable") {
    pi::ai::UserMessage user;
    user.content = std::string("hello");

    pi::agent::AgentMessage message = user;
    CHECK(std::holds_alternative<pi::ai::UserMessage>(message));

    pi::agent::EvAgentStart start;
    pi::agent::AgentEvent event = start;
    nlohmann::json j = event;
    CHECK(j.at("type") == "agent_start");
}
