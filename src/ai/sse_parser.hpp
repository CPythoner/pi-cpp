#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace pi::ai::detail {

struct SseEvent {
    std::string event = "message";
    std::string data;
    std::string id;
    std::optional<std::int64_t> retryMs;
};

class SseParser {
public:
    using Handler = std::function<void(SseEvent)>;

    explicit SseParser(Handler handler);

    // Feed arbitrary transport chunks. Chunk boundaries may split UTF-8 bytes,
    // field names, values, CRLF pairs, or event separators.
    void feed(std::string_view chunk);

    // Flushes a final complete line if the transport closes. A pending SSE event
    // without the required blank-line delimiter is intentionally not dispatched.
    void finish();

private:
    void parseAvailable(bool finishing);
    void consumeLine(std::string_view line);
    void dispatch();

    Handler handler_;
    std::string buffer_;
    std::string eventType_;
    std::string data_;
    std::string lastEventId_;
    std::optional<std::int64_t> retryMs_;
    bool hasData_ = false;
};

} // namespace pi::ai::detail
