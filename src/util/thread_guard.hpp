#pragma once
#include <thread>
#include <utility>

namespace pi {
// RAII：析构时 join（若 joinable）。替代 C++20 jthread 的自动 join。
class ThreadGuard {
public:
    explicit ThreadGuard(std::thread t) noexcept : t_(std::move(t)) {}
    ThreadGuard(ThreadGuard&& o) noexcept : t_(std::move(o.t_)) {}

    // 移动赋值：先 join 自己持有的，再接管 other 的；自移动为无操作
    ThreadGuard& operator=(ThreadGuard&& other) noexcept {
        if (this != &other) {
            if (t_.joinable()) t_.join();
            t_ = std::move(other.t_);
        }
        return *this;
    }

    // join if joinable（join 过或被移空后析构为安全无操作）
    ~ThreadGuard() {
        if (t_.joinable()) t_.join();
    }

    bool joinable() const noexcept { return t_.joinable(); }
    void join() {
        if (t_.joinable()) t_.join();
    }
    std::thread::id get_id() const noexcept { return t_.get_id(); }

    ThreadGuard(const ThreadGuard&) = delete;
    ThreadGuard& operator=(const ThreadGuard&) = delete;
private:
    std::thread t_;
};
} // namespace pi
