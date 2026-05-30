// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

/// Raw Ethernet Packet Monitor Example
/// Opens an Ethernet interface and prints incoming packets to stderr.
/// Supports three backends: BPF (macOS + Linux), PACKET_MMAP (Linux), AF_XDP (Linux).
///
/// Note: XDP intercepts packets from the kernel stack — redirected packets
/// are removed from the normal networking path. Unlike BPF and MMAP (which
/// are passive/copy-based), XDP requires an --ethertype filter so that only
/// matching traffic is diverted to the application while everything else
/// passes through to the kernel normally.
///
/// Usage: net_raw_example --interface=<name> [options]
/// Examples:
///   sudo net_raw_example --interface=en0
///   sudo net_raw_example --interface=eth0 --backend=mmap
///   sudo net_raw_example --interface=eth0 --backend=xdp --ethertype=0x88F7  (filter required for xdp)
///   sudo net_raw_example --interface=en0 --verbose --count=100

#include "statusbar/config/config.hpp"
#include "statusbar/ieee/ieee.hpp"
#include "statusbar/itc/itc_stop_token.hpp"
#include "statusbar/net/net.hpp"
#include "statusbar/status/status.hpp"

#include <poll.h>

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <format>
#include <optional>
#include <print>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

using namespace statusbar;
using namespace statusbar::net;

namespace {

struct Config
{
    std::string interface_name;
    std::string backend{"bpf"};
    uint64_t ethertype{0};
    bool verbose{false};
    uint64_t count{0};
};

auto build_arg_specs(Config& config) -> statusbar::args::ArgumentSpecs
{
    statusbar::args::ArgumentSpecs specs;

    specs.add_device("interface", "Network interface (required)", "", [&](auto v) { config.interface_name = std::string{v}; });
    specs.add_choice(
        "backend",
        "Port backend",
#ifdef __linux__
        {"bpf", "mmap", "xdp"},
#else
        {"bpf"},
#endif
        "bpf",
        [&](auto v) { config.backend = std::string{v}; });
    specs.add<uint64_t>("ethertype", "Ethertype filter (hex or decimal, 0 = all)", 0, [&](auto v) { config.ethertype = v; });
    specs.add_flag("verbose", "Print hex dump of first 64 bytes", [&](auto v) { config.verbose = v; });
    specs.add<uint64_t>("count", "Stop after N packets (0 = unlimited)", 0, [&](auto v) { config.count = v; });

    return specs;
}

void print_usage(char const* program_name, statusbar::args::ArgumentSpecs const& specs)
{
    std::println(stderr, "Usage: {} --interface=<name> [options]", program_name);
    std::println(stderr, "");
    std::println(stderr, "Raw Ethernet packet monitor.");
    std::println(stderr, "Captures and displays incoming Ethernet frames.");
    std::println(stderr, "");
    std::println(stderr, "Options:");

    std::string help;
    specs.format_help_to(std::back_inserter(help));
    std::print(stderr, "{}", help);

    std::println(stderr, "Backends:");
    std::println(stderr, "  bpf   - BPF/AF_PACKET copy-based (macOS + Linux, default)");
#ifdef __linux__
    std::println(stderr, "  mmap  - PACKET_MMAP zero-copy ring buffers (Linux only)");
    std::println(stderr, "  xdp   - AF_XDP kernel bypass (Linux only, requires libbpf)");
#endif
    std::println(stderr, "");
    std::println(stderr, "Examples:");
    std::println(stderr, "  sudo {} --interface=en0", program_name);
    std::println(stderr, "  sudo {} --interface=en0 --verbose --count=10", program_name);
#ifdef __linux__
    std::println(stderr, "  sudo {} --interface=eth0 --backend=mmap", program_name);
    std::println(stderr, "  sudo {} --interface=eth0 --backend=xdp --ethertype=0x88F7", program_name);
#endif
}

/// Log a one-line packet summary, optionally with hex dump
auto log_packet(std::span<uint8_t const> data, int64_t timestamp_ns, int64_t& first_timestamp_ns, bool verbose) -> void
{
    if (first_timestamp_ns == 0) {
        first_timestamp_ns = timestamp_ns;
    }
    double rel_sec = static_cast<double>(timestamp_ns - first_timestamp_ns) / 1'000'000'000.0;

    if (data.size() < 14) {
        std::println(stderr, "[{:10.6f}] <short frame {} bytes>", rel_sec, data.size());
        return;
    }

    auto const* dst = data.data();
    auto const* src = data.data() + 6;
    uint16_t const ethertype = static_cast<uint16_t>((data[12] << 8) | data[13]);

    std::println(
        stderr,
        "[{:10.6f}] {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x} -> "
        "{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}  {:6s}  {} bytes",
        rel_sec,
        src[0],
        src[1],
        src[2],
        src[3],
        src[4],
        src[5],
        dst[0],
        dst[1],
        dst[2],
        dst[3],
        dst[4],
        dst[5],
        statusbar::ieee::ethertype_name(ethertype).view(),
        data.size());

    if (verbose) {
        size_t const dump_len = std::min(data.size(), size_t{64});
        for (size_t i = 0; i < dump_len; i += 16) {
            std::print(stderr, "  {:04x}: ", i);
            for (size_t j = i; j < i + 16 && j < dump_len; ++j) {
                std::print(stderr, "{:02x} ", data[j]);
            }
            std::println(stderr, "");
        }
    }
}

/// Generic RX monitor loop — works with any EthernetPort backend.
template <EthernetPort Port>
auto run_monitor(Port& port, statusbar::itc::StopToken& stop, bool verbose, uint64_t max_count) -> int
{
    std::println(stderr, "Monitoring on interface (MAC: {})", statusbar::ieee::to_string(port.hardware_address()).view());
    if (max_count > 0) {
        std::println(stderr, "Will stop after {} packets", max_count);
    }
    std::println(stderr, "Press Ctrl+C to stop");

    int64_t first_timestamp_ns = 0;
    uint64_t packet_count = 0;

    struct pollfd pfd{.fd = port.fd(), .events = POLLIN, .revents = 0};

    while (!stop.stop_requested()) {
        int const ret = ::poll(&pfd, 1, 100);
        if (ret < 0) {
            if (errno == EINTR) {
                continue;
            }
            std::println(stderr, "Error: poll() failed: {}", std::strerror(errno));
            return 1;
        }

        port.begin_cycle();

        while (true) {
            auto guard_result = rx_guard(port);
            if (!guard_result) {
                std::println(stderr, "Error: rx_guard() failed: {}", guard_result.error().message());
                return 1;
            }
            auto& maybe_guard = *guard_result;
            if (!maybe_guard) {
                break;
            }

            log_packet(maybe_guard->buffer(), maybe_guard->timestamp_ns(), first_timestamp_ns, verbose);
            ++packet_count;

            // Guard auto-releases on scope exit; explicit release is optional

            if (max_count > 0 && packet_count >= max_count) {
                std::println(stderr, "\nReached {} packets, stopping.", max_count);
                return 0;
            }
        }

        if (auto s = port.end_cycle(); !s) {
            std::println(stderr, "Error: end_cycle() failed: {}", s.error().message());
            return 1;
        }
    }

    std::println(stderr, "\nStopped. {} packets captured.", packet_count);
    return 0;
}

}  // namespace

auto main(int argc, char** argv) -> int
{
    Config config;
    auto specs = build_arg_specs(config);

    auto cli_result = statusbar::config::parse_cli_args(argc, argv, specs, print_usage);
    if (!cli_result) {
        return statusbar::config::handled_builtin_command(cli_result) ? 0 : 1;
    }

    if (config.interface_name.empty()) {
        std::println(stderr, "Error: --interface is required");
        print_usage(argv[0], specs);
        return 1;
    }

    auto& stop = statusbar::itc::install_stop_signal();

    auto const ethertype = static_cast<uint16_t>(config.ethertype);

    std::println(
        stderr,
        "Backend: {}, interface: {}, ethertype: {}",
        config.backend,
        config.interface_name,
        ethertype == 0 ? std::string{"all"} : std::format("0x{:04x}", ethertype));

    if (config.backend == "bpf") {
        BpfPortContext port;
        BpfPortConfig const port_config{
            .interface_name = config.interface_name,
            .ethertype = ethertype,
        };
        if (auto s = port.open(port_config); !s) {
            std::println(stderr, "Error: Failed to open BPF port: {}", s.error().message());
            return 1;
        }
        return run_monitor(port, stop, config.verbose, config.count);
    }

#ifdef __linux__
    if (config.backend == "mmap") {
        MmapContext port;
        MmapConfig const port_config{
            .interface_name = config.interface_name,
            .ethertype = ethertype,
        };
        if (auto s = port.open(port_config); !s) {
            std::println(stderr, "Error: Failed to open MMAP port: {}", s.error().message());
            return 1;
        }
        return run_monitor(port, stop, config.verbose, config.count);
    }

    if (config.backend == "xdp") {
        if (ethertype == 0) {
            std::println(
                stderr,
                "Error: XDP backend requires --ethertype filter.\n"
                "XDP diverts matching packets from the kernel stack. Without a filter,\n"
                "all traffic would be removed from the kernel, breaking networking.\n"
                "Use --backend=bpf or --backend=mmap for unfiltered monitoring.");
            return 1;
        }
        XdpContext port;
        // XDP intercepts matching packets from the kernel stack.
        // Only redirect the specified ethertype; pass everything else to kernel.
        auto const built =
            ClassifierRuleBuilder<32>{}.ethertype(static_cast<std::uint16_t>(ethertype)).vlan_any().result(flag::process).build();
        XdpConfig const port_config{
            .interface_name = config.interface_name,
            .filter_rules = built,
            .default_action = XdpAction::pass_kernel,
        };
        if (auto s = port.open(port_config); !s) {
            std::println(stderr, "Error: Failed to open XDP port: {}", s.error().message());
            return 1;
        }
        return run_monitor(port, stop, config.verbose, config.count);
    }
#endif

    std::println(stderr, "Error: Unknown backend '{}'", config.backend);
    return 1;
}