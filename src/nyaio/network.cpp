#include "nyaio/network.hpp"

#include <cstring>

#include <arpa/inet.h>

using namespace nyaio;
using namespace nyaio::detail;

nyaio::ip_address::ip_address(std::string_view addr) : m_addr(), m_is_ipv6(false), m_scope_id() {
    char buffer[INET6_ADDRSTRLEN]{};
    sa_family_t family;

    auto throw_if_error = [addr](bool condition) -> void {
        if (!condition)
            throw std::invalid_argument(
                std::string("invalid IP address").append(addr.data(), addr.size()));
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

auto nyaio::tcp_stream::connect(const nyaio::inet_address &addr) noexcept -> std::errc {
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

auto nyaio::tcp_stream::connect_async(const nyaio::inet_address &addr) noexcept -> task<std::errc> {
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
