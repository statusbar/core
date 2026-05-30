// Copyright 2026 Jeff Koftinoff <jeff.koftinoff@statusbar.com>
// SPDX-License-Identifier: MIT

#include "statusbar/buffer/buffer.hpp"
#include "statusbar/test/test.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <system_error>
#include <vector>

using namespace statusbar;

struct S
{
    int x;
    double y;
};

union U
{
    int x;
    float y;
};

static_assert(PlainType<int>);
static_assert(PlainType<double const>);
static_assert(PlainType<unsigned long long>);
static_assert(PlainType<std::uint64_t>);
#if defined(__SIZEOF_INT128__)
// __int128 / __uint128_t are compiler extensions: std::is_arithmetic_v
// rejects them under a strict -std=c++NN dialect (GCC, and Clang outside
// GNU mode), so PlainElement accepts them explicitly via is_extended_integer_v.
static_assert(PlainType<__uint128_t>);
static_assert(PlainType<__int128_t>);
#endif
static_assert(PlainType<std::array<int, 4>>);
static_assert(PlainLinearCollection<std::array<int, 4>>);
static_assert(PlainType<std::array<double, 2> const>);
static_assert(PlainType<int[4]>);
static_assert(PlainLinearCollection<double[8]>);
static_assert(PlainType<float const[8]>);

static_assert(!PlainType<int*>);
static_assert(!PlainType<float*>);
static_assert(!PlainType<unsigned char*>);
static_assert(!PlainType<std::array<int*, 4>>);
static_assert(!PlainType<S>);
static_assert(!PlainType<U>);
static_assert(PlainLinearCollection<std::span<uint8_t>>);
static_assert(PlainType<std::span<uint8_t>>);
static_assert(!PlainType<std::span<float*>>);
static_assert(!PlainType<std::span<float*, 10>>);
static_assert(PlainType<std::span<uint8_t, 10>>);
static_assert(PlainStdVector<std::vector<float>>);
static_assert(!PlainStdVector<std::vector<uint8_t const*>>);
static_assert(!PlainType<std::vector<float>>);
static_assert(!PlainType<std::vector<uint8_t*>>);

static_assert(traits::PlainInplaceVector<statusbar::sg14::inplace_vector<float, 8>>);
static_assert(traits::PlainInplaceVector<statusbar::sg14::inplace_vector<uint32_t, 4>>);
static_assert(!traits::PlainInplaceVector<statusbar::sg14::inplace_vector<uint8_t const*, 4>>);
static_assert(traits::PlainSizedContiguous<std::vector<float>>);
static_assert(traits::PlainSizedContiguous<statusbar::sg14::inplace_vector<float, 8>>);
static_assert(!traits::PlainSizedContiguous<std::array<float, 8>>);

TEST(traits, general)
{
    static_assert(!PlainType<S>);
    EXPECT_TRUE(true);
}

TEST_MAIN(statusbar_buffer, buffer_traits_test)