#include "nyaio/network.hpp"

#include <doctest/doctest.h>

using namespace nyaio;

TEST_CASE("ip address ipv4 loopback") {
    CHECK_NOTHROW(std::ignore = ip_address("127.0.0.1"));
    CHECK_THROWS_AS(std::ignore = ip_address(""), std::invalid_argument &);
    CHECK_THROWS_AS(std::ignore = ip_address("256.123.12.345"), std::invalid_argument &);

    ip_address addr(127, 0, 0, 1);
    CHECK(addr == ipv4_loopback);
    CHECK(addr == ip_address("127.0.0.1"));

    CHECK(addr.is_ipv4());
    CHECK(addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(!addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(addr.to_ipv4() == addr);
}

TEST_CASE("ip address ipv4 broadcast") {
    CHECK_NOTHROW(std::ignore = ip_address("255.255.255.255"));

    ip_address addr(255, 255, 255, 255);
    CHECK(addr == ipv4_broadcast);
    CHECK(addr == ip_address("255.255.255.255"));

    CHECK(addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(!addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(addr.to_ipv4() == addr);
}

TEST_CASE("ip address ipv4 any") {
    CHECK_NOTHROW(std::ignore = ip_address("0.0.0.0"));

    ip_address addr(0, 0, 0, 0);
    CHECK(addr == ipv4_any);
    CHECK(addr == ip_address("0.0.0.0"));

    CHECK(addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(!addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(addr.to_ipv4() == addr);
}

TEST_CASE("ip address ipv4 private") {
    // some class A private network
    CHECK_NOTHROW(std::ignore = ip_address("10.0.3.1"));
    // some class B private network
    CHECK_NOTHROW(std::ignore = ip_address("172.18.2.144"));
    // some class C private network
    CHECK_NOTHROW(std::ignore = ip_address("192.168.233.233"));

    ip_address a(10, 0, 3, 1);
    CHECK(a == ip_address("10.0.3.1"));

    CHECK(a.is_ipv4());
    CHECK(!a.is_ipv4_loopback());
    CHECK(!a.is_ipv4_any());
    CHECK(!a.is_ipv4_broadcast());
    CHECK(a.is_ipv4_private());
    CHECK(!a.is_ipv4_linklocal());
    CHECK(!a.is_ipv4_multicast());

    CHECK(!a.is_ipv6());
    CHECK(!a.is_ipv6_loopback());
    CHECK(!a.is_ipv6_any());
    CHECK(!a.is_ipv6_multicast());
    CHECK(!a.is_ipv4_mapped_ipv6());

    CHECK(a.to_ipv4() == a);

    ip_address b(172, 18, 2, 144);
    CHECK(b == ip_address("172.18.2.144"));

    CHECK(b.is_ipv4());
    CHECK(!b.is_ipv4_loopback());
    CHECK(!b.is_ipv4_any());
    CHECK(!b.is_ipv4_broadcast());
    CHECK(b.is_ipv4_private());
    CHECK(!b.is_ipv4_linklocal());
    CHECK(!b.is_ipv4_multicast());

    CHECK(!b.is_ipv6());
    CHECK(!b.is_ipv6_loopback());
    CHECK(!b.is_ipv6_any());
    CHECK(!b.is_ipv6_multicast());
    CHECK(!b.is_ipv4_mapped_ipv6());

    CHECK(b.to_ipv4() == b);

    ip_address c(192, 168, 233, 233);
    CHECK(c == ip_address("192.168.233.233"));

    CHECK(c.is_ipv4());
    CHECK(!c.is_ipv4_loopback());
    CHECK(!c.is_ipv4_any());
    CHECK(!c.is_ipv4_broadcast());
    CHECK(c.is_ipv4_private());
    CHECK(!c.is_ipv4_linklocal());
    CHECK(!c.is_ipv4_multicast());

    CHECK(!c.is_ipv6());
    CHECK(!c.is_ipv6_loopback());
    CHECK(!c.is_ipv6_any());
    CHECK(!c.is_ipv6_multicast());
    CHECK(!c.is_ipv4_mapped_ipv6());

    CHECK(c.to_ipv4() == c);
}

TEST_CASE("ip address ipv4 link-local") {
    CHECK_NOTHROW(std::ignore = ip_address("169.254.1.23"));

    ip_address addr(169, 254, 1, 23);
    CHECK(addr == ip_address("169.254.1.23"));

    CHECK(addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(!addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(addr.to_ipv4() == addr);
}

TEST_CASE("ip address ipv4 multicast") {
    CHECK_NOTHROW(std::ignore = ip_address("224.1.2.3"));

    ip_address addr(224, 1, 2, 3);
    CHECK(addr == ip_address("224.1.2.3"));

    CHECK(addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(addr.is_ipv4_multicast());

    CHECK(!addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(addr.to_ipv4() == addr);
}

TEST_CASE("ip address ipv6 loopback") {
    CHECK_NOTHROW(std::ignore = ip_address("::1"));
    CHECK_THROWS_AS(std::ignore = ip_address(":::"), std::invalid_argument &);
    CHECK_THROWS_AS(std::ignore = ip_address("FFFF0::"), std::invalid_argument &);

    ip_address addr(0, 0, 0, 0, 0, 0, 0, 1);
    CHECK(addr == ipv6_loopback);
    CHECK(addr == ip_address("::1"));

    CHECK(!addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(addr.is_ipv6());
    CHECK(addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(!addr.to_ipv4().has_value());
    CHECK(addr.to_ipv6() == addr);

    CHECK(addr.scope_id() == 0);
    addr.set_scope_id(1234);
    CHECK(addr.scope_id() == 1234);
}

TEST_CASE("ip address ipv6 any") {
    CHECK_NOTHROW(std::ignore = ip_address("::"));

    ip_address addr(0, 0, 0, 0, 0, 0, 0, 0);
    CHECK(addr == ipv6_any);
    CHECK(addr == ip_address("::"));

    CHECK(!addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(!addr.to_ipv4().has_value());
    CHECK(addr.to_ipv6() == addr);

    CHECK(addr.scope_id() == 0);
    addr.set_scope_id(1234);
    CHECK(addr.scope_id() == 1234);
}

TEST_CASE("ip address ipv6 multi-cast") {
    CHECK_NOTHROW(std::ignore = ip_address("FF02::1"));

    ip_address addr(0xFF02, 0, 0, 0, 0, 0, 0, 1);
    CHECK(addr == ip_address("FF02::1"));

    CHECK(!addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());

    CHECK(addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(addr.is_ipv6_multicast());
    CHECK(!addr.is_ipv4_mapped_ipv6());

    CHECK(!addr.to_ipv4().has_value());
    CHECK(addr.to_ipv6() == addr);

    CHECK(addr.scope_id() == 0);
    addr.set_scope_id(1234);
    CHECK(addr.scope_id() == 1234);
}

TEST_CASE("ip address ipv4-mapped ipv6") {
    CHECK_NOTHROW(std::ignore = ip_address("::FFFF:7F00:1"));

    ip_address addr(0, 0, 0, 0, 0, 0xFFFF, 0x7F00, 0x0001);
    CHECK(addr == ip_address("::FFFF:7F00:1"));

    CHECK(!addr.is_ipv4());
    CHECK(!addr.is_ipv4_loopback());
    CHECK(!addr.is_ipv4_any());
    CHECK(!addr.is_ipv4_broadcast());
    CHECK(!addr.is_ipv4_private());
    CHECK(!addr.is_ipv4_linklocal());
    CHECK(!addr.is_ipv4_multicast());
    CHECK(addr.is_ipv4_mapped_ipv6());

    CHECK(addr.is_ipv6());
    CHECK(!addr.is_ipv6_loopback());
    CHECK(!addr.is_ipv6_any());
    CHECK(!addr.is_ipv6_multicast());

    CHECK(addr.to_ipv4().value() == ip_address(127, 0, 0, 1));
    CHECK(addr.to_ipv6() == addr);

    CHECK(addr == ip_address(127, 0, 0, 1).to_ipv6());
}
