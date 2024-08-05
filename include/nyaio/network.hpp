#pragma once

#include "async.hpp"

#include <bit>
#include <string_view>

#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>

namespace nyaio {
namespace detail {

/// @brief
///   Convert a host endian multi-byte value into network byte order.
/// @tparam T
///   Type of the value to be converted.
/// @param value
///   The integer value to be converted.
/// @return
///   The value in network byte order.
template <class T>
    requires(std::is_integral_v<T>)
constexpr auto to_network_endian(T value) noexcept -> T {
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap<T>(value);
    else
        return value;
}

/// @brief
///   Convert a network endian multi-byte value into host byte order.
/// @tparam T
///   Type of the value to be converted.
/// @param value
///   The integer value to be converted.
/// @return
///   The value in host byte order.
template <class T>
    requires(std::is_integral_v<T>)
constexpr auto to_host_endian(T value) noexcept -> T {
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap<T>(value);
    else
        return value;
}

} // namespace detail

/// @class ip_address
/// @brief
///   Wrapper class for IP address. Both IPv4 and IPv6 are supported.
class [[nodiscard]] ip_address {
public:
    /// @brief
    ///   Create an empty IP address.
    constexpr ip_address() noexcept : m_addr(), m_is_ipv6(), m_scope_id() {}

    /// @brief
    ///   Create a new IP address from string. IPv4 and IPv6 are determined by the address format.
    /// @param addr
    ///   String representation of the IP address. Both IPv4 and IPv6 are supported.
    /// @throws std::invalid_argument
    ///   Thrown if @p addr is not a valid IP address.
    NYAIO_API explicit ip_address(std::string_view addr);

    /// @brief
    ///   Create an IPv4 address from integers.
    /// @param v0
    ///   First value of the IPv4 address.
    /// @param v1
    ///   Second value of the IPv4 address.
    /// @param v2
    ///   Third value of the IPv4 address.
    /// @param v3
    ///   Fourth value of the IPv4 address.
    constexpr ip_address(uint8_t v0, uint8_t v1, uint8_t v2, uint8_t v3) noexcept
        : m_addr{.v4{
              .s_addr = detail::to_network_endian(
                  (static_cast<uint32_t>(v0) << 24) | (static_cast<uint32_t>(v1) << 16) |
                  (static_cast<uint32_t>(v2) << 8) | static_cast<uint32_t>(v3)),
          }},
          m_is_ipv6(false), m_scope_id() {}

    /// @brief
    ///   Create an IPv6 address from integers.
    /// @param v0
    ///   First value of the IPv6 address in host endian.
    /// @param v1
    ///   Second value of the IPv6 address in host endian.
    /// @param v2
    ///   Third value of the IPv6 address in host endian.
    /// @param v3
    ///   Fourth value of the IPv6 address in host endian.
    /// @param v4
    ///   Fifth value of the IPv6 address in host endian.
    /// @param v5
    ///   Sixth value of the IPv6 address in host endian.
    /// @param v6
    ///   Seventh value of the IPv6 address in host endian.
    /// @param v7
    ///   Eighth value of the IPv6 address in host endian.
    constexpr ip_address(uint16_t v0, uint16_t v1, uint16_t v2, uint16_t v3, uint16_t v4,
                         uint16_t v5, uint16_t v6, uint16_t v7) noexcept
        : m_addr(), m_is_ipv6(true), m_scope_id() {
        m_addr.v6.s6_addr16[0] = detail::to_network_endian(v0);
        m_addr.v6.s6_addr16[1] = detail::to_network_endian(v1);
        m_addr.v6.s6_addr16[2] = detail::to_network_endian(v2);
        m_addr.v6.s6_addr16[3] = detail::to_network_endian(v3);
        m_addr.v6.s6_addr16[4] = detail::to_network_endian(v4);
        m_addr.v6.s6_addr16[5] = detail::to_network_endian(v5);
        m_addr.v6.s6_addr16[6] = detail::to_network_endian(v6);
        m_addr.v6.s6_addr16[7] = detail::to_network_endian(v7);
    }

    /// @brief
    ///   @c ip_address is trivially copyable.
    /// @param other
    ///   The @c ip_address to be copied from.
    constexpr ip_address(const ip_address &other) noexcept = default;

    /// @brief
    ///   @c ip_address is trivially movable.
    /// @param other
    ///   The @c ip_address to be moved from.
    constexpr ip_address(ip_address &&other) noexcept = default;

    /// @brief
    ///   @c ip_address is trivially destructible.
    constexpr ~ip_address() = default;

    /// @brief
    ///   @c ip_address is trivially copyable.
    /// @param other
    ///   The @c ip_address to be copied from.
    /// @return
    ///   Reference to this @c ip_address.
    constexpr auto operator=(const ip_address &other) noexcept -> ip_address & = default;

    /// @brief
    ///   @c ip_address is trivially movable.
    /// @param other
    ///   The @c ip_address to be moved from.
    /// @return
    ///   Reference to this @c ip_address.
    constexpr auto operator=(ip_address &&other) noexcept -> ip_address & = default;

    /// @brief
    ///   Checks if this is an IPv4 address.
    /// @retval true
    ///   This is an IPv4 address.
    /// @retval false
    ///   This is not an IPv4 address.
    [[nodiscard]]
    constexpr auto is_ipv4() const noexcept -> bool {
        return !m_is_ipv6;
    }

    /// @brief
    ///   Checks if this is an IPv6 address.
    /// @retval true
    ///   This is an IPv6 address.
    /// @retval false
    ///   This is not an IPv6 address.
    [[nodiscard]]
    constexpr auto is_ipv6() const noexcept -> bool {
        return m_is_ipv6;
    }

    /// @brief
    ///   Set scope ID for IPv6 address. Scope ID is ignored for IPv4 address.
    /// @param id
    ///   The scope ID to be set in host endian.
    constexpr auto set_scope_id(uint32_t id) noexcept -> void {
        m_scope_id = id;
    }

    /// @brief
    ///   Get IPv6 scope ID.
    /// @return
    ///   IPv6 scope ID in host endian.
    [[nodiscard]]
    constexpr auto scope_id() const noexcept -> uint32_t {
        return m_scope_id;
    }

    /// @brief
    ///   Get pointer to the raw address data. Length of the address data is determined by address
    ///   type. For IPv4, the length is 4 bytes. For IPv6, the length is 16 bytes.
    /// @note
    ///   Address is stored in network byte order.
    /// @return
    ///   Pointer to the raw IP address data.
    [[nodiscard]]
    auto address() noexcept -> void * {
        return &m_addr;
    }

    /// @brief
    ///   Get pointer to the raw address data. Length of the address data is determined by address
    ///   type. For IPv4, the length is 4 bytes. For IPv6, the length is 16 bytes.
    /// @note
    ///   Address is stored in network byte order.
    /// @return
    ///   Pointer to the raw IP address data.
    [[nodiscard]]
    auto address() const noexcept -> const void * {
        return &m_addr;
    }

    /// @brief
    ///   Checks if this address is an IPv4 loopback address. IPv4 loopback address is @c 127.0.0.1.
    /// @retval true
    ///   This address is an IPv4 loopback address.
    /// @retval false
    ///   This address is not an IPv4 loopback address.
    [[nodiscard]]
    constexpr auto is_ipv4_loopback() const noexcept -> bool {
        return !m_is_ipv6 && (m_addr.v4.s_addr == detail::to_network_endian(INADDR_LOOPBACK));
    }

    /// @brief
    ///   Checks if this address is an IPv4 any address. IPv4 any address is @c 0.0.0.0.
    /// @retval true
    ///   This address is an IPv4 any address.
    /// @retval false
    ///   This address is not an IPv4 any address.
    [[nodiscard]]
    constexpr auto is_ipv4_any() const noexcept -> bool {
        return !m_is_ipv6 && (m_addr.v4.s_addr == detail::to_network_endian(INADDR_ANY));
    }

    /// @brief
    ///   Checks if this address is an IPv4 broadcast address. IPv4 broadcast address is
    ///   @c 255.255.255.255.
    /// @retval true
    ///   This address is an IPv4 broadcast address.
    /// @retval false
    ///   This address is not an IPv4 broadcast address.
    [[nodiscard]]
    constexpr auto is_ipv4_broadcast() const noexcept -> bool {
        return !m_is_ipv6 && (m_addr.v4.s_addr == detail::to_network_endian(INADDR_BROADCAST));
    }

    /// @brief
    ///   Checks if this address is an IPv4 private address. An IPv4 private network is a network
    ///   that used for local area networks. Private address ranges are defined in RFC 1918 as
    ///   follows:
    ///   - @c 10.0.0.0/8
    ///   - @c 172.16.0.0/12
    ///   - @c 192.168.0.0/16
    /// @retval true
    ///   This address is an IPv4 private address.
    /// @retval false
    ///   This address is not an IPv4 private address.
    [[nodiscard]]
    constexpr auto is_ipv4_private() const noexcept -> bool {
        if (m_is_ipv6)
            return false;

        // 10.0.0.0/8
        if ((detail::to_host_endian(m_addr.v4.s_addr) >> 24) == 0x0A)
            return true;

        // 172.16.0.0/12
        if ((detail::to_host_endian(m_addr.v4.s_addr) >> 20) == 0xAC1)
            return true;

        // 192.168.0.0/16
        if ((detail::to_host_endian(m_addr.v4.s_addr) >> 16) == 0xC0A8)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv4 link local address. IPv4 link local address is
    ///   @c 169.254.0.0/16 as defined in RFC 3927.
    /// @retval true
    ///   This address is an IPv4 link local address.
    /// @retval false
    ///   This address is not an IPv4 link local address.
    [[nodiscard]]
    constexpr auto is_ipv4_linklocal() const noexcept -> bool {
        if (m_is_ipv6)
            return false;

        if ((detail::to_host_endian(m_addr.v4.s_addr) >> 16) == 0xA9FE)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv4 multicast address. IPv4 multicast address is
    ///   @c 224.0.0.0/4 as defined in RFC 5771.
    /// @retval true
    ///   This address is an IPv4 multicast address.
    /// @retval false
    ///   This address is not an IPv4 multicast address.
    [[nodiscard]]
    constexpr auto is_ipv4_multicast() const noexcept -> bool {
        if (m_is_ipv6)
            return false;

        if ((detail::to_host_endian(m_addr.v4.s_addr) >> 28) == 0xE)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv6 loopback address. IPv6 loopback address is @c ::1.
    /// @retval true
    ///   This address is an IPv6 loopback address.
    /// @retval false
    ///   This address is not an IPv6 loopback address.
    [[nodiscard]]
    constexpr auto is_ipv6_loopback() const noexcept -> bool {
        if (!m_is_ipv6)
            return false;

        return ((m_addr.v6.s6_addr16[0] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[6] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[7] == detail::to_network_endian<uint16_t>(1)));
    }

    /// @brief
    ///   Checks if this address is an IPv6 any address. IPv6 any address is @c ::.
    /// @retval true
    ///   This address is an IPv6 any address.
    /// @retval false
    ///   This address is not an IPv6 any address.
    [[nodiscard]]
    constexpr auto is_ipv6_any() const noexcept -> bool {
        if (!m_is_ipv6)
            return false;

        return ((m_addr.v6.s6_addr16[0] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[6] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[7] == detail::to_network_endian<uint16_t>(0)));
    }

    /// @brief
    ///   Checks if this address is an IPv6 multicast address. IPv6 multicast address is @c FF00::/8
    ///   as defined in RFC 4291.
    /// @retval true
    ///   This address is an IPv6 multicast address.
    /// @retval false
    ///   This address is not an IPv6 multicast address.
    [[nodiscard]]
    constexpr auto is_ipv6_multicast() const noexcept -> bool {
        if (!m_is_ipv6)
            return false;

        return (m_addr.v6.s6_addr[0] == 0xFF);
    }

    /// @brief
    ///   Checks if this address is an IPv4 mapped IPv6 address. IPv4 mapped IPv6 address is
    ///   @c ::FFFF:0:0/96.
    /// @retval true
    ///   This is an IPv4 mapped IPv6 address.
    /// @retval false
    ///   This is not an IPv4 mapped IPv6 address.
    [[nodiscard]]
    constexpr auto is_ipv4_mapped_ipv6() const noexcept -> bool {
        if (!m_is_ipv6)
            return false;

        return ((m_addr.v6.s6_addr16[0] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == detail::to_network_endian<uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == detail::to_network_endian<uint16_t>(0xFFFF)));
    }

    /// @brief
    ///   Converts this IP address to IPv4 address.
    /// @return
    ///   Return this address if this is an IPv4 address. Return mapped IPv4 address if this is an
    ///   IPv4-mapped IPv6 address. Otherwise, return null.
    [[nodiscard]]
    auto to_ipv4() const noexcept -> std::optional<ip_address> {
        if (!m_is_ipv6)
            return *this;

        if (!is_ipv4_mapped_ipv6())
            return {};

        return ip_address(m_addr.v6.s6_addr[12], m_addr.v6.s6_addr[13], m_addr.v6.s6_addr[14],
                          m_addr.v6.s6_addr[15]);
    }

    /// @brief
    ///   Converts this IP address to IPv6 address.
    /// @return
    ///   Return this address if this is an IPv6 address. Otherwise, return IPv4-mapped IPv6 address
    ///   from this address.
    [[nodiscard]]
    auto to_ipv6() const noexcept -> ip_address {
        if (m_is_ipv6)
            return *this;

        ip_address addr{0, 0, 0, 0, 0, 0xFFFF, 0, 0};
        addr.m_addr.v6.s6_addr32[3] = m_addr.v4.s_addr;

        return addr;
    }

    /// @brief
    ///   Checks if two @c ip_address are exactly equal.
    friend constexpr auto operator==(const ip_address &lhs,
                                     const ip_address &rhs) noexcept -> bool {
        if (lhs.m_is_ipv6 != rhs.m_is_ipv6)
            return false;

        if (lhs.is_ipv4())
            return !__builtin_memcmp(&lhs.m_addr.v4, &rhs.m_addr.v4, sizeof(in_addr));

        return (lhs.m_scope_id == rhs.m_scope_id) &&
               !__builtin_memcmp(&lhs.m_addr.v6, &rhs.m_addr.v6, sizeof(in6_addr));
    }

    friend class inet_address;

private:
    union {
        in_addr v4;
        in6_addr v6;
    } m_addr;
    bool m_is_ipv6;
    uint32_t m_scope_id;
};

/// @brief
///   IPv4 loopback address.
inline constexpr ip_address ipv4_loopback(127, 0, 0, 1);

/// @brief
///   IPv4 broadcast address.
inline constexpr ip_address ipv4_broadcast(255, 255, 255, 255);

/// @brief
///   IPv4 address that listens to any incoming address.
inline constexpr ip_address ipv4_any(0, 0, 0, 0);

/// @brief
///   IPv6 loopback address.
inline constexpr ip_address ipv6_loopback(0, 0, 0, 0, 0, 0, 0, 1);

/// @brief
///   IPv6 address that listens to any incoming address.
inline constexpr ip_address ipv6_any(0, 0, 0, 0, 0, 0, 0, 0);

/// @class inet_address
/// @brief
///   Wrapper class for Internet socket address. This class could be directly used as @c sockaddr_in
///   and @c sockaddr_in6.
class [[nodiscard]] inet_address {
public:
    /// @brief
    ///   Create an empty Internet address. Empty Internet address cannot be used for any network
    ///   operation.
    constexpr inet_address() noexcept : m_addr() {}

    /// @brief
    ///   Create a new Internet address from an IP address and port.
    /// @param ip
    ///   IP address of this Internet address.
    /// @param port
    ///   Port number in host endian.
    constexpr inet_address(const ip_address &ip, uint16_t port) noexcept : m_addr() {
        if (ip.is_ipv4()) {
            m_addr.v4.sin_family = AF_INET;
            m_addr.v4.sin_port   = detail::to_network_endian(port);
            m_addr.v4.sin_addr   = ip.m_addr.v4;
        } else {
            m_addr.v6.sin6_family   = AF_INET6;
            m_addr.v6.sin6_port     = detail::to_network_endian(port);
            m_addr.v6.sin6_flowinfo = detail::to_network_endian<uint32_t>(0);
            m_addr.v6.sin6_addr     = ip.m_addr.v6;
            m_addr.v6.sin6_scope_id = detail::to_network_endian(ip.m_scope_id);
        }
    }

    /// @brief
    ///   @c inet_address is trivially copyable.
    /// @param other
    ///   The Internet address to be copied from.
    constexpr inet_address(const inet_address &other) noexcept = default;

    /// @brief
    ///   @c inet_address is trivially movable.
    /// @param other
    ///   The Internet address to be moved from.
    constexpr inet_address(inet_address &&other) noexcept = default;

    /// @brief
    ///   @c inet_address is trivially destructible.
    constexpr ~inet_address() = default;

    /// @brief
    ///   @c inet_address is trivially copyable.
    /// @param other
    ///   The Internet address to be copied from.
    /// @return
    ///   Reference to this Internet address.
    constexpr auto operator=(const inet_address &other) noexcept -> inet_address & = default;

    /// @brief
    ///   @c inet_address is trivially movable.
    /// @param other
    ///   The Internet address to be moved from.
    /// @return
    ///   Reference to this Internet address.
    constexpr auto operator=(inet_address &&other) noexcept -> inet_address & = default;

    /// @brief
    ///   Checks if this is an IPv4 Internet address.
    /// @retval true
    ///   This is an IPv4 Internet address.
    /// @retval false
    ///   This is not an IPv4 Internet address.
    [[nodiscard]]
    constexpr auto is_ipv4() const noexcept -> bool {
        return m_addr.v4.sin_family == AF_INET;
    }

    /// @brief
    ///   Checks if this is an IPv6 Internet address.
    /// @retval true
    ///   This is an IPv6 Internet address.
    /// @retval false
    ///   This is not an IPv6 Internet address.
    [[nodiscard]]
    constexpr auto is_ipv6() const noexcept -> bool {
        return m_addr.v6.sin6_family == AF_INET6;
    }

    /// @brief
    ///   Get actual size in byte of this Internet address object.
    /// @return
    ///   Actual size in byte of this Internet address object. It is undefined behavior to get size
    ///   for empty Internet address object.
    [[nodiscard]]
    constexpr auto size() const noexcept -> socklen_t {
        return is_ipv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    }

    /// @brief
    ///   Get IP address of this Internet address. It is undefined behavior to get IP address from
    ///   empty Internet address.
    /// @return
    ///   IP address of this Internet address.
    [[nodiscard]]
    constexpr auto ip() const noexcept -> ip_address {
        if (m_addr.v4.sin_family == AF_INET) {
            ip_address addr;
            addr.m_addr.v4  = m_addr.v4.sin_addr;
            addr.m_is_ipv6  = false;
            addr.m_scope_id = 0;
            return addr;
        } else if (m_addr.v6.sin6_family == AF_INET6) {
            ip_address addr;
            addr.m_addr.v6  = m_addr.v6.sin6_addr;
            addr.m_is_ipv6  = true;
            addr.m_scope_id = detail::to_host_endian(m_addr.v6.sin6_scope_id);
            return addr;
        } else [[unlikely]] {
            return {};
        }
    }

    /// @brief
    ///   Get port of this Internet address. It is undefined behavior to get port number from empty
    ///   Internet address.
    /// @return
    ///   Port number in host endian.
    [[nodiscard]]
    constexpr auto port() const noexcept -> uint16_t {
        return detail::to_host_endian(m_addr.v4.sin_port);
    }

    /// @brief
    ///   Set port of this Internet address.
    /// @param port
    ///   The port in host endian to be set.
    constexpr auto set_port(uint16_t port) noexcept -> void {
        m_addr.v4.sin_port = detail::to_network_endian(port);
    }

    /// @brief
    ///   Get IPv6 flow label.
    /// @return
    ///   IPv6 flow label. The return value is undefined if this is not an IPv6 address.
    [[nodiscard]]
    constexpr auto flow_label() const noexcept -> uint32_t {
        return detail::to_host_endian(m_addr.v6.sin6_flowinfo);
    }

    /// @brief
    ///   Set flow label for this IPv6 Internet address. It is undefined behavior to set flow label
    ///   for IPv4 address.
    /// @param label
    ///   The flow label in host endian to be set.
    constexpr auto set_flow_label(uint32_t label) noexcept -> void {
        m_addr.v6.sin6_flowinfo = detail::to_network_endian(label);
    }

    /// @brief
    ///   Get IPv6 scope ID.
    /// @return
    ///   IPv6 scope ID in host endian. It is undefined behavior to get scope ID from IPv4 Internet
    ///   address.
    [[nodiscard]]
    constexpr auto scope_id() const noexcept -> uint32_t {
        return detail::to_host_endian(m_addr.v6.sin6_scope_id);
    }

    /// @brief
    ///   Set scope ID for this IPv6 Internet address. It is undefined behavior to set scope ID for
    ///   IPv4 Internet address.
    /// @param id
    ///   The scope ID in host endian to be set.
    constexpr auto set_scope_id(uint32_t id) noexcept -> void {
        m_addr.v6.sin6_scope_id = detail::to_network_endian(id);
    }

    /// @brief
    ///   Checks if two @c inet_address are exactly equal.
    friend constexpr auto operator==(const inet_address &lhs,
                                     const inet_address &rhs) noexcept -> bool {
        const size_t size = lhs.is_ipv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
        return !__builtin_memcmp(&lhs, &rhs, size);
    }

private:
    union {
        sockaddr_in v4;
        sockaddr_in6 v6;
    } m_addr;
};

/// @class tcp_stream
/// @brief
///   Wrapper class for TCP connection. This class is used for TCP socket IO.
class tcp_stream {
public:
    /// @brief
    ///   Create an empty @c tcp_stream. Empty @c tcp_stream object cannot be used for IO
    ///   operations.
    tcp_stream() noexcept : m_socket(-1), m_addr() {}

    /// @brief
    ///   For internal usage. Wrap a raw socket file descriptor and address as a @c tcp_stream.
    /// @param socket
    ///   Socket file descriptor of the TCP connection.
    /// @param addr
    ///   Address of peer endpoint.
    tcp_stream(int socket, const inet_address &addr) noexcept : m_socket(socket), m_addr(addr) {}

    /// @brief
    ///   @c tcp_stream is not copyable.
    tcp_stream(const tcp_stream &other) = delete;

    /// @brief
    ///   Move constructor of @c tcp_stream.
    /// @param[in, out] other
    ///   The @c tcp_stream object to be moved. The moved object will be empty and can not be used
    ///   for IO operations.
    tcp_stream(tcp_stream &&other) noexcept : m_socket(other.m_socket), m_addr(other.m_addr) {
        other.m_socket = -1;
    }

    /// @brief
    ///   Close the TCP connection and destroy this object.
    ~tcp_stream() {
        if (m_socket != -1)
            ::close(m_socket);
    }

    /// @brief
    ///   @c tcp_stream is not copyable.
    auto operator=(const tcp_stream &other) = delete;

    /// @brief
    ///   Move assignment of @c tcp_stream.
    /// @param[in, out] other
    ///   The @c tcp_stream object to be moved. The moved object will be empty and can not be used
    ///   for IO operations.
    /// @return
    ///   Reference to this @c tcp_stream.
    auto operator=(tcp_stream &&other) noexcept -> tcp_stream & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_socket != -1)
            ::close(m_socket);

        m_socket = other.m_socket;
        m_addr   = other.m_addr;

        other.m_socket = -1;
        return *this;
    }

    /// @brief
    ///   Connect to the specified peer address. This method will block current thread until the
    ///   connection is established or any error occurs. If this TCP stream is currently not empty,
    ///   the old connection will be closed once the new connection is established. The old
    ///   connection will not be closed if the new connection fails.
    /// @param addr
    ///   Peer Internet address to connect.
    /// @return
    ///   Error code of the connect operation. The error code is 0 if succeeded.
    NYAIO_API auto connect(const inet_address &addr) noexcept -> std::errc;

    /// @brief
    ///   Connect to the specified peer address. This method will suspend this coroutine until the
    ///   new connection is established or any error occurs. If this TCP stream is currently not
    ///   empty, the old connection will be closed once the new connection is established. The old
    ///   connection will not be closed if the new connection fails.
    /// @param addr
    ///   Peer Internet address to connect.
    /// @return
    ///   Error code of the connect operation. The error code is 0 if succeeded.
    NYAIO_API auto connect_async(const inet_address &addr) noexcept -> task<std::errc>;

    /// @brief
    ///   Receive data from peer TCP endpoint. This method will block current thread until any data
    ///   is received or error occurs.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store the received data.
    /// @param size
    ///   Maximum available size to be received.
    /// @return
    ///   Actual size in byte of data received if succeeded. Otherwise, return an system error code.
    auto receive(void *buffer, uint32_t size) const noexcept -> std::expected<uint32_t, std::errc> {
        ssize_t result = ::recv(m_socket, buffer, size, 0);
        if (result == -1) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(errno));
        return static_cast<uint32_t>(result);
    }

    /// @brief
    ///   Receive data from peer TCP endpoint. This method always returns immediately.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store the received data.
    /// @param size
    ///   Maximum available size to be received.
    /// @return
    ///   Actual size in byte of data received if succeeded. Otherwise, return an system error code.
    ///   This method returns @c std::errc::resource_unavailable_try_again or
    ///   @c std::errc::operation_would_block if there is no data available currently.
    auto receive_nonblock(void *buffer,
                          uint32_t size) const noexcept -> std::expected<uint32_t, std::errc> {
        ssize_t result = ::recv(m_socket, buffer, size, MSG_DONTWAIT);
        if (result == -1) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(errno));
        return static_cast<uint32_t>(result);
    }

    /// @brief
    ///   Async receive data from peer TCP endpoint. This method will suspend this coroutine until
    ///   any data is received or any error occurs.
    /// @param[out] buffer
    ///   Pointer to start of buffer to store the received data.
    /// @param size
    ///   Maximum available size to be received.
    /// @return
    ///   Actual size in byte of data received if succeeded. Otherwise, return an system error code.
    [[nodiscard]]
    auto receive_async(void *buffer, uint32_t size) const noexcept -> recv_awaitable {
        return {m_socket, buffer, size, 0};
    }

    /// @brief
    ///   Send data to peer TCP endpoint. This method will block current thread until all data is
    ///   sent or any error occurs.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @return
    ///   Actual size in byte of data sent if succeeded. Otherwise, return an system error code.
    auto send(const void *data,
              uint32_t size) const noexcept -> std::expected<uint32_t, std::errc> {
        ssize_t result = ::send(m_socket, data, size, MSG_NOSIGNAL);
        if (result == -1) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(errno));
        return static_cast<uint32_t>(result);
    }

    /// @brief
    ///   Send data to peer TCP endpoint. This method always returns immediately.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @return
    ///   Actual size in byte of data sent if succeeded. Otherwise, return an system error code.
    ///   This method returns @c std::errc::resource_unavailable_try_again or
    ///   @c std::errc::operation_would_block if send buffer is currently not available.
    auto send_nonblock(const void *data,
                       uint32_t size) const noexcept -> std::expected<uint32_t, std::errc> {
        ssize_t result = ::send(m_socket, data, size, MSG_NOSIGNAL | MSG_DONTWAIT);
        if (result == -1) [[unlikely]]
            return std::unexpected(static_cast<std::errc>(errno));
        return static_cast<uint32_t>(result);
    }

    /// @brief
    ///   Async send data to peer TCP endpoint. This method will suspend this coroutine until any
    ///   data is sent or any error occurs.
    /// @param data
    ///   Pointer to start of data to be sent.
    /// @param size
    ///   Expected size in byte of data to be sent.
    /// @return
    ///   Actual size in byte of data sent if succeeded. Otherwise, return an system error code.
    [[nodiscard]]
    auto send_async(const void *data, uint32_t size) const noexcept -> send_awaitable {
        return {m_socket, data, size, MSG_NOSIGNAL};
    }

    /// @brief
    ///   Enable or disable keepalive for this TCP connection.
    /// @param enable
    ///   Specifies whether to enable or disable keepalive for this TCP stream.
    /// @return
    ///   An error code that indicates whether succeeded to enable or disable keepalive for this TCP
    ///   stream. The error code is 0 if succeeded to set keepalive attribute for this TCP stream.
    auto set_keepalive(bool enable) noexcept -> std::errc {
        const int v = enable ? 1 : 0;
        if (::setsockopt(m_socket, SOL_SOCKET, SO_KEEPALIVE, &v, sizeof(v)) == -1) [[unlikely]]
            return static_cast<std::errc>(errno);
        return {};
    }

    /// @brief
    ///   Enable or disable nodelay for this TCP stream.
    /// @param enable
    ///   Specifies whether to enable or disable nodelay for this TCP stream.
    /// @return
    ///   An error code that indicates whether succeeded to enable or disable nodelay for this TCP
    ///   stream. The error code is 0 if succeeded to set nodelay attribute for this TCP stream.
    auto set_nodelay(bool enable) noexcept -> std::errc {
        const int v = enable ? 1 : 0;
        if (::setsockopt(m_socket, SOL_TCP, TCP_NODELAY, &v, sizeof(v)) == -1) [[unlikely]]
            return static_cast<std::errc>(errno);
        return {};
    }

    /// @brief
    ///   Set timeout event for receive operation. @c tcp_stream::receive and
    ///   @c tcp_stream::receive_async may generate an error that indicates the timeout event if
    ///   timeout event occurs. The TCP connection may be in an undefined state and should be closed
    ///   if receive timeout event occurs.
    /// @tparam Rep
    ///   Type representation of duration type. See @c std::chrono::duration for details.
    /// @tparam Period
    ///   Ratio type that is used to measure how to do conversion between different duration types.
    ///   See @c std::chrono::duration for details.
    /// @param duration
    ///   Timeout duration of receive operation. Ratios less than microseconds are not allowed.
    /// @return
    ///   An error code that indicates whether succeeded to set the timeout event. The error code is
    ///   0 if succeeded to set or remove receive timeout event for this TCP connection.
    template <class Rep, class Period>
        requires(std::ratio_greater_equal_v<std::micro, Period>)
    auto set_receive_timeout(std::chrono::duration<Rep, Period> duration) noexcept -> std::errc {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
        auto count        = static_cast<uint64_t>(microseconds.count());

        const timeval t{
            .tv_sec  = static_cast<uint32_t>(count / 1000000),
            .tv_usec = static_cast<uint32_t>(count % 1000000),
        };

        if (::setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &t, sizeof(t)) == -1) [[unlikely]]
            return static_cast<std::errc>(errno);

        return {};
    }

    /// @brief
    ///   Set timeout event for send operation. @c tcp_stream::send and @c tcp_stream::send_async
    ///   may generate an error that indicates the timeout event if timeout event occurs. The TCP
    ///   connection may be in an undefined state and should be closed if send timeout event occurs.
    /// @tparam Rep
    ///   Type representation of duration type. See @c std::chrono::duration for details.
    /// @tparam Period
    ///   Ratio type that is used to measure how to do conversion between different duration types.
    ///   See @c std::chrono::duration for details.
    /// @param duration
    ///   Timeout duration of send operation. Ratios less than microseconds are not allowed.
    /// @return
    ///   An error code that indicates whether succeeded to set the timeout event. The error code is
    ///   0 if succeeded to set or remove send timeout event for this TCP connection.
    template <class Rep, class Period>
        requires(std::ratio_greater_equal_v<std::micro, Period>)
    auto set_send_timeout(std::chrono::duration<Rep, Period> duration) noexcept -> std::errc {
        auto microseconds = std::chrono::duration_cast<std::chrono::microseconds>(duration);
        auto count        = static_cast<uint64_t>(microseconds.count());

        const timeval t{
            .tv_sec  = static_cast<uint32_t>(count / 1000000),
            .tv_usec = static_cast<uint32_t>(count % 1000000),
        };

        if (::setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, &t, sizeof(t)) == -1) [[unlikely]]
            return static_cast<std::errc>(errno);

        return {};
    }

    /// @brief
    ///   Close this TCP stream and release all resources. Closing a TCP stream with pending IO
    ///   requirements may cause errors for the IO results. This method does nothing if current TCP
    ///   stream is empty.
    auto close() noexcept -> void {
        if (m_socket != -1) {
            ::close(m_socket);
            m_socket = -1;
        }
    }

private:
    int m_socket;
    inet_address m_addr;
};

/// @class tcp_server
/// @brief
///   Wrapper class for TCP server. This class is used for accepting incoming TCP connections.
class tcp_server {
public:
    /// @brief
    ///   Create an empty @c tcp_server. Empty @c tcp_server object cannot be used for accepting new
    ///   TCP connections.
    tcp_server() noexcept : m_socket(-1), m_addr() {}

    /// @brief
    ///   @c tcp_server is not copyable.
    tcp_server(const tcp_server &other) = delete;

    /// @brief
    ///   Move constructor of @c tcp_server.
    /// @param[in, out] other
    ///   The @c tcp_server object to be moved from. The moved object will be empty and can not be
    ///   used for accepting new TCP connections.
    tcp_server(tcp_server &&other) noexcept : m_socket(other.m_socket), m_addr(other.m_addr) {
        other.m_socket = -1;
    }

    /// @brief
    ///   Stop listening to incoming TCP connections and destroy this object.
    ~tcp_server() {
        if (m_socket != -1)
            ::close(m_socket);
    }

    /// @brief
    ///   @c tcp_server is not copyable.
    auto operator=(const tcp_server &other) = delete;

    /// @brief
    ///   Move assignment of @c tcp_server.
    /// @param[in, out] other
    ///   The @c tcp_server object to be moved from. The moved object will be empty and can not be
    ///   used for accepting new TCP connections.
    /// @return
    ///   Reference to this @c tcp_server.
    auto operator=(tcp_server &&other) noexcept -> tcp_server & {
        if (this == &other) [[unlikely]]
            return *this;

        if (m_socket != -1)
            ::close(m_socket);

        m_socket = other.m_socket;
        m_addr   = other.m_addr;

        other.m_socket = -1;
        return *this;
    }

    /// @brief
    ///   Start listening to incoming TCP connections. This method will create a new TCP server
    ///   socket and bind to the specified address. The old TCP server socket will be closed if
    ///   succeeded to listen to the new address.
    /// @param address
    ///   The local address to bind for listening incoming TCP connections.
    /// @return
    ///   A system error code that indicates whether succeeded to start listening to incoming TCP
    ///   connections. The error code is 0 if succeeded to start listening. The original TCP server
    ///   socket will not be affected if any error occurs.
    NYAIO_API auto listen(const inet_address &address) noexcept -> std::errc;

    /// @brief
    ///   Accept a new incoming TCP connection. This method will block current thread until a new
    ///   TCP connection is established or any error occurs.
    /// @return
    ///   A new @c tcp_stream object that represents the new incoming TCP connection if succeeded.
    ///   Otherwise, return a system error code.
    [[nodiscard]]
    NYAIO_API auto accept() const noexcept -> std::expected<tcp_stream, std::errc>;

    /// @brief
    ///   Async accept a new incoming TCP connection. This method will suspend this coroutine until
    ///   a new TCP connection is established or any error occurs.
    /// @note
    ///   Returning task will allocate heap memory for each async accept operation. It is
    ///   recommended to use @c tcp_server::acceptor() for better performance.
    /// @return
    ///   A new @c tcp_stream object that represents the new incoming TCP connection if succeeded.
    ///   Otherwise, return a system error code.
    [[nodiscard]]
    NYAIO_API auto accept_async() const noexcept -> task<std::expected<tcp_stream, std::errc>>;

    /// @brief
    ///   Acquire an acceptor object for async accept operation. Acceptor can be used for multiple
    ///   async accept operations.
    /// @return
    ///   An acceptor object that can be used for async accept operation.
    [[nodiscard]]
    NYAIO_API auto acceptor() const noexcept -> task<std::expected<tcp_stream, std::errc>>;

    /// @brief
    ///   Get local listening address.
    /// @return
    ///   Local listening address of this TCP server. The return value is undefined if this TCP
    ///   server is empty.
    [[nodiscard]]
    auto address() const noexcept -> const inet_address & {
        return m_addr;
    }

    /// @brief
    ///   Stop listening to incoming TCP connections and close this TCP server. This TCP server will
    ///   be set to empty after this call. Call @c tcp_server::listen() to start listening to
    ///   incoming again.
    auto close() noexcept -> void {
        if (m_socket != -1) {
            ::close(m_socket);
            m_socket = -1;
        }
    }

private:
    int m_socket;
    inet_address m_addr;
};

} // namespace nyaio
