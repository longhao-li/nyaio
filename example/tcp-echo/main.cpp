#include "nyaio/io.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nyaio;

namespace {

auto echo(TcpStream stream) noexcept -> Task<> {
    char buffer[4096];
    while (true) {
        auto [bytes, error] = co_await stream.receiveAsync(buffer, sizeof(buffer));

        // Handle error.
        if (error != std::errc{}) [[unlikely]] {
            std::fprintf(stderr, "stream receive error: %s\n",
                         std::strerror(static_cast<int>(error)));
            break;
        }

        // Connection closed.
        if (bytes == 0) [[unlikely]]
            break;

        // Send received data back.
        std::uint32_t totalSent = 0;
        while (totalSent < bytes) {
            auto [sent, error] = co_await stream.sendAsync(buffer + totalSent, bytes - totalSent);

            // Handle error.
            if (error != std::errc{}) [[unlikely]] {
                std::fprintf(stderr, "stream send error: %s\n",
                             std::strerror(static_cast<int>(error)));
                co_return;
            }

            totalSent += sent;
        }
    }
}

auto listener(const InetAddress &address) noexcept -> Task<> {
    // Bind server to the address.
    TcpServer server;
    std::errc error = server.bind(address);
    if (error != std::errc{}) [[unlikely]] {
        std::fprintf(stderr, "server bind error: %s\n", std::strerror(static_cast<int>(error)));
        std::terminate();
    }

    // Listen for incoming connections.
    while (true) {
        auto [stream, error] = co_await server.acceptAsync();

        // Handle error.
        if (error != std::errc{}) [[unlikely]] {
            std::fprintf(stderr, "server accept error: %s\n",
                         std::strerror(static_cast<int>(error)));
            continue;
        }

        // Schedule new connection in worker. Listener does not care about result of the connection.
        co_await ScheduleAwaitable(echo(std::move(stream)));
    }
}

} // namespace

auto main(int argc, char **argv) -> int {
    if (argc != 3) {
        std::fprintf(stderr, "Usage: %s <address> <port>\n", argv[0]);
        return EXIT_FAILURE;
    }

    // FIXME: may throw exception.
    IpAddress address(argv[1]);
    std::uint16_t port = std::atoi(argv[2]);
    InetAddress addr(address, port);

    IoContext ctx;
    ctx.dispatch(listener, addr);

    // Actually noreturn here.
    ctx.run();

    return 0;
}
