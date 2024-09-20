#include "nyaio/task.hpp"

#include <doctest/doctest.h>

#include <cstring>

#include <fcntl.h>
#include <sys/un.h>
#include <unistd.h>

using namespace nyaio;
using namespace std::chrono_literals;

namespace {

int DummyGlobalInt;

auto task2() noexcept -> Task<int &> {
    co_return DummyGlobalInt;
}

auto task1() noexcept -> Task<std::string> {
    int &value = co_await task2();
    CHECK(value == 0);
    co_return "Hello, world!";
}

auto task0(IoContext &ctx) noexcept -> Task<> {
    std::string str = co_await task1();
    CHECK(str == "Hello, world!");

    int value = co_await task2();
    CHECK(value == 0);

    str = co_await task1();
    CHECK(str == "Hello, world!");

    ctx.stop();
}

} // namespace

TEST_CASE("[task] Basic Task") {
    IoContext ctx(1);
    ctx.schedule(task0(ctx));
    ctx.run();
}

namespace {

auto generator2() noexcept -> Task<int &> {
    while (true) {
        co_yield DummyGlobalInt;
    }
}

auto generator1() noexcept -> Task<int> {
    int value = 0;
    while (true) {
        co_yield value++;
    }
}

auto generatorTask(IoContext &ctx) noexcept -> Task<> {
    auto gen1 = generator1();
    for (int i = 0; i < 100; ++i) {
        int value = co_await gen1;
        CHECK(value == i);
    }

    auto gen2 = generator2();
    for (int i = 0; i < 100; ++i) {
        int &value = co_await gen2;
        CHECK(&value == &DummyGlobalInt);
    }

    ctx.stop();
}

} // namespace

TEST_CASE("[task] Basic Generator") {
    IoContext ctx(1);
    ctx.schedule(generatorTask(ctx));
    ctx.run();
}

namespace {

auto yieldAwaitableTask(IoContext &ctx, int &flag, int id) noexcept -> Task<> {
    flag = id;
    while (flag == id)
        co_await YieldAwaitable();

    CHECK(flag != id);
    flag = id;

    if (id == 0)
        ctx.stop();
}

} // namespace

TEST_CASE("[task] YieldAwaitable") {
    IoContext ctx(1);
    int flag = 0;

    ctx.schedule(yieldAwaitableTask(ctx, flag, 0));
    ctx.schedule(yieldAwaitableTask(ctx, flag, 1));

    ctx.run();
}

namespace {

auto timeoutAwaitableTask(IoContext &ctx) noexcept -> Task<> {
    for (std::size_t i = 0; i < 5; ++i) {
        auto start = std::chrono::steady_clock::now();
        co_await TimeoutAwaitable(100ms);
        auto end = std::chrono::steady_clock::now();
        CHECK(end - start >= 100ms);
    }

    ctx.stop();
}

} // namespace

TEST_CASE("[task] TimeoutAwaitable") {
    IoContext ctx(1);
    ctx.dispatch(timeoutAwaitableTask, ctx);
    ctx.run();
}

namespace {

auto scheduleAwaitableTask(IoContext &ctx) noexcept -> Task<> {
    std::atomic<pid_t> tid;
    std::atomic<pid_t> childTid;

    tid.store(gettid(), std::memory_order_relaxed);

    auto t = [](std::atomic<pid_t> &tid, std::atomic<pid_t> &childTid) -> Task<> {
        CHECK(tid.load(std::memory_order_relaxed) == gettid());
        childTid.store(gettid(), std::memory_order_relaxed);
        co_return;
    }(tid, childTid);

    // The sub-coroutine runs in the same thread as the parent coroutine.
    co_await ScheduleAwaitable(t);
    while (!t.isCompleted())
        co_await YieldAwaitable();

    CHECK(childTid.load(std::memory_order_relaxed) == gettid());
    ctx.stop();
}

} // namespace

TEST_CASE("[task] ScheduleAwaitable") {
    IoContext ctx(1);
    ctx.dispatch(scheduleAwaitableTask, ctx);
    ctx.run();
}

namespace {

auto waitAllTask0() noexcept -> Task<int> {
    co_await TimeoutAwaitable(100ms);
    co_return 1;
}

auto waitAllTask1() noexcept -> Task<std::string> {
    co_return "Hello, world!";
}

auto waitAllTask2() noexcept -> Task<std::string> {
    std::string str  = co_await waitAllTask1();
    str             += co_await waitAllTask1();
    co_return str;
}

auto waitAllAwaitableTask(IoContext &ctx) noexcept -> Task<> {
    auto awaitable    = waitAll(waitAllTask0(), waitAllTask2());
    auto [value, str] = co_await awaitable;
    CHECK(value == 1);
    CHECK(str == "Hello, world!Hello, world!");
    ctx.stop();
}

} // namespace

TEST_CASE("[task] WaitAllAwaitable") {
    IoContext ctx(1);
    ctx.schedule(waitAllAwaitableTask(ctx));
    ctx.run();
}

namespace {

auto readAwaitableTask(IoContext &ctx) noexcept -> Task<> {
    int zero = ::open("/dev/zero", O_RDONLY);
    CHECK(zero >= 0);

    constexpr char zeros[1024] = {};
    char buffer[1024];

    for (std::size_t i = 0; i < std::size(buffer); ++i) {
        auto [bytes, error] = co_await ReadAwaitable(zero, buffer, i, 0);
        CHECK(error == std::errc{});
        CHECK(bytes == i);
        CHECK(std::memcmp(buffer, zeros, i) == 0);
    }

    ::close(zero);
    ctx.stop();
}

} // namespace

TEST_CASE("[task] ReadAwaitable") {
    IoContext ctx(1);
    ctx.schedule(readAwaitableTask(ctx));
    ctx.run();
}

namespace {

auto writeAwaitableTask(IoContext &ctx) noexcept -> Task<> {
    int null = ::open("/dev/null", O_WRONLY);
    CHECK(null >= 0);

    constexpr char zeros[1024] = {};

    for (std::size_t i = 0; i < std::size(zeros); ++i) {
        auto [bytes, error] = co_await WriteAwaitable(null, zeros, i, 0);
        CHECK(error == std::errc{});
        CHECK(bytes == i);
    }

    ::close(null);
    ctx.stop();
}

} // namespace

TEST_CASE("[task] WriteAwaitable") {
    IoContext ctx(1);
    ctx.schedule(writeAwaitableTask(ctx));
    ctx.run();
}

namespace {

auto sendAwaitableTask(IoContext &ctx, const char *address, std::atomic_bool &couldConnect) noexcept
    -> Task<> {
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address, std::size(addr.sun_path));

    while (!couldConnect.load(std::memory_order_relaxed))
        co_await YieldAwaitable();

    int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(s >= 0);

    std::errc e =
        co_await ConnectAwaitable(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    CHECK(e == std::errc{});

    for (std::size_t i = 0; i < 1024; ++i) {
        auto [bytes, error] = co_await SendAwaitable(s, &i, sizeof(i), MSG_NOSIGNAL);
        CHECK(error == std::errc{});
        CHECK(bytes == sizeof(i));
    }

    ::close(s);
}

auto recvAwaitableTask(IoContext &ctx, const char *address, std::atomic_bool &couldConnect) noexcept
    -> Task<> {
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address, std::size(addr.sun_path));

    int s = ::socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(s >= 0);

    int ret = ::bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    CHECK(ret >= 0);

    ret = ::listen(s, 1);
    CHECK(ret >= 0);

    couldConnect.store(true, std::memory_order_relaxed);

    auto [client, e] = co_await AcceptAwaitable(s, nullptr, nullptr, SOCK_CLOEXEC);
    CHECK(e == std::errc{});
    CHECK(client >= 0);

    for (std::size_t i = 0; i < 1024; ++i) {
        std::size_t buffer;
        auto [bytes, error] = co_await ReceiveAwaitable(client, &buffer, sizeof(buffer), 0);
        CHECK(error == std::errc{});
        CHECK(bytes == sizeof(buffer));
        CHECK(buffer == i);
    }

    std::size_t buffer;
    auto [bytes, error] = co_await ReceiveAwaitable(client, &buffer, sizeof(buffer), 0);
    CHECK(error == std::errc{});
    CHECK(bytes == 0);

    ::close(client);
    ::close(s);

    ctx.stop();
}

} // namespace

TEST_CASE("[task] Send/ReceiveAwaitable") {
    IoContext ctx(1);
    std::atomic_bool couldConnect = false;

    constexpr const char *address = "nyaio-send-recv.sock";
    ctx.schedule(sendAwaitableTask(ctx, address, couldConnect));
    ctx.schedule(recvAwaitableTask(ctx, address, couldConnect));

    ctx.run();
    ::unlink(address);
}

namespace {

auto sendtoAwaitableTask(IoContext &ctx, const char *address,
                         std::atomic_bool &couldConnect) noexcept -> Task<> {
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address, std::size(addr.sun_path));

    while (!couldConnect.load(std::memory_order_relaxed))
        co_await YieldAwaitable();

    int s = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    CHECK(s >= 0);

    for (std::size_t i = 0; i < 1024; ++i) {
        auto [bytes, error] =
            co_await SendToAwaitable(s, &i, sizeof(i), MSG_NOSIGNAL,
                                     reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
        CHECK(error == std::errc{});
        CHECK(bytes == sizeof(i));
    }

    ::close(s);
}

auto recvfromAwaitableTask(IoContext &ctx, const char *address,
                           std::atomic_bool &couldConnect) noexcept -> Task<> {
    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    std::strncpy(addr.sun_path, address, std::size(addr.sun_path));

    int s = ::socket(AF_UNIX, SOCK_DGRAM, 0);
    CHECK(s >= 0);

    int ret = ::bind(s, reinterpret_cast<struct sockaddr *>(&addr), sizeof(addr));
    CHECK(ret >= 0);

    couldConnect.store(true, std::memory_order_relaxed);

    for (std::size_t i = 0; i < 1024; ++i) {
        std::size_t buffer;
        auto [bytes, error] =
            co_await ReceiveFromAwaitable(s, &buffer, sizeof(buffer), 0, nullptr, 0);
        CHECK(error == std::errc{});
        CHECK(bytes == sizeof(buffer));
        CHECK(buffer == i);
    }

    ::close(s);
    ctx.stop();
}

} // namespace

TEST_CASE("[task] SendTo/ReceiveFromAwaitable") {
    IoContext ctx(1);
    std::atomic_bool couldConnect = false;

    constexpr const char *address = "nyaio-sendto-recvfrom.sock";
    ctx.schedule(sendtoAwaitableTask(ctx, address, couldConnect));
    ctx.schedule(recvfromAwaitableTask(ctx, address, couldConnect));

    ctx.run();
    ::unlink(address);
}
