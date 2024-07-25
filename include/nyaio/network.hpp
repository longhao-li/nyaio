#pragma once

#include "async.hpp"

#include <bit>
#include <string_view>

#include <netinet/in.h>

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

} // namespace nyaio
