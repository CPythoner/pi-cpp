#include <doctest/doctest.h>

#include <pi/ai/cancellation.hpp>

#include <atomic>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

TEST_CASE("① request 幂等：重复 request 无副作用") {
    pi::ai::CancellationToken tok;
    CHECK_FALSE(tok.requested());
    int called = 0;
    tok.registerCallback([&called] { ++called; });
    tok.request();
    CHECK(tok.requested());
    CHECK_EQ(called, 1);
    tok.request();
    tok.request();
    CHECK(tok.requested());
    CHECK_EQ(called, 1);
}

TEST_CASE("② wait_for：超时返回 false，cancel 后立即 true") {
    pi::ai::CancellationToken tok;
    const auto t0 = std::chrono::steady_clock::now();
    CHECK_FALSE(tok.wait_for(std::chrono::milliseconds(30)));
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0);
    CHECK(elapsed >= std::chrono::milliseconds(25));
    CHECK_FALSE(tok.requested());

    tok.request();
    CHECK(tok.wait_for(std::chrono::milliseconds(0)));
    CHECK(tok.wait_for(std::chrono::milliseconds(100)));
}

TEST_CASE("② wait_for：被并发 request 唤醒而非超时（容忍虚假唤醒的谓词循环）") {
    pi::ai::CancellationToken tok;
    std::atomic<bool> result{false};
    std::atomic<bool> done{false};
    std::thread waiter([&] {
        result = tok.wait_for(std::chrono::seconds(10));
        done = true;
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK_FALSE(done.load());
    tok.request();
    waiter.join();
    CHECK(done.load());
    CHECK(result.load());
}

TEST_CASE("③ 回调恰好执行一次且按注册顺序") {
    pi::ai::CancellationToken tok;
    std::vector<int> order;
    const auto id1 = tok.registerCallback([&order] { order.push_back(1); });
    const auto id2 = tok.registerCallback([&order] { order.push_back(2); });
    const auto id3 = tok.registerCallback([&order] { order.push_back(3); });
    CHECK_NE(id1, 0);
    CHECK_NE(id1, id2);
    CHECK_NE(id2, id3);
    tok.request();
    CHECK_EQ(order, std::vector<int>({1, 2, 3}));
}

TEST_CASE("③ 已取消后注册的回调立即同步执行") {
    pi::ai::CancellationToken tok;
    tok.request();
    int called = 0;
    const auto id = tok.registerCallback([&called] { ++called; });
    CHECK_EQ(id, 0);
    CHECK_EQ(called, 1);
}

TEST_CASE("④ unregister 在 request 前生效则回调不执行") {
    pi::ai::CancellationToken tok;
    int called = 0;
    const auto id = tok.registerCallback([&called] { ++called; });
    CHECK(tok.unregisterCallback(id));
    tok.request();
    CHECK_EQ(called, 0);
    CHECK_FALSE(tok.unregisterCallback(id));
}

TEST_CASE("④ 已执行后的 unregister 返回 false") {
    pi::ai::CancellationToken tok;
    int called = 0;
    const auto id = tok.registerCallback([&called] { ++called; });
    tok.request();
    CHECK_EQ(called, 1);
    CHECK_FALSE(tok.unregisterCallback(id));
}

TEST_CASE("④ unregister 与并发 request 竞态：回调执行零或一次，无死锁") {
    pi::ai::CancellationToken tok;
    std::atomic<int> called{0};
    const auto id = tok.registerCallback([&called] { ++called; });
    std::thread requester([&] { tok.request(); });
    std::thread remover([&] { tok.unregisterCallback(id); });
    requester.join();
    remover.join();
    CHECK_LE(called.load(), 1);
    CHECK(tok.requested());
}

TEST_CASE("析构时回调表已清空：栈上引用不悬空") {
    int counter = 0;
    {
        pi::ai::CancellationToken tok;
        tok.registerCallback([&counter] { ++counter; });
        tok.request();
        CHECK(tok.wait_for(std::chrono::milliseconds(0)));
    }
    CHECK_EQ(counter, 1);
}

TEST_CASE("CombinedCancellation：任一子令牌取消则组合令牌取消") {
    auto a = std::make_shared<pi::ai::CancellationToken>();
    auto b = std::make_shared<pi::ai::CancellationToken>();
    {
        pi::ai::CombinedCancellation comb({a, b});
        CHECK_FALSE(comb.token().requested());
        b->request();
        CHECK(comb.token().requested());
    }
    a->request();
}

TEST_CASE("CombinedCancellation：构造时已有子令牌被取消") {
    auto a = std::make_shared<pi::ai::CancellationToken>();
    a->request();
    pi::ai::CombinedCancellation comb({a});
    CHECK(comb.token().requested());
}
