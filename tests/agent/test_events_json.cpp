#include <doctest/doctest.h>

#include <pi/agent/events.hpp>

#include <nlohmann/json.hpp>

#include <cstddef>
#include <string>
#include <variant>
#include <vector>

namespace ai = pi::ai;
namespace agent = pi::agent;

namespace {

ai::AssistantMessage makePartial() {
    ai::TextContent text;
    text.text = "部分输出";
    ai::AssistantMessage m;
    m.api = "openai-completions";
    m.provider = "deepseek";
    m.model = "deepseek-chat";
    m.content = {text};
    m.usage.input = 10;
    m.usage.output = 2;
    m.usage.totalTokens = 12;
    m.stopReason = ai::StopReason::Stop;
    m.timestamp = 1770000005000;
    return m;
}

nlohmann::json dumpL1(const ai::AssistantMessageEvent& e) { return nlohmann::json(e); }
nlohmann::json dumpL2(const agent::AgentEvent& e) { return nlohmann::json(e); }

} // namespace

TEST_CASE("EvStart round-trip") {
    ai::EvStart e;
    e.partial = makePartial();

    nlohmann::json j = dumpL1(e);
    CHECK(j.at("type") == "start");
    CHECK(j.at("partial").at("model") == "deepseek-chat");
    auto e2 = j.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvStart>(e2));
    CHECK(dumpL1(e2) == j);
}

TEST_CASE("EvText 生命周期 round-trip（start/delta/end）") {
    const auto partial = makePartial();

    ai::EvTextStart s;
    s.contentIndex = 1;
    s.partial = partial;
    nlohmann::json js = dumpL1(s);
    CHECK(js.at("type") == "text_start");
    CHECK(js.at("contentIndex") == 1);
    CHECK(js.at("partial").at("usage").at("input") == 10);
    auto s2 = js.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvTextStart>(s2));
    CHECK(std::get<ai::EvTextStart>(s2).contentIndex == 1);
    CHECK(dumpL1(s2) == js);

    ai::EvTextDelta d;
    d.contentIndex = 1;
    d.delta = "你好";
    d.partial = partial;
    nlohmann::json jd = dumpL1(d);
    CHECK(jd.at("type") == "text_delta");
    CHECK(jd.at("delta") == "你好");
    auto d2 = jd.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvTextDelta>(d2));
    CHECK(dumpL1(d2) == jd);

    ai::EvTextEnd e;
    e.contentIndex = 1;
    e.content = "你好，世界";
    e.partial = partial;
    nlohmann::json je = dumpL1(e);
    CHECK(je.at("type") == "text_end");
    CHECK(je.at("content") == "你好，世界");
    auto e2 = je.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvTextEnd>(e2));
    CHECK(dumpL1(e2) == je);
}

TEST_CASE("EvThinking 生命周期 round-trip（start/delta/end）") {
    const auto partial = makePartial();

    ai::EvThinkingStart s;
    s.contentIndex = 0;
    s.partial = partial;
    nlohmann::json js = dumpL1(s);
    CHECK(js.at("type") == "thinking_start");
    auto s2 = js.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvThinkingStart>(s2));
    CHECK(dumpL1(s2) == js);

    ai::EvThinkingDelta d;
    d.contentIndex = 0;
    d.delta = "思考片段";
    d.partial = partial;
    nlohmann::json jd = dumpL1(d);
    CHECK(jd.at("type") == "thinking_delta");
    auto d2 = jd.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvThinkingDelta>(d2));
    CHECK(dumpL1(d2) == jd);

    ai::EvThinkingEnd e;
    e.contentIndex = 0;
    e.content = "完整思考";
    e.partial = partial;
    nlohmann::json je = dumpL1(e);
    CHECK(je.at("type") == "thinking_end");
    auto e2 = je.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvThinkingEnd>(e2));
    CHECK(dumpL1(e2) == je);
}

TEST_CASE("EvToolCall 生命周期 round-trip（start/delta/end）") {
    const auto partial = makePartial();

    ai::EvToolCallStart s;
    s.contentIndex = 2;
    s.partial = partial;
    nlohmann::json js = dumpL1(s);
    CHECK(js.at("type") == "toolcall_start");
    auto s2 = js.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvToolCallStart>(s2));
    CHECK(dumpL1(s2) == js);

    ai::EvToolCallDelta d;
    d.contentIndex = 2;
    d.delta = R"({"path":"REA)";
    d.partial = partial;
    nlohmann::json jd = dumpL1(d);
    CHECK(jd.at("type") == "toolcall_delta");
    CHECK(jd.at("delta") == R"({"path":"REA)");
    auto d2 = jd.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvToolCallDelta>(d2));
    CHECK(dumpL1(d2) == jd);

    ai::ToolCall call;
    call.id = "call_001";
    call.name = "read";
    call.arguments = nlohmann::json::parse(R"({"path":"README.md"})");
    ai::EvToolCallEnd e;
    e.contentIndex = 2;
    e.toolCall = call;
    e.partial = partial;
    nlohmann::json je = dumpL1(e);
    CHECK(je.at("type") == "toolcall_end");
    CHECK(je.at("toolCall").at("arguments").at("path") == "README.md");
    auto e2 = je.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvToolCallEnd>(e2));
    CHECK(dumpL1(e2) == je);
}

TEST_CASE("EvDone round-trip（reason 与 message）") {
    ai::EvDone e;
    e.reason = ai::StopReason::ToolUse;
    e.message = makePartial();

    nlohmann::json j = dumpL1(e);
    CHECK(j.at("type") == "done");
    CHECK(j.at("reason") == "toolUse");
    CHECK(j.at("message").at("model") == "deepseek-chat");
    auto e2 = j.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvDone>(e2));
    CHECK(std::get<ai::EvDone>(e2).reason == ai::StopReason::ToolUse);
    CHECK(dumpL1(e2) == j);
}

TEST_CASE("EvError round-trip：携带 stopReason:error 的 AssistantMessage") {
    ai::AssistantMessage err = makePartial();
    err.stopReason = ai::StopReason::Error;
    err.errorMessage = "connection reset";

    ai::EvError e;
    e.reason = ai::StopReason::Error;
    e.error = err;

    nlohmann::json j = dumpL1(e);
    CHECK(j.at("type") == "error");
    CHECK(j.at("reason") == "error");
    CHECK(j.at("error").at("stopReason") == "error");
    CHECK(j.at("error").at("errorMessage") == "connection reset");
    auto e2 = j.get<ai::AssistantMessageEvent>();
    CHECK(std::holds_alternative<ai::EvError>(e2));
    const auto& err2 = std::get<ai::EvError>(e2).error;
    CHECK(err2.stopReason == ai::StopReason::Error);
    REQUIRE(err2.errorMessage.has_value());
    CHECK(*err2.errorMessage == "connection reset");
    CHECK(dumpL1(e2) == j);
}

TEST_CASE("未知 L1 事件 type 抛错且带 type 值") {
    nlohmann::json j = nlohmann::json::parse(R"({"type":"alien_event"})");
    ai::AssistantMessageEvent e;
    CHECK_THROWS_WITH(j.get_to(e), "unknown assistant message event type: alien_event");
}

TEST_CASE("EvAgentStart/EvTurnStart 空 payload round-trip") {
    agent::EvAgentStart as;
    nlohmann::json js = dumpL2(as);
    CHECK(js == nlohmann::json::parse(R"({"type":"agent_start"})"));
    auto as2 = js.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvAgentStart>(as2));
    CHECK(dumpL2(as2) == js);

    agent::EvTurnStart ts;
    nlohmann::json jt = dumpL2(ts);
    CHECK(jt == nlohmann::json::parse(R"({"type":"turn_start"})"));
    auto ts2 = jt.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvTurnStart>(ts2));
    CHECK(dumpL2(ts2) == jt);
}

TEST_CASE("EvAgentEnd round-trip：messages 数组") {
    ai::UserMessage u1;
    u1.content = std::string{"q1"};
    ai::UserMessage u2;
    u2.content = std::string{"q2"};

    agent::EvAgentEnd e;
    e.messages = {u1, u2};

    nlohmann::json j = dumpL2(e);
    CHECK(j.at("type") == "agent_end");
    CHECK(j.at("messages").size() == 2);
    CHECK(j.at("messages").at(1).at("role") == "user");
    auto e2 = j.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvAgentEnd>(e2));
    CHECK(std::get<agent::EvAgentEnd>(e2).messages.size() == 2);
    CHECK(dumpL2(e2) == j);
}

TEST_CASE("EvTurnEnd round-trip：message 与 toolResults") {
    ai::AssistantMessage am = makePartial();
    ai::ToolResultMessage tr;
    tr.toolCallId = "call_001";
    tr.toolName = "read";

    agent::EvTurnEnd e;
    e.message = am;
    e.toolResults = {tr};

    nlohmann::json j = dumpL2(e);
    CHECK(j.at("type") == "turn_end");
    CHECK(j.at("message").at("role") == "assistant");
    CHECK(j.at("toolResults").at(0).at("role") == "toolResult");
    auto e2 = j.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvTurnEnd>(e2));
    CHECK(dumpL2(e2) == j);
}

TEST_CASE("EvMessageStart/End round-trip") {
    ai::UserMessage u;
    u.content = std::string{"hi"};

    agent::EvMessageStart s;
    s.message = u;
    nlohmann::json js = dumpL2(s);
    CHECK(js.at("type") == "message_start");
    CHECK(js.at("message").at("role") == "user");
    auto s2 = js.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvMessageStart>(s2));
    CHECK(dumpL2(s2) == js);

    agent::EvMessageEnd e;
    e.message = u;
    nlohmann::json je = dumpL2(e);
    CHECK(je.at("type") == "message_end");
    auto e2 = je.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvMessageEnd>(e2));
    CHECK(dumpL2(e2) == je);
}

TEST_CASE("EvMessageUpdate round-trip：内嵌 AssistantMessageEvent") {
    ai::AssistantMessage partial = makePartial();
    ai::EvTextDelta inner;
    inner.contentIndex = 0;
    inner.delta = "流";
    inner.partial = partial;

    agent::EvMessageUpdate e;
    e.message = partial;
    e.assistantMessageEvent = inner;

    nlohmann::json j = dumpL2(e);
    CHECK(j.at("type") == "message_update");
    CHECK(j.at("message").at("role") == "assistant");
    CHECK(j.at("assistantMessageEvent").at("type") == "text_delta");
    CHECK(j.at("assistantMessageEvent").at("delta") == "流");
    CHECK(j.at("assistantMessageEvent").at("partial").at("usage").at("input") == 10);

    auto e2 = j.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvMessageUpdate>(e2));
    const auto& upd = std::get<agent::EvMessageUpdate>(e2);
    CHECK(std::holds_alternative<ai::EvTextDelta>(upd.assistantMessageEvent));
    CHECK(std::get<ai::EvTextDelta>(upd.assistantMessageEvent).delta == "流");
    CHECK(dumpL2(e2) == j);
}

TEST_CASE("EvToolExecution 三段 round-trip（start/update/end）") {
    const nlohmann::json args = nlohmann::json::parse(R"({"path":"README.md","line":3})");

    agent::EvToolExecutionStart s;
    s.toolCallId = "call_001";
    s.toolName = "read";
    s.args = args;
    nlohmann::json js = dumpL2(s);
    CHECK(js.at("type") == "tool_execution_start");
    CHECK(js.at("args").at("line") == 3);
    auto s2 = js.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvToolExecutionStart>(s2));
    CHECK(dumpL2(s2) == js);

    agent::EvToolExecutionUpdate u;
    u.toolCallId = "call_001";
    u.toolName = "read";
    u.args = args;
    u.partialResult = nlohmann::json::parse(R"({"lines":10})");
    nlohmann::json ju = dumpL2(u);
    CHECK(ju.at("type") == "tool_execution_update");
    CHECK(ju.at("partialResult").at("lines") == 10);
    auto u2 = ju.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvToolExecutionUpdate>(u2));
    CHECK(dumpL2(u2) == ju);

    agent::EvToolExecutionEnd e;
    e.toolCallId = "call_001";
    e.toolName = "read";
    e.result = nlohmann::json::parse(R"({"totalLines":42})");
    e.isError = false;
    nlohmann::json je = dumpL2(e);
    CHECK(je.at("type") == "tool_execution_end");
    CHECK(je.at("result").at("totalLines") == 42);
    CHECK(je.at("isError") == false);
    auto e2 = je.get<agent::AgentEvent>();
    CHECK(std::holds_alternative<agent::EvToolExecutionEnd>(e2));
    CHECK(dumpL2(e2) == je);
}

TEST_CASE("未知 L2 事件 type 抛错且带 type 值") {
    nlohmann::json j = nlohmann::json::parse(R"({"type":"alien_l2"})");
    agent::AgentEvent e;
    CHECK_THROWS_WITH(j.get_to(e), "unknown agent event type: alien_l2");
}
