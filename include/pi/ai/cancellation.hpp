#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <map>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace pi::ai {

class CancellationToken {
public:
    using Callback = std::function<void()>;
    using Clock = std::chrono::steady_clock;

    void request() {
        std::vector<Callback> callbacks;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (cancelled_.load(std::memory_order_acquire)) return;

            cancelled_.store(true, std::memory_order_release);
            callbacks.reserve(callbacks_.size());
            for (auto& entry : callbacks_) {
                callbacks.push_back(std::move(entry.second));
            }
            callbacks_.clear();
        }

        cv_.notify_all();
        for (auto& callback : callbacks) {
            if (callback) callback();
        }
    }

    bool requested() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    bool wait_for(std::chrono::milliseconds timeout) const {
        std::unique_lock<std::mutex> lock(mtx_);
        const auto deadline = Clock::now() + timeout;
        return cv_.wait_until(lock, deadline, [this] {
            return cancelled_.load(std::memory_order_acquire);
        });
    }

    std::uint64_t registerCallback(Callback cb) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!cancelled_.load(std::memory_order_acquire)) {
                const std::uint64_t id = ++nextId_;
                callbacks_.emplace(id, std::move(cb));
                return id;
            }
        }

        if (cb) cb();
        return 0;
    }

    bool unregisterCallback(std::uint64_t id) {
        std::lock_guard<std::mutex> lock(mtx_);
        return callbacks_.erase(id) != 0;
    }

private:
    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;
    std::atomic<bool> cancelled_{false};
    std::uint64_t nextId_ = 0;
    std::map<std::uint64_t, Callback> callbacks_;
};

class CombinedCancellation {
public:
    explicit CombinedCancellation(
        std::initializer_list<std::shared_ptr<CancellationToken>> tokens)
        : state_(std::make_shared<State>()) {
        registrations_.reserve(tokens.size());
        const std::weak_ptr<State> weakState = state_;

        for (const auto& token : tokens) {
            if (!token) continue;

            const auto id = token->registerCallback([weakState] {
                if (const auto state = weakState.lock()) {
                    state->combined.request();
                }
            });
            registrations_.push_back({token, id});
        }
    }

    ~CombinedCancellation() {
        state_.reset();
        for (auto& registration : registrations_) {
            if (registration.id != 0) {
                registration.token->unregisterCallback(registration.id);
            }
        }
    }

    CombinedCancellation(const CombinedCancellation&) = delete;
    CombinedCancellation& operator=(const CombinedCancellation&) = delete;

    CancellationToken& token() noexcept { return state_->combined; }
    const CancellationToken& token() const noexcept { return state_->combined; }

private:
    struct State {
        CancellationToken combined;
    };

    struct Registration {
        std::shared_ptr<CancellationToken> token;
        std::uint64_t id = 0;
    };

    std::shared_ptr<State> state_;
    std::vector<Registration> registrations_;
};

} // namespace pi::ai
