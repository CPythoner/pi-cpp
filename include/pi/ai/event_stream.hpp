#pragma once

#include <pi/ai/events.hpp>

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace pi::ai {

template <typename T, typename R = T>
class EventStream {
public:
    using IsComplete = std::function<bool(const T&)>;
    using ExtractResult = std::function<R(const T&)>;

    EventStream(IsComplete isComplete, ExtractResult extractResult)
        : state_(std::make_shared<State>(std::move(isComplete), std::move(extractResult))) {}

    void push(T event) {
        const bool complete = state_->isComplete(event);
        std::optional<R> extracted;
        if (complete) {
            extracted.emplace(state_->extractResult(event));
        }

        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (state_->done) return;

            if (complete) {
                state_->done = true;
                state_->finalResult = std::move(extracted);
            }
            state_->queue.push_back(std::move(event));
        }
        state_->cv.notify_all();
    }

    void end() {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->done = true;
        }
        state_->cv.notify_all();
    }

    void end(R result) {
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            if (!state_->finalResult) {
                state_->finalResult = std::move(result);
            }
            state_->done = true;
        }
        state_->cv.notify_all();
    }

    std::optional<T> next() {
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [this] {
            return !state_->queue.empty() || state_->done;
        });

        if (state_->queue.empty()) return std::nullopt;

        T event = std::move(state_->queue.front());
        state_->queue.pop_front();
        return event;
    }

    R result() const {
        std::unique_lock<std::mutex> lock(state_->mutex);
        state_->cv.wait(lock, [this] {
            return state_->finalResult.has_value() || state_->done;
        });

        if (!state_->finalResult) {
            throw std::logic_error("event stream ended without a final result");
        }
        return *state_->finalResult;
    }

    bool done() const {
        std::lock_guard<std::mutex> lock(state_->mutex);
        return state_->done;
    }

private:
    struct State {
        State(IsComplete complete, ExtractResult extract)
            : isComplete(std::move(complete)), extractResult(std::move(extract)) {}

        mutable std::mutex mutex;
        mutable std::condition_variable cv;
        std::deque<T> queue;
        bool done = false;
        std::optional<R> finalResult;
        IsComplete isComplete;
        ExtractResult extractResult;
    };

    std::shared_ptr<State> state_;
};

class AssistantMessageEventStream
    : public EventStream<AssistantMessageEvent, AssistantMessage> {
public:
    AssistantMessageEventStream()
        : EventStream(
              [](const AssistantMessageEvent& event) {
                  return std::holds_alternative<EvDone>(event) ||
                         std::holds_alternative<EvError>(event);
              },
              [](const AssistantMessageEvent& event) -> AssistantMessage {
                  if (const auto* done = std::get_if<EvDone>(&event)) {
                      return done->message;
                  }
                  if (const auto* error = std::get_if<EvError>(&event)) {
                      return error->error;
                  }
                  throw std::logic_error("unexpected assistant event for final result");
              }) {}
};

inline AssistantMessageEventStream createAssistantMessageEventStream() {
    return AssistantMessageEventStream{};
}

} // namespace pi::ai
