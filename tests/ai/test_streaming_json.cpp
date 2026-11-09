#include <doctest/doctest.h>

#include "ai/streaming_json.hpp"

#include <nlohmann/json.hpp>

#include <string>

namespace detail = pi::ai::detail;

TEST_CASE("streaming json parses complete object unchanged") {
    const auto parsed = detail::parseStreamingJson(R"({"path":"README.md","line":12})");
    REQUIRE(parsed.is_object());
    CHECK_EQ(parsed.at("path"), "README.md");
    CHECK_EQ(parsed.at("line"), 12);
}

TEST_CASE("streaming json keeps partial string value") {
    const auto parsed = detail::parseStreamingJson(R"({"path":"READ)");
    REQUIRE(parsed.is_object());
    REQUIRE(parsed.contains("path"));
    CHECK_EQ(parsed.at("path"), "READ");
}

TEST_CASE("streaming json keeps nested partial collections") {
    const auto parsed = detail::parseStreamingJson(
        R"({"query":"ok","nested":{"name":"par)");
    REQUIRE(parsed.is_object());
    CHECK_EQ(parsed.at("query"), "ok");
    REQUIRE(parsed.at("nested").is_object());
    CHECK_EQ(parsed.at("nested").at("name"), "par");
}

TEST_CASE("streaming json keeps partial array members") {
    const auto parsed = detail::parseStreamingJson(R"({"items":[1,2,"thr)");
    REQUIRE(parsed.is_object());
    REQUIRE(parsed.at("items").is_array());
    REQUIRE_EQ(parsed.at("items").size(), 3);
    CHECK_EQ(parsed.at("items")[0], 1);
    CHECK_EQ(parsed.at("items")[1], 2);
    CHECK_EQ(parsed.at("items")[2], "thr");
}

TEST_CASE("streaming json repairs raw control characters in strings") {
    const std::string input = std::string("{\"text\":\"line1") + '\n' + "line2\"}";
    const auto parsed = detail::parseStreamingJson(input);
    REQUIRE(parsed.is_object());
    CHECK_EQ(parsed.at("text"), std::string("line1\nline2"));
}

TEST_CASE("streaming json preserves invalid escape as literal backslash") {
    const auto parsed = detail::parseStreamingJson(R"({"path":"a\q"})");
    REQUIRE(parsed.is_object());
    CHECK_EQ(parsed.at("path"), R"(a\q)");
}

TEST_CASE("streaming json returns empty object for unrecoverable input") {
    const auto parsed = detail::parseStreamingJson("not-json");
    REQUIRE(parsed.is_object());
    CHECK(parsed.empty());
}
