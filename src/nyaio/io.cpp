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
