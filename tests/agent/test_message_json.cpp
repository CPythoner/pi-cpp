#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <variant>

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

// ---- T8 内容块 ----

TEST_CASE("TextContent round-trip 与 textSignature 缺省") {
    pi::TextContent t;
    t.text = "我先读一下 README。";

    nlohmann::json j = t;
    CHECK(j.at("text") == "我先读一下 README。");
    CHECK_FALSE(j.contains("textSignature"));

    auto t2 = nlohmann::json::parse(R"({"text":"hi"})").get<pi::TextContent>();
    CHECK(t2.text == "hi");
    CHECK_FALSE(t2.textSignature.has_value());

    pi::TextContent s;
    s.text = "signed";
    s.textSignature = "sig_abc";
    nlohmann::json js = s;
    CHECK(js.at("textSignature") == "sig_abc");
    auto s2 = js.get<pi::TextContent>();
    REQUIRE(s2.textSignature.has_value());
    CHECK(*s2.textSignature == "sig_abc");
    CHECK(nlohmann::json(s2) == js);
}

TEST_CASE("ThinkingContent round-trip 与 redacted/thinkingSignature 缺省") {
    pi::ThinkingContent t;
    t.thinking = "让我想想";
    t.thinkingSignature = "sig_t";
    t.redacted = true;

    nlohmann::json j = t;
    CHECK(j.at("thinking") == "让我想想");
    CHECK(j.at("thinkingSignature") == "sig_t");
    CHECK(j.at("redacted") == true);

    auto t2 = j.get<pi::ThinkingContent>();
    CHECK(t2.redacted == true);
    REQUIRE(t2.thinkingSignature.has_value());
    CHECK(*t2.thinkingSignature == "sig_t");
    CHECK(nlohmann::json(t2) == j);

    // 缺省：无 signature、无 redacted → nullopt/false
    auto t3 = nlohmann::json::parse(R"({"thinking":"raw"})").get<pi::ThinkingContent>();
    CHECK(t3.thinking == "raw");
    CHECK_FALSE(t3.thinkingSignature.has_value());
    CHECK(t3.redacted == false);
}

TEST_CASE("ImageContent round-trip") {
    pi::ImageContent i;
    i.data = "aGVsbG8=";   // base64
    i.mimeType = "image/png";

    nlohmann::json j = i;
    CHECK(j.at("data") == "aGVsbG8=");
    CHECK(j.at("mimeType") == "image/png");
    auto i2 = j.get<pi::ImageContent>();
    CHECK(i2.data == i.data);
    CHECK(i2.mimeType == i.mimeType);
    CHECK(nlohmann::json(i2) == j);
}

TEST_CASE("ToolCall arguments 保留任意 JSON（嵌套/数字/布尔/null）") {
    pi::ToolCall tc;
    tc.id = "call_001";
    tc.name = "edit";
    tc.arguments = nlohmann::json::parse(R"({"path":"a.txt","opts":{"line":3,"force":true,"note":null,"tags":["x","y"]}})");
    tc.thoughtSignature = "sig_th";

    nlohmann::json j = tc;
    CHECK(j.at("id") == "call_001");
    CHECK(j.at("arguments").at("opts").at("line") == 3);
    CHECK(j.at("arguments").at("opts").at("force") == true);
    CHECK(j.at("arguments").at("opts").at("note").is_null());
    CHECK(j.at("arguments").at("opts").at("tags").size() == 2);
    CHECK(j.at("thoughtSignature") == "sig_th");

    auto tc2 = j.get<pi::ToolCall>();
    CHECK(tc2.arguments == tc.arguments);   // 任意 JSON 结构原样保留
    REQUIRE(tc2.thoughtSignature.has_value());
    CHECK(*tc2.thoughtSignature == "sig_th");
    CHECK(nlohmann::json(tc2) == j);

    // thoughtSignature 缺省容忍
    auto tc3 = nlohmann::json::parse(R"({"id":"c2","name":"read","arguments":{"path":"README.md"}})").get<pi::ToolCall>();
    CHECK(tc3.name == "read");
    CHECK_FALSE(tc3.thoughtSignature.has_value());
    CHECK(tc3.arguments.at("path") == "README.md");
}

TEST_CASE("ContentBlock variant 四块 round-trip（type 判别）") {
    pi::TextContent text;
    text.text = "hello";
    pi::ThinkingContent think;
    think.thinking = "hmm";
    pi::ImageContent img;
    img.data = "AAAA";
    img.mimeType = "image/jpeg";
    pi::ToolCall call;
    call.id = "call_001";
    call.name = "read";
    call.arguments = nlohmann::json::parse(R"({"path":"README.md"})");

    const pi::ContentBlock blocks[] = {text, think, img, call};
    const std::string types[] = {"text", "thinking", "image", "toolCall"};

    for (std::size_t i = 0; i < 4; ++i) {
        nlohmann::json j = blocks[i];
        CHECK(j.at("type") == types[i]);
        auto b2 = j.get<pi::ContentBlock>();
        CHECK(nlohmann::json(b2) == j);   // 语义相等（字段序无关）
    }

    CHECK(std::holds_alternative<pi::TextContent>(nlohmann::json(text).get<pi::ContentBlock>()));
    CHECK(std::holds_alternative<pi::ToolCall>(nlohmann::json(call).get<pi::ContentBlock>()));
}

TEST_CASE("未知 content type 抛错且带 type 值") {
    nlohmann::json j = nlohmann::json::parse(R"({"type":"alien","text":"?"})");
    pi::ContentBlock b;
    CHECK_THROWS_WITH(j.get_to(b), "unknown content block type: alien");
}
