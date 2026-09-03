#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include "agent/message.hpp"

// ---- T7 基础值对象 ----

TEST_CASE("StopReason 五值双向 round-trip") {
    const std::pair<pi::StopReason, std::string> cases[] = {
        {pi::StopReason::Stop, "stop"},
        {pi::StopReason::Length, "length"},
        {pi::StopReason::ToolUse, "toolUse"},
        {pi::StopReason::Error, "error"},
        {pi::StopReason::Aborted, "aborted"},
    };
    for (const auto& [reason, wire] : cases) {
        nlohmann::json j = reason;
        CHECK(j.get<std::string>() == wire);
        CHECK(j.get<pi::StopReason>() == reason);
    }
}

TEST_CASE("Cost round-trip 全字段") {
    pi::Cost c;
    c.input = 0.000014;
    c.output = 0.0000016;
    c.cacheRead = 0;
    c.cacheWrite = 0;
    c.total = 0.0000156;

    nlohmann::json j = c;
    CHECK(j.at("input") == 0.000014);
    CHECK(j.at("total") == 0.0000156);
    CHECK(j.contains("cacheRead"));

    auto c2 = j.get<pi::Cost>();
    CHECK(c2.input == c.input);
    CHECK(c2.output == c.output);
    CHECK(c2.cacheRead == c.cacheRead);
    CHECK(c2.cacheWrite == c.cacheWrite);
    CHECK(c2.total == c.total);

    CHECK(nlohmann::json(c2) == j);
}

TEST_CASE("Usage round-trip：cacheWrite1h 缺省与嵌套 cost") {
    pi::Usage u;
    u.input = 120;
    u.output = 8;
    u.cacheRead = 64;
    u.cacheWrite = 32;
    u.cacheWrite1h = 16;
    u.totalTokens = 240;
    u.cost.input = 0.000014;
    u.cost.total = 0.0000156;

    nlohmann::json j = u;
    CHECK(j.at("input") == 120);
    CHECK(j.at("totalTokens") == 240);
    CHECK(j.contains("cacheWrite1h"));
    CHECK(j.at("cacheWrite1h") == 16);
    CHECK(j.at("cost").at("input") == 0.000014);

    auto u2 = j.get<pi::Usage>();
    CHECK(u2.input == 120);
    CHECK(u2.output == 8);
    CHECK(u2.cacheRead == 64);
    CHECK(u2.cacheWrite == 32);
    REQUIRE(u2.cacheWrite1h.has_value());
    CHECK(*u2.cacheWrite1h == 16);
    CHECK(u2.cost.input == 0.000014);
    CHECK(u2.cost.total == 0.0000156);

    CHECK(nlohmann::json(u2) == j);
}

TEST_CASE("Usage 缺省字段容忍：cacheWrite1h 省略时序列化为缺省") {
    pi::Usage u;
    u.input = 10;

    nlohmann::json j = u;
    CHECK_FALSE(j.contains("cacheWrite1h"));   // nullopt 不写成 null（对齐 pi/tau wire）

    auto u2 = nlohmann::json::parse(R"({"input":10,"output":0,"cacheRead":0,"cacheWrite":0,"totalTokens":0,"cost":{"input":0,"output":0,"cacheRead":0,"cacheWrite":0,"total":0}})").get<pi::Usage>();
    CHECK(u2.input == 10);
    CHECK_FALSE(u2.cacheWrite1h.has_value());
}

TEST_CASE("DiagnosticError round-trip：可选 name/stack/code 省略") {
    pi::DiagnosticError de;
    de.message = "boom";
    de.code = "E_TIMEOUT";

    nlohmann::json j = de;
    CHECK(j.at("message") == "boom");
    CHECK(j.at("code") == "E_TIMEOUT");
    CHECK_FALSE(j.contains("name"));
    CHECK_FALSE(j.contains("stack"));

    auto de2 = j.get<pi::DiagnosticError>();
    CHECK(de2.message == "boom");
    REQUIRE(de2.code.has_value());
    CHECK(*de2.code == "E_TIMEOUT");
    CHECK_FALSE(de2.name.has_value());

    CHECK(nlohmann::json(de2) == j);
}

TEST_CASE("Diagnostic round-trip：嵌套 error 与自由结构 details") {
    pi::DiagnosticError de;
    de.name = "TypeError";
    de.message = "cannot read properties of undefined";

    pi::Diagnostic d;
    d.type = "usage_limit";
    d.timestamp = 1770000005000;
    d.error = de;
    d.details = nlohmann::json::parse(R"({"limit":100,"extra":[1,2,{"k":null}]})");

    nlohmann::json j = d;
    CHECK(j.at("type") == "usage_limit");
    CHECK(j.at("error").at("name") == "TypeError");
    CHECK(j.at("details").at("limit") == 100);
    CHECK(j.at("details").at("extra").size() == 3);

    auto d2 = j.get<pi::Diagnostic>();
    CHECK(d2.type == "usage_limit");
    REQUIRE(d2.error.has_value());
    CHECK(d2.error->name == "TypeError");
    CHECK(d2.details == d.details);

    CHECK(nlohmann::json(d2) == j);
}

TEST_CASE("Diagnostic error 缺省容忍") {
    auto d = nlohmann::json::parse(R"({"type":"info","timestamp":42})").get<pi::Diagnostic>();
    CHECK(d.type == "info");
    CHECK(d.timestamp == 42);
    CHECK_FALSE(d.error.has_value());
}
