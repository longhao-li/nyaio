#include "nyaio/io.hpp"

#include <doctest/doctest.h>

#include <stdexcept>
#include <tuple>

using namespace nyaio;

TEST_CASE("[io/ip] IpAddress IPv4 loopback") {
    CHECK_NOTHROW(std::ignore = IpAddress("127.0.0.1"));
    CHECK_THROWS_AS(std::ignore = IpAddress(""), std::invalid_argument &);
    CHECK_THROWS_AS(std::ignore = IpAddress("256.123.12.345"), std::invalid_argument &);

    IpAddress addr(127, 0, 0, 1);
    CHECK(addr == Ipv4Loopback);
    CHECK(addr == IpAddress("127.0.0.1"));

    CHECK(addr.isIpv4());
    CHECK(addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(!addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv4() == addr);
}

TEST_CASE("[io/ip] IpAddress IPv4 broadcast") {
    CHECK_NOTHROW(std::ignore = IpAddress("255.255.255.255"));

    IpAddress addr(255, 255, 255, 255);
    CHECK(addr == Ipv4Broadcast);
    CHECK(addr == IpAddress("255.255.255.255"));

    CHECK(addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(!addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv4() == addr);
}

TEST_CASE("[io/ip] IpAddress IPv4 any") {
    CHECK_NOTHROW(std::ignore = IpAddress("0.0.0.0"));

    IpAddress addr(0, 0, 0, 0);
    CHECK(addr == Ipv4Any);
    CHECK(addr == IpAddress("0.0.0.0"));

    CHECK(addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(!addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv4() == addr);
}

TEST_CASE("[io/ip] IpAddress IPv4 private") {
    // some class A private network
    CHECK_NOTHROW(std::ignore = IpAddress("10.0.3.1"));
    // some class B private network
    CHECK_NOTHROW(std::ignore = IpAddress("172.18.2.144"));
    // some class C private network
    CHECK_NOTHROW(std::ignore = IpAddress("192.168.233.233"));

    IpAddress a(10, 0, 3, 1);
    CHECK(a == IpAddress("10.0.3.1"));

    CHECK(a.isIpv4());
    CHECK(!a.isIpv4Loopback());
    CHECK(!a.isIpv4Any());
    CHECK(!a.isIpv4Broadcast());
    CHECK(a.isIpv4Private());
    CHECK(!a.isIpv4Linklocal());
    CHECK(!a.isIpv4Multicast());

    CHECK(!a.isIpv6());
    CHECK(!a.isIpv6Loopback());
    CHECK(!a.isIpv6Any());
    CHECK(!a.isIpv6Multicast());
    CHECK(!a.isIpv4MappedIpv6());

    CHECK(a.toIpv4() == a);

    IpAddress b(172, 18, 2, 144);
    CHECK(b == IpAddress("172.18.2.144"));

    CHECK(b.isIpv4());
    CHECK(!b.isIpv4Loopback());
    CHECK(!b.isIpv4Any());
    CHECK(!b.isIpv4Broadcast());
    CHECK(b.isIpv4Private());
    CHECK(!b.isIpv4Linklocal());
    CHECK(!b.isIpv4Multicast());

    CHECK(!b.isIpv6());
    CHECK(!b.isIpv6Loopback());
    CHECK(!b.isIpv6Any());
    CHECK(!b.isIpv6Multicast());
    CHECK(!b.isIpv4MappedIpv6());

    CHECK(b.toIpv4() == b);

    IpAddress c(192, 168, 233, 233);
    CHECK(c == IpAddress("192.168.233.233"));

    CHECK(c.isIpv4());
    CHECK(!c.isIpv4Loopback());
    CHECK(!c.isIpv4Any());
    CHECK(!c.isIpv4Broadcast());
    CHECK(c.isIpv4Private());
    CHECK(!c.isIpv4Linklocal());
    CHECK(!c.isIpv4Multicast());

    CHECK(!c.isIpv6());
    CHECK(!c.isIpv6Loopback());
    CHECK(!c.isIpv6Any());
    CHECK(!c.isIpv6Multicast());
    CHECK(!c.isIpv4MappedIpv6());

    CHECK(c.toIpv4() == c);
}

TEST_CASE("[io/ip] IpAddress IPv4 link-local") {
    CHECK_NOTHROW(std::ignore = IpAddress("169.254.1.23"));

    IpAddress addr(169, 254, 1, 23);
    CHECK(addr == IpAddress("169.254.1.23"));

    CHECK(addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(!addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv4() == addr);
}

TEST_CASE("[io/ip] IpAddress IPv4 multicast") {
    CHECK_NOTHROW(std::ignore = IpAddress("224.1.2.3"));

    IpAddress addr(224, 1, 2, 3);
    CHECK(addr == IpAddress("224.1.2.3"));

    CHECK(addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(addr.isIpv4Multicast());

    CHECK(!addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv4() == addr);
}

TEST_CASE("[io/ip] IpAddress IPv6 loopback") {
    CHECK_NOTHROW(std::ignore = IpAddress("::1"));
    CHECK_THROWS_AS(std::ignore = IpAddress(":::"), std::invalid_argument &);
    CHECK_THROWS_AS(std::ignore = IpAddress("FFFF0::"), std::invalid_argument &);

    IpAddress addr(0, 0, 0, 0, 0, 0, 0, 1);
    CHECK(addr == Ipv6Loopback);
    CHECK(addr == IpAddress("::1"));

    CHECK(!addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(addr.isIpv6());
    CHECK(addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv6() == addr);
}

TEST_CASE("[io/ip] IpAddress IPv6 any") {
    CHECK_NOTHROW(std::ignore = IpAddress("::"));

    IpAddress addr(0, 0, 0, 0, 0, 0, 0, 0);
    CHECK(addr == Ipv6Any);
    CHECK(addr == IpAddress("::"));

    CHECK(!addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv6() == addr);
}

TEST_CASE("[ip] IpAddress IPv6 multi-cast") {
    CHECK_NOTHROW(std::ignore = IpAddress("FF02::1"));

    IpAddress addr(0xFF02, 0, 0, 0, 0, 0, 0, 1);
    CHECK(addr == IpAddress("FF02::1"));

    CHECK(!addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());

    CHECK(addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(addr.isIpv6Multicast());
    CHECK(!addr.isIpv4MappedIpv6());

    CHECK(addr.toIpv6() == addr);
}

TEST_CASE("[ip] IpAddress IPv4-mapped IPv6") {
    CHECK_NOTHROW(std::ignore = IpAddress("::FFFF:7F00:1"));

    IpAddress addr(0, 0, 0, 0, 0, 0xFFFF, 0x7F00, 0x0001);
    CHECK(addr == IpAddress("::FFFF:7F00:1"));

    CHECK(!addr.isIpv4());
    CHECK(!addr.isIpv4Loopback());
    CHECK(!addr.isIpv4Any());
    CHECK(!addr.isIpv4Broadcast());
    CHECK(!addr.isIpv4Private());
    CHECK(!addr.isIpv4Linklocal());
    CHECK(!addr.isIpv4Multicast());
    CHECK(addr.isIpv4MappedIpv6());

    CHECK(addr.isIpv6());
    CHECK(!addr.isIpv6Loopback());
    CHECK(!addr.isIpv6Any());
    CHECK(!addr.isIpv6Multicast());

    CHECK(addr.toIpv4() == IpAddress(127, 0, 0, 1));
    CHECK(addr.toIpv6() == addr);

    CHECK(addr == IpAddress(127, 0, 0, 1).toIpv6());
}
