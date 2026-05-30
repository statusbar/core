// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT
//
// IEEE benchmark tool — measures EthernetFrame load/store and Eui48 ops.
// These are on the hot path for any raw-Ethernet processing, so the
// difference between "a few cycles" and "a few tens of cycles" matters.
//
// Build: make build
// Run:   ./build/build-Release/statusbar/statusbar-ieee-bench

#include "statusbar/benchmark/benchmark.hpp"
#include "statusbar/buffer/buffer.hpp"
#include "statusbar/ieee/ieee.hpp"

#include <array>
#include <cstdint>
#include <print>
#include <span>

using namespace statusbar::benchmark;
using namespace statusbar;
using namespace statusbar::ieee;
using statusbar::protocol::can_load;
using statusbar::protocol::can_store;
using statusbar::protocol::load;
using statusbar::protocol::store;
using statusbar::protocol::wire_size;

namespace {

auto make_ethernet_bytes() -> std::array<uint8_t, 18>
{
    return std::array<uint8_t, 18>{
        // dest MAC
        0x01,
        0x80,
        0xC2,
        0x00,
        0x00,
        0x0E,
        // src MAC
        0x00,
        0x11,
        0x22,
        0x33,
        0x44,
        0x55,
        // VLAN tag (tpid=0x8100, tci=0x0064)
        0x81,
        0x00,
        0x00,
        0x64,
        // ethertype (0x88F7 = gPTP)
        0x88,
        0xF7};
}

void bench_ethernet_load()
{
    std::println("=== EthernetFrame load ===\n");

    BenchmarkConfig const cfg{.warmup_iterations = 500, .measurement_iterations = 5000, .batch_size = 100};

    auto const bytes = make_ethernet_bytes();
    std::span<uint8_t const> const buf{bytes};

    {
        auto stats = run_hw(
            "ethernet/load_unchecked",
            [&](size_t n) {
                EthernetFrame frame{};
                for (size_t k = 0; k < n; ++k) {
                    auto consumed = load_unchecked(buf, &frame);
                    do_not_optimize(consumed);
                    do_not_optimize(frame);
                }
            },
            cfg);
        report("ethernet/load_unchecked", stats);
    }

    {
        auto stats = run_hw(
            "ethernet/can_load",
            [&](size_t n) {
                EthernetFrame frame{};
                for (size_t k = 0; k < n; ++k) {
                    auto result = can_load(buf, &frame);
                    do_not_optimize(result);
                }
            },
            cfg);
        report("ethernet/can_load", stats);
    }

    std::println("");
}

void bench_ethernet_store()
{
    std::println("=== EthernetFrame store ===\n");

    BenchmarkConfig const cfg{.warmup_iterations = 500, .measurement_iterations = 5000, .batch_size = 100};

    EthernetFrame frame{};
    frame.dest_mac = Eui48{0x01, 0x80, 0xC2, 0x00, 0x00, 0x0E};
    frame.src_mac = Eui48{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};
    frame.ethertype = 0x88F7;

    std::array<uint8_t, 18> out{};
    std::span<uint8_t> const buf{out};

    auto stats = run_hw(
        "ethernet/store_unchecked",
        [&](size_t n) {
            for (size_t k = 0; k < n; ++k) {
                auto written = store_unchecked(buf, frame);
                do_not_optimize(written);
                do_not_optimize(out);
            }
        },
        cfg);
    report("ethernet/store_unchecked", stats);

    std::println("");
}

void bench_eui48()
{
    std::println("=== Eui48 ===\n");

    BenchmarkConfig const cfg{.warmup_iterations = 500, .measurement_iterations = 5000, .batch_size = 100};

    Eui48 const mac{0x00, 0x11, 0x22, 0x33, 0x44, 0x55};

    {
        auto stats = run_hw(
            "eui48/to_uint64",
            [&](size_t n) {
                uint64_t v = 0;
                for (size_t k = 0; k < n; ++k) {
                    v ^= mac.to_uint64();
                }
                do_not_optimize(v);
            },
            cfg);
        report("eui48/to_uint64", stats);
    }

    {
        auto stats = run_hw(
            "eui48/from_uint64",
            [&](size_t n) {
                Eui48 m;
                for (size_t k = 0; k < n; ++k) {
                    m.from_uint64(0x0011223344556677ULL ^ k);
                }
                do_not_optimize(m);
            },
            cfg);
        report("eui48/from_uint64", stats);
    }

    {
        auto stats = run_hw(
            "eui48/is_set",
            [&](size_t n) {
                bool b = false;
                for (size_t k = 0; k < n; ++k) {
                    b ^= mac.is_set();
                }
                do_not_optimize(b);
            },
            cfg);
        report("eui48/is_set", stats);
    }

    std::println("");
}

}  // namespace

auto main() -> int
{
    std::println("StatusBar IEEE Benchmarks (C++)\n");
    std::println("Using HardwareTimer (counter frequency: {} Hz)\n", HardwareTimer::ticks_per_second());

    bench_ethernet_load();
    bench_ethernet_store();
    bench_eui48();

    std::println("Done.");
    return 0;
}
