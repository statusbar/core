// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

// Verifies, on the host, that ZephyrEthPort satisfies the EthernetPort concept.
// The static_assert lives in the header and fires on inclusion; this TU exists
// so it is checked in the normal (non-board) build. ZephyrEthPort's methods are
// defined only in net_zephyr_eth.cpp (Zephyr-only) and are not called here, so
// there is nothing to link.

#include "statusbar/net/net_zephyr_eth.hpp"
#include "statusbar/test/test.hpp"

using namespace statusbar;

TEST(net_zephyr_eth_concept, satisfies_ethernet_port)
{
    // The contract is the header's static_assert(EthernetPort<ZephyrEthPort>);
    // reaching here means the TU compiled, i.e. the concept is satisfied.
    static_assert(net::EthernetPort<net::ZephyrEthPort>);
    EXPECT_TRUE(true);
}

TEST_MAIN(statusbar_net, net_zephyr_eth_concept_test)
