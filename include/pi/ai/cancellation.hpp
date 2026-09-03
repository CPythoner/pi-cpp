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
        std::lock_guard<std::mutex> lock(mtx_);
        if (cancelled_.load(std::memory_order_acquire)) return;
        cancelled_.store(true, std::memory_order_release);
        for (auto& entry : callbacks_) {
            if (entry.second) entry.second();
        }
        callbacks_.clear();
        cv_.notify_all();
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
        std::lock_guard<std::mutex> lock(mtx_);
        if (cancelled_.load(std::memory_order_acquire)) {
            if (cb) cb();
            return 0;
        }
        const std::uint64_t id = ++nextId_;
        callbacks_.emplace(id, std::move(cb));
        return id;
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
        : tokens_(tokens) {
        ids_.reserve(tokens_.size());
        for (auto& token : tokens_) {
            ids_.push_back(token->registerCallback([this] { combined_.request(); }));
        }
    }

    ~CombinedCancellation() {
        for (std::size_t i = 0; i < tokens_.size(); ++i) {
            tokens_[i]->unregisterCallback(ids_[i]);
        }
    }

    CombinedCancellation(const CombinedCancellation&) = delete;
    CombinedCancellation& operator=(const CombinedCancellation&) = delete;

    const CancellationToken& token() const { return combined_; }

private:
    std::vector<std::shared_ptr<CancellationToken>> tokens_;
    std::vector<std::uint64_t> ids_;
    CancellationToken combined_;
};

} // namespace pi::ai
