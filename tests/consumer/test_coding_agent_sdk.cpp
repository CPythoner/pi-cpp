#include <doctest/doctest.h>

#include <pi/coding-agent/fwd.hpp>

TEST_CASE("coding-agent sdk target exposes its public namespace") {
    pi::coding_agent::CodingAgent* agent = nullptr;
    CHECK(agent == nullptr);
}
