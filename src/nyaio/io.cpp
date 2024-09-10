#include "nyaio/io.hpp"

#include <arpa/inet.h>
#include <stdexcept>

using namespace nyaio;

IpAddress::IpAddress(std::string_view addr) : m_isV6(), m_addr() {
    char buffer[INET6_ADDRSTRLEN];
    sa_family_t family;

    { // Copy addr into buffer.
        std::size_t count = std::min(addr.size(), std::size(buffer) - 1);
        addr.copy(buffer, count);
        buffer[count] = 0;
    }

    if (addr.find('.') != std::string_view::npos) {
        family = AF_INET;
        m_isV6 = false;
    } else {
        family = AF_INET6;
        m_isV6 = true;
    }

    int ret = ::inet_pton(family, buffer, &m_addr);
    if (ret != 1)
        throw std::invalid_argument("Invalid IP address format.");
}

auto TcpStream::connect(const InetAddress &address) noexcept -> std::errc {
    // Create a new socket to establish connection.
    auto *addr = reinterpret_cast<const struct sockaddr *>(&address);
    int s      = ::socket(addr->sa_family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (s == -1) [[unlikely]]
        return static_cast<std::errc>(errno);

    if (::connect(s, addr, address.size()) == -1) [[unlikely]] {
        int error = errno;
        ::close(s);
        return static_cast<std::errc>(error);
    }

    if (m_socket != -1)
        ::close(m_socket);

    m_socket  = s;
    m_address = address;

    return {};
}

auto TcpServer::bind(const InetAddress &address) noexcept -> std::errc {
    auto *addr = reinterpret_cast<const sockaddr *>(&address);
    int s      = ::socket(addr->sa_family, SOCK_STREAM | SOCK_CLOEXEC, IPPROTO_TCP);
    if (s == -1) [[unlikely]]
        return std::errc{errno};

    { // Enable reuse address and reuse port.
        const int value = 1;
        ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
        ::setsockopt(s, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    }

    if (::bind(s, addr, address.size()) == -1) [[unlikely]] {
        int error = errno;
        ::close(s);
        return std::errc{error};
    }

    if (::listen(s, SOMAXCONN) == -1) [[unlikely]] {
        int error = errno;
        ::close(s);
        return std::errc{error};
    }

    if (m_socket != -1)
        ::close(m_socket);

    m_socket  = s;
    m_address = address;

    return {};
}

auto TcpServer::accept() noexcept -> AcceptResult {
    InetAddress address;
    socklen_t length = sizeof(address);

    int s =
        ::accept4(m_socket, reinterpret_cast<struct sockaddr *>(&address), &length, SOCK_CLOEXEC);
    if (s == -1) [[unlikely]]
        return {TcpStream{}, std::errc{errno}};

    return {TcpStream{s, address}, std::errc{}};
}
