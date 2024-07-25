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
