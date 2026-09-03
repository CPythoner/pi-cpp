#include <doctest/doctest.h>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

#include "agent/events.hpp"

namespace {

pi::AssistantMessage makePartial() {
    pi::TextContent text;
    text.text = "部分输出";
    pi::AssistantMessage m;
    m.api = "openai-completions";
    m.provider = "deepseek";
    m.model = "deepseek-chat";
    m.content = {text};
    m.usage.input = 10;
    m.usage.output = 2;
    m.usage.totalTokens = 12;
    m.stopReason = pi::StopReason::Stop;
    m.timestamp = 1770000005000;
    return m;
}

// 事件必须经 variant 序列化（"type" 判别字段由分发层写入）
nlohmann::json dumpL1(const pi::AssistantMessageEvent& e) { return nlohmann::json(e); }
nlohmann::json dumpL2(const pi::AgentEvent& e) { return nlohmann::json(e); }

} // namespace

// ---- L1 provider 事件（12 种）----

TEST_CASE("EvStart round-trip") {
    pi::EvStart e;
    e.partial = makePartial();

    nlohmann::json j = dumpL1(e);
    CHECK(j.at("type") == "start");
    CHECK(j.at("partial").at("model") == "deepseek-chat");
    auto e2 = j.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvStart>(e2));
    CHECK(dumpL1(e2) == j);
}

TEST_CASE("EvText 生命周期 round-trip（start/delta/end）") {
    const auto partial = makePartial();

    pi::EvTextStart s;
    s.contentIndex = 1;
    s.partial = partial;
    nlohmann::json js = dumpL1(s);
    CHECK(js.at("type") == "text_start");
    CHECK(js.at("contentIndex") == 1);
    CHECK(js.at("partial").at("usage").at("input") == 10);
    auto s2 = js.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvTextStart>(s2));
    CHECK(std::get<pi::EvTextStart>(s2).contentIndex == 1);
    CHECK(dumpL1(s2) == js);

    pi::EvTextDelta d;
    d.contentIndex = 1;
    d.delta = "你好";
    d.partial = partial;
    nlohmann::json jd = dumpL1(d);
    CHECK(jd.at("type") == "text_delta");
    CHECK(jd.at("delta") == "你好");
    auto d2 = jd.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvTextDelta>(d2));
    CHECK(dumpL1(d2) == jd);

    pi::EvTextEnd e;
    e.contentIndex = 1;
    e.content = "你好，世界";
    e.partial = partial;
    nlohmann::json je = dumpL1(e);
    CHECK(je.at("type") == "text_end");
    CHECK(je.at("content") == "你好，世界");
    auto e2 = je.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvTextEnd>(e2));
    CHECK(dumpL1(e2) == je);
}

TEST_CASE("EvThinking 生命周期 round-trip（start/delta/end）") {
    const auto partial = makePartial();

    pi::EvThinkingStart s;
    s.contentIndex = 0;
    s.partial = partial;
    nlohmann::json js = dumpL1(s);
    CHECK(js.at("type") == "thinking_start");
    auto s2 = js.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvThinkingStart>(s2));
    CHECK(dumpL1(s2) == js);

    pi::EvThinkingDelta d;
    d.contentIndex = 0;
    d.delta = "思考片段";
    d.partial = partial;
    nlohmann::json jd = dumpL1(d);
    CHECK(jd.at("type") == "thinking_delta");
    auto d2 = jd.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvThinkingDelta>(d2));
    CHECK(dumpL1(d2) == jd);

    pi::EvThinkingEnd e;
    e.contentIndex = 0;
    e.content = "完整思考";
    e.partial = partial;
    nlohmann::json je = dumpL1(e);
    CHECK(je.at("type") == "thinking_end");
    auto e2 = je.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvThinkingEnd>(e2));
    CHECK(dumpL1(e2) == je);
}

TEST_CASE("EvToolCall 生命周期 round-trip（start/delta/end）") {
    const auto partial = makePartial();

    pi::EvToolCallStart s;
    s.contentIndex = 2;
    s.partial = partial;
    nlohmann::json js = dumpL1(s);
    CHECK(js.at("type") == "toolcall_start");
    auto s2 = js.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvToolCallStart>(s2));
    CHECK(dumpL1(s2) == js);

    pi::EvToolCallDelta d;
    d.contentIndex = 2;
    d.delta = R"({"path":"REA)";
    d.partial = partial;
    nlohmann::json jd = dumpL1(d);
    CHECK(jd.at("type") == "toolcall_delta");
    CHECK(jd.at("delta") == R"({"path":"REA)");
    auto d2 = jd.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvToolCallDelta>(d2));
    CHECK(dumpL1(d2) == jd);

    pi::ToolCall call;
    call.id = "call_001";
    call.name = "read";
    call.arguments = nlohmann::json::parse(R"({"path":"README.md"})");
    pi::EvToolCallEnd e;
    e.contentIndex = 2;
    e.toolCall = call;
    e.partial = partial;
    nlohmann::json je = dumpL1(e);
    CHECK(je.at("type") == "toolcall_end");
    CHECK(je.at("toolCall").at("arguments").at("path") == "README.md");
    auto e2 = je.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvToolCallEnd>(e2));
    CHECK(dumpL1(e2) == je);
}

TEST_CASE("EvDone round-trip（reason 与 message）") {
    pi::EvDone e;
    e.reason = pi::StopReason::ToolUse;
    e.message = makePartial();

    nlohmann::json j = dumpL1(e);
    CHECK(j.at("type") == "done");
    CHECK(j.at("reason") == "toolUse");
    CHECK(j.at("message").at("model") == "deepseek-chat");
    auto e2 = j.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvDone>(e2));
    CHECK(std::get<pi::EvDone>(e2).reason == pi::StopReason::ToolUse);
    CHECK(dumpL1(e2) == j);
}

TEST_CASE("EvError round-trip：携带 stopReason:error 的 AssistantMessage") {
    pi::AssistantMessage err = makePartial();
    err.stopReason = pi::StopReason::Error;
    err.errorMessage = "connection reset";

    pi::EvError e;
    e.reason = pi::StopReason::Error;
    e.error = err;

    nlohmann::json j = dumpL1(e);
    CHECK(j.at("type") == "error");
    CHECK(j.at("reason") == "error");
    CHECK(j.at("error").at("stopReason") == "error");
    CHECK(j.at("error").at("errorMessage") == "connection reset");
    auto e2 = j.get<pi::AssistantMessageEvent>();
    CHECK(std::holds_alternative<pi::EvError>(e2));
    const auto& err2 = std::get<pi::EvError>(e2).error;
    CHECK(err2.stopReason == pi::StopReason::Error);
    REQUIRE(err2.errorMessage.has_value());
    CHECK(*err2.errorMessage == "connection reset");
    CHECK(dumpL1(e2) == j);
}

TEST_CASE("未知 L1 事件 type 抛错且带 type 值") {
    nlohmann::json j = nlohmann::json::parse(R"({"type":"alien_event"})");
    pi::AssistantMessageEvent e;
    CHECK_THROWS_WITH(j.get_to(e), "unknown assistant message event type: alien_event");
}

// ---- L2 agent 事件（10 种）----

TEST_CASE("EvAgentStart/EvTurnStart 空 payload round-trip") {
    pi::EvAgentStart as;
    nlohmann::json js = dumpL2(as);
    CHECK(js == nlohmann::json::parse(R"({"type":"agent_start"})"));
    auto as2 = js.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvAgentStart>(as2));
    CHECK(dumpL2(as2) == js);

    pi::EvTurnStart ts;
    nlohmann::json jt = dumpL2(ts);
    CHECK(jt == nlohmann::json::parse(R"({"type":"turn_start"})"));
    auto ts2 = jt.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvTurnStart>(ts2));
    CHECK(dumpL2(ts2) == jt);
}

TEST_CASE("EvAgentEnd round-trip：messages 数组") {
    pi::UserMessage u1;
    u1.content = std::string{"q1"};
    pi::UserMessage u2;
    u2.content = std::string{"q2"};

    pi::EvAgentEnd e;
    e.messages = {u1, u2};

    nlohmann::json j = dumpL2(e);
    CHECK(j.at("type") == "agent_end");
    CHECK(j.at("messages").size() == 2);
    CHECK(j.at("messages").at(1).at("role") == "user");
    auto e2 = j.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvAgentEnd>(e2));
    CHECK(std::get<pi::EvAgentEnd>(e2).messages.size() == 2);
    CHECK(dumpL2(e2) == j);
}

TEST_CASE("EvTurnEnd round-trip：message 与 toolResults") {
    pi::AssistantMessage am = makePartial();
    pi::ToolResultMessage tr;
    tr.toolCallId = "call_001";
    tr.toolName = "read";

    pi::EvTurnEnd e;
    e.message = am;
    e.toolResults = {tr};

    nlohmann::json j = dumpL2(e);
    CHECK(j.at("type") == "turn_end");
    CHECK(j.at("message").at("role") == "assistant");
    CHECK(j.at("toolResults").at(0).at("role") == "toolResult");
    auto e2 = j.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvTurnEnd>(e2));
    CHECK(dumpL2(e2) == j);
}

TEST_CASE("EvMessageStart/End round-trip") {
    pi::UserMessage u;
    u.content = std::string{"hi"};

    pi::EvMessageStart s;
    s.message = u;
    nlohmann::json js = dumpL2(s);
    CHECK(js.at("type") == "message_start");
    CHECK(js.at("message").at("role") == "user");
    auto s2 = js.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvMessageStart>(s2));
    CHECK(dumpL2(s2) == js);

    pi::EvMessageEnd e;
    e.message = u;
    nlohmann::json je = dumpL2(e);
    CHECK(je.at("type") == "message_end");
    auto e2 = je.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvMessageEnd>(e2));
    CHECK(dumpL2(e2) == je);
}

TEST_CASE("EvMessageUpdate round-trip：内嵌 AssistantMessageEvent") {
    pi::AssistantMessage partial = makePartial();
    pi::EvTextDelta inner;
    inner.contentIndex = 0;
    inner.delta = "流";
    inner.partial = partial;

    pi::EvMessageUpdate e;
    e.message = partial;   // 流式中的 assistant 消息
    e.assistantMessageEvent = inner;

    nlohmann::json j = dumpL2(e);
    CHECK(j.at("type") == "message_update");
    CHECK(j.at("message").at("role") == "assistant");
    CHECK(j.at("assistantMessageEvent").at("type") == "text_delta");
    CHECK(j.at("assistantMessageEvent").at("delta") == "流");
    CHECK(j.at("assistantMessageEvent").at("partial").at("usage").at("input") == 10);

    auto e2 = j.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvMessageUpdate>(e2));
    const auto& upd = std::get<pi::EvMessageUpdate>(e2);
    CHECK(std::holds_alternative<pi::EvTextDelta>(upd.assistantMessageEvent));
    CHECK(std::get<pi::EvTextDelta>(upd.assistantMessageEvent).delta == "流");
    CHECK(dumpL2(e2) == j);   // 嵌套事件整体 round-trip 语义相等
}

TEST_CASE("EvToolExecution 三段 round-trip（start/update/end）") {
    const nlohmann::json args = nlohmann::json::parse(R"({"path":"README.md","line":3})");

    pi::EvToolExecutionStart s;
    s.toolCallId = "call_001";
    s.toolName = "read";
    s.args = args;
    nlohmann::json js = dumpL2(s);
    CHECK(js.at("type") == "tool_execution_start");
    CHECK(js.at("args").at("line") == 3);
    auto s2 = js.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvToolExecutionStart>(s2));
    CHECK(dumpL2(s2) == js);

    pi::EvToolExecutionUpdate u;
    u.toolCallId = "call_001";
    u.toolName = "read";
    u.args = args;
    u.partialResult = nlohmann::json::parse(R"({"lines":10})");
    nlohmann::json ju = dumpL2(u);
    CHECK(ju.at("type") == "tool_execution_update");
    CHECK(ju.at("partialResult").at("lines") == 10);
    auto u2 = ju.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvToolExecutionUpdate>(u2));
    CHECK(dumpL2(u2) == ju);

    pi::EvToolExecutionEnd e;
    e.toolCallId = "call_001";
    e.toolName = "read";
    e.result = nlohmann::json::parse(R"({"totalLines":42})");
    e.isError = false;
    nlohmann::json je = dumpL2(e);
    CHECK(je.at("type") == "tool_execution_end");
    CHECK(je.at("result").at("totalLines") == 42);
    CHECK(je.at("isError") == false);
    auto e2 = je.get<pi::AgentEvent>();
    CHECK(std::holds_alternative<pi::EvToolExecutionEnd>(e2));
    CHECK(dumpL2(e2) == je);
}

TEST_CASE("未知 L2 事件 type 抛错且带 type 值") {
    nlohmann::json j = nlohmann::json::parse(R"({"type":"alien_l2"})");
    pi::AgentEvent e;
    CHECK_THROWS_WITH(j.get_to(e), "unknown agent event type: alien_l2");
}
