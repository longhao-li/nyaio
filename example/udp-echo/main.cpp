#include "nyaio/io.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace nyaio;

namespace {

auto echo(const InetAddress &address) noexcept -> Task<> {
    UdpSocket server;

    // Bind server to the address.
    std::errc error = server.bind(address);
    if (error != std::errc{}) [[unlikely]] {
        std::fprintf(stderr, "server bind error: %s\n", std::strerror(static_cast<int>(error)));
        std::terminate();
    }

    char buffer[65536];
    InetAddress peer;

    while (true) {
        auto [bytes, error] = co_await server.receiveFromAsync(peer, buffer, sizeof(buffer));

        // Handle error.
        if (error != std::errc{}) [[unlikely]] {
            std::fprintf(stderr, "server receive error: %s\n",
                         std::strerror(static_cast<int>(error)));
            std::terminate();
        }

        // Send received data back.
        auto [ignored, error2] = co_await server.sendToAsync(peer, buffer, bytes);
        if (error2 != std::errc{}) [[unlikely]] {
            std::fprintf(stderr, "server send error: %s\n",
                         std::strerror(static_cast<int>(error2)));
            std::terminate();
        }
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

    // This is same as default constructed IoContext.
    IoContext ctx(0);
    ctx.dispatch(echo, addr);

    // Actually noreturn here.
    ctx.run();

    return 0;
}
