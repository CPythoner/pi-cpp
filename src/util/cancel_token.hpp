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

namespace pi {

// 语义对齐 std::stop_token（C++20）：一次性广播、幂等 request、回调恰好一次。
// v0.0.1 交付并全测；未来升 C++20 可平替为 jthread/stop_token。
//
// 实现注记：回调在 request() 持锁期间按注册顺序执行后清除（std::map 保序），
// 这同时保证了 unregisterCallback 会阻塞至进行中的 request 完成——
// CombinedCancellation 析构注销回调时因此不存在 use-after-free 窗口。
// 约束：回调内不得再调用本令牌的任何接口（std::mutex 不可重入）。
class CancellationToken {
public:
    using Callback = std::function<void()>;
    using Clock = std::chrono::steady_clock;

    // 幂等；已注册回调在锁内执行后清除
    void request() {
        std::lock_guard<std::mutex> lock(mtx_);
        if (cancelled_.load(std::memory_order_acquire)) return;  // 幂等：无副作用
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

    // 返回是否因 cancel 返回（false = 超时）；谓词循环容忍虚假唤醒
    bool wait_for(std::chrono::milliseconds timeout) const {
        std::unique_lock<std::mutex> lock(mtx_);
        const auto deadline = Clock::now() + timeout;
        return cv_.wait_until(lock, deadline, [this] {
            return cancelled_.load(std::memory_order_acquire);
        });
    }

    // 已取消则立即同步执行并返回 0；否则注册，cancel 时在 request() 线程执行
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

    // 取消注册（若尚未执行）
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

// 组合令牌：任一取消则整体取消（对应 pi 的 combineAbortSignals）
class CombinedCancellation {
public:
    explicit CombinedCancellation(
        std::initializer_list<std::shared_ptr<CancellationToken>> tokens)
        : tokens_(tokens) {
        ids_.reserve(tokens_.size());
        for (auto& t : tokens_) {
            // 子令牌已取消时 registerCallback 会立即执行回调（组合令牌随之取消）
            ids_.push_back(t->registerCallback([this] { combined_.request(); }));
        }
    }

    // 析构先移除回调再返回：unregister 会阻塞等待进行中的 request 完成，
    // 保证析构返回后子令牌再取消也不会触碰已亡的组合对象
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
    CancellationToken combined_;  // 声明在最后：构造函数体内注册回调时已就绪
};

} // namespace pi
