#include "nyaio/network.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

static auto server(tcp_stream stream) noexcept -> task<> {
    char ground_truth[1024];
    __builtin_memset(ground_truth, 0x55, sizeof(ground_truth));

    char buffer[1024];
    while (true) {
        auto result = co_await stream.receive_async(buffer, sizeof(buffer));
        CHECK(result.has_value());

        if (*result == 0)
            break;

        CHECK(__builtin_memcmp(buffer, ground_truth, *result) == 0);
    }
}

static auto listener(uint16_t port) noexcept -> task<> {
    tcp_server s;

    std::errc error = s.listen({ipv4_loopback, port});
    CHECK(error == std::errc{});

    auto acceptor = s.acceptor();
    while (true) {
        auto connection = co_await acceptor;
        CHECK(connection.has_value());
        co_await schedule_awaitable(server(std::move(*connection)));
    }
}

static auto client(uint16_t port, io_context &ctx, std::atomic_int &count) noexcept -> task<> {
    tcp_stream s;

    std::errc error = s.connect({ipv4_loopback, port});
    CHECK(error == std::errc{});

    char buffer[1024];
    __builtin_memset(buffer, 0x55, sizeof(buffer));
    for (size_t i = 0; i < 5; ++i) {
        auto result = co_await s.send_async(buffer, sizeof(buffer));
        CHECK(result.has_value());
        CHECK(*result == sizeof(buffer));
    }

    s.close();

    if (count.fetch_sub(1, std::memory_order_relaxed) == 1)
        ctx.stop();
}

TEST_CASE("async tcp stream send-receive") {
    constexpr int client_count = 1000;
    constexpr uint16_t port    = 23333;

    io_context ctx;
    std::atomic_int count = client_count;

    ctx.dispatch(listener, port);
    for (size_t i = 0; i < client_count; ++i)
        ctx.schedule(client(port, ctx, count));

    ctx.run();
}

static constexpr uint8_t pingpong_server_unit = 0x55;
static constexpr uint8_t pingpong_client_unit = 0xAA;

static auto pingpong_server(tcp_stream stream) noexcept -> task<> {
    uint8_t peer[1024];
    __builtin_memset(peer, pingpong_client_unit, sizeof(peer));

    uint8_t local[1024];
    __builtin_memset(local, pingpong_server_unit, sizeof(local));

    uint8_t buffer[1024];
    while (true) {
        auto result = co_await stream.receive_async(buffer, sizeof(buffer));
        CHECK(result.has_value());

        if (*result == 0)
            break;

        CHECK(__builtin_memcmp(buffer, peer, *result) == 0);
        co_await stream.send_async(local, sizeof(local));
    }
}

static auto pingpong_listener(uint16_t port) noexcept -> task<> {
    tcp_server s;

    std::errc error = s.listen({ipv4_loopback, port});
    CHECK(error == std::errc{});

    while (true) {
        auto connection = co_await s.accept_async();
        CHECK(connection.has_value());
        co_await schedule_awaitable(pingpong_server(std::move(*connection)));
    }
}

static auto pingpong_client(uint16_t port, io_context &ctx,
                            std::atomic_int &count) noexcept -> task<> {
    tcp_stream s;

    std::errc error = s.connect({ipv4_loopback, port});
    CHECK(error == std::errc{});

    uint8_t peer[1024];
    __builtin_memset(peer, pingpong_server_unit, sizeof(peer));

    uint8_t local[1024];
    __builtin_memset(local, pingpong_client_unit, sizeof(local));

    uint8_t buffer[1024];
    for (size_t i = 0; i < 5; ++i) {
        auto result = co_await s.send_async(peer, sizeof(peer));
        CHECK(result.has_value());
        CHECK(*result == sizeof(peer));

        auto response = co_await s.receive_async(buffer, sizeof(buffer));
        CHECK(response.has_value());
        CHECK(*response == sizeof(buffer));
        CHECK(__builtin_memcmp(buffer, local, sizeof(buffer)) == 0);
    }

    s.close();

    if (count.fetch_sub(1, std::memory_order_relaxed) == 1)
        ctx.stop();
}

TEST_CASE("async tcp stream ping-pong") {
    constexpr int client_count = 1000;
    constexpr uint16_t port    = 23334;

    io_context ctx;
    std::atomic_int count = client_count;

    ctx.dispatch(pingpong_listener, port);
    for (size_t i = 0; i < client_count; ++i)
        ctx.schedule(pingpong_client(port, ctx, count));

    ctx.run();
}
