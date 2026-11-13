// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/net/net_socket.hpp"
#include "statusbar/test/test.hpp"

#include <cstring>
#include <system_error>

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>

using namespace statusbar;
using namespace statusbar::net;

// Joining on loopback needs no privilege and no hardware, so the whole path -
// socket, setsockopt, the ip_mreqn the platform wants - is exercised here.
TEST(group_membership, a_group_is_joined_on_the_named_interface_and_held)
{
#if defined(__linux__)
    auto const lo = ::if_nametoindex("lo");
#else
    auto const lo = ::if_nametoindex("lo0");  // what Darwin and the BSDs call it
#endif
    EXPECT_TRUE(lo > 0);
    if (lo == 0) {
        return;
    }
    auto membership = open_group_membership(statusbar::ip::IPv4Address{239, 69, 1, 2}, lo);
    EXPECT_TRUE(membership.has_value());
    if (membership) {
        EXPECT_TRUE(membership->valid());
    }
}

// The membership lasts exactly as long as the value: an AF_XDP receiver has no
// kernel socket on the group, so this is the only thing telling the switch to
// keep forwarding it.
TEST(group_membership, letting_the_membership_go_closes_its_socket)
{
    auto const lo = ::if_nametoindex("lo");
    if (lo == 0) {
        return;
    }
    int borrowed = -1;
    {
        auto membership = open_group_membership(statusbar::ip::IPv4Address{239, 69, 1, 3}, lo);
        EXPECT_TRUE(membership.has_value());
        if (!membership) {
            return;
        }
        borrowed = membership->get();
        EXPECT_TRUE(borrowed >= 0);
    }
    // The descriptor is closed, so asking the kernel about it fails. (A later
    // open may reuse the number, which is why the assertion is on the call and
    // not on the number.)
    EXPECT_TRUE(::fcntl(borrowed, F_GETFD) == -1);
}

// A unicast address is not a group. The kernel refuses it, and the refusal is
// reported rather than a socket with no membership being handed back.
TEST(group_membership, an_address_that_is_not_a_group_is_refused)
{
    auto const lo = ::if_nametoindex("lo");
    if (lo == 0) {
        return;
    }
    auto const membership = open_group_membership(statusbar::ip::IPv4Address{192, 168, 1, 1}, lo);
    EXPECT_FALSE(membership.has_value());
}

// An interface index nothing answers to is refused too.
TEST(group_membership, an_interface_that_is_not_there_is_refused)
{
    constexpr unsigned int NO_SUCH_INTERFACE = 0x7FFFFFFF;
    auto const membership = open_group_membership(statusbar::ip::IPv4Address{239, 69, 1, 4}, NO_SUCH_INTERFACE);
    EXPECT_FALSE(membership.has_value());
}

//
// Source-specific groups (RFC 4607)
//

// 232.0.0.0/8 is the source-specific range, and nothing on either side of it.
TEST(source_group_membership, the_source_specific_range_is_232_slash_8)
{
    using statusbar::ip::IPv4Address;
    static_assert(!is_source_specific_group(IPv4Address{231, 255, 255, 255}));
    static_assert(is_source_specific_group(IPv4Address{232, 0, 0, 0}));
    static_assert(is_source_specific_group(IPv4Address{232, 255, 255, 255}));
    static_assert(!is_source_specific_group(IPv4Address{233, 0, 0, 0}));
    static_assert(!is_source_specific_group(IPv4Address{239, 69, 1, 2}));
    EXPECT_TRUE(is_source_specific_group(IPv4Address{232, 1, 2, 3}));
}

// The sockaddr the join carries names the address, the family and, where the
// stack has the field, its own length.
TEST(source_group_membership, the_storage_names_the_address)
{
    auto const storage = ipv4_sockaddr_storage(statusbar::ip::IPv4Address{192, 0, 2, 1});
    sockaddr_in in{};
    std::memcpy(&in, &storage, sizeof(in));
    EXPECT_EQ(static_cast<int>(in.sin_family), AF_INET);
    EXPECT_EQ(ntohl(in.sin_addr.s_addr), 0xC0000201U);
#if defined(__APPLE__) || defined(__FreeBSD__)
    EXPECT_EQ(static_cast<size_t>(in.sin_len), sizeof(in));
#endif
}

// A join is refused before the kernel sees it when the group is not a group,
// or the source is not a host: silence on the socket later would be the only
// other symptom.
TEST(source_group_membership, a_bad_group_or_source_is_refused_as_invalid_argument)
{
    using statusbar::ip::IPv4Address;
    auto socket = make_udp_socket();
    EXPECT_TRUE(socket.has_value());
    auto const invalid = std::make_error_code(std::errc::invalid_argument);
    auto const unicast_group = join_source_group(socket->get(), IPv4Address{192, 0, 2, 9}, IPv4Address{192, 0, 2, 1}, 0);
    EXPECT_FALSE(unicast_group.has_value());
    EXPECT_EQ(unicast_group.error(), invalid);
    auto const multicast_source = join_source_group(socket->get(), IPv4Address{232, 1, 2, 3}, IPv4Address{239, 0, 0, 1}, 0);
    EXPECT_FALSE(multicast_source.has_value());
    EXPECT_EQ(multicast_source.error(), invalid);
    auto const unspecified_source = join_source_group(socket->get(), IPv4Address{232, 1, 2, 3}, IPv4Address{0, 0, 0, 0}, 0);
    EXPECT_FALSE(unspecified_source.has_value());
    EXPECT_EQ(unspecified_source.error(), invalid);
    auto const membership = open_source_group_membership(IPv4Address{192, 0, 2, 9}, IPv4Address{192, 0, 2, 1}, 0);
    EXPECT_FALSE(membership.has_value());
}

// A real INCLUDE-mode join on the loopback interface, held by its socket. A
// stack that cannot take one on loopback says so with an errno, which is
// reported rather than failed: the join itself is the kernel's to grant.
TEST(source_group_membership, a_source_specific_group_is_joined_on_loopback_and_held)
{
    using statusbar::ip::IPv4Address;
#if defined(__linux__)
    auto const lo = ::if_nametoindex("lo");
#else
    auto const lo = ::if_nametoindex("lo0");
#endif
    if (lo == 0) {
        SKIP("no loopback interface by either name");
    }
    auto membership = open_source_group_membership(IPv4Address{232, 1, 2, 3}, IPv4Address{192, 0, 2, 1}, lo);
    if (!membership) {
        auto const err = membership.error();
        bool const platform_said_no = err == std::make_error_code(std::errc::address_not_available) ||
            err == std::make_error_code(std::errc::no_such_device) ||
            err == std::make_error_code(std::errc::operation_not_permitted) ||
            err == std::make_error_code(std::errc::operation_not_supported);
        if (platform_said_no) {
            SKIP("the stack refused a source-specific join on loopback");
        }
    }
    EXPECT_TRUE(membership.has_value());
    if (membership) {
        EXPECT_TRUE(membership->valid());
    }
}

TEST_MAIN(statusbar_net, net_group_membership_test)
