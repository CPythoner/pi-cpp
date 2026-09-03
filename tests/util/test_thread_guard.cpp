#include <doctest/doctest.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include "util/thread_guard.hpp"

TEST_CASE("析构自动 join（线程确实跑完）") {
    std::atomic<int> count{0};
    {
        pi::ThreadGuard guard{std::thread([&count] {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            ++count;
        })};
        CHECK(guard.joinable());
    }  // 析构必须 join：此后计数可见
    CHECK_EQ(count.load(), 1);
}

TEST_CASE("移动构造后原 guard 不再持有线程") {
    std::atomic<int> count{0};
    std::unique_ptr<pi::ThreadGuard> moved;
    {
        pi::ThreadGuard original{std::thread([&count] {
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
            ++count;
        })};
        moved = std::make_unique<pi::ThreadGuard>(std::move(original));
        CHECK_FALSE(original.joinable());  // 原 guard 已不持有：析构不 join
        CHECK(moved->joinable());
    }  // original 析构（无事发生）
    CHECK_EQ(count.load(), 0);  // 线程尚未结束（30ms），尚未计数
    moved.reset();              // moved 析构 join
    CHECK_EQ(count.load(), 1);
}

TEST_CASE("join() 后再析构安全") {
    std::atomic<int> count{0};
    {
        pi::ThreadGuard guard{std::thread([&count] { ++count; })};
        guard.join();
        CHECK_EQ(count.load(), 1);
        CHECK_FALSE(guard.joinable());
    }  // join 过的 guard 再析构：安全无操作
    CHECK_EQ(count.load(), 1);
}

TEST_CASE("get_id 与线程一致") {
    pi::ThreadGuard guard{std::thread([] {})};
    // 注意：不能用 CHECK_NE 直接比较 std::thread::id——doctest 会尝试字符串化操作数，
    // libstdc++13 下 operator<< 的实例化在 <thread> 内部失败（平台差异）。
    // 包装为 bool 后 doctest 无需字符串化 thread::id。
    const bool hasValidId = (guard.get_id() != std::thread::id());
    CHECK(hasValidId);
    guard.join();
}

TEST_CASE("无线程的 guard：join 与析构均为无操作") {
    pi::ThreadGuard guard{std::thread()};
    CHECK_FALSE(guard.joinable());
    guard.join();  // 不得崩溃
}

TEST_CASE("移动赋值：先 join 自己持有的线程，再接管对方线程") {
    std::atomic<int> a{0}, b{0};
    pi::ThreadGuard g1{std::thread([&a] {
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        ++a;
    })};
    pi::ThreadGuard g2{std::thread([&b] { ++b; })};
    g1 = std::move(g2);
    CHECK_EQ(a.load(), 1);       // g1 原线程在赋值内被 join
    CHECK_EQ(b.load(), 1);       // g2 线程很快完成
    CHECK_FALSE(g2.joinable());  // 被移走的 guard 为空
    CHECK(g1.joinable());        // g1 现持有原 g2 的线程
    g1.join();
}

TEST_CASE("自移动赋值安全（经引用规避编译器自移动告警）") {
    std::atomic<int> count{0};
    pi::ThreadGuard guard{std::thread([&count] { ++count; })};
    pi::ThreadGuard& ref = guard;
    guard = std::move(ref);  // 自移动：实现选择为无操作
    CHECK(guard.joinable());
    guard.join();
    CHECK_EQ(count.load(), 1);
}
