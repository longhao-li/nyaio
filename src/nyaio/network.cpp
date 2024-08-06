#include "nyaio/network.hpp"

#include <cstring>

#include <arpa/inet.h>

using namespace nyaio;
using namespace nyaio::detail;

nyaio::ip_address::ip_address(std::string_view addr) : m_addr(), m_is_ipv6(false), m_scope_id() {
    char buffer[INET6_ADDRSTRLEN]{};
    sa_family_t family;

    auto throw_if_error = [addr](bool condition) -> void {
        if (condition)
            throw std::invalid_argument(
                std::string("invalid IP address ").append(addr.data(), addr.size()));
    };

    if (addr.find('.') != std::string_view::npos) {
        throw_if_error(addr.size() >= INET_ADDRSTRLEN);

        memcpy(buffer, addr.data(), addr.size());
        family    = AF_INET;
        m_is_ipv6 = false;
    } else {
        throw_if_error(addr.size() >= INET6_ADDRSTRLEN);

        memcpy(buffer, addr.data(), addr.size());
        family    = AF_INET6;
        m_is_ipv6 = true;
    }

    throw_if_error(inet_pton(family, buffer, &m_addr) != 1);
}

auto nyaio::tcp_stream::connect(const inet_address &addr) noexcept -> std::errc {
    auto *a = reinterpret_cast<const sockaddr *>(&addr);
    int s   = ::socket(a->sa_family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (s < 0) [[unlikely]]
        return static_cast<std::errc>(errno);

    if (::connect(s, a, addr.size()) == -1) [[unlikely]] {
        int error = errno;
        ::close(s);
        return static_cast<std::errc>(error);
    }

    if (m_socket != -1)
        ::close(m_socket);

    m_socket = s;
    m_addr   = addr;

    return {};
}

auto nyaio::tcp_stream::connect_async(const inet_address &addr) noexcept -> task<std::errc> {
    auto *a = reinterpret_cast<const sockaddr *>(&addr);
    int s   = ::socket(a->sa_family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (s < 0) [[unlikely]]
        co_return static_cast<std::errc>(errno);

    if (auto e = co_await connect_awaitable(s, a, addr.size()); e != std::errc()) [[unlikely]] {
        ::close(s);
        co_return e;
    }

    if (m_socket != -1)
        ::close(m_socket);

    m_socket = s;
    m_addr   = addr;

    co_return {};
}

auto nyaio::tcp_server::listen(const inet_address &address) noexcept -> std::errc {
    auto *a = reinterpret_cast<const sockaddr *>(&address);
    int s   = ::socket(a->sa_family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);

    if (s == -1) [[unlikely]]
        return static_cast<std::errc>(errno);

    { // set reuse address and port
        const int value = 1;
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
        setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    }

    if (::bind(s, a, address.size()) == -1) [[unlikely]] {
        int error = errno;
        ::close(s);
        return static_cast<std::errc>(error);
    }

    if (::listen(s, SOMAXCONN) == -1) [[unlikely]] {
        int error = errno;
        ::close(s);
        return static_cast<std::errc>(error);
    }

    if (m_socket != -1)
        ::close(m_socket);

    m_socket = s;
    m_addr   = address;

    return std::errc{};
}

auto nyaio::tcp_server::accept() const noexcept -> std::expected<tcp_stream, std::errc> {
    inet_address addr;
    socklen_t len = sizeof(addr);

    int socket = ::accept4(m_socket, reinterpret_cast<sockaddr *>(&addr), &len, SOCK_CLOEXEC);
    if (socket == -1) [[unlikely]]
        return std::unexpected(static_cast<std::errc>(errno));

    return tcp_stream(socket, addr);
}

auto nyaio::tcp_server::accept_async() const noexcept
    -> task<std::expected<tcp_stream, std::errc>> {
    inet_address addr;
    socklen_t len = sizeof(addr);

    auto result = co_await accept_awaitable(m_socket, reinterpret_cast<sockaddr *>(&addr), &len,
                                            SOCK_CLOEXEC);
    co_return result.transform(
        [&addr](int socket) noexcept -> tcp_stream { return {socket, addr}; });
}

auto nyaio::tcp_server::acceptor() const noexcept -> task<std::expected<tcp_stream, std::errc>> {
    inet_address addr;
    socklen_t len;

    while (true) {
        len         = sizeof(addr);
        auto result = co_await accept_awaitable(m_socket, reinterpret_cast<sockaddr *>(&addr), &len,
                                                SOCK_CLOEXEC);
        co_yield result.transform(
            [&addr](int socket) noexcept -> tcp_stream { return {socket, addr}; });
    }
}
