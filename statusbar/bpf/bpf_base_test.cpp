// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/bpf/bpf.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <chrono>
#include <cstdlib>
#include <system_error>

using namespace statusbar::bpf;

//
// Static Assertions - Compile-time Verification
//
// Verify constants are sensible
static_assert(ETHERNET_HEADER_MIN_SIZE == 14, "Ethernet header: 6 (dst) + 6 (src) + 2 (ethertype)");
static_assert(bpf_buffer_size == 16384, "Default BPF buffer size is 16KB");
static_assert(bpf_buffer_size >= ETHERNET_HEADER_MIN_SIZE, "Buffer must hold at least one header");
static_assert(bpf_max_device_num == 255, "Darwin BPF devices range from 0-255");
static_assert(bpf_max_device_num > 0, "Must have at least one device");
static_assert(bpf_filter_max_packet == 524288, "Max packet size is 512KB");
static_assert(bpf_filter_max_packet > bpf_buffer_size, "Max packet can exceed default buffer");

// Verify BpfError enum values are distinct and start from 1
static_assert(static_cast<int>(BpfError::device_not_found) == 1, "BpfError should start at 1");
static_assert(static_cast<int>(BpfError::device_busy) == 2, "device_busy should be 2");
static_assert(static_cast<int>(BpfError::callback_exception) == 17, "Last error should be 17");

// Verify BpfStatistics is default constructible with zeros
static_assert(BpfStatistics{}.packets_received == 0, "Default packets_received should be 0");
static_assert(BpfStatistics{}.packets_dropped == 0, "Default packets_dropped should be 0");
static_assert(BpfStatistics{}.read_errors == 0, "Default read_errors should be 0");
static_assert(BpfStatistics{}.callback_errors == 0, "Default callback_errors should be 0");
static_assert(BpfStatistics{}.buffer_overflows == 0, "Default buffer_overflows should be 0");

// Verify AcquisitionTimeAssociation is aggregate-initializable
static_assert(AcquisitionTimeAssociation{100, 200}.bpf_time_ns == 100, "bpf_time_ns initializes correctly");
static_assert(AcquisitionTimeAssociation{100, 200}.monotonic_clock_time_ns == 200, "monotonic_clock_time_ns initializes correctly");

// Verify FilterParams defaults
static_assert(FilterParams{.ethertype = 0x0800}.promiscuous == false, "Default promiscuous should be false");

//
// Error Category and Error Codes Tests
//
TEST(statusbar_bpf, error_category_name)
{
    auto const& cat = bpf_error_category();
    EXPECT_EQ(std::string{cat.name()}, std::string{"statusbar.bpf"});
}

TEST(statusbar_bpf, error_code_device_not_found)
{
    auto ec = make_error_code(BpfError::device_not_found);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"BPF device not found"});
    EXPECT_EQ(ec.value(), 1);
}

TEST(statusbar_bpf, error_code_device_busy)
{
    auto ec = make_error_code(BpfError::device_busy);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"All BPF devices are busy"});
}

TEST(statusbar_bpf, error_code_set_buffer_failed)
{
    auto ec = make_error_code(BpfError::set_buffer_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to set BPF buffer size"});
}

TEST(statusbar_bpf, error_code_set_interface_failed)
{
    auto ec = make_error_code(BpfError::set_interface_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to set network interface"});
}

TEST(statusbar_bpf, error_code_set_immediate_failed)
{
    auto ec = make_error_code(BpfError::set_immediate_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to set immediate mode"});
}

TEST(statusbar_bpf, error_code_set_filter_failed)
{
    auto ec = make_error_code(BpfError::set_filter_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to set BPF filter"});
}

TEST(statusbar_bpf, error_code_set_promiscuous_failed)
{
    auto ec = make_error_code(BpfError::set_promiscuous_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to set promiscuous mode"});
}

TEST(statusbar_bpf, error_code_set_blocking_failed)
{
    auto ec = make_error_code(BpfError::set_blocking_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to set blocking mode"});
}

TEST(statusbar_bpf, error_code_read_failed)
{
    auto ec = make_error_code(BpfError::read_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to read from BPF device"});
}

TEST(statusbar_bpf, error_code_clock_failed)
{
    auto ec = make_error_code(BpfError::clock_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to get system time"});
}

TEST(statusbar_bpf, error_code_invalid_packet)
{
    auto ec = make_error_code(BpfError::invalid_packet);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Invalid packet received"});
}

TEST(statusbar_bpf, error_code_buffer_overflow)
{
    auto ec = make_error_code(BpfError::buffer_overflow);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Buffer overflow detected"});
}

TEST(statusbar_bpf, error_code_interface_not_found)
{
    auto ec = make_error_code(BpfError::interface_not_found);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Network interface not found"});
}

TEST(statusbar_bpf, error_code_socket_creation_failed)
{
    auto ec = make_error_code(BpfError::socket_creation_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to create socket"});
}

TEST(statusbar_bpf, error_code_bind_failed)
{
    auto ec = make_error_code(BpfError::bind_failed);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Failed to bind socket"});
}

TEST(statusbar_bpf, error_code_invalid_file_descriptor)
{
    auto ec = make_error_code(BpfError::invalid_file_descriptor);
    EXPECT_TRUE(ec);
    EXPECT_EQ(ec.message(), std::string{"Invalid file descriptor"});
}

//
// FileDescriptor RAII Tests
//
TEST(statusbar_bpf, file_descriptor_default_construction)
{
    FileDescriptor fd;
    EXPECT_EQ(fd.get(), -1);
    EXPECT_FALSE(fd.valid());
}

TEST(statusbar_bpf, file_descriptor_construction_with_valid_fd)
{
    FileDescriptor fd{42};
    EXPECT_EQ(fd.get(), 42);
    EXPECT_TRUE(fd.valid());
    // Don't actually close - it's not a real fd
    (void)fd.release();
}

TEST(statusbar_bpf, file_descriptor_construction_with_invalid_fd)
{
    FileDescriptor fd{-1};
    EXPECT_EQ(fd.get(), -1);
    EXPECT_FALSE(fd.valid());
}

TEST(statusbar_bpf, file_descriptor_move_construction)
{
    FileDescriptor fd1{42};
    FileDescriptor fd2{std::move(fd1)};

    // Intentionally accessing moved-from object to verify it's in expected state (-1)
    EXPECT_EQ(fd1.get(), -1);   // NOLINT(clang-analyzer-cplusplus.Move)
    EXPECT_FALSE(fd1.valid());  // NOLINT(clang-analyzer-cplusplus.Move)
    EXPECT_EQ(fd2.get(), 42);
    EXPECT_TRUE(fd2.valid());

    (void)fd2.release();
}

TEST(statusbar_bpf, file_descriptor_move_assignment)
{
    FileDescriptor fd1{42};
    FileDescriptor fd2{99};

    fd2 = std::move(fd1);

    // Intentionally accessing moved-from object to verify it's in expected state (-1)
    EXPECT_EQ(fd1.get(), -1);   // NOLINT(clang-analyzer-cplusplus.Move)
    EXPECT_FALSE(fd1.valid());  // NOLINT(clang-analyzer-cplusplus.Move)
    EXPECT_EQ(fd2.get(), 42);
    EXPECT_TRUE(fd2.valid());

    (void)fd2.release();
}

TEST(statusbar_bpf, file_descriptor_self_move_assignment)
{
    FileDescriptor fd{42};

    // Self-assignment should be safe
    fd = std::move(fd);

    EXPECT_EQ(fd.get(), 42);
    EXPECT_TRUE(fd.valid());

    (void)fd.release();
}

TEST(statusbar_bpf, file_descriptor_release)
{
    FileDescriptor fd{42};
    int released = fd.release();

    EXPECT_EQ(released, 42);
    EXPECT_EQ(fd.get(), -1);
    EXPECT_FALSE(fd.valid());
}

TEST(statusbar_bpf, file_descriptor_close)
{
    FileDescriptor fd{42};
    (void)fd.release();  // Prevent actual close of fake fd
    fd.close();

    EXPECT_EQ(fd.get(), -1);
    EXPECT_FALSE(fd.valid());
}

//
// Data Structure Tests
//
TEST(statusbar_bpf, acquisition_time_association_construction)
{
    AcquisitionTimeAssociation ata{1000, 2000};
    EXPECT_EQ(ata.bpf_time_ns, 1000);
    EXPECT_EQ(ata.monotonic_clock_time_ns, 2000);
}

TEST(statusbar_bpf, bpf_statistics_default_construction)
{
    BpfStatistics stats{};
    EXPECT_EQ(stats.packets_received, 0U);
    EXPECT_EQ(stats.packets_dropped, 0U);
    EXPECT_EQ(stats.read_errors, 0U);
    EXPECT_EQ(stats.callback_errors, 0U);
    EXPECT_EQ(stats.buffer_overflows, 0U);
}

TEST(statusbar_bpf, bpf_statistics_initialization)
{
    BpfStatistics stats{
        .packets_received = 100, .packets_dropped = 5, .read_errors = 2, .callback_errors = 1, .buffer_overflows = 3};

    EXPECT_EQ(stats.packets_received, 100U);
    EXPECT_EQ(stats.packets_dropped, 5U);
    EXPECT_EQ(stats.read_errors, 2U);
    EXPECT_EQ(stats.callback_errors, 1U);
    EXPECT_EQ(stats.buffer_overflows, 3U);
}

TEST(statusbar_bpf, filter_params_construction)
{
    FilterParams params{
        .ethertype = 0x88f7, .promiscuous = true, .buffer_size = 32768, .read_timeout = std::chrono::milliseconds{100}};

    EXPECT_EQ(params.ethertype, 0x88f7);
    EXPECT_TRUE(params.promiscuous);
    EXPECT_TRUE(params.buffer_size.has_value());
    EXPECT_EQ(params.buffer_size.value(), 32768U);
    EXPECT_TRUE(params.read_timeout.has_value());
    EXPECT_EQ(params.read_timeout.value().count(), 100);
}

TEST(statusbar_bpf, filter_params_defaults)
{
    FilterParams params{.ethertype = 0x0800};

    EXPECT_EQ(params.ethertype, 0x0800);
    EXPECT_FALSE(params.promiscuous);
    EXPECT_FALSE(params.buffer_size.has_value());
    EXPECT_FALSE(params.read_timeout.has_value());
}

//
// Constant Values Tests
//
TEST(statusbar_bpf, constants_ethernet_header_min_size)
{
    EXPECT_EQ(ETHERNET_HEADER_MIN_SIZE, 14U);
}

TEST(statusbar_bpf, constants_bpf_buffer_size)
{
    EXPECT_EQ(bpf_buffer_size, 16384U);
}

TEST(statusbar_bpf, constants_bpf_max_device_num)
{
    EXPECT_EQ(bpf_max_device_num, 255);
}

TEST(statusbar_bpf, constants_bpf_filter_max_packet)
{
    EXPECT_EQ(bpf_filter_max_packet, 524288U);
}

//
// Error Code Edge Cases
//
TEST(bpf_error_edge, unknown_error_code)
{
    // Create an error code with an invalid enum value
    std::error_code ec{999, bpf_error_category()};
    // Should return "Unknown BPF error" for invalid codes
    EXPECT_EQ(ec.message(), std::string{"Unknown BPF error"});
}

TEST(bpf_error_edge, error_code_comparison)
{
    auto ec1 = make_error_code(BpfError::device_not_found);
    auto ec2 = make_error_code(BpfError::device_not_found);
    auto ec3 = make_error_code(BpfError::device_busy);

    EXPECT_TRUE(ec1 == ec2);
    EXPECT_FALSE(ec1 == ec3);
}

TEST(bpf_error_edge, error_code_category_same)
{
    auto ec1 = make_error_code(BpfError::read_failed);
    auto ec2 = make_error_code(BpfError::clock_failed);

    // Both should use the same category
    EXPECT_TRUE(&ec1.category() == &ec2.category());
    EXPECT_EQ(std::string{ec1.category().name()}, "statusbar.bpf");
}

TEST(bpf_error_edge, all_error_codes_have_messages)
{
    // Verify every BpfError has a non-empty message
    for (int i = 1; i <= 17; ++i) {
        std::error_code ec{i, bpf_error_category()};
        EXPECT_FALSE(ec.message().empty());
        EXPECT_NE(ec.message(), "Unknown BPF error");
    }
}

//
// FileDescriptor Edge Cases
//
TEST(bpf_fd_edge, double_close_safe)
{
    FileDescriptor fd{42};
    (void)fd.release();  // Release to prevent actual close
    fd.close();          // First close
    fd.close();          // Second close should be safe (already -1)

    EXPECT_FALSE(fd.valid());
    EXPECT_EQ(fd.get(), -1);
}

TEST(bpf_fd_edge, release_invalid_returns_minus_one)
{
    FileDescriptor fd;  // Default construction (-1)
    int released = fd.release();

    EXPECT_EQ(released, -1);
    EXPECT_FALSE(fd.valid());
}

TEST(bpf_fd_edge, move_from_invalid)
{
    FileDescriptor fd1;  // Invalid
    FileDescriptor fd2{std::move(fd1)};

    // Both should be invalid after moving from invalid
    EXPECT_FALSE(fd1.valid());  // NOLINT(clang-analyzer-cplusplus.Move)
    EXPECT_FALSE(fd2.valid());
    EXPECT_EQ(fd2.get(), -1);
}

//
// FilterParams Edge Cases
//
TEST(bpf_filter_edge, common_ethertypes)
{
    // Verify common EtherTypes can be configured
    FilterParams ipv4{.ethertype = 0x0800};
    FilterParams ipv6{.ethertype = 0x86DD};
    FilterParams arp{.ethertype = 0x0806};
    FilterParams ptp{.ethertype = 0x88F7};
    FilterParams avtp{.ethertype = 0x22F0};

    EXPECT_EQ(ipv4.ethertype, 0x0800);
    EXPECT_EQ(ipv6.ethertype, 0x86DD);
    EXPECT_EQ(arp.ethertype, 0x0806);
    EXPECT_EQ(ptp.ethertype, 0x88F7);
    EXPECT_EQ(avtp.ethertype, 0x22F0);
}

TEST(bpf_filter_edge, zero_timeout)
{
    FilterParams params{.ethertype = 0x0800, .read_timeout = std::chrono::milliseconds{0}};

    EXPECT_TRUE(params.read_timeout.has_value());
    EXPECT_EQ(params.read_timeout.value().count(), 0);
}

TEST(bpf_filter_edge, large_buffer_size)
{
    FilterParams params{.ethertype = 0x0800, .buffer_size = 1024 * 1024};  // 1MB

    EXPECT_TRUE(params.buffer_size.has_value());
    EXPECT_EQ(params.buffer_size.value(), 1024U * 1024U);
}

//
// BpfStatistics Edge Cases
//
TEST(bpf_stats_edge, increment_counters)
{
    BpfStatistics stats{};

    stats.packets_received++;
    stats.packets_dropped += 5;
    stats.read_errors = 10;

    EXPECT_EQ(stats.packets_received, 1U);
    EXPECT_EQ(stats.packets_dropped, 5U);
    EXPECT_EQ(stats.read_errors, 10U);
}

TEST(bpf_stats_edge, large_counters)
{
    BpfStatistics stats{
        .packets_received = 1'000'000'000ULL,
        .packets_dropped = 500'000'000ULL,
    };

    EXPECT_EQ(stats.packets_received, 1'000'000'000ULL);
    EXPECT_EQ(stats.packets_dropped, 500'000'000ULL);
}

//
// BpfDeviceBase accessor tests
//
// Minimal concrete subclass for testing (receive_pdus is pure virtual)
class TestBpfDevice : public BpfDeviceBase
{
  public:
    using BpfDeviceBase::BpfDeviceBase;
    [[nodiscard]] auto receive_pdus() -> statusbar::Status override { return statusbar::success(); }
};

TEST(bpf_device_base, construction_invalid_fd)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x22F0}, -1};
    EXPECT_FALSE(dev.is_valid());
    EXPECT_EQ(dev.file_descriptor(), -1);
    EXPECT_EQ(dev.network_device(), "eth0");
    EXPECT_EQ(dev.ethertype(), 0x22F0);
}

TEST(bpf_device_base, construction_with_fd)
{
    // Use a fake fd — BpfDeviceBase stores it but doesn't use it until receive_pdus
    TestBpfDevice dev{"en0", FilterParams{.ethertype = 0x88F7, .promiscuous = true}, 42};
    EXPECT_TRUE(dev.is_valid());
    EXPECT_EQ(dev.file_descriptor(), 42);
    EXPECT_EQ(dev.network_device(), "en0");
    EXPECT_EQ(dev.ethertype(), 0x88F7);
    EXPECT_TRUE(dev.filter_params().promiscuous);
}

TEST(bpf_device_base, statistics_default_zero)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x0800}, -1};
    auto const& stats = dev.statistics();
    EXPECT_EQ(stats.packets_received, 0U);
    EXPECT_EQ(stats.packets_dropped, 0U);
    EXPECT_EQ(stats.read_errors, 0U);
    EXPECT_EQ(stats.callback_errors, 0U);
    EXPECT_EQ(stats.buffer_overflows, 0U);
}

TEST(bpf_device_base, reset_statistics)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x0800}, -1};
    // Can't directly modify stats from outside, but reset should work
    dev.reset_statistics();
    auto const& stats = dev.statistics();
    EXPECT_EQ(stats.packets_received, 0U);
}

TEST(bpf_device_base, filter_params_accessible)
{
    FilterParams params{
        .ethertype = 0x88F7,
        .promiscuous = true,
        .buffer_size = 32768,
        .read_timeout = std::chrono::milliseconds{50},
    };
    TestBpfDevice dev{"eth0", params, -1};
    auto const& fp = dev.filter_params();
    EXPECT_EQ(fp.ethertype, 0x88F7);
    EXPECT_TRUE(fp.promiscuous);
    EXPECT_TRUE(fp.buffer_size.has_value());
    EXPECT_EQ(fp.buffer_size.value(), 32768U);
    EXPECT_TRUE(fp.read_timeout.has_value());
    EXPECT_EQ(fp.read_timeout.value().count(), 50);
}

TEST(bpf_device_base, set_callback)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x0800}, -1};
    bool called = false;
    dev.set_callback([&](std::span<uint8_t const>, AcquisitionTimeAssociation const) { called = true; });

    // Verify callback is set (invoke it directly)
    auto const& cb = dev.callback();
    EXPECT_TRUE(cb);
    cb({}, AcquisitionTimeAssociation{0, 0});
    EXPECT_TRUE(called);
}

TEST(bpf_device_base, callback_default_empty)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x0800}, -1};
    auto const& cb = dev.callback();
    EXPECT_FALSE(cb);
}

TEST(bpf_device_base, is_blocking_default)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x0800}, -1};
    EXPECT_TRUE(dev.is_blocking());
}

TEST(bpf_device_base, receive_pdus_succeeds)
{
    TestBpfDevice dev{"eth0", FilterParams{.ethertype = 0x0800}, -1};
    auto result = dev.receive_pdus();
    EXPECT_TRUE(result.has_value());
}

//
// Main test runner
//

TEST_MAIN(statusbar_bpf, bpf_base_test)