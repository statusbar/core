// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT
//
// Buffer benchmark tool — exercises the core serialize/deserialize primitives.
// FastCompiledDeserializer in particular is designed for zero-overhead parsing;
// this tool makes that claim measurable.
//
// Build: make build
// Run:   ./build/build-Release/statusbar/statusbar-buffer-bench

#include "statusbar/benchmark/benchmark.hpp"
#include "statusbar/buffer/buffer.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <print>
#include <span>
#include <vector>

using namespace statusbar::benchmark;
using namespace statusbar;

namespace {

struct PacketHeader
{
    uint8_t type;
    uint8_t flags;
    uint16_t length;
    uint32_t timestamp;
    uint32_t sequence;
    uint16_t checksum;
};

auto make_packet_bytes() -> std::array<uint8_t, sizeof(PacketHeader)>
{
    std::array<uint8_t, sizeof(PacketHeader)> bytes{};
    for (size_t i = 0; i < bytes.size(); ++i) {
        bytes[i] = static_cast<uint8_t>(i + 1);
    }
    return bytes;
}

void bench_hand_written_memcpy()
{
    // Baseline: direct memcpy from a byte buffer into a struct.
    // Establishes the lower bound that compiled deserializers are aiming at.
    std::println("=== Hand-written memcpy (baseline) ===\n");

    BenchmarkConfig const cfg{.warmup_iterations = 500, .measurement_iterations = 5000, .batch_size = 100};

    auto const packet_bytes = make_packet_bytes();

    auto stats = run_hw(
        "hand_written/memcpy_packet_header",
        [&](size_t n) {
            for (size_t k = 0; k < n; ++k) {
                PacketHeader hdr{};
                std::memcpy(&hdr, packet_bytes.data(), sizeof(PacketHeader));
                do_not_optimize(hdr);
            }
        },
        cfg);
    report("hand_written/memcpy_packet_header", stats);

    std::println("");
}

void bench_callback_compiled_deserializer()
{
    std::println("=== CompiledDeserializer (callback) ===\n");

    BenchmarkConfig const cfg{.warmup_iterations = 500, .measurement_iterations = 5000, .batch_size = 100};

    auto const packet_bytes = make_packet_bytes();
    std::span<uint8_t const> const buf{packet_bytes};

    uint8_t type = 0;
    uint8_t flags = 0;
    uint16_t length = 0;
    uint32_t timestamp = 0;
    uint32_t sequence = 0;
    uint16_t checksum = 0;

    auto const deserializer = make_deserializer(
        field<uint8_t>([&type](uint8_t v) { type = v; }),
        field<uint8_t>([&flags](uint8_t v) { flags = v; }),
        field<uint16_t>([&length](uint16_t v) { length = v; }),
        field<uint32_t>([&timestamp](uint32_t v) { timestamp = v; }),
        field<uint32_t>([&sequence](uint32_t v) { sequence = v; }),
        field<uint16_t>([&checksum](uint16_t v) { checksum = v; }));

    auto stats = run_hw(
        "callback_compiled/parse_packet_header",
        [&](size_t n) {
            for (size_t k = 0; k < n; ++k) {
                auto result = deserializer.parse(buf);
                do_not_optimize(result);
            }
            do_not_optimize(type);
            do_not_optimize(flags);
            do_not_optimize(length);
            do_not_optimize(timestamp);
            do_not_optimize(sequence);
            do_not_optimize(checksum);
        },
        cfg);
    report("callback_compiled/parse_packet_header", stats);

    std::println("");
}

void bench_mutable_buffer_append()
{
    std::println("=== MutableBuffer Append ===\n");

    BenchmarkConfig const cfg{.warmup_iterations = 500, .measurement_iterations = 5000, .batch_size = 100};

    for (size_t chunk_size : {4UL, 16UL, 64UL, 256UL}) {
        std::vector<uint8_t> storage(4096);
        std::vector<uint8_t> chunk(chunk_size, 0xAB);
        auto name = std::format("mutable_buffer/append/{}", chunk_size);
        auto stats = run_hw(
            name,
            [&](size_t n) {
                for (size_t k = 0; k < n; ++k) {
                    MutableBuffer mb{std::span<uint8_t>{storage}};
                    size_t written = 0;
                    while (written + chunk_size <= storage.size()) {
                        auto status = mb.append(std::span<uint8_t const>{chunk});
                        do_not_optimize(status);
                        written += chunk_size;
                    }
                }
                do_not_optimize(storage.data());
            },
            cfg);
        report(name, stats);
    }

    std::println("");
}

}  // namespace

auto main() -> int
{
    std::println("StatusBar Buffer Benchmarks (C++)\n");
    std::println("Using HardwareTimer (counter frequency: {} Hz)\n", HardwareTimer::ticks_per_second());

    bench_hand_written_memcpy();
    bench_callback_compiled_deserializer();
    bench_mutable_buffer_append();

    std::println("Done.");
    return 0;
}
