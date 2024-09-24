#include "nyaio/io.hpp"

#include <doctest/doctest.h>

using namespace nyaio;
using namespace std::chrono_literals;

namespace {

inline constexpr std::size_t NumPingPongConnections = 100;
inline constexpr std::size_t PingPongTotalSize      = 1000 * 1024;
inline constexpr std::size_t PingPongBufferSize     = 1024;

auto pingPongServer(IoContext &ctx, TcpStream stream, std::atomic_int &completedServers,
                    std::atomic_int &completedClients) noexcept -> Task<> {
    char buffer[PingPongBufferSize];
    std::size_t totalSize = 0;
    while (totalSize < PingPongTotalSize) {
        std::size_t receiveSize = std::min(PingPongBufferSize, PingPongTotalSize - totalSize);

        auto [size, error] = co_await stream.receiveAsync(buffer, receiveSize);
        CHECK(error == std::errc{});
        totalSize += size;

        std::size_t totalSendSize = 0;
        while (totalSendSize < size) {
            auto [sendSize, error] =
                co_await stream.sendAsync(buffer + totalSendSize, size - totalSendSize);
            CHECK(error == std::errc{});
            totalSendSize += sendSize;
        }
    }

    int serverCompleteCount  = completedServers.fetch_add(1, std::memory_order_relaxed);
    serverCompleteCount     += 1;

    if (serverCompleteCount == NumPingPongConnections &&
        completedClients.load(std::memory_order_relaxed) == NumPingPongConnections)
        ctx.stop();
}

auto pingPongListener(IoContext &ctx, const InetAddress &address, std::atomic_int &completedServers,
                      std::atomic_int &completedClients) noexcept -> Task<> {
    TcpServer server;

    std::errc error = server.bind(address);
    CHECK(error == std::errc{});
    CHECK(server.address() == address);

    for (std::size_t i = 0; i < NumPingPongConnections; ++i) {
        auto [stream, error] = co_await server.acceptAsync();
        CHECK(error == std::errc{});
        co_await ScheduleAwaitable(
            pingPongServer(ctx, std::move(stream), completedServers, completedClients));
    }
}

auto pingPongClient(IoContext &ctx, const InetAddress &address, std::atomic_int &completedServers,
                    std::atomic_int &completedClients) noexcept -> Task<> {
    TcpStream client;
    std::errc error = co_await client.connectAsync(address);
    CHECK(error == std::errc{});
    CHECK(client.remoteAddress() == address);

    error = client.setKeepAlive(true);
    CHECK(error == std::errc{});

    error = client.setNoDelay(true);
    CHECK(error == std::errc{});

    char buffer[PingPongBufferSize]{};
    std::size_t totalSize = 0;

    while (totalSize < PingPongTotalSize) {
        std::size_t sendSize = std::min(PingPongBufferSize, PingPongTotalSize - totalSize);
        auto [size, error]   = co_await client.sendAsync(buffer, sendSize);
        CHECK(error == std::errc{});
        totalSize += size;

        std::size_t totalReceiveSize = 0;
        while (totalReceiveSize < size) {
            auto [receiveSize, error] =
                co_await client.receiveAsync(buffer + totalReceiveSize, size - totalReceiveSize);
            CHECK(error == std::errc{});
            totalReceiveSize += receiveSize;
        }
    }

    client.close();

    int clientCompleteCount  = completedClients.fetch_add(1, std::memory_order_relaxed);
    clientCompleteCount     += 1;
    if (clientCompleteCount == NumPingPongConnections &&
        completedServers.load(std::memory_order_relaxed) == NumPingPongConnections)
        ctx.stop();
}

} // namespace

TEST_CASE("[tcp] TCP stream ping-pong") {
    IoContext ctx(1);

    std::atomic_int completedServers{0};
    std::atomic_int completedClients{0};

    InetAddress address(Ipv6Loopback, 23456);
    ctx.schedule(pingPongListener(ctx, address, completedServers, completedClients));
    for (std::size_t i = 0; i < NumPingPongConnections; ++i)
        ctx.schedule(pingPongClient(ctx, address, completedServers, completedClients));

    ctx.run();
}

TEST_CASE("[tcp] TCP blocked IO") {
    std::atomic_bool serverReady{false};
    InetAddress address(IpAddress("127.0.0.1"), 23457);

    std::jthread serverThread([&serverReady, &address]() {
        TcpServer server;
        server.bind(address);
        serverReady.store(true, std::memory_order_relaxed);

        auto [stream, error] = server.accept();
        CHECK(error == std::errc{});

        char buffer[128];
        std::size_t totalSize = 0;
        while (totalSize < std::size(buffer)) {
            auto [size, error] = stream.receive(buffer + totalSize, std::size(buffer) - totalSize);
            CHECK(error == std::errc{});
            totalSize += size;
        }
    });

    std::jthread clientThread([&serverReady, &address]() {
        while (!serverReady.load(std::memory_order_relaxed))
            std::this_thread::yield();

        TcpStream stream;
        std::errc error = stream.connect(address);

        char buffer[128]{};
        std::size_t totalSize = 0;
        while (totalSize < std::size(buffer)) {
            auto [size, error] = stream.send(buffer + totalSize, std::size(buffer) - totalSize);
            CHECK(error == std::errc{});
            totalSize += size;
        }
    });

    serverThread.join();
    clientThread.join();
}

TEST_CASE("[tcp] TCP client connect to bad address") {
    InetAddress badAddr;
    TcpStream client;
    std::errc error = client.connect(badAddr);
    CHECK(error != std::errc{});
}

TEST_CASE("[tcp] TCP server listen to bad address") {
    InetAddress badAddr;
    TcpServer server;
    std::errc error = server.bind(badAddr);
    CHECK(error != std::errc{});
}

namespace {

auto tcpStreamReceiveTimeout(IoContext &ctx, const InetAddress &address) noexcept -> Task<> {
    TcpStream stream;

    std::errc error = co_await stream.connectAsync(address);
    CHECK(error == std::errc{});

    std::size_t buffer;

    // Set timeout.
    CHECK(stream.setReceiveTimeout(-1s) == std::errc::invalid_argument);
    CHECK(stream.setReceiveTimeout(100ms) == std::errc{});

    { // Block IO timeout.
        auto [bytes, error] = stream.receive(&buffer, sizeof(buffer));
        CHECK((error == std::errc::resource_unavailable_try_again ||
               error == std::errc::operation_would_block));
        CHECK(bytes == 0);
    }
}

auto tcpStreamTimeoutServer(IoContext &ctx) -> Task<> {
    InetAddress address(IpAddress("::1"), 23458);
    TcpServer server;

    std::errc error = server.bind(address);
    CHECK(error == std::errc{});
    auto task = tcpStreamReceiveTimeout(ctx, address);
    co_await schedule(task);

    auto [stream, err] = co_await server.acceptAsync();
    CHECK(err == std::errc{});

    // Wait for client complete.
    while (!task.isCompleted())
        co_await yield();

    ctx.stop();
}

} // namespace

TEST_CASE("[tcp] TCP stream receive timeout") {
    IoContext ctx(1);
    ctx.schedule(tcpStreamTimeoutServer(ctx));
    ctx.run();
}
