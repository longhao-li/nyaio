#pragma once

#include "task.hpp"

#include <bit>

#include <netinet/in.h>

namespace nyaio {

/// @brief
///   Convert a multi-byte integer from host endian to network endian.
/// @tparam T
///   Type of the integer to be converted.
/// @param value
///   The integer to be converted.
/// @return
///   The value in network endian byte order.
template <class T>
    requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= 8)
constexpr auto toNetworkEndian(T value) noexcept -> T {
    if constexpr (std::endian::native == std::endian::little) {
        if constexpr (sizeof(T) == 1)
            return value;
        else if constexpr (sizeof(T) == 2)
            return static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        else if constexpr (sizeof(T) == 4)
            return static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
    } else {
        return value;
    }
}

/// @brief
///   Convert a multi-byte integer from network endian to host endian.
/// @tparam T
///   Type of the integer to be converted.
/// @param value
///   The integer to be converted.
/// @return
///   The value in host endian byte order.
template <class T>
    requires(std::is_integral_v<T> && !std::is_same_v<T, bool> && sizeof(T) <= 8)
constexpr auto toHostEndian(T value) noexcept -> T {
    if constexpr (std::endian::native == std::endian::little) {
        if constexpr (sizeof(T) == 1)
            return value;
        else if constexpr (sizeof(T) == 2)
            return static_cast<T>(__builtin_bswap16(static_cast<std::uint16_t>(value)));
        else if constexpr (sizeof(T) == 4)
            return static_cast<T>(__builtin_bswap32(static_cast<std::uint32_t>(value)));
        else if constexpr (sizeof(T) == 8)
            return static_cast<T>(__builtin_bswap64(static_cast<std::uint64_t>(value)));
    } else {
        return value;
    }
}

/// @class IpAddress
/// @brief
///   Wrapper class for IP address. Both IPv4 and IPv6 are supported.
class IpAddress {
public:
    using Self = IpAddress;

    /// @brief
    ///   Create an empty IP address.
    constexpr IpAddress() noexcept : m_isV6(), m_addr() {}

    /// @brief
    ///   Create a new IP address from string. IPv4 and IPv6 are determined by the address format.
    /// @param addr
    ///   String representation of the IP address. Both IPv4 and IPv6 are supported.
    /// @throws std::invalid_argument
    ///   Thrown if @p addr is not a valid IP address.
    NYAIO_API explicit IpAddress(std::string_view addr);

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
    constexpr IpAddress(std::uint8_t v0, std::uint8_t v1, std::uint8_t v2, std::uint8_t v3) noexcept
        : m_isV6(), m_addr() {
        m_addr.v4.s_addr = toNetworkEndian(
            static_cast<std::uint32_t>(v0) << 24 | static_cast<std::uint32_t>(v1) << 16 |
            static_cast<std::uint32_t>(v2) << 8 | static_cast<std::uint32_t>(v3));
    }

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
    constexpr IpAddress(std::uint16_t v0, std::uint16_t v1, std::uint16_t v2, std::uint16_t v3,
                        std::uint16_t v4, std::uint16_t v5, std::uint16_t v6,
                        std::uint16_t v7) noexcept
        : m_isV6(true), m_addr() {
        m_addr.v6.s6_addr16[0] = toNetworkEndian(v0);
        m_addr.v6.s6_addr16[1] = toNetworkEndian(v1);
        m_addr.v6.s6_addr16[2] = toNetworkEndian(v2);
        m_addr.v6.s6_addr16[3] = toNetworkEndian(v3);
        m_addr.v6.s6_addr16[4] = toNetworkEndian(v4);
        m_addr.v6.s6_addr16[5] = toNetworkEndian(v5);
        m_addr.v6.s6_addr16[6] = toNetworkEndian(v6);
        m_addr.v6.s6_addr16[7] = toNetworkEndian(v7);
    }

    /// @brief
    ///   @c IpAddress is trivially copyable.
    /// @param other
    ///   The @c IpAddress to be copied from.
    constexpr IpAddress(const Self &other) noexcept = default;

    /// @brief
    ///   @c IpAddress is trivially movable.
    /// @param other
    ///   The @c IpAddress to be moved from.
    constexpr IpAddress(Self &&other) noexcept = default;

    /// @brief
    ///   @c IpAddress is trivially destructible.
    constexpr ~IpAddress() = default;

    /// @brief
    ///   @c IpAddress is trivially copyable.
    /// @param other
    ///   The @c IpAddress to be copied from.
    /// @return
    ///   Reference to this @c ip_address.
    constexpr auto operator=(const Self &other) noexcept -> Self & = default;

    /// @brief
    ///   @c IpAddress is trivially movable.
    /// @param other
    ///   The @c IpAddress to be moved from.
    /// @return
    ///   Reference to this @c IpAddress.
    constexpr auto operator=(Self &&other) noexcept -> Self & = default;

    /// @brief
    ///   Checks if this is an IPv4 address.
    /// @retval true
    ///   This is an IPv4 address.
    /// @retval false
    ///   This is not an IPv4 address.
    [[nodiscard]]
    constexpr auto isIpv4() const noexcept -> bool {
        return !m_isV6;
    }

    /// @brief
    ///   Checks if this is an IPv6 address.
    /// @retval true
    ///   This is an IPv6 address.
    /// @retval false
    ///   This is not an IPv6 address.
    [[nodiscard]]
    constexpr auto isIpv6() const noexcept -> bool {
        return m_isV6;
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
    constexpr auto isIpv4Loopback() const noexcept -> bool {
        return isIpv4() && (m_addr.v4.s_addr == toNetworkEndian(INADDR_LOOPBACK));
    }

    /// @brief
    ///   Checks if this address is an IPv4 any address. IPv4 any address is @c 0.0.0.0.
    /// @retval true
    ///   This address is an IPv4 any address.
    /// @retval false
    ///   This address is not an IPv4 any address.
    [[nodiscard]]
    constexpr auto isIpv4Any() const noexcept -> bool {
        return isIpv4() && (m_addr.v4.s_addr == toNetworkEndian(INADDR_ANY));
    }

    /// @brief
    ///   Checks if this address is an IPv4 broadcast address. IPv4 broadcast address is @c
    ///   255.255.255.255.
    /// @retval true
    ///   This address is an IPv4 broadcast address.
    /// @retval false
    ///   This address is not an IPv4 broadcast address.
    [[nodiscard]]
    constexpr auto isIpv4Broadcast() const noexcept -> bool {
        return isIpv4() && (m_addr.v4.s_addr == toNetworkEndian(INADDR_BROADCAST));
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
    constexpr auto isIpv4Private() const noexcept -> bool {
        if (!isIpv4())
            return false;

        // 10.0.0.0/8
        if ((toHostEndian(m_addr.v4.s_addr) >> 24) == 0x0A)
            return true;

        // 172.16.0.0/12
        if ((toHostEndian(m_addr.v4.s_addr) >> 20) == 0xAC1)
            return true;

        // 192.168.0.0/16
        if ((toHostEndian(m_addr.v4.s_addr) >> 16) == 0xC0A8)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv4 link local address. IPv4 link local address is @c
    ///   169.254.0.0/16 as defined in RFC 3927.
    /// @retval true
    ///   This address is an IPv4 link local address.
    /// @retval false
    ///   This address is not an IPv4 link local address.
    [[nodiscard]]
    constexpr auto isIpv4Linklocal() const noexcept -> bool {
        if (!isIpv4())
            return false;

        if ((toHostEndian(m_addr.v4.s_addr) >> 16) == 0xA9FE)
            return true;

        return false;
    }

    /// @brief
    ///   Checks if this address is an IPv4 multicast address. IPv4 multicast address is @c
    ///   224.0.0.0/4 as defined in RFC 5771.
    /// @retval true
    ///   This address is an IPv4 multicast address.
    /// @retval false
    ///   This address is not an IPv4 multicast address.
    [[nodiscard]]
    constexpr auto isIpv4Multicast() const noexcept -> bool {
        if (!isIpv4())
            return false;

        if ((toHostEndian(m_addr.v4.s_addr) >> 28) == 0xE)
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
    constexpr auto isIpv6Loopback() const noexcept -> bool {
        if (!isIpv6())
            return false;

        return ((m_addr.v6.s6_addr16[0] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[6] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[7] == toNetworkEndian<std::uint16_t>(1)));
    }

    /// @brief
    ///   Checks if this address is an IPv6 any address. IPv6 any address is @c ::.
    /// @retval true
    ///   This address is an IPv6 any address.
    /// @retval false
    ///   This address is not an IPv6 any address.
    [[nodiscard]]
    constexpr auto isIpv6Any() const noexcept -> bool {
        if (!isIpv6())
            return false;

        return ((m_addr.v6.s6_addr16[0] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[6] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[7] == toNetworkEndian<std::uint16_t>(0)));
    }

    /// @brief
    ///   Checks if this address is an IPv6 multicast address. IPv6 multicast address is @c FF00::/8
    ///   as defined in RFC 4291.
    /// @retval true
    ///   This address is an IPv6 multicast address.
    /// @retval false
    ///   This address is not an IPv6 multicast address.
    [[nodiscard]]
    constexpr auto isIpv6Multicast() const noexcept -> bool {
        if (!isIpv6())
            return false;
        return (m_addr.v6.s6_addr[0] == 0xFF);
    }

    /// @brief
    ///   Checks if this address is an IPv4 mapped IPv6 address. IPv4 mapped IPv6 address is @c
    ///   ::FFFF:0:0/96.
    /// @retval true
    ///   This is an IPv4 mapped IPv6 address.
    /// @retval false
    ///   This is not an IPv4 mapped IPv6 address.
    [[nodiscard]]
    constexpr auto isIpv4MappedIpv6() const noexcept -> bool {
        if (!isIpv6())
            return false;

        return ((m_addr.v6.s6_addr16[0] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[1] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[2] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[3] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[4] == toNetworkEndian<std::uint16_t>(0)) &&
                (m_addr.v6.s6_addr16[5] == toNetworkEndian<std::uint16_t>(0xFFFF)));
    }

    /// @brief
    ///   Converts this IP address to IPv4 address.
    /// @return
    ///   Return this address if this is an IPv4 or IPv4-mapped IPv6 address. It is undefined
    ///   behavior if this is either not an IPv4 address or an IPv4-mapped IPv6 address.
    [[nodiscard]]
    constexpr auto toIpv4() const noexcept -> IpAddress {
        if (isIpv4())
            return *this;
        return {m_addr.v6.s6_addr[12], m_addr.v6.s6_addr[13], m_addr.v6.s6_addr[14],
                m_addr.v6.s6_addr[15]};
    }

    /// @brief
    ///   Converts this IP address to IPv6 address.
    /// @return
    ///   Return an IPv4-mapped IPv6 address if this is an IPv4 address. Otherwise, return this IPv6
    ///   address itself.
    [[nodiscard]]
    constexpr auto toIpv6() const noexcept -> IpAddress {
        if (isIpv6())
            return *this;

        const std::uint32_t v4 = toHostEndian(m_addr.v4.s_addr);
        const auto high        = static_cast<std::uint16_t>(v4 >> 16);
        const auto low         = static_cast<std::uint16_t>(v4 & 0xFFFF);

        return {0, 0, 0, 0, 0, 0xFFFF, high, low};
    }

    /// @brief
    ///   Checks if this @c IpAddress is the same as another one.
    /// @param other
    ///   The @c IpAddress to be compared with.
    /// @retval true
    ///   This @c IpAddress is the same as @p other.
    /// @retval false
    ///   This @c IpAddress is not the same as @p other.
    constexpr auto operator==(const Self &other) const noexcept -> bool {
        if (m_isV6 != other.m_isV6)
            return false;

        if (isIpv4())
            return !__builtin_memcmp(&m_addr.v4, &other.m_addr.v4, sizeof(m_addr.v4));
        return !__builtin_memcmp(&m_addr.v6, &other.m_addr.v6, sizeof(m_addr.v6));
    }

    friend class InetAddress;

private:
    bool m_isV6;
    union {
        struct in_addr v4;
        struct in6_addr v6;
    } m_addr;
};

/// @brief
///   IPv4 loopback address.
inline constexpr IpAddress Ipv4Loopback{127, 0, 0, 1};

/// @brief
///   IPv4 address that listens to any incoming address.
inline constexpr IpAddress Ipv4Any(0, 0, 0, 0);

/// @brief
///   IPv4 broadcast address.
inline constexpr IpAddress Ipv4Broadcast(255, 255, 255, 255);

/// @brief
///   IPv6 loopback address.
inline constexpr IpAddress Ipv6Loopback(0, 0, 0, 0, 0, 0, 0, 1);

/// @brief
///   IPv6 address that listens to any incoming address.
inline constexpr IpAddress Ipv6Any(0, 0, 0, 0, 0, 0, 0, 0);

/// @class InetAddress
/// @brief
///   Wrapper class for Internet socket address. This class could be directly used as @c sockaddr_in
///   and @c sockaddr_in6.
class InetAddress {
public:
    using Self = InetAddress;

    /// @brief
    ///   Create an empty Internet address. Empty Internet address cannot be used for any network
    ///   operation.
    constexpr InetAddress() noexcept : m_addr() {}

    /// @brief
    ///   Create a new Internet address from an IP address and port.
    /// @param ip
    ///   IP address of this Internet address.
    /// @param port
    ///   Port number in host endian.
    constexpr InetAddress(const IpAddress &ip, std::uint16_t port) noexcept : m_addr() {
        if (ip.isIpv4()) {
            m_addr.v4.sin_family = AF_INET;
            m_addr.v4.sin_port   = toNetworkEndian(port);
            m_addr.v4.sin_addr   = ip.m_addr.v4;
        } else {
            m_addr.v6.sin6_family   = AF_INET6;
            m_addr.v6.sin6_port     = toNetworkEndian(port);
            m_addr.v6.sin6_flowinfo = 0;
            m_addr.v6.sin6_addr     = ip.m_addr.v6;
            m_addr.v6.sin6_scope_id = 0;
        }
    }

    /// @brief
    ///   @c InetAddress is trivially copyable.
    /// @param other
    ///   The Internet address to be copied from.
    constexpr InetAddress(const Self &other) noexcept = default;

    /// @brief
    ///   @c InetAddress is trivially movable.
    /// @param other
    ///   The Internet address to be moved from.
    constexpr InetAddress(Self &&other) noexcept = default;

    /// @brief
    ///   @c InetAddress is trivially destructible.
    constexpr ~InetAddress() = default;

    /// @brief
    ///   @c InetAddress is trivially copyable.
    /// @param other
    ///   The Internet address to be copied from.
    /// @return
    ///   Reference to this Internet address.
    constexpr auto operator=(const Self &other) noexcept -> Self & = default;

    /// @brief
    ///   @c InetAddress is trivially movable.
    /// @param other
    ///   The Internet address to be moved from.
    /// @return
    ///   Reference to this Internet address.
    constexpr auto operator=(Self &&other) noexcept -> Self & = default;

    /// @brief
    ///   Checks if this is an IPv4 Internet address.
    /// @retval true
    ///   This is an IPv4 Internet address.
    /// @retval false
    ///   This is not an IPv4 Internet address.
    [[nodiscard]]
    constexpr auto isIpv4() const noexcept -> bool {
        return m_addr.v4.sin_family == AF_INET;
    }

    /// @brief
    ///   Checks if this is an IPv6 Internet address.
    /// @retval true
    ///   This is an IPv6 Internet address.
    /// @retval false
    ///   This is not an IPv6 Internet address.
    [[nodiscard]]
    constexpr auto isIpv6() const noexcept -> bool {
        return m_addr.v6.sin6_family == AF_INET6;
    }

    /// @brief
    ///   Get actual size in byte of this Internet address object.
    /// @return
    ///   Actual size in byte of this Internet address object. It is undefined behavior to get size
    ///   for empty Internet address object.
    [[nodiscard]]
    constexpr auto size() const noexcept -> socklen_t {
        return isIpv4() ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
    }

    /// @brief
    ///   Get IP address of this Internet address. It is undefined behavior to get IP address from
    ///   empty Internet address.
    /// @return
    ///   IP address of this Internet address.
    [[nodiscard]]
    constexpr auto ip() const noexcept -> IpAddress {
        if (isIpv4()) {
            IpAddress addr;
            addr.m_isV6    = false;
            addr.m_addr.v4 = m_addr.v4.sin_addr;
            return addr;
        } else {
            IpAddress addr;
            addr.m_isV6    = true;
            addr.m_addr.v6 = m_addr.v6.sin6_addr;
            return addr;
        }
    }

    /// @brief
    ///   Get port of this Internet address. It is undefined behavior to get port number from empty
    ///   Internet address.
    /// @return
    ///   Port number in host endian.
    [[nodiscard]]
    constexpr auto port() const noexcept -> std::uint16_t {
        return toHostEndian(m_addr.v4.sin_port);
    }

    /// @brief
    ///   Set port of this Internet address.
    /// @param port
    ///   The port in host endian to be set.
    constexpr auto setPort(std::uint16_t port) noexcept -> void {
        m_addr.v4.sin_port = toNetworkEndian(port);
    }

    /// @brief
    ///   Get IPv6 flow label.
    /// @return
    ///   IPv6 flow label. The return value is undefined if this is not an IPv6 address.
    [[nodiscard]]
    constexpr auto flowLabel() const noexcept -> std::uint32_t {
        return toHostEndian(m_addr.v6.sin6_flowinfo);
    }

    /// @brief
    ///   Set flow label for this IPv6 Internet address. It is undefined behavior to set flow label
    ///   for IPv4 address.
    /// @param label
    ///   The flow label in host endian to be set.
    constexpr auto setFlowLabel(std::uint32_t label) noexcept -> void {
        m_addr.v6.sin6_flowinfo = toNetworkEndian(label);
    }

    /// @brief
    ///   Get IPv6 scope ID.
    /// @return
    ///   IPv6 scope ID in host endian. It is undefined behavior to get scope ID from IPv4 Internet
    ///   address.
    [[nodiscard]]
    constexpr auto scopeId() const noexcept -> std::uint32_t {
        return toHostEndian(m_addr.v6.sin6_scope_id);
    }

    /// @brief
    ///   Set scope ID for this IPv6 Internet address. It is undefined behavior to set scope ID for
    ///   IPv4 Internet address.
    /// @param id
    ///   The scope ID in host endian to be set.
    constexpr auto setScopeId(std::uint32_t id) noexcept -> void {
        m_addr.v6.sin6_scope_id = toNetworkEndian(id);
    }

    /// @brief
    ///   Checks if this @c InetAddress is the same as another one.
    /// @param other
    ///   The @c InetAddress to be compared with.
    /// @retval true
    ///   This @c InetAddress is the same as @p other.
    /// @retval false
    ///   This @c InetAddress is not the same as @p other.
    constexpr auto operator==(const Self &other) const noexcept -> bool {
        if (isIpv4())
            return !__builtin_memcmp(&m_addr.v4, &other.m_addr.v4, sizeof(m_addr.v4));
        return !__builtin_memcmp(&m_addr.v6, &other.m_addr.v6, sizeof(m_addr.v6));
    }

private:
    union {
        struct sockaddr_in v4;
        struct sockaddr_in6 v6;
    } m_addr;
};

} // namespace nyaio
