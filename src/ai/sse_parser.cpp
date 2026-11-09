#include "ai/sse_parser.hpp"

#include <algorithm>
#include <cctype>
#include <limits>
#include <stdexcept>
#include <utility>

namespace pi::ai::detail {

namespace {

std::optional<std::int64_t> parseRetry(std::string_view value) {
    if (value.empty()) return std::nullopt;
    if (!std::all_of(value.begin(), value.end(), [](unsigned char ch) {
            return std::isdigit(ch) != 0;
        })) {
        return std::nullopt;
    }

    std::int64_t result = 0;
    for (const char ch : value) {
        const auto digit = static_cast<std::int64_t>(ch - '0');
        if (result > (std::numeric_limits<std::int64_t>::max() - digit) / 10) {
            return std::nullopt;
        }
        result = result * 10 + digit;
    }
    return result;
}

} // namespace

SseParser::SseParser(Handler handler)
    : handler_(std::move(handler)) {
    if (!handler_) {
        throw std::invalid_argument("SseParser requires a handler");
    }
}

void SseParser::feed(std::string_view chunk) {
    buffer_.append(chunk.data(), chunk.size());
    parseAvailable(false);
}

void SseParser::finish() {
    parseAvailable(true);
}

void SseParser::parseAvailable(bool finishing) {
    std::size_t offset = 0;

    while (offset < buffer_.size()) {
        const auto pos = buffer_.find_first_of("\r\n", offset);
        if (pos == std::string::npos) break;

        std::size_t terminatorLength = 1;
        if (buffer_[pos] == '\r') {
            if (pos + 1 == buffer_.size() && !finishing) {
                break; // A CRLF pair may be split across transport chunks.
            }
            if (pos + 1 < buffer_.size() && buffer_[pos + 1] == '\n') {
                terminatorLength = 2;
            }
        }

        consumeLine(std::string_view(buffer_).substr(offset, pos - offset));
        offset = pos + terminatorLength;
    }

    if (offset != 0) {
        buffer_.erase(0, offset);
    }

    if (finishing && !buffer_.empty()) {
        consumeLine(buffer_);
        buffer_.clear();
    }
}

void SseParser::consumeLine(std::string_view line) {
    if (line.empty()) {
        dispatch();
        return;
    }

    if (line.front() == ':') {
        return; // comment / heartbeat
    }

    const auto colon = line.find(':');
    const auto field = colon == std::string_view::npos ? line : line.substr(0, colon);

    std::string_view value;
    if (colon != std::string_view::npos) {
        value = line.substr(colon + 1);
        if (!value.empty() && value.front() == ' ') {
            value.remove_prefix(1);
        }
    }

    if (field == "data") {
        data_.append(value.data(), value.size());
        data_.push_back('\n');
        hasData_ = true;
    } else if (field == "event") {
        eventType_.assign(value.data(), value.size());
    } else if (field == "id") {
        if (value.find('\0') == std::string_view::npos) {
            lastEventId_.assign(value.data(), value.size());
        }
    } else if (field == "retry") {
        retryMs_ = parseRetry(value);
    }
}

void SseParser::dispatch() {
    if (!hasData_) {
        eventType_.clear();
        retryMs_.reset();
        return;
    }

    if (!data_.empty() && data_.back() == '\n') {
        data_.pop_back();
    }

    SseEvent event;
    event.event = eventType_.empty() ? "message" : eventType_;
    event.data = std::move(data_);
    event.id = lastEventId_;
    event.retryMs = retryMs_;

    data_.clear();
    eventType_.clear();
    retryMs_.reset();
    hasData_ = false;

    handler_(std::move(event));
}

} // namespace pi::ai::detail
