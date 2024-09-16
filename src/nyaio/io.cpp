#include "nyaio/io.hpp"

#include <stdexcept>

#include <arpa/inet.h>
#include <fcntl.h>

using namespace nyaio;

auto File::open(std::string_view path, FileFlag flags, FileMode mode) noexcept -> std::errc {
    std::string temp(path);
    int openFlags = O_CLOEXEC;

    if ((flags & FileFlag::Read) != FileFlag::None) {
        if ((flags & FileFlag::Write) != FileFlag::None) {
            openFlags |= O_RDWR;
        } else {
            flags      = FileFlag::Read;
            openFlags |= O_RDONLY;
        }
    }

    if ((flags & FileFlag::Write) != FileFlag::None) {
        if ((flags & FileFlag::Read) == FileFlag::None)
            openFlags |= O_WRONLY;

        if ((flags & FileFlag::Append) != FileFlag::None)
            openFlags |= O_APPEND;

        if ((flags & FileFlag::Create) != FileFlag::None)
            openFlags |= O_CREAT;

        if ((flags & FileFlag::Truncate) != FileFlag::None)
            openFlags |= O_TRUNC;
    }

    if ((flags & FileFlag::Direct) != FileFlag::None)
        openFlags |= O_DIRECT;

    if ((flags & FileFlag::Sync) != FileFlag::None)
        openFlags |= O_SYNC;

    int file = ::open(temp.c_str(), openFlags, static_cast<mode_t>(mode));
    if (file == -1)
        return std::errc{errno};

    if (m_file != -1)
        ::close(m_file);

    m_file  = file;
    m_flags = flags;
    m_path  = std::move(temp);

    return {};
}

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
        return std::errc{errno};

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

auto TcpServer::accept() const noexcept -> AcceptResult {
    InetAddress address;
    socklen_t length = sizeof(address);

    int s =
        ::accept4(m_socket, reinterpret_cast<struct sockaddr *>(&address), &length, SOCK_CLOEXEC);
    if (s == -1) [[unlikely]]
        return {TcpStream{}, std::errc{errno}};

    return {TcpStream{s, address}, std::errc{}};
}

auto UdpSocket::bind(const InetAddress &address) noexcept -> std::errc {
    auto *addr = reinterpret_cast<const sockaddr *>(&address);

    // Create the socket if necessary.
    if (m_socket == -1) {
        m_socket = ::socket(addr->sa_family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
        if (m_socket == -1) [[unlikely]]
            return std::errc{errno};

        const int value = 1;
        ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
        ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    }

    if (::bind(m_socket, addr, address.size()) == -1) [[unlikely]]
        return std::errc{errno};

    m_localAddress = address;
    return {};
}

auto UdpSocket::connect(const InetAddress &address) noexcept -> std::errc {
    auto *addr = reinterpret_cast<const sockaddr *>(&address);

    // Create the socket if necessary.
    if (m_socket == -1) {
        m_socket = ::socket(addr->sa_family, SOCK_DGRAM | SOCK_CLOEXEC, IPPROTO_UDP);
        if (m_socket == -1) [[unlikely]]
            return std::errc{errno};

        const int value = 1;
        ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value));
        ::setsockopt(m_socket, SOL_SOCKET, SO_REUSEPORT, &value, sizeof(value));
    }

    if (::connect(m_socket, addr, address.size()) == -1) [[unlikely]]
        return std::errc{errno};

    m_remoteAddress = address;
    return {};
}
