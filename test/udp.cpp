#include "nyaio/io.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

namespace {

inline constexpr std::size_t PingPongBufferSize = 1024;
inline constexpr std::size_t NumPingPongCount   = 1024;

auto pingPongServer(UdpSocket udp) noexcept -> Task<> {
    char buffer[PingPongBufferSize];
    InetAddress peer;

    for (std::size_t i = 0; i < NumPingPongCount; ++i) {
        auto [size, error] = co_await udp.receiveFromAsync(peer, buffer, sizeof(buffer));
        CHECK(error == std::errc{});
        CHECK(size == PingPongBufferSize);

        auto [sendSize, error2] = co_await udp.sendToAsync(peer, buffer, size);
        CHECK(error2 == std::errc{});
        CHECK(sendSize == size);
    }
}

auto pingPongClient(IoContext &ctx, const InetAddress &address) noexcept -> Task<> {
    UdpSocket udp;
    std::errc error = co_await udp.connectAsync(address);
    CHECK(error == std::errc{});
    CHECK(udp.remoteAddress() == address);

    char data[PingPongBufferSize]{};
    for (std::size_t i = 0; i < NumPingPongCount; ++i) {
        auto [size, err] = co_await udp.sendAsync(data, sizeof(data));
        CHECK(err == std::errc{});
        CHECK(size == PingPongBufferSize);

        auto [recvSize, error2] = co_await udp.receiveAsync(data, sizeof(data));
        CHECK(error2 == std::errc{});
        CHECK(recvSize == size);
    }

    udp.close();
    ctx.stop();
}

} // namespace

TEST_CASE("[udp] UDP ping-pong") {
    IoContext ctx(1);

    InetAddress address(Ipv6Loopback, 23460);

    CHECK(address.isIpv6());
    CHECK(!address.isIpv4());
    CHECK(address.ip() == Ipv6Loopback);
    CHECK(address.port() == 23460);
    CHECK(address.flowLabel() == 0);
    CHECK(address.scopeId() == 0);

    UdpSocket server;
    std::errc error = server.bind(address);
    CHECK(error == std::errc{});
    CHECK(server.localAddress() == address);

    ctx.schedule(pingPongServer(std::move(server)));
    ctx.schedule(pingPongClient(ctx, address));

    ctx.run();
}

TEST_CASE("[udp] UDP blocked IO") {
    InetAddress address(Ipv4Loopback, 23461);
    CHECK(!address.isIpv6());
    CHECK(address.isIpv4());
    CHECK(address.ip() == Ipv4Loopback);
    CHECK(address.port() == 23461);

    UdpSocket server;
    std::errc error = server.bind(address);
    CHECK(error == std::errc{});

    std::jthread serverThread([&server]() {
        char buffer[PingPongBufferSize];
        InetAddress peer;

        for (std::size_t i = 0; i < NumPingPongCount; ++i) {
            auto [size, error] = server.receiveFrom(peer, buffer, sizeof(buffer));
            CHECK(error == std::errc{});
            CHECK(size == PingPongBufferSize);

            auto [sendSize, error2] = server.sendTo(peer, buffer, size);
            CHECK(error2 == std::errc{});
            CHECK(sendSize == size);
        }
    });

    std::jthread clientThread([&address]() {
        UdpSocket client;
        std::errc error = client.connect(address);
        CHECK(error == std::errc{});

        char data[PingPongBufferSize]{};
        for (std::size_t i = 0; i < NumPingPongCount; ++i) {
            auto [size, err] = client.send(data, sizeof(data));
            CHECK(err == std::errc{});
            CHECK(size == PingPongBufferSize);

            auto [recvSize, error2] = client.receive(data, sizeof(data));
            CHECK(error2 == std::errc{});
            CHECK(recvSize == size);
        }
    });

    serverThread.join();
    clientThread.join();
}
