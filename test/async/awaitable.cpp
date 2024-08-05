#include "nyaio/async.hpp"

#include <doctest/doctest.h>

#include <fcntl.h>
#include <unistd.h>

using namespace nyaio;
using namespace std::chrono_literals;

static auto yield_awaitable_task(io_context &ctx, int &flag, int id) -> task<> {
    flag = id;
    while (flag == id)
        co_await yield_awaitable();

    CHECK(flag != id);
    flag = id;

    ctx.stop();
}

TEST_CASE("async yield awaitable") {
    io_context ctx(1);
    int flag = 0;

    ctx.schedule(yield_awaitable_task(ctx, flag, 0));
    ctx.schedule(yield_awaitable_task(ctx, flag, 1));

    ctx.run();
}

static auto schedule_awaitable_task(io_context &ctx) -> task<> {
    std::atomic<std::thread::id> tid;
    std::atomic<std::thread::id> child_tid;
    tid.store(std::this_thread::get_id(), std::memory_order_relaxed);

    auto t = [](std::atomic<std::thread::id> &tid,
                std::atomic<std::thread::id> &child_tid) -> task<> {
        CHECK(tid.load(std::memory_order_relaxed) == std::this_thread::get_id());
        child_tid.store(std::this_thread::get_id(), std::memory_order_relaxed);
        co_return;
    }(tid, child_tid);

    co_await schedule_awaitable<void>(t);
    while (!t.done())
        co_await yield_awaitable();

    CHECK(child_tid.load(std::memory_order_relaxed) == std::this_thread::get_id());
    ctx.stop();
}

TEST_CASE("async schedule awaitable") {
    io_context ctx(1);
    ctx.schedule(schedule_awaitable_task(ctx));
    ctx.run();
}

static auto timeout_awaitable_task(io_context &ctx) -> task<> {
    for (size_t i = 0; i < 5; ++i) {
        auto start = std::chrono::steady_clock::now();
        co_await timeout_awaitable(100ms);
        auto end = std::chrono::steady_clock::now();
        CHECK(end - start >= 100ms);
    }

    ctx.stop();
}

TEST_CASE("async timeout awaitable") {
    io_context ctx(1);
    ctx.schedule(timeout_awaitable_task(ctx));
    ctx.run();
}

TEST_CASE("async read awaitable") {
    io_context ctx(1);

    auto read_task = [](io_context &ctx) -> task<> {
        int fd = ::open("/dev/zero", O_RDONLY);
        CHECK(fd >= 0);

        char buf[1024];
        auto result = co_await read_awaitable(fd, buf, sizeof(buf), 0);
        CHECK(result.has_value());
        CHECK(*result == sizeof(buf));
        for (auto c : buf)
            CHECK(c == 0);

        ::close(fd);
        ctx.stop();
    }(ctx);

    ctx.schedule(read_task);
    ctx.run();
}

TEST_CASE("async write awaitable") {
    io_context ctx(1);

    auto write_task = [](io_context &ctx) -> task<> {
        int fd = ::open("/dev/null", O_WRONLY);
        CHECK(fd >= 0);

        char buf[1024];
        __builtin_memset(buf, 0, sizeof(buf));

        auto result = co_await write_awaitable(fd, buf, sizeof(buf), 0);
        CHECK(result.has_value());
        CHECK(*result == sizeof(buf));

        ::close(fd);
        ctx.stop();
    }(ctx);

    ctx.schedule(write_task);
    ctx.run();
}
