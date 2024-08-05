#include "nyaio/async.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

static int dummy_global_int;

static auto async_task_2() -> task<int &> {
    co_return dummy_global_int;
}

static auto async_task_1() -> task<std::string> {
    int &value = co_await async_task_2();
    CHECK(value == 0);
    co_return "Hello, world!";
}

static auto async_task_0(io_context &ctx) -> task<> {
    std::string str = co_await async_task_1();
    CHECK(str == "Hello, world!");

    int value = co_await async_task_2();
    CHECK(value == 0);

    str = co_await async_task_1();
    CHECK(str == "Hello, world!");

    ctx.stop();
}

TEST_CASE("async task calls") {
    // use 1 worker thread
    io_context ctx(1);
    ctx.schedule(async_task_0(ctx));
    ctx.run();
}

static auto async_task_yield_2() -> task<int &> {
    while (true) {
        co_yield dummy_global_int;
    }
}

static auto async_task_yield_1() -> task<int> {
    int value = 0;
    while (true) {
        co_yield value++;
    }
}

static auto async_task_yield_0(io_context &ctx) -> task<> {
    auto generator = async_task_yield_1();
    for (int i = 0; i < 100; ++i) {
        int value = co_await generator;
        CHECK(value == i);
    }

    auto generator2 = async_task_yield_2();
    for (int i = 0; i < 100; ++i) {
        int &value = co_await generator2;
        CHECK(&value == &dummy_global_int);
    }

    ctx.stop();
}

TEST_CASE("async task yields") {
    io_context ctx(1);
    ctx.schedule(async_task_yield_0(ctx));
    ctx.run();
}
