#include "nyaio/io.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("[file] File IO") {
    constexpr std::string_view content = "0123456789abcdefghijklmnopqrstuvwxyz";

    { // Test write.
        File file;
        std::errc error = file.open("nyaio-test-file.txt",
                                    FileFlag::Write | FileFlag::Create | FileFlag::Truncate);
        CHECK(error == std::errc{});
        CHECK(file.isOpened());
        CHECK(!file.isClosed());
        CHECK(!file.canRead());
        CHECK(file.canWrite());
        CHECK(file.flags() == (FileFlag::Write | FileFlag::Create | FileFlag::Truncate));
        CHECK(file.path() == "nyaio-test-file.txt");
        CHECK(file.size() == 0);

        auto result = file.write(content.data(), content.size());
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());
        CHECK(file.size() == content.size());

        error = file.flush();
        CHECK(error == std::errc{});
        file.close();
    }

    { // Test read.
        File file;
        std::errc error = file.open("nyaio-test-file.txt", FileFlag::Read);
        CHECK(error == std::errc{});

        CHECK(file.isOpened());
        CHECK(!file.isClosed());
        CHECK(file.canRead());
        CHECK(!file.canWrite());
        CHECK(file.flags() == FileFlag::Read);

        char buffer[content.size()];
        auto result = file.read(buffer, sizeof(buffer));
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());

        CHECK(std::string_view(buffer, sizeof(buffer)) == content);

        result = file.read(buffer, sizeof(buffer));
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == 0);

        // Read at offset.
        result = file.read(buffer, sizeof(buffer), 0);
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());
        CHECK(std::string_view(buffer, sizeof(buffer)) == content);

        // Seek.
        error = file.seek(SeekBase::Begin, 0);
        CHECK(error == std::errc{});

        result = file.read(buffer, sizeof(buffer));
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());
        CHECK(std::string_view(buffer, sizeof(buffer)) == content);
    }

    { // Truncate.
        File file;
        std::errc error =
            file.open("nyaio-test-file.txt", FileFlag::Read | FileFlag::Write | FileFlag::Sync);

        CHECK(file.isOpened());
        CHECK(!file.isClosed());

        char buffer[content.size()];

        error = file.truncate(1);
        CHECK(error == std::errc{});
        CHECK(file.size() == 1);

        auto result = file.read(buffer, sizeof(buffer), 0);
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == 1);
        CHECK(std::string_view(buffer, 1) == "0");
    }
}

namespace {

auto asyncFileIo(IoContext &ctx) noexcept -> Task<> {
    constexpr std::string_view content = "0123456789abcdefghijklmnopqrstuvwxyz";

    { // Test write.
        File file;
        std::errc error = file.open("nyaio-test-file.txt",
                                    FileFlag::Write | FileFlag::Create | FileFlag::Truncate);
        CHECK(error == std::errc{});
        CHECK(!file.canRead());
        CHECK(file.canWrite());
        CHECK(file.flags() == (FileFlag::Write | FileFlag::Create | FileFlag::Truncate));
        CHECK(file.path() == "nyaio-test-file.txt");
        CHECK(file.size() == 0);

        auto result = co_await file.writeAsync(content.data(), content.size());
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());
        CHECK(file.size() == content.size());

        error = co_await file.flushAsync();
        CHECK(error == std::errc{});
        file.close();
    }

    { // Test read.
        File file;
        std::errc error = file.open("nyaio-test-file.txt", FileFlag::Read);
        CHECK(error == std::errc{});

        CHECK(file.canRead());
        CHECK(!file.canWrite());
        CHECK(file.flags() == FileFlag::Read);

        char buffer[content.size()];
        auto result = co_await file.readAsync(buffer, sizeof(buffer));
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());

        CHECK(std::string_view(buffer, sizeof(buffer)) == content);

        result = co_await file.readAsync(buffer, sizeof(buffer));
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == 0);

        // Read at offset.
        result = co_await file.readAsync(buffer, sizeof(buffer), 0);
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());
        CHECK(std::string_view(buffer, sizeof(buffer)) == content);

        // Seek.
        error = file.seek(SeekBase::Begin, 0);
        CHECK(error == std::errc{});

        result = co_await file.readAsync(buffer, sizeof(buffer));
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == content.size());
        CHECK(std::string_view(buffer, sizeof(buffer)) == content);
    }

    { // Truncate.
        File file;
        std::errc error =
            file.open("nyaio-test-file.txt", FileFlag::Read | FileFlag::Write | FileFlag::Sync);

        char buffer[content.size()];

        error = file.truncate(1);
        CHECK(error == std::errc{});
        CHECK(file.size() == 1);

        auto result = co_await file.readAsync(buffer, sizeof(buffer), 0);
        CHECK(result.error == std::errc{});
        CHECK(result.bytes == 1);
        CHECK(std::string_view(buffer, 1) == "0");
    }

    ctx.stop();
}

} // namespace

TEST_CASE("[file] File async IO") {
    IoContext ctx(1);
    ctx.schedule(asyncFileIo(ctx));
    ctx.run();
}
