#include "nyaio/io.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

namespace {

inline constexpr std::size_t SendCount  = 1024;
inline constexpr std::size_t BufferSize = 1024;

auto channelServer(IoContext &ctx, Channel &ch) noexcept -> Task<> {
    char buffer[BufferSize];
    for (std::size_t i = 0; i < SendCount; ++i) {
        auto result = co_await ch.readAsync(buffer, BufferSize);
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == BufferSize);
    }

    ctx.stop();
}

auto channelClient(Channel &ch) noexcept -> Task<> {
    char buffer[BufferSize]{};
    for (std::size_t i = 0; i < SendCount; ++i) {
        auto result = co_await ch.writeAsync(buffer, BufferSize);
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == BufferSize);
    }
}

} // namespace

TEST_CASE("[channel] Channel IO") {
    IoContext ctx(1);
    Channel ch;
    CHECK(ch.open() == std::errc{});

    ctx.schedule(channelServer(ctx, ch));
    ctx.schedule(channelClient(ch));

    ctx.run();
}
