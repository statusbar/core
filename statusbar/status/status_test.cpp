// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/status/status.hpp"

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/status/catch_or_status.hpp"
#include "statusbar/test/test.hpp"

#include <cstdlib>
#include <expected>
#include <stdexcept>
#include <string>
#include <system_error>

using namespace statusbar;

//
// Status (void) Tests
//
TEST(statusbar_status, status_success_construction)
{
    Status s = success();
    EXPECT_TRUE(s);
    EXPECT_TRUE(s.has_value());
}

TEST(statusbar_status, status_failure_construction_error_code)
{
    Status s = failure(std::make_error_code(std::errc::invalid_argument));
    EXPECT_FALSE(s);
    EXPECT_FALSE(s.has_value());
    EXPECT_EQ(s.error(), std::errc::invalid_argument);
}

TEST(statusbar_status, status_failure_construction_errc)
{
    Status s = failure(std::errc::permission_denied);
    EXPECT_FALSE(s);
    EXPECT_EQ(s.error(), std::errc::permission_denied);
}

TEST(statusbar_status, status_bool_conversion)
{
    Status success_status = success();
    Status failure_status = failure(std::errc::io_error);

    if (success_status) {
        // Success path
    } else {
        EXPECT_TRUE(false);  // Should not reach here
    }

    if (!failure_status) {
        // Expected failure path
    } else {
        EXPECT_TRUE(false);  // Should not reach here
    }
}

//
// StatusValue<T> Tests
//
TEST(statusbar_status, status_value_success_construction)
{
    StatusValue<int> sv = success(42);
    EXPECT_TRUE(sv);
    EXPECT_TRUE(sv.has_value());
    EXPECT_EQ(sv.value(), 42);
    EXPECT_EQ(*sv, 42);
}

TEST(statusbar_status, status_value_success_string)
{
    StatusValue<std::string> sv = success(std::string{"hello"});
    EXPECT_TRUE(sv);
    EXPECT_EQ(sv.value(), "hello");
}

TEST(statusbar_status, status_value_failure_construction_error_code)
{
    StatusValue<int> sv = failure(std::make_error_code(std::errc::invalid_argument));
    EXPECT_FALSE(sv);
    EXPECT_FALSE(sv.has_value());
    EXPECT_EQ(sv.error(), std::errc::invalid_argument);
}

TEST(statusbar_status, status_value_failure_construction_errc)
{
    StatusValue<double> sv = failure(std::errc::no_such_file_or_directory);
    EXPECT_FALSE(sv);
    EXPECT_EQ(sv.error(), std::errc::no_such_file_or_directory);
}

//
// Query Helpers Tests
//
TEST(statusbar_status, is_success_query)
{
    Status success_status = success();
    Status failure_status = failure(std::errc::io_error);

    EXPECT_TRUE(is_success(success_status));
    EXPECT_FALSE(is_success(failure_status));

    StatusValue<int> success_value = success(100);
    StatusValue<int> failure_value = failure(std::errc::io_error);

    EXPECT_TRUE(is_success(success_value));
    EXPECT_FALSE(is_success(failure_value));
}

TEST(statusbar_status, is_failure_query)
{
    Status success_status = success();
    Status failure_status = failure(std::errc::io_error);

    EXPECT_FALSE(is_failure(success_status));
    EXPECT_TRUE(is_failure(failure_status));

    StatusValue<int> success_value = success(100);
    StatusValue<int> failure_value = failure(std::errc::io_error);

    EXPECT_FALSE(is_failure(success_value));
    EXPECT_TRUE(is_failure(failure_value));
}

//
// Error Forwarding Tests
//
TEST(statusbar_status, forward_failure_int_to_double)
{
    StatusValue<int> int_failure = failure(std::errc::invalid_argument);
    StatusValue<double> double_failure = forward_failure(int_failure);

    EXPECT_FALSE(double_failure);
    EXPECT_EQ(double_failure.error(), std::errc::invalid_argument);
}

TEST(statusbar_status, forward_failure_preserves_error_code)
{
    StatusValue<std::string> str_failure = failure(std::errc::no_such_file_or_directory);
    StatusValue<int> int_failure = forward_failure(str_failure);

    EXPECT_FALSE(int_failure);
    EXPECT_EQ(int_failure.error(), str_failure.error());
}

//
// Monadic Operations — delegated to std::expected's built-in
// and_then / or_else / transform / value_or — no project-level tests.
//
// Edge Cases Tests
//
TEST(statusbar_status, empty_string_value)
{
    StatusValue<std::string> sv = success(std::string{});
    EXPECT_TRUE(sv);
    EXPECT_EQ(sv.value(), "");
    EXPECT_TRUE(sv.value().empty());
}

TEST(statusbar_status, zero_value)
{
    StatusValue<int> sv = success(0);
    EXPECT_TRUE(sv);
    EXPECT_EQ(sv.value(), 0);
    // Ensure 0 is not confused with failure
    EXPECT_TRUE(is_success(sv));
    EXPECT_FALSE(is_failure(sv));
}

TEST(statusbar_status, multiple_error_forwards)
{
    // Test forwarding errors through multiple type changes
    StatusValue<int> int_fail = failure(std::errc::permission_denied);
    StatusValue<double> double_fail = forward_failure(int_fail);
    StatusValue<std::string> string_fail = forward_failure(double_fail);

    EXPECT_FALSE(string_fail);
    EXPECT_EQ(string_fail.error(), std::errc::permission_denied);
}

//
// forward_failure from Status (void) Tests
//
TEST(statusbar_status, forward_failure_from_status_void)
{
    // Test forward_failure from Status (void expected)
    Status void_fail = failure(std::errc::address_in_use);
    auto forwarded = forward_failure(void_fail);

    // forwarded is std::unexpected<std::error_code>
    EXPECT_EQ(forwarded.error(), std::errc::address_in_use);
}

//
// Custom Error Code Enum Tests (using BufferError as example)
//
TEST(statusbar_status, failure_custom_enum_implicit)
{
    // Test failure(CustomEnum) -> std::unexpected (implicit conversion)
    Status s = failure(BufferError::insufficient_data);
    EXPECT_FALSE(s);
    // Error code category should be buffer error category
    EXPECT_EQ(s.error().category().name(), std::string_view("statusbar.buffer"));
}

TEST(statusbar_status, failure_custom_enum_preserves_value)
{
    // Verify the error value is preserved through the conversion
    Status s1 = failure(BufferError::insufficient_data);
    Status s2 = failure(BufferError::insufficient_space);

    // Different error values should produce different error codes
    EXPECT_NE(s1.error().value(), s2.error().value());

    // Error messages should be different
    EXPECT_NE(s1.error().message(), s2.error().message());
}

//
// catch_or_status / run_guarded Tests
//
TEST(statusbar_status, catch_or_status_forwards_success)
{
    StatusValue<int> r = catch_or_status([]() -> StatusValue<int> { return success(42); }, std::errc::io_error);
    EXPECT_TRUE(r);
    EXPECT_EQ(*r, 42);
}

TEST(statusbar_status, catch_or_status_forwards_value_status)
{
    Status r = catch_or_status([]() -> Status { return success(); }, std::errc::io_error);
    EXPECT_TRUE(r);
}

TEST(statusbar_status, catch_or_status_forwards_failure_unchanged)
{
    StatusValue<int> r =
        catch_or_status([]() -> StatusValue<int> { return failure(std::errc::permission_denied); }, std::errc::io_error);
    EXPECT_FALSE(r);
    EXPECT_EQ(r.error(), std::errc::permission_denied);
}

#if __cpp_exceptions
TEST(statusbar_status, catch_or_status_maps_exception_to_ec)
{
    StatusValue<int> r =
        catch_or_status([]() -> StatusValue<int> { throw std::runtime_error("boom"); }, std::errc::invalid_argument);
    EXPECT_FALSE(r);
    EXPECT_EQ(r.error(), std::errc::invalid_argument);
}

TEST(statusbar_status, catch_or_status_preserves_system_error_code)
{
    // A std::system_error keeps its code() — round-trips throw_or_abort(ec).
    auto const original = std::make_error_code(std::errc::no_such_file_or_directory);
    StatusValue<int> r =
        catch_or_status([original]() -> StatusValue<int> { throw std::system_error(original); }, std::errc::io_error);
    EXPECT_FALSE(r);
    EXPECT_EQ(r.error(), original);
}

TEST(statusbar_status, run_guarded_swallows_exception)
{
    // A throwing body must not propagate (which would std::terminate).
    bool ran = false;
    run_guarded("test thread", [&]() {
        ran = true;
        throw std::runtime_error("boom");
    });
    EXPECT_TRUE(ran);  // reached the throw, and control returned here normally
}
#endif

TEST(statusbar_status, run_guarded_runs_normal_body)
{
    int value = 0;
    run_guarded("test thread", [&]() { value = 7; });
    EXPECT_EQ(value, 7);
}

//
// Main test runner
//

TEST_MAIN(statusbar_status, status_test)